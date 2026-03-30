#pragma once

#include "Spell.h"

namespace SDK::SpellTypes {

    class BaseSpell : public Spell {
    public:
        using Spell::Spell;
    };

    class Targeted : public Spell {
    public:
        Targeted() = default;
        Targeted(SpellSlot slot, float range = FLT_MAX, float delay = 0.25f, float speed = FLT_MAX)
            : Spell(slot, range) {
            SetTargetted(delay, speed);
            Type = SpellType::Targeted;
        }
    };

    class TargetedMissile : public Targeted {
    public:
        using Targeted::Targeted;
    };

    class Skillshot : public Spell {
    public:
        Skillshot() = default;
        Skillshot(SpellSlot slot, float range = FLT_MAX)
            : Spell(slot, range) {}
    };

    class SkillshotLine : public Skillshot {
    public:
        SkillshotLine() = default;
        SkillshotLine(SpellSlot slot, float range, float delay, float width, float speed, bool collision = false)
            : Skillshot(slot, range) {
            SetSkillshot(delay, width, speed, collision, SpellType::Line);
        }
    };

    class SkillshotCircle : public Skillshot {
    public:
        SkillshotCircle() = default;
        SkillshotCircle(SpellSlot slot, float range, float delay, float radius, float speed = FLT_MAX, bool collision = false)
            : Skillshot(slot, range) {
            SetSkillshot(delay, radius, speed, collision, SpellType::Circle);
        }
    };

    class SkillshotCone : public Skillshot {
    public:
        SkillshotCone() = default;
        SkillshotCone(SpellSlot slot, float range, float delay, float angleOrRadius, float speed = FLT_MAX, bool collision = false)
            : Skillshot(slot, range) {
            SetSkillshot(delay, angleOrRadius, speed, collision, SpellType::Cone);
        }
    };

    class SkillshotMissile : public SkillshotLine {
    public:
        using SkillshotLine::SkillshotLine;
    };

    class SkillshotMissileLine : public SkillshotLine {
    public:
        using SkillshotLine::SkillshotLine;
    };

    class SkillshotMissileCircle : public SkillshotCircle {
    public:
        using SkillshotCircle::SkillshotCircle;
    };

    class SkillshotMissileCone : public SkillshotCone {
    public:
        using SkillshotCone::SkillshotCone;
    };

    class SkillshotMissileArc : public SkillshotMissileCircle {
    public:
        using SkillshotMissileCircle::SkillshotMissileCircle;
    };

    class SkillshotRing : public SkillshotCircle {
    public:
        float InnerRadius = 0.0f;

        SkillshotRing() = default;
        SkillshotRing(SpellSlot slot, float range, float delay, float outerRadius, float innerRadius, float speed = FLT_MAX, bool collision = false)
            : SkillshotCircle(slot, range, delay, outerRadius, speed, collision),
              InnerRadius(innerRadius) {}
    };

} // namespace SDK::SpellTypes
