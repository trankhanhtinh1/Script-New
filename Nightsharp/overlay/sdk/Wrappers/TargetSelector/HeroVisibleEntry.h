#pragma once

#include "../../Core/Game.h"
#include "../../Core/Variables.h"
#include "../../GameObjects/GameObjects.h"

namespace SDK {

class HeroVisibleEntry {
public:
    AIHeroClient Hero;
    int LastVisibleChangeTick = 0;
    bool Visible = false;

    explicit HeroVisibleEntry(AIHeroClient hero)
        : Hero(hero)
        , LastVisibleChangeTick(Variables::TickCount())
        , Visible(hero.IsVisible()) {
    }
};

} // namespace SDK
