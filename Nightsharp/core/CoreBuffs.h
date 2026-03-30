#pragma once

#include "CoreRuntime.h"
#include "Vector.h"

#include <cstdint>

namespace CoreBuffs {

    struct BuffRef {
        uintptr_t address = 0;

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        int GetType() const {
            return Globals::Read<int>(address + Offset::BuffManager::BuffType);
        }

        int GetStacks() const {
            const int stacks = Globals::Read<int>(address + Offset::BuffManager::BuffStacks);
            if (stacks > 0) {
                return stacks;
            }
            return Globals::Read<int>(address + Offset::BuffManager::BuffStacksAlt);
        }

        float GetStartTime() const {
            return Globals::Read<float>(address + Offset::BuffManager::BuffStartTime);
        }

        float GetEndTime() const {
            return Globals::Read<float>(address + Offset::BuffManager::BuffEndTime);
        }

        float GetRemainingTime(float gameTime) const {
            const float endTime = GetEndTime();
            return endTime > gameTime ? (endTime - gameTime) : 0.0f;
        }

        bool IsActive(float gameTime) const {
            return IsValid() && GetEndTime() > gameTime;
        }

        bool ReadName(char* out, int maxOut) const {
            if (!out || maxOut <= 0 || !IsValid()) {
                if (out && maxOut > 0) out[0] = 0;
                return false;
            }

            const auto namePtr = Globals::Read<uintptr_t>(address + Offset::BuffManager::BuffNamePtr);
            if (!Globals::IsValidPtr(namePtr)) {
                out[0] = 0;
                return false;
            }

            return Globals::ReadGameString(namePtr + Offset::BuffManager::BuffNameStr, out, maxOut);
        }
    };

    inline uintptr_t GetBuffManager(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) {
            return 0;
        }
        return obj + Offset::BuffManager::Offset;
    }

    inline int Enumerate(uintptr_t obj, uintptr_t* out, int maxOut) {
        if (!out || maxOut <= 0) {
            return 0;
        }

        const auto manager = GetBuffManager(obj);
        if (!Globals::IsValidPtr(manager)) {
            return 0;
        }

        const auto begin = Globals::Read<uintptr_t>(manager);
        const auto end = Globals::Read<uintptr_t>(manager + Offset::BuffManager::EntriesEnd);
        if (!Globals::IsValidPtr(begin) || !Globals::IsValidPtr(end) || end < begin) {
            return 0;
        }

        const auto bytes = static_cast<size_t>(end - begin);
        const auto count = static_cast<int>(bytes / 0x18);
        if (count <= 0 || count > maxOut || count > 512) {
            return 0;
        }

        int written = 0;
        for (int i = 0; i < count; ++i) {
            const auto entry = begin + static_cast<uintptr_t>(i * 0x18);
            const auto buff = Globals::Read<uintptr_t>(entry + Offset::BuffManager::EntryBuff);
            if (!Globals::IsValidPtr(buff)) {
                continue;
            }
            out[written++] = buff;
        }
        return written;
    }

    inline bool HasBuff(uintptr_t obj, const char* name) {
        if (!name || !name[0]) {
            return false;
        }

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) {
                continue;
            }
            if (lstrcmpiA(buf, name) == 0) {
                return true;
            }
        }
        return false;
    }

    inline bool NameContainsInsensitive(const char* text, const char* token) {
        if (!text || !token || !text[0] || !token[0]) {
            return false;
        }

        for (const char* p = text; *p; ++p) {
            const char* a = p;
            const char* b = token;
            while (*a && *b) {
                char ca = *a;
                char cb = *b;
                if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
                if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
                if (ca != cb) {
                    break;
                }
                ++a;
                ++b;
            }
            if (!*b) {
                return true;
            }
        }
        return false;
    }

    inline bool HasBuffContaining(uintptr_t obj, const char* token, int requiredType = -1) {
        if (!token || !token[0]) {
            return false;
        }

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.IsValid()) {
                continue;
            }
            if (requiredType >= 0 && buff.GetType() != requiredType) {
                continue;
            }
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) {
                continue;
            }
            if (NameContainsInsensitive(buf, token)) {
                return true;
            }
        }
        return false;
    }

    inline int Count(uintptr_t obj) {
        uintptr_t buffs[256] = {};
        return Enumerate(obj, buffs, static_cast<int>(sizeof(buffs) / sizeof(buffs[0])));
    }

    inline BuffRef FindByName(uintptr_t obj, const char* name) {
        if (!name || !name[0]) {
            return {};
        }

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) {
                continue;
            }
            if (lstrcmpiA(buf, name) == 0) {
                return buff;
            }
        }
        return {};
    }

    inline int GetBuffStacks(uintptr_t obj, const char* name) {
        const auto buff = FindByName(obj, name);
        return buff.IsValid() ? buff.GetStacks() : 0;
    }

    inline float GetBuffRemainingTime(uintptr_t obj, const char* name, float gameTime) {
        const auto buff = FindByName(obj, name);
        return buff.IsValid() ? buff.GetRemainingTime(gameTime) : 0.0f;
    }

    inline bool HasBuffType(uintptr_t obj, int type) {
        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        for (int i = 0; i < count; ++i) {
            const BuffRef buff{ buffs[i] };
            if (buff.IsValid() && buff.GetType() == type) {
                return true;
            }
        }
        return false;
    }

} // namespace CoreBuffs
