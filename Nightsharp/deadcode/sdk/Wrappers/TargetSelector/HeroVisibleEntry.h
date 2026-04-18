#pragma once

#include "../../Core/Objects.h"
#include "../../Core/Game.h"

namespace SDK {

struct HeroVisibleEntry {
    int NetworkId = 0;
    Vector3 LastPosition = {};
    int LastVisibleTick = 0;
    bool Visible = false;
    bool Dead = false;

    static HeroVisibleEntry FromHero(const AIHeroClient& hero) {
        HeroVisibleEntry out = {};
        if (!hero.IsValid()) {
            return out;
        }

        out.NetworkId = hero.NetworkId();
        out.LastPosition = hero.Position();
        out.Visible = hero.IsVisible();
        out.Dead = hero.IsDead();
        out.LastVisibleTick = out.Visible ? Game::TickCount() : 0;
        return out;
    }
};

} // namespace SDK
