#pragma once
// ============================================================================
// ConfigStore.h — Auto-persist every menu setting to %AppData%\NightSharp\config\
//
// Design (approved brainstorm 2026-07-08):
//   - Generic: walks the SDK::UI menu tree, so EVERY plugin persists with zero
//     per-plugin code. New plugins get persistence for free.
//   - One INI file per root menu: <internalId>.ini, and for Champion plugins the
//     currently-played champion is appended: <internalId>.<champion>.ini so that
//     multi-champion AIOs (7UPAIO, SharpShooterAIO) keep a separate config per
//     champion instead of overwriting each other (they share one root Name).
//   - Global/core settings (ZoomHack, SkinChanger, PermaShow, Language, ...) live
//     in core.ini.
//   - Auto-save on change, debounced (~0.7s). Load: core at startup, each root
//     when it attaches to MenuManager.
//   - WinAPI-only file IO + hand-rolled INI (no JSON, manual-map friendly).
//
// Wiring (see PluginBootstrap.h / NightSharpMenu.h):
//   ConfigStore::Init(&championProvider);   // installs hooks + LoadCore()
//   ConfigStore::Tick();                     // called every frame (Render)
//   ConfigStore::FlushAll();                 // on shutdown (best effort)
// ============================================================================

#include <Windows.h>
#include <shlobj.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../SDK/UI/UI.h"
#include "MenuConfig.h"
#include "../Plugins/PluginRegistry.h"
#include "../DebugLog.h"

namespace ConfigStore {

    using SDK::UI::AMenuComponent;
    using SDK::UI::Menu;
    using SDK::UI::MenuItem;
    using SDK::UI::MenuManager;
    using SDK::UI::MenuValueType;

    // ── State ───────────────────────────────────────────────────────────────
    inline const char* (*g_ChampionNameFn)() = nullptr; // set by bootstrap
    inline bool  g_applying   = false;   // suppress dirty-marking while loading
    inline bool  g_coreLoaded = false;   // core.ini has been read at least once

    constexpr DWORD kDebounceMs = 700;
    inline bool  g_menuDirty   = false;
    inline DWORD g_menuDueTick  = 0;
    inline bool  g_coreDirty   = false;
    inline DWORD g_coreDueTick  = 0;

    // ── Path helpers ────────────────────────────────────────────────────────
    inline void ConfigDir(char* out, int maxLen) {
        char appData[MAX_PATH] = {};
        SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData);
        char parent[MAX_PATH] = {};
        wsprintfA(parent, "%s\\NightSharp", appData);
        CreateDirectoryA(parent, nullptr);
        wsprintfA(out, "%s\\config", parent);
        CreateDirectoryA(out, nullptr);
        (void)maxLen;
    }

    // Keep only [a-z0-9._-]; lower-case; collapse runs of illegal chars to '_'.
    inline std::string SanitizeFileName(const std::string& in) {
        std::string r;
        for (char c : in) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                            c == '.' || c == '-' || c == '_';
            if (ok) {
                r += c;
            } else if (!r.empty() && r.back() != '_') {
                r += '_';
            }
        }
        while (!r.empty() && (r.back() == '_' || r.back() == '.')) r.pop_back();
        return r.empty() ? std::string("plugin") : r;
    }

    // File key for a root menu: internalId, plus ".<champion>" for Champion plugins.
    inline std::string FileKeyForRoot(Menu* root) {
        std::string key = root->Name.c_str();

        const int idx = PluginRegistry::FindByInternalId(root->Name.c_str());
        const bool isChampion =
            idx >= 0 &&
            PluginRegistry::Plugins[idx].Category ==
                PluginRegistry::PluginCategory::Champion;

        if (isChampion && g_ChampionNameFn) {
            const char* champ = g_ChampionNameFn();
            if (champ && champ[0]) {
                key += ".";
                key += champ;
            }
        }
        return SanitizeFileName(key);
    }

    inline std::string RootFilePath(Menu* root) {
        char dir[MAX_PATH] = {};
        ConfigDir(dir, MAX_PATH);
        return std::string(dir) + "\\" + FileKeyForRoot(root) + ".ini";
    }

    inline std::string CoreFilePath() {
        char dir[MAX_PATH] = {};
        ConfigDir(dir, MAX_PATH);
        return std::string(dir) + "\\core.ini";
    }

    // ── Tiny INI model ──────────────────────────────────────────────────────
    struct IniKV  { std::string key; std::string val; };
    struct IniSec { std::string name; std::vector<IniKV> kvs; };
    struct IniDoc {
        std::vector<IniSec> secs;

        IniSec& Section(const std::string& name) {
            for (auto& s : secs) if (s.name == name) return s;
            secs.push_back(IniSec{ name, {} });
            return secs.back();
        }
        void Set(const std::string& sec, const std::string& key, const std::string& val) {
            IniSec& s = Section(sec);
            for (auto& kv : s.kvs) if (kv.key == key) { kv.val = val; return; }
            s.kvs.push_back(IniKV{ key, val });
        }
        const std::string* Get(const std::string& sec, const std::string& key) const {
            for (const auto& s : secs) {
                if (s.name != sec) continue;
                for (const auto& kv : s.kvs) if (kv.key == key) return &kv.val;
            }
            return nullptr;
        }
    };

    inline std::string Trim(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
        return s.substr(a, b - a);
    }

    inline void WriteDoc(const std::string& path, const IniDoc& doc) {
        std::string text;
        for (const auto& s : doc.secs) {
            if (s.kvs.empty()) continue;
            text += "[" + s.name + "]\r\n";
            for (const auto& kv : s.kvs) text += kv.key + "=" + kv.val + "\r\n";
            text += "\r\n";
        }
        HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        CloseHandle(h);
    }

    inline bool ReadDoc(const std::string& path, IniDoc& doc) {
        HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;

        std::string raw;
        char buf[4096];
        DWORD got = 0;
        while (ReadFile(h, buf, sizeof(buf), &got, nullptr) && got > 0) {
            raw.append(buf, got);
        }
        CloseHandle(h);

        std::string section = "General";
        size_t i = 0;
        while (i < raw.size()) {
            size_t eol = raw.find('\n', i);
            std::string line = Trim(raw.substr(i, (eol == std::string::npos ? raw.size() : eol) - i));
            i = (eol == std::string::npos) ? raw.size() : eol + 1;
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line.front() == '[' && line.back() == ']') {
                section = line.substr(1, line.size() - 2);
                continue;
            }
            const size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            doc.Set(section, Trim(line.substr(0, eq)), Trim(line.substr(eq + 1)));
        }
        return true;
    }

    // ── Value <-> string ────────────────────────────────────────────────────
    inline std::string FmtInt(int v)   { char b[32]; std::snprintf(b, sizeof(b), "%d", v); return b; }
    inline std::string FmtFloat(float v){ char b[48]; std::snprintf(b, sizeof(b), "%.6g", v); return b; }
    inline std::string FmtHex(unsigned v){ char b[16]; std::snprintf(b, sizeof(b), "0x%08X", v); return b; }

    inline int          ParseInt(const std::string& s, int def)   { return s.empty() ? def : std::atoi(s.c_str()); }
    inline float        ParseFloat(const std::string& s, float def){ return s.empty() ? def : static_cast<float>(std::atof(s.c_str())); }
    inline bool         ParseBool(const std::string& s, bool def)  { return s.empty() ? def : (std::atoi(s.c_str()) != 0); }
    inline unsigned     ParseHex(const std::string& s, unsigned def){ return s.empty() ? def : static_cast<unsigned>(std::strtoul(s.c_str(), nullptr, 0)); }

    // Section path for an item = "/"-joined submenu Names between root and item.
    inline std::string SectionForItem(MenuItem* item, Menu* root) {
        std::vector<std::string> parts;
        AMenuComponent* p = item->Parent;
        while (p && p != root) { parts.push_back(p->Name.c_str()); p = p->Parent; }
        if (parts.empty()) return "General";
        std::string s;
        for (int k = static_cast<int>(parts.size()) - 1; k >= 0; --k) {
            if (!s.empty()) s += "/";
            s += parts[k];
        }
        return s;
    }

    // ── Serialize one item into the doc ─────────────────────────────────────
    inline void SaveItem(MenuItem* it, Menu* root, IniDoc& doc) {
        const std::string sec = SectionForItem(it, root);
        const std::string key = it->Name.c_str();
        switch (it->Kind()) {
        case MenuValueType::Boolean:
            doc.Set(sec, key, FmtInt(it->As<SDK::UI::MenuBool>()->Value ? 1 : 0));
            break;
        case MenuValueType::Slider:
            doc.Set(sec, key, FmtInt(it->As<SDK::UI::MenuSlider>()->Value));
            break;
        case MenuValueType::SliderF:
            doc.Set(sec, key, FmtFloat(it->As<SDK::UI::MenuSliderF>()->Value));
            break;
        case MenuValueType::List:
            doc.Set(sec, key, FmtInt(it->As<SDK::UI::MenuList>()->Index));
            break;
        case MenuValueType::Color:
            doc.Set(sec, key, FmtHex(it->As<SDK::UI::MenuColor>()->Value));
            break;
        case MenuValueType::KeyBind: {
            auto* k = it->As<SDK::UI::MenuKeyBind>();
            doc.Set(sec, key + ".key", FmtInt(k->Key));
            // Momentary (Press) keybinds have no meaningful persistent state.
            if (k->Type == SDK::UI::KeyBindType::Toggle) {
                doc.Set(sec, key + ".active", FmtInt(k->Active ? 1 : 0));
            }
            break;
        }
        case MenuValueType::SliderBtn: {
            auto* s = it->As<SDK::UI::MenuSliderButton>();
            doc.Set(sec, key + ".value", FmtInt(s->Value));
            doc.Set(sec, key + ".enabled", FmtInt(s->Enabled ? 1 : 0));
            break;
        }
        default:
            break; // Button / Separator / None — nothing to persist
        }
    }

    // ── Apply one item from the doc (fields set directly; fire item callback) ─
    inline void ApplyItem(MenuItem* it, Menu* root, const IniDoc& doc) {
        const std::string sec = SectionForItem(it, root);
        const std::string key = it->Name.c_str();
        switch (it->Kind()) {
        case MenuValueType::Boolean:
            if (const std::string* v = doc.Get(sec, key)) {
                it->As<SDK::UI::MenuBool>()->Value = ParseBool(*v, true);
                it->FireValueChanged();
            }
            break;
        case MenuValueType::Slider:
            if (const std::string* v = doc.Get(sec, key)) {
                auto* s = it->As<SDK::UI::MenuSlider>();
                int nv = ParseInt(*v, s->Value);
                if (nv < s->MinValue) nv = s->MinValue;
                if (nv > s->MaxValue) nv = s->MaxValue;
                s->Value = nv;
                it->FireValueChanged();
            }
            break;
        case MenuValueType::SliderF:
            if (const std::string* v = doc.Get(sec, key)) {
                auto* s = it->As<SDK::UI::MenuSliderF>();
                float nv = ParseFloat(*v, s->Value);
                if (nv < s->MinValue) nv = s->MinValue;
                if (nv > s->MaxValue) nv = s->MaxValue;
                s->Value = nv;
                it->FireValueChanged();
            }
            break;
        case MenuValueType::List:
            if (const std::string* v = doc.Get(sec, key)) {
                auto* l = it->As<SDK::UI::MenuList>();
                int idx = ParseInt(*v, l->Index);
                if (idx < 0) idx = 0;
                if (!l->Options.empty() && idx >= l->Options.size()) idx = l->Options.size() - 1;
                l->Index = idx;
                it->FireValueChanged();
            }
            break;
        case MenuValueType::Color:
            if (const std::string* v = doc.Get(sec, key)) {
                it->As<SDK::UI::MenuColor>()->Value =
                    ParseHex(*v, it->As<SDK::UI::MenuColor>()->Value);
                it->FireValueChanged();
            }
            break;
        case MenuValueType::KeyBind: {
            auto* k = it->As<SDK::UI::MenuKeyBind>();
            bool changed = false;
            if (const std::string* v = doc.Get(sec, key + ".key")) {
                const int nk = ParseInt(*v, k->Key);
                if (nk > 0) { k->Key = nk; k->WasDown = false; changed = true; }
            }
            if (k->Type == SDK::UI::KeyBindType::Toggle) {
                if (const std::string* v = doc.Get(sec, key + ".active")) {
                    k->Active = ParseBool(*v, k->Active);
                    changed = true;
                }
            }
            if (changed) it->FireValueChanged();
            break;
        }
        case MenuValueType::SliderBtn: {
            auto* s = it->As<SDK::UI::MenuSliderButton>();
            bool changed = false;
            if (const std::string* v = doc.Get(sec, key + ".value")) {
                int nv = ParseInt(*v, s->Value);
                if (nv < s->MinValue) nv = s->MinValue;
                if (nv > s->MaxValue) nv = s->MaxValue;
                s->Value = nv; changed = true;
            }
            if (const std::string* v = doc.Get(sec, key + ".enabled")) {
                s->Enabled = ParseBool(*v, s->Enabled); changed = true;
            }
            if (changed) it->FireValueChanged();
            break;
        }
        default:
            break;
        }
    }

    // ── Tree walk ───────────────────────────────────────────────────────────
    template <typename Fn>
    inline void ForEachItem(Menu* menu, Menu* root, Fn&& fn) {
        for (int i = 0; i < menu->Components.size(); ++i) {
            AMenuComponent* c = menu->Components[i];
            if (!c) continue;
            if (c->IsMenu()) {
                ForEachItem(static_cast<Menu*>(c), root, fn);
            } else {
                fn(static_cast<MenuItem*>(c), root);
            }
        }
    }

    inline void SaveRoot(Menu* root) {
        if (!root) return;
        IniDoc doc;
        ForEachItem(root, root, [&](MenuItem* it, Menu* r) { SaveItem(it, r, doc); });
        WriteDoc(RootFilePath(root), doc);
    }

    inline void ApplyLoaded(Menu* root) {
        if (!root) return;
        IniDoc doc;
        if (!ReadDoc(RootFilePath(root), doc)) return; // no saved config yet
        g_applying = true;
        ForEachItem(root, root, [&](MenuItem* it, Menu* r) { ApplyItem(it, r, doc); });
        g_applying = false;
    }

    inline void SaveAllRoots() {
        auto& mm = MenuManager::Instance();
        for (int i = 0; i < mm.Menus.size(); ++i) SaveRoot(mm.Menus[i]);
    }

    // Apply saved config to every root ALREADY attached to MenuManager. The SDK
    // root (EnsoulSharpMenu, which holds Orbwalker + TargetSelector) attaches
    // during Bootstrap::Init — before Init() installs g_MenuAttachedHook — so its
    // OnMenuAttached/ApplyLoaded never fires. Sweep those already-registered
    // roots here so their persisted values are restored on startup, matching how
    // plugin menus load when they attach later during LoadAuto().
    inline void ApplyAllAttachedRoots() {
        auto& mm = MenuManager::Instance();
        for (int i = 0; i < mm.Menus.size(); ++i) ApplyLoaded(mm.Menus[i]);
    }

    // ── Core settings (Config:: globals) ────────────────────────────────────
    struct CoreSnap {
        bool  zoomEnabled;  float zoomMax;
        bool  skinEnabled;  // skinId is per-champion now (skins.ini), not core.ini
        bool  bypassObs;    bool  clickThrough;
        bool  permaEnabled; int   permaW, permaInd, permaX, permaY; bool permaPosInit;
        int   langIndex;
    };

    inline CoreSnap CaptureCore() {
        CoreSnap s{};
        s.zoomEnabled  = Config::ZoomHack::enabled;
        s.zoomMax      = Config::ZoomHack::maxZoom;
        s.skinEnabled  = Config::SkinChanger::enabled;
        s.bypassObs    = Config::StreamProtection::bypassObs;
        s.clickThrough = Config::OverlayInput::clickThrough;
        s.permaEnabled = Config::PermaShow::enabled;
        s.permaW       = Config::PermaShow::width;
        s.permaInd     = Config::PermaShow::indicatorWidth;
        s.permaX       = Config::PermaShow::x;
        s.permaY       = Config::PermaShow::y;
        s.permaPosInit = Config::PermaShow::positionInitialized;
        s.langIndex    = Config::Language::index;
        return s;
    }

    inline bool CoreEquals(const CoreSnap& a, const CoreSnap& b) {
        return a.zoomEnabled == b.zoomEnabled && a.zoomMax == b.zoomMax &&
               a.skinEnabled == b.skinEnabled &&
               a.bypassObs == b.bypassObs && a.clickThrough == b.clickThrough &&
               a.permaEnabled == b.permaEnabled && a.permaW == b.permaW &&
               a.permaInd == b.permaInd && a.permaX == b.permaX &&
               a.permaY == b.permaY && a.permaPosInit == b.permaPosInit &&
               a.langIndex == b.langIndex;
    }

    inline CoreSnap g_lastCore{};

    inline void SaveCore() {
        IniDoc doc;
        doc.Set("ZoomHack", "enabled", FmtInt(Config::ZoomHack::enabled ? 1 : 0));
        doc.Set("ZoomHack", "maxZoom", FmtFloat(Config::ZoomHack::maxZoom));
        doc.Set("SkinChanger", "enabled", FmtInt(Config::SkinChanger::enabled ? 1 : 0));
        doc.Set("StreamProtection", "bypassObs", FmtInt(Config::StreamProtection::bypassObs ? 1 : 0));
        doc.Set("Overlay", "clickThrough", FmtInt(Config::OverlayInput::clickThrough ? 1 : 0));
        doc.Set("PermaShow", "enabled", FmtInt(Config::PermaShow::enabled ? 1 : 0));
        doc.Set("PermaShow", "width", FmtInt(Config::PermaShow::width));
        doc.Set("PermaShow", "indicatorWidth", FmtInt(Config::PermaShow::indicatorWidth));
        doc.Set("PermaShow", "x", FmtInt(Config::PermaShow::x));
        doc.Set("PermaShow", "y", FmtInt(Config::PermaShow::y));
        doc.Set("PermaShow", "positionInitialized", FmtInt(Config::PermaShow::positionInitialized ? 1 : 0));
        doc.Set("Language", "index", FmtInt(Config::Language::index));
        WriteDoc(CoreFilePath(), doc);
        g_lastCore = CaptureCore();
    }

    inline void LoadCore() {
        IniDoc doc;
        if (ReadDoc(CoreFilePath(), doc)) {
            auto B = [&](const char* s, const char* k, bool d) {
                const std::string* v = doc.Get(s, k); return v ? ParseBool(*v, d) : d; };
            auto I = [&](const char* s, const char* k, int d) {
                const std::string* v = doc.Get(s, k); return v ? ParseInt(*v, d) : d; };
            auto F = [&](const char* s, const char* k, float d) {
                const std::string* v = doc.Get(s, k); return v ? ParseFloat(*v, d) : d; };

            Config::ZoomHack::enabled           = B("ZoomHack", "enabled", Config::ZoomHack::enabled);
            Config::ZoomHack::maxZoom           = F("ZoomHack", "maxZoom", Config::ZoomHack::maxZoom);
            Config::SkinChanger::enabled        = B("SkinChanger", "enabled", Config::SkinChanger::enabled);
            Config::StreamProtection::bypassObs = B("StreamProtection", "bypassObs", Config::StreamProtection::bypassObs);
            Config::OverlayInput::clickThrough  = B("Overlay", "clickThrough", Config::OverlayInput::clickThrough);
            Config::PermaShow::enabled          = B("PermaShow", "enabled", Config::PermaShow::enabled);
            Config::PermaShow::width            = I("PermaShow", "width", Config::PermaShow::width);
            if (Config::PermaShow::width < 500) {
                Config::PermaShow::width = 500;
            }
            Config::PermaShow::indicatorWidth   = I("PermaShow", "indicatorWidth", Config::PermaShow::indicatorWidth);
            Config::PermaShow::x                = I("PermaShow", "x", Config::PermaShow::x);
            Config::PermaShow::y                = I("PermaShow", "y", Config::PermaShow::y);
            Config::PermaShow::positionInitialized = B("PermaShow", "positionInitialized", Config::PermaShow::positionInitialized);
            Config::Language::index             = I("Language", "index", Config::Language::index);
        }
        g_lastCore   = CaptureCore();
        g_coreLoaded = true;
    }

    // ── Per-champion skin settings (skins.ini) ──────────────────────────────
    // The Skin Changer's skin id is remembered per champion: [skins] Aatrox=12,
    // MissFortune=4, ... The global `enabled` toggle stays in core.ini; only the
    // chosen skin id follows the champion you are playing. Config::SkinChanger::
    // skinId acts as the live "current champion" value that the menu slider edits
    // and CoreSkinChanger applies — TickSkins() keeps it in sync with the map.
    inline std::map<std::string, int> g_skinByChampion;
    inline std::string g_skinChampion;    // champion currently synced into skinId
    inline int   g_skinLastId  = 0;
    inline bool  g_skinsDirty  = false;
    inline DWORD g_skinsDueTick = 0;

    inline std::string SkinsFilePath() {
        char dir[MAX_PATH] = {};
        ConfigDir(dir, MAX_PATH);
        return std::string(dir) + "\\skins.ini";
    }

    inline void LoadSkins() {
        g_skinByChampion.clear();
        IniDoc doc;
        if (!ReadDoc(SkinsFilePath(), doc)) return;
        for (const auto& sec : doc.secs) {
            if (sec.name != "skins") continue;
            for (const auto& kv : sec.kvs) {
                if (!kv.key.empty()) g_skinByChampion[kv.key] = ParseInt(kv.val, 0);
            }
        }
    }

    inline void SaveSkins() {
        IniDoc doc;
        for (const auto& kv : g_skinByChampion) {
            doc.Set("skins", kv.first, FmtInt(kv.second));
        }
        WriteDoc(SkinsFilePath(), doc);
    }

    // Sync Config::SkinChanger::skinId <-> the per-champion map every frame:
    //   * when the played champion resolves/changes, load its saved skin id;
    //   * when the user moves the slider, persist it for the current champion.
    inline void TickSkins() {
        const char* champRaw = g_ChampionNameFn ? g_ChampionNameFn() : nullptr;
        const std::string champ = champRaw ? champRaw : "";
        if (!champ.empty()) {
            if (champ != g_skinChampion) {
                g_skinChampion = champ;
                auto it = g_skinByChampion.find(champ);
                Config::SkinChanger::skinId =
                    (it != g_skinByChampion.end()) ? it->second : 0;
                g_skinLastId = Config::SkinChanger::skinId;
            } else if (Config::SkinChanger::skinId != g_skinLastId) {
                g_skinLastId = Config::SkinChanger::skinId;
                g_skinByChampion[champ] = Config::SkinChanger::skinId;
                g_skinsDirty  = true;
                g_skinsDueTick = GetTickCount() + kDebounceMs;
            }
        }
        if (g_skinsDirty && static_cast<int>(GetTickCount() - g_skinsDueTick) >= 0) {
            g_skinsDirty = false;
            SaveSkins();
        }
    }

    // ── Change hooks / debounce ─────────────────────────────────────────────
    inline void OnMenuValueChanged(MenuItem* /*item*/) {
        if (g_applying) return;
        g_menuDirty  = true;
        g_menuDueTick = GetTickCount() + kDebounceMs;
    }

    inline void OnMenuAttached(Menu* root) {
        ApplyLoaded(root);
    }

    inline void CheckCoreChanged() {
        const CoreSnap now = CaptureCore();
        if (!CoreEquals(now, g_lastCore)) {
            g_coreDirty   = true;
            g_coreDueTick = GetTickCount() + kDebounceMs;
            g_lastCore    = now; // baseline advances so we debounce, not spam
        }
    }

    inline bool Elapsed(DWORD due) {
        return static_cast<int>(GetTickCount() - due) >= 0;
    }

    inline void Tick() {
        if (!g_coreLoaded) return;
        TickSkins();
        CheckCoreChanged();
        if (g_menuDirty && Elapsed(g_menuDueTick)) {
            g_menuDirty = false;
            SaveAllRoots();
        }
        if (g_coreDirty && Elapsed(g_coreDueTick)) {
            g_coreDirty = false;
            SaveCore();
        }
    }

    inline void FlushAll() {
        if (!g_coreLoaded) return;
        SaveAllRoots();
        SaveCore();
        SaveSkins();
    }

    // ── Init ────────────────────────────────────────────────────────────────
    inline void Init(const char* (*championFn)()) {
        g_ChampionNameFn = championFn;
        SDK::UI::g_MenuValueChangedHook = &OnMenuValueChanged;
        SDK::UI::g_MenuAttachedHook     = &OnMenuAttached;
        LoadCore();
        LoadSkins();
        // Roots that attached before this hook was installed (the SDK's
        // EnsoulSharpMenu with Orbwalker + TargetSelector) never got their
        // ApplyLoaded on attach — restore them now.
        ApplyAllAttachedRoots();
        NightSharpDebug::Logf("[ConfigStore] Init complete");
    }

} // namespace ConfigStore
