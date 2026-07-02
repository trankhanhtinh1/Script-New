#pragma once
/*
 * NightSharp SDK / UI / PermaShow
 *
 * Empty registry by default. Plugins push items in by calling
 * `.AddPermashow()` on a MenuItem (mirrors EnsoulSharp's
 * `MenuItem.Permashow(true, ...)` extension method).
 *
 * The actual rendering happens inside menu/NightSharpMenu.h
 * (DrawPermaShowOverlay), which iterates SDK::UI::PermaShow::Items().
 */

#include <Windows.h>
#include <cstdio>

#include "UI.h"
#include "../../imgui/imgui.h"

namespace SDK { namespace UI { namespace PermaShow {

    struct Entry {
        MenuItem*   Item;
        char        DisplayName[96];
        unsigned int Color;       // ImU32
    };

    // Fixed pool — manual-map safe, no heap fragmentation.
    constexpr int MAX_ITEMS = 64;
    inline Entry s_Items[MAX_ITEMS] = {};
    inline int   s_Count = 0;

    inline int Count()                  { return s_Count; }
    inline const Entry& At(int i)       { return s_Items[i]; }
    inline Entry* Items()               { return s_Items; }

    inline void Initialize(Menu* menu = nullptr) {
        (void)menu;
    }

    // Returns -1 if not found.
    inline int IndexOf(MenuItem* item) {
        if (!item) return -1;
        for (int i = 0; i < s_Count; ++i) {
            if (s_Items[i].Item == item) return i;
        }
        return -1;
    }

    // Add an item to the perma-show registry. No-op if already present.
    // `customDisplayName` overrides the item's DisplayName when provided.
    // `color` is an ImU32 (default white).
    inline void Add(MenuItem* item,
                    const char* customDisplayName = nullptr,
                    unsigned int color           = IM_COL32(255, 255, 255, 255)) {
        if (!item || s_Count >= MAX_ITEMS) return;
        if (IndexOf(item) >= 0) return;

        Entry& e = s_Items[s_Count++];
        e.Item  = item;
        e.Color = color;

        const char* src = customDisplayName && customDisplayName[0]
            ? customDisplayName
            : item->DisplayName.c_str();
        int j = 0;
        while (src[j] && j < 95) { e.DisplayName[j] = src[j]; ++j; }
        e.DisplayName[j] = 0;
    }

    inline void Remove(MenuItem* item) {
        int idx = IndexOf(item);
        if (idx < 0) return;
        for (int i = idx; i < s_Count - 1; ++i) s_Items[i] = s_Items[i + 1];
        --s_Count;
    }

    inline void Clear() { s_Count = 0; }

}}} // namespace SDK::UI::PermaShow

// ---------- MenuItem chaining helpers ----------
namespace SDK { namespace UI {

    inline MenuItem* MenuItem::AddPermashow(const char* customName, unsigned int color) {
        PermaShow::Add(this, customName, color);
        return this;
    }

    inline MenuItem* MenuItem::RemovePermashow() {
        PermaShow::Remove(this);
        return this;
    }

}} // namespace SDK::UI
