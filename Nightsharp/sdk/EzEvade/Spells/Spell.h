#pragma once
#include <string>
#include <vector>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include "../../GameObjects/GameObjects.h"
#include "../../Game.h"
#include "SpellData.h"

// Forward declaration for bounding box
namespace SDK {
    class GameObject;
}

namespace EzEvade {

    class Spell {
    public:
        float startTime = 0.0f;
        float endTime = 0.0f;
        Vec2 startPos = { 0, 0 };
        Vec2 endPos = { 0, 0 };
        Vec2 direction = { 0, 0 };
        float height = 0.0f;
        int heroID = 0;
        int projectileID = 0;
        SpellData info;
        int spellID = 0;
        SDK::GameObject* spellObject = nullptr;
        SpellType spellType = SpellType::None;

        Vec2 cnLeft = { 0, 0 };
        Vec2 cnRight = { 0, 0 };
        Vec2 cnStart = { 0, 0 };
        Vec2 currentSpellPosition = { 0, 0 };
        Vec2 currentNegativePosition = { 0, 0 };
        Vec2 predictedEndPos = { 0, 0 };

        float radius = 0.0f;
        int dangerlevel = 1;

        float evadeTime = -FLT_MAX;      // C# float.MinValue
        float spellHitTime = -FLT_MAX;  // C# float.MinValue

        Spell() = default;

        // Methods equivalent to SpellExtensions in C#
        float GetSpellRadius() const;
        int GetSpellDangerLevel() const;
        std::string GetSpellDangerString() const;
        bool hasProjectile() const;
        Vec2 GetSpellProjection(const Vec2& pos, bool predictPos = false) const;
        SDK::GameObject* CheckSpellCollision(bool ignoreSelf = true) const;
        float GetSpellHitTime(const Vec2& pos) const;
        bool CanHeroEvade(const SDK::GameObject& hero, float& rEvadeTime, float& rSpellHitTime) const;
        // BoundingBox GetLinearSpellBoundingBox() const; // Geometry bounding box
        Vec2 GetSpellEndPosition() const;
        void UpdateSpellInfo();
        Vec2 GetCurrentSpellPosition(bool allowNegative = false, float delay = 0, float extraDistance = 0) const;
        bool LineIntersectLinearSpell(const Vec2& a, const Vec2& b) const;
        bool LineIntersectLinearSpellEx(const Vec2& a, const Vec2& b, Vec2& intersection) const;
    };

} // namespace EzEvade
