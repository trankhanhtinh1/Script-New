#pragma once

#include "PluginManager.h"
#include "core/OrbwalkerPlugin.h"
#include "core/TargetSelectorPlugin.h"
#include "core/RenderTestPlugin.h"
#include "core/OffsetScannerPlugin.h"
#include "champions/EzrealPlugin.h"
#include "champions/JinxPlugin.h"
#include "champions/XerathPlugin.h"
#include "champions/KarthusPlugin.h"
#include "champions/KogmawPlugin.h"
#include "champions/ViktorPlugin.h"
#include "champions/ZedPlugin.h"
#include "champions/KalistaPlugin.h"
#include "champions/RTXPower/Ezreal.h"

namespace Plugins {
namespace PluginBootstrap {

    inline void WriteBootLog(const char* msg) {
        OutputDebugStringA(msg);
        HANDLE h = CreateFileA("C:\\Users\\Public\\ns_stage.txt",
            FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD w = 0;
            WriteFile(h, msg, (DWORD)lstrlenA(msg), &w, nullptr);
            CloseHandle(h);
        }
    }

    template<typename T>
    inline bool SafeRegister(PluginManager& mgr, const char* name) {
        char buf[128] = {};
        wsprintfA(buf, "[NightSharp] PluginBootstrap: registering %s...\r\n", name);
        WriteBootLog(buf);
        __try {
            mgr.Register<T>();
            wsprintfA(buf, "[NightSharp] PluginBootstrap: %s OK\r\n", name);
            WriteBootLog(buf);
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            wsprintfA(buf, "[NightSharp] PluginBootstrap: %s CRASHED (0x%08X)\r\n",
                name, GetExceptionCode());
            WriteBootLog(buf);
            return false;
        }
    }

    inline void EnsureRegistered() {
        static bool registered = false;
        if (registered) return;
        registered = true;

        WriteBootLog("[NightSharp] PluginBootstrap::EnsureRegistered() enter\r\n");
        WriteBootLog("[NightSharp] PluginBootstrap: calling PluginManager::Get()...\r\n");
        auto& manager = PluginManager::Get();
        WriteBootLog("[NightSharp] PluginBootstrap: PluginManager::Get() OK\r\n");
        SafeRegister<OrbwalkerPlugin>(manager, "OrbwalkerPlugin");
        SafeRegister<TargetSelectorPlugin>(manager, "TargetSelectorPlugin");
        SafeRegister<RenderTestPlugin>(manager, "RenderTestPlugin");
        SafeRegister<OffsetScannerPlugin>(manager, "OffsetScannerPlugin");
        SafeRegister<EzrealPlugin>(manager, "EzrealPlugin");
        SafeRegister<JinxPlugin>(manager, "JinxPlugin");
        SafeRegister<XerathPlugin>(manager, "XerathPlugin");
        SafeRegister<KarthusPlugin>(manager, "KarthusPlugin");
        SafeRegister<KogmawPlugin>(manager, "KogmawPlugin");
        SafeRegister<ViktorPlugin>(manager, "ViktorPlugin");
        SafeRegister<ZedPlugin>(manager, "ZedPlugin");
        SafeRegister<KalistaPlugin>(manager, "KalistaPlugin");
        SafeRegister<RTXPowerPlugin>(manager, "RTXPowerPlugin");
        WriteBootLog("[NightSharp] PluginBootstrap::EnsureRegistered() done\r\n");
    }

} // namespace PluginBootstrap
} // namespace Plugins
