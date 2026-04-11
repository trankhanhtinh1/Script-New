#pragma once

#include <Windows.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Translations {

    inline bool GetConfigDir_(char* out, int maxLen) {
        char appdata[MAX_PATH] = {};
        DWORD len = GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return false;
        wsprintfA(out, "%s\\NightSharp\\config", appdata);
        return true;
    }

    inline void EnsureDirectory_(const char* path) {
        char tmp[MAX_PATH] = {};
        wsprintfA(tmp, "%s", path);
        for (char* p = tmp + 1; *p; ++p) {
            if (*p == '\\' || *p == '/') {
                *p = '\0';
                CreateDirectoryA(tmp, nullptr);
                *p = '\\';
            }
        }
        CreateDirectoryA(tmp, nullptr);
    }

    struct LangEntry {
        const char* cn;
        const char* vn;
    };

    inline int langIndex = 0;
    inline volatile LONG initOnce = 0;

    inline std::unordered_map<std::string, LangEntry>* g_dict = nullptr;
    inline std::unordered_set<std::string>* g_missing = nullptr;
    inline bool g_missingDirty = false;

    inline void SaveLangIndex() {
        char dir[MAX_PATH] = {};
        if (!GetConfigDir_(dir, MAX_PATH)) return;
        EnsureDirectory_(dir);

        char path[MAX_PATH] = {};
        wsprintfA(path, "%s\\language.ini", dir);

        HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;
        char buf[32] = {};
        wsprintfA(buf, "lang=%d\r\n", langIndex);
        DWORD written = 0;
        WriteFile(hFile, buf, (DWORD)lstrlenA(buf), &written, nullptr);
        CloseHandle(hFile);
    }

    inline void LoadLangIndex() {
        char dir[MAX_PATH] = {};
        if (!GetConfigDir_(dir, MAX_PATH)) return;

        char path[MAX_PATH] = {};
        wsprintfA(path, "%s\\language.ini", dir);

        HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;

        char buf[256] = {};
        DWORD bytesRead = 0;
        ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, nullptr);
        CloseHandle(hFile);
        buf[bytesRead] = '\0';

        for (DWORD i = 0; i + 5 <= bytesRead; i++) {
            if (buf[i] == 'l' && buf[i+1] == 'a' && buf[i+2] == 'n' && buf[i+3] == 'g' && buf[i+4] == '=') {
                int v = buf[i+5] - '0';
                if (v >= 0 && v <= 2) langIndex = v;
                break;
            }
        }
    }

    inline void FlushMissTranslations() {
        if (!g_missing || g_missing->empty() || !g_missingDirty) return;
        g_missingDirty = false;

        const char* path = "C:\\Users\\Public\\miss_translations.txt";

        HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;

        const char* header = "Nightsharp\\menu\\Translations.h\r\n\r\n";
        DWORD hw = 0;
        WriteFile(hFile, header, (DWORD)lstrlenA(header), &hw, nullptr);

        for (const auto& key : *g_missing) {
            char line[512] = {};
            wsprintfA(line, "d[\"%s\"] = { .cn = \"\", .vn = \"\" };\r\n", key.c_str());
            DWORD written = 0;
            WriteFile(hFile, line, (DWORD)lstrlenA(line), &written, nullptr);
        }
        CloseHandle(hFile);
    }

    inline void PopulateDictionary() {
        if (!g_dict) return;
        auto& d = *g_dict;

        d["Core"]           = { .cn = "核心",       .vn = "" };
        d["Language"]       = { .cn = "语言",       .vn = "" };
        d["Menu"]           = { .cn = "菜单",       .vn = "" };
        d["Debug Info"]     = { .cn = "调试信息",   .vn = "" };
        d["SDK Diagnostics"]= { .cn = "SDK 诊断",   .vn = "" };
        d["SDK Plugins"]    = { .cn = "SDK 插件",   .vn = "" };
        d["Plugins"]        = { .cn = "插件",       .vn = "" };
        d["Orbwalker 2.0"]  = { .cn = "走砍2.0",    .vn = "" };
        d["PluginSandbox"]  = { .cn = "插件沙盒",   .vn = "" };
        d["Target Selector"]= { .cn = "目标选择器", .vn = "" };

        d["Menu Settings"]  = { .cn = "菜单设置",   .vn = "" };
        d["Skin Changer"]   = { .cn = "皮肤更换",   .vn = "" };
        d["Zoom Hack"]      = { .cn = "视角修改",   .vn = "" };
        d["Bypass OBS"]     = { .cn = "绕过 OBS",   .vn = "" };
        d["Bypass OBS: overlay hidden from screen capture"] = { .cn = "绕过 OBS: 覆盖层将隐藏在屏幕捕捉中", .vn = "" };
        d["(requires Win10 2004+)"] = { .cn = "(需要 Win10 2004+)", .vn = "" };

        d["SDK Built-ins"]  = { .cn = "SDK 内置",   .vn = "" };
        d["Orbwalker and Target Selector are always on."] = { .cn = "走砍和目标选择器始终开启。", .vn = "" };
        d["This section is read-only for built-in SDK modules."] = { .cn = "此部分为 SDK 内置模块，仅可读。", .vn = "" };
        d["No SDK plugins registered."] = { .cn = "没有注册的 SDK 插件。", .vn = "" };

        d["Plugin Manager"] = { .cn = "插件管理",   .vn = "" };
        d["No source plugins registered."] = { .cn = "没有注册的源插件。", .vn = "" };
        d["No external plugins loaded."]   = { .cn = "没有加载的外部插件。", .vn = "" };
        d["Drop plugin DLLs into /plugins/ folder."] = { .cn = "将插件 DLL 放入 /plugins/ 文件夹。", .vn = "" };

        d["Built-in"]       = { .cn = "内置",       .vn = "" };
        d["Unload"]         = { .cn = "卸载",       .vn = "" };
        d["Load"]           = { .cn = "加载",       .vn = "" };
        d["Error"]          = { .cn = "错误",       .vn = "" };
        d["N/A"]            = { .cn = "不可用",     .vn = "" };
        d["Always Load"]    = { .cn = "始终加载",   .vn = "" };
        d["(no menu)"]      = { .cn = "(无菜单)",   .vn = "" };
        d["(menu init failed)"] = { .cn = "(菜单初始化失败)", .vn = "" };
        d["(wrong champion)"]   = { .cn = "(英雄不匹配)",     .vn = "" };

        d["On"]             = { .cn = "开",   .vn = "" };
        d["Off"]            = { .cn = "关",   .vn = "" };
        d["Press"]          = { .cn = "按下", .vn = "" };
        d["Toggle"]         = { .cn = "切换", .vn = "" };
        d["Press key..."]   = { .cn = "按下按键...", .vn = "" };

        d["Menu Customizer"]= { .cn = "菜单自定义", .vn = "" };
        d["Font Scale"]     = { .cn = "字体缩放",   .vn = "" };
        d["Menu Alpha"]     = { .cn = "菜单透明度", .vn = "" };
        d["Item Spacing"]   = { .cn = "项目间距",   .vn = "" };
        d["Rounding"]       = { .cn = "圆角",       .vn = "" };
        d["Lock Position"]  = { .cn = "锁定位置",   .vn = "" };
        d["Accent Color"]   = { .cn = "强调色",     .vn = "" };
        d["Background"]     = { .cn = "背景色",     .vn = "" };
        d["Apply"]          = { .cn = "应用",       .vn = "" };
        d["Reset"]          = { .cn = "重置",       .vn = "" };

        d["Keys"]           = { .cn = "按键", .vn = "" };
        d["General"]        = { .cn = "常规", .vn = "" };
        d["Drawings"]       = { .cn = "绘制", .vn = "" };
        d["Advanced"]       = { .cn = "高级", .vn = "" };
        d["Miscellaneous"]  = { .cn = "杂项", .vn = "" };
        d["Priority"]       = { .cn = "优先级", .vn = "" };
        d["Drawing"]        = { .cn = "绘制", .vn = "" };
        d["Humanizer"]      = { .cn = "人性化", .vn = "" };
        d["Weights"]        = { .cn = "权重", .vn = "" };
        d["Combo"]          = { .cn = "连招", .vn = "" };
        d["Harass"]         = { .cn = "骚扰", .vn = "" };
        d["LaneClear"]      = { .cn = "清线", .vn = "" };
        d["LastHit"]        = { .cn = "补刀", .vn = "" };
        d["Permashow"]      = { .cn = "常驻显示", .vn = "" };

        d["Runtime Diagnostics"] = { .cn = "运行时诊断", .vn = "" };
        d["Ready"]          = { .cn = "就绪",     .vn = "" };
        d["NotLearned"]     = { .cn = "未学习",   .vn = "" };
        d["Cooldown"]       = { .cn = "冷却中",   .vn = "" };
        d["NoMana"]         = { .cn = "没有蓝量", .vn = "" };
        d["Disabled"]       = { .cn = "已禁用",   .vn = "" };
        d["Unknown"]        = { .cn = "未知",     .vn = "" };

        d["ready"]          = { .cn = "就绪",       .vn = "" };
        d["not ready"]      = { .cn = "未就绪",     .vn = "" };
        d["runtime-ready"]  = { .cn = "运行时就绪", .vn = "" };
        d["waiting for live game state"] = { .cn = "等待游戏状态", .vn = "" };
        d["Orbwalker Settings"] = { .cn = "走砍设置", .vn = "" };
        d["Attackable Unit"] = { .cn = "攻击单位", .vn = "" };
        d["Extra Range Setting"] = { .cn = "额外范围设置", .vn = "" };
        d["Prioritize"] = { .cn = "优先级", .vn = "" };
        d["Farm"] = { .cn = "发育", .vn = "" };
        d["Ezreal"] = { .cn = "伊澤瑞爾", .vn = "" };
    }

    inline void InitTranslations() {
        if (InterlockedCompareExchange(&initOnce, 1, 0) != 0) return;

        LoadLangIndex();

        g_dict = new(std::nothrow) std::unordered_map<std::string, LangEntry>();
        g_missing = new(std::nothrow) std::unordered_set<std::string>();

        if (g_dict) PopulateDictionary();
    }

    inline const char* T(const char* key) {
        if (!key) return "";

        if (initOnce == 0) InitTranslations();

        if (!g_dict) return key;

        auto it = g_dict->find(key);
        bool found = (it != g_dict->end());

        if (!found) {
            if (g_missing) {
                auto r = g_missing->insert(key);
                if (r.second) g_missingDirty = true;
            }
        }

        if (langIndex == 0) return key;

        if (found) {
            const char* result = (langIndex == 1) ? it->second.cn : it->second.vn;
            if (result && result[0]) return result;
        }

        return key;
    }

}
