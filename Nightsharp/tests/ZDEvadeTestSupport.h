#pragma once

#include <cmath>
#include <cstdio>
#include <deque>

#include "plugins/ZDEvade/Database/SpellData.h"
#include "plugins/ZDEvade/Detection/Threat.h"

namespace ZDEvadeTest {

inline int& FailureCount() {
    static int failures = 0;
    return failures;
}

inline void ExpectTrue(const char* name, bool value) {
    if (!value) {
        std::printf("FAIL: %s\n", name);
        ++FailureCount();
    }
}

inline void ExpectNear(const char* name,
                       float actual,
                       float expected,
                       float epsilon = 0.001f) {
    if (std::fabs(actual - expected) > epsilon) {
        std::printf("FAIL: %s expected %.3f got %.3f\n", name, expected, actual);
        ++FailureCount();
    }
}

inline void ExpectEq(const char* name, int actual, int expected) {
    if (actual != expected) {
        std::printf("FAIL: %s expected %d got %d\n", name, expected, actual);
        ++FailureCount();
    }
}

inline int Finish(const char* suiteName) {
    if (FailureCount() == 0) {
        std::printf("ALL %s TESTS PASSED\n", suiteName);
        return 0;
    }
    std::printf("%d FAILURE(S)\n", FailureCount());
    return 1;
}

inline ZDEvade::SpellData MakeSpell(ZDEvade::ZDSpellType type) {
    ZDEvade::SpellData spell;
    spell.spellType = type;
    spell.dangerlevel = 3;
    spell.spellDelay = 250;
    spell.range = 1000.0f;
    spell.radius = 100.0f;
    spell.projectileSpeed = 2000.0f;

    switch (type) {
    case ZDEvade::ZDSpellType::Line:
        break;
    case ZDEvade::ZDSpellType::Circular:
        spell.projectileSpeed = 0.0f;
        break;
    case ZDEvade::ZDSpellType::Cone:
        spell.coneAngleDegrees = 60.0f;
        break;
    case ZDEvade::ZDSpellType::Arc:
        break;
    case ZDEvade::ZDSpellType::Ring:
        spell.innerRadius = 100.0f;
        spell.radius = 200.0f;
        break;
    }

    return spell;
}

inline ZDEvade::Threat MakeThreat(const ZDEvade::SpellData& data) {
    static thread_local std::deque<ZDEvade::SpellData> spellStorage;
    spellStorage.push_back(data);
    const ZDEvade::SpellData& stored = spellStorage.back();

    ZDEvade::Threat threat;
    threat.id = static_cast<int>(spellStorage.size());
    threat.data = &stored;
    threat.startPos = Vec2(0.0f, 0.0f);
    threat.endPos = Vec2(stored.range, 0.0f);
    if (threat.endPos.IsValid() && !threat.endPos.IsZero()) {
        threat.direction = (threat.endPos - threat.startPos).Normalized();
    }
    threat.startTick = 1000;
    threat.endTick = threat.startTick + stored.spellDelay + 2000;
    return threat;
}

} // namespace ZDEvadeTest
