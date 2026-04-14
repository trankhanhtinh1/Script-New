#pragma once
/*
 * Translations — CRT-Free version
 * Fixed array lookup, NO std::string/unordered_map/unordered_set.
 * Safe for manual-mapped DLLs without _initterm.
 */

#include <Windows.h>

namespace Translations {

    struct LangEntry {
        const char* en;
        const char* cn;
        const char* vn;
    };

    inline int langIndex = 0;        // 0=EN, 1=CN, 2=VN
    inline bool g_missingDirty = false;
    inline volatile LONG initOnce = 0;

    // ── Fixed dictionary — all entries are compile-time constants ──
    inline const LangEntry g_entries[] = {
        { "Core",            "核心",       "" },
        { "Language",        "语言",       "" },
        { "Menu",            "菜单",       "" },
        { "Debug Info",      "调试信息",   "" },
        { "SDK Diagnostics", "SDK 诊断",   "" },
        { "SDK Plugins",     "SDK 插件",   "" },
        { "Plugins",         "插件",       "" },
        { "Orbwalker 2.0",   "走砍2.0",    "" },
        { "PluginSandbox",   "插件沙盒",   "" },
        { "Target Selector", "目标选择器", "" },
        { "Menu Settings",   "菜单设置",   "" },
        { "Skin Changer",    "皮肤更换",   "" },
        { "Zoom Hack",       "视角修改",   "" },
        { "Bypass OBS",      "绕过 OBS",   "" },
        { "Bypass OBS: overlay hidden from screen capture", "绕过 OBS: 覆盖层将隐藏在屏幕捕捉中", "" },
        { "(requires Win10 2004+)", "(需要 Win10 2004+)", "" },
        { "SDK Built-ins",   "SDK 内置",   "" },
        { "Orbwalker and Target Selector are always on.", "走砍和目标选择器始终开启。", "" },
        { "This section is read-only for built-in SDK modules.", "此部分为 SDK 内置模块，仅可读。", "" },
        { "No SDK plugins registered.", "没有注册的 SDK 插件。", "" },
        { "Plugin Manager",  "插件管理",   "" },
        { "No source plugins registered.", "没有注册的源插件。", "" },
        { "No external plugins loaded.",   "没有加载的外部插件。", "" },
        { "Drop plugin DLLs into /plugins/ folder.", "将插件 DLL 放入 /plugins/ 文件夹。", "" },
        { "Built-in",        "内置",       "" },
        { "Unload",          "卸载",       "" },
        { "Load",            "加载",       "" },
        { "Error",           "错误",       "" },
        { "N/A",             "不可用",     "" },
        { "Always Load",     "始终加载",   "" },
        { "(no menu)",       "(无菜单)",   "" },
        { "(menu init failed)", "(菜单初始化失败)", "" },
        { "(wrong champion)",   "(英雄不匹配)",     "" },
        { "On",              "开",   "" },
        { "Off",             "关",   "" },
        { "Press",           "按下", "" },
        { "Toggle",          "切换", "" },
        { "Press key...",    "按下按键...", "" },
        { "Debug Window",    "调试窗口",   "" },
        { "Menu Customizer", "菜单自定义", "" },
        { "Font Scale",      "字体缩放",   "" },
        { "Menu Alpha",      "菜单透明度", "" },
        { "Item Spacing",    "项目间距",   "" },
        { "Rounding",        "圆角",       "" },
        { "Lock Position",   "锁定位置",   "" },
        { "Accent Color",    "强调色",     "" },
        { "Background",      "背景色",     "" },
        { "Apply",           "应用",       "" },
        { "Reset",           "重置",       "" },
        { "Keys",            "按键", "" },
        { "General",         "常规", "" },
        { "Drawings",        "绘制", "" },
        { "Advanced",        "高级", "" },
        { "Miscellaneous",   "杂项", "" },
        { "Priority",        "优先级", "" },
        { "Drawing",         "绘制", "" },
        { "Humanizer",       "人性化", "" },
        { "Weights",         "权重", "" },
        { "Combo",           "连招", "" },
        { "Harass",          "骚扰", "" },
        { "LaneClear",       "清线", "" },
        { "LastHit",         "补刀", "" },
        { "Permashow",       "常驻显示", "" },
        { "Runtime Diagnostics", "运行时诊断", "" },
        { "Ready",           "就绪",     "" },
        { "NotLearned",      "未学习",   "" },
        { "Cooldown",        "冷却中",   "" },
        { "NoMana",          "没有蓝量", "" },
        { "Disabled",        "已禁用",   "" },
        { "Unknown",         "未知",     "" },
        { "ready",           "就绪",       "" },
        { "not ready",       "未就绪",     "" },
        { "runtime-ready",   "运行时就绪", "" },
        { "waiting for live game state", "等待游戏状态", "" },
        { "Orbwalker Settings", "走砍设置", "" },
        { "Attackable Unit", "攻击单位", "" },
        { "Extra Range Setting", "额外范围设置", "" },
        { "Prioritize",     "优先级", "" },
        { "Farm",            "发育", "" },
        { "Ezreal",          "伊澤瑞爾", "" },
    };
    inline constexpr int g_entryCount = sizeof(g_entries) / sizeof(g_entries[0]);

    // ── Config persistence (CRT-free) ──

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
        // No-op in CRT-free version (no dynamic missing tracking)
        g_missingDirty = false;
    }

    inline void InitTranslations() {
        if (InterlockedCompareExchange(&initOnce, 1, 0) != 0) return;
        LoadLangIndex();
    }

    // ── Translate: O(N) linear scan on small fixed array ──
    inline const char* T(const char* key) {
        if (!key || !key[0]) return "";
        if (langIndex == 0) return key;  // EN = passthrough

        if (initOnce == 0) InitTranslations();

        for (int i = 0; i < g_entryCount; ++i) {
            if (lstrcmpA(key, g_entries[i].en) == 0) {
                const char* result = (langIndex == 1) ? g_entries[i].cn : g_entries[i].vn;
                if (result && result[0]) return result;
                return key;
            }
        }
        return key;
    }

}
