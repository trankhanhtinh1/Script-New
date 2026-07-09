#pragma once

#include "../../../SDK/SDK.h"

namespace Plugins::KuroEvade {

struct HeroInfo {
    SDK::AIHeroClient hero;
    Vec2 serverPos2D;
    Vec2 serverPos2DExtra;
    Vec2 serverPos2DPing;
    Vec2 currentPosition;
    bool isMoving = false;
    float boundingRadius = 0.0f;
    float moveSpeed = 0.0f;

    HeroInfo() = default;
    explicit HeroInfo(const SDK::AIHeroClient& source)
        : hero(source) {
        UpdateInfo();
    }

    void UpdateInfo() {
        if (!hero.IsValid()) {
            hero = SDK::ObjectManager::Player();
        }
        if (!hero.IsValid()) {
            return;
        }
        serverPos2D = hero.ServerPosition().To2D();
        serverPos2DExtra = serverPos2D;
        serverPos2DPing = serverPos2D;
        currentPosition = hero.Position().To2D();
        boundingRadius = hero.BoundingRadius();
        moveSpeed = hero.MoveSpeed();
        isMoving = hero.IsMoving();
    }
};

} // namespace Plugins::KuroEvade
