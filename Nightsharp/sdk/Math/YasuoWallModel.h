#pragma once

#include "../../core/Vector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace SDK::YasuoWallModel {

inline constexpr int kPendingNameLifetimeMs = 500;
inline constexpr int kWallLifetimeMs = 5000;
inline constexpr int kPairWindowMs = 500;
inline constexpr float kMaxMidpointDistance = 75.0f;

enum class ObjectRole {
    Other,
    Main,
    Endpoint,
    Visual,
};

struct Identity {
    std::uintptr_t address = 0;
    std::uint32_t networkId = 0;
    std::uint32_t index = 0;

    bool IsValid() const {
        return networkId != 0 && networkId != 0xFFFFFFFFu;
    }
};

inline bool SameIdentity(const Identity& a, const Identity& b) {
    if (a.IsValid() && b.IsValid()) {
        return a.networkId == b.networkId;
    }
    return a.address != 0 && a.address == b.address && a.index == b.index;
}

inline bool IdentityLess(const Identity& a, const Identity& b) {
    return std::tie(a.networkId, a.index, a.address) <
           std::tie(b.networkId, b.index, b.address);
}

inline std::string LowerAscii(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch);
    });
    return out;
}

// The windwall emitter name carries the caster's skin, and the suffix is not
// always a bare rank digit. Names read live off 16.14 with two Yasuos in the game
// (object name at +0x68, enumerated through the object manager):
//     Yasuo_Base_W_windwall1             base skin, rank in the last character
//     Yasuo_Skin02_w_windwall_enemy_05   skinned caster, "_enemy_05" suffix
//     Yasuo_Skin02_W_windwall_activate   activation effect, NOT the wall itself
// The previous matcher required exactly "yasuo_base_w_windwall" plus one
// character, so every Yasuo not on the base skin was invisible to the tracker and
// its wall was never tracked, drawn, or collided against.
// Requiring a trailing rank number rather than denylisting known siblings: both
// real wall emitters end in digits ("windwall1", "_enemy_05") while the companion
// particles spawned from the same ability do not ("..._W_windwall_activate").
// Denylisting only "activate" left every other sibling in that family free to be
// misclassified as a wall, and a level fallback would have made each one a valid
// rank-1 wall with a bogus segment.
inline bool IsWindwallName(std::string_view lowerName) {
    if (lowerName.rfind("yasuo", 0) != 0) {
        return false;
    }
    if (lowerName.find("_w_windwall") == std::string_view::npos) {
        return false;
    }
    return !lowerName.empty() &&
           lowerName.back() >= '0' &&
           lowerName.back() <= '9';
}

inline int ParseMainLevel(std::string_view name) {
    const std::string lower = LowerAscii(name);
    if (!IsWindwallName(lower)) {
        return 0;
    }

    // Trailing digits are the rank: "windwall1" -> 1, "_enemy_05" -> 5.
    std::size_t begin = lower.size();
    while (begin > 0 && lower[begin - 1] >= '0' && lower[begin - 1] <= '9') {
        --begin;
    }

    int level = 0;
    for (std::size_t k = begin; k < lower.size(); ++k) {
        level = level * 10 + (lower[k] - '0');
        if (level > 5) {
            return 5;
        }
    }
    return std::clamp(level, 1, 5);
}

inline ObjectRole ClassifyName(std::string_view name) {
    const std::string lower = LowerAscii(name);
    if (ParseMainLevel(lower) != 0) {
        return ObjectRole::Main;
    }
    if (lower == "yasuowchildmis") {
        return ObjectRole::Endpoint;
    }
    if (lower == "yasuow_visualmis") {
        return ObjectRole::Visual;
    }
    return ObjectRole::Other;
}

struct ObjectState {
    Identity identity = {};
    ObjectRole role = ObjectRole::Other;
    int level = 0;
    int createdTick = 0;
    Vec2 position = {};
    bool pendingName = false;
    // Raw team slot off the object. The windwall emitters themselves always read
    // 0, but the co-located YasuoW_VisualMis object carries the caster's real
    // slot (1 / 2), which is where a wall's ownership comes from.
    std::uint32_t team = 0;
};

// A wall's own emitter has no usable team, so ownership is taken from the nearest
// Visual object spawned alongside it. Anything further away than this is a
// different Yasuo's wall and must not donate its team.
inline constexpr float kMaxVisualTeamDistance = 200.0f;

struct WallSegment {
    Identity main = {};
    Identity endpointA = {};
    Identity endpointB = {};
    int level = 1;
    int spawnTick = 0;
    Vec2 center = {};
    Vec2 start = {};
    Vec2 end = {};
    // Raw team slot of the caster, 0 when it could not be established.
    std::uint32_t team = 0;

    float Span() const {
        return start.Distance(end);
    }
};

inline float PointSegmentDistanceSqr(Vec2 point, Vec2 start, Vec2 end) {
    const Vec2 segment = end - start;
    const float lengthSqr = segment.LengthSqr();
    if (lengthSqr <= 0.000001f) {
        return point.DistanceSqr(start);
    }

    const float t = std::clamp(
        (point - start).Dot(segment) / lengthSqr,
        0.0f,
        1.0f);
    return point.DistanceSqr(start + segment * t);
}

inline float Cross(Vec2 a, Vec2 b, Vec2 c) {
    return (b - a).Cross(c - a);
}

inline bool OnSegment(Vec2 a, Vec2 b, Vec2 point) {
    constexpr float epsilon = 0.0001f;
    return std::fabs(Cross(a, b, point)) <= epsilon &&
           point.x >= std::min(a.x, b.x) - epsilon &&
           point.x <= std::max(a.x, b.x) + epsilon &&
           point.y >= std::min(a.y, b.y) - epsilon &&
           point.y <= std::max(a.y, b.y) + epsilon;
}

inline bool SegmentsIntersect(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    const float abC = Cross(a, b, c);
    const float abD = Cross(a, b, d);
    const float cdA = Cross(c, d, a);
    const float cdB = Cross(c, d, b);

    if (((abC > 0.0f && abD < 0.0f) || (abC < 0.0f && abD > 0.0f)) &&
        ((cdA > 0.0f && cdB < 0.0f) || (cdA < 0.0f && cdB > 0.0f))) {
        return true;
    }

    return OnSegment(a, b, c) || OnSegment(a, b, d) ||
           OnSegment(c, d, a) || OnSegment(c, d, b);
}

inline float SegmentDistanceSqr(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    if (SegmentsIntersect(a, b, c, d)) {
        return 0.0f;
    }
    return std::min({
        PointSegmentDistanceSqr(a, c, d),
        PointSegmentDistanceSqr(b, c, d),
        PointSegmentDistanceSqr(c, a, b),
        PointSegmentDistanceSqr(d, a, b),
    });
}

inline bool IntersectsProjectilePath(
    Vec2 pathStart,
    Vec2 pathEnd,
    const WallSegment& wall,
    float radius) {
    if (!pathStart.IsValid() || !pathEnd.IsValid() ||
        !wall.start.IsValid() || !wall.end.IsValid() ||
        !std::isfinite(radius) || radius < 0.0f) {
        return false;
    }

    return SegmentDistanceSqr(pathStart, pathEnd, wall.start, wall.end) <=
           radius * radius + 0.0001f;
}

class Registry {
public:
    void OnCreate(
        Identity identity,
        int tick,
        std::string_view name,
        Vec2 position,
        std::uint32_t team = 0) {
        if (!identity.IsValid()) {
            return;
        }

        const ObjectRole role = ClassifyName(name);
        if (!name.empty() && role == ObjectRole::Other) {
            return;
        }

        auto it = Find(identity);
        if (it == entries_.end()) {
            entries_.push_back({
                identity,
                role,
                ParseMainLevel(name),
                tick,
                position,
                name.empty(),
                team,
            });
            return;
        }

        Update(identity, tick, name, position, team);
    }

    void Update(
        Identity identity,
        int tick,
        std::string_view name,
        Vec2 position,
        std::uint32_t team = 0) {
        auto it = Find(identity);
        if (it == entries_.end()) {
            OnCreate(identity, tick, name, position, team);
            return;
        }

        if (!name.empty()) {
            const ObjectRole role = ClassifyName(name);
            if (role == ObjectRole::Other) {
                entries_.erase(it);
                return;
            }
            it->role = role;
            it->level = ParseMainLevel(name);
            it->pendingName = false;
        }

        if (position.IsValid()) {
            it->position = position;
        }
        if (team != 0) {
            it->team = team;
        }
        it->identity = identity;
    }

    void OnDelete(const Identity& identity) {
        entries_.erase(
            std::remove_if(
                entries_.begin(),
                entries_.end(),
                [&](const ObjectState& item) {
                    return SameIdentity(item.identity, identity);
                }),
            entries_.end());
        RefreshWalls();
    }

    void Refresh(int now) {
        entries_.erase(
            std::remove_if(
                entries_.begin(),
                entries_.end(),
                [&](const ObjectState& item) {
                    const int age = now - item.createdTick;
                    return (item.pendingName && age > kPendingNameLifetimeMs) ||
                           (!item.pendingName && age > kWallLifetimeMs);
                }),
            entries_.end());
        RefreshWalls();
    }

    const std::vector<ObjectState>& Entries() const {
        return entries_;
    }

    const std::vector<WallSegment>& ActiveWalls() const {
        return walls_;
    }

private:
    std::vector<ObjectState> entries_;
    std::vector<WallSegment> walls_;

    auto Find(const Identity& identity) {
        return std::find_if(
            entries_.begin(),
            entries_.end(),
            [&](const ObjectState& item) {
                return SameIdentity(item.identity, identity);
            });
    }

    // Ownership of a wall. The emitter and the endpoints report team 0, so the
    // caster's slot is taken from the closest YasuoW_VisualMis spawned with the
    // wall. Verified live with an ally and an enemy Yasuo walling at once: the two
    // Visual objects sat on their respective wall centres carrying slots 1 and 2
    // while all three windwall emitters read 0. Endpoints are consulted first in
    // case a build ever does populate them.
    static std::uint32_t ResolveTeam(
        const ObjectState* main,
        const ObjectState* endpointA,
        const ObjectState* endpointB,
        const std::vector<const ObjectState*>& visuals) {
        if (endpointA && endpointA->team != 0) {
            return endpointA->team;
        }
        if (endpointB && endpointB->team != 0) {
            return endpointB->team;
        }
        if (!main) {
            return 0;
        }

        std::uint32_t team = main->team;
        float bestDistanceSqr = kMaxVisualTeamDistance * kMaxVisualTeamDistance;
        for (const ObjectState* visual : visuals) {
            if (!visual || visual->team == 0) {
                continue;
            }
            const float distanceSqr = visual->position.DistanceSqr(main->position);
            if (distanceSqr <= bestDistanceSqr) {
                bestDistanceSqr = distanceSqr;
                team = visual->team;
            }
        }
        return team;
    }

    void RefreshWalls() {
        walls_.clear();

        std::vector<const ObjectState*> mains;
        std::vector<const ObjectState*> endpoints;
        std::vector<const ObjectState*> visuals;
        for (const auto& item : entries_) {
            if (item.role == ObjectRole::Main) {
                mains.push_back(&item);
            } else if (item.role == ObjectRole::Endpoint) {
                endpoints.push_back(&item);
            } else if (item.role == ObjectRole::Visual) {
                visuals.push_back(&item);
            }
        }

        std::sort(
            mains.begin(),
            mains.end(),
            [](const ObjectState* a, const ObjectState* b) {
                if (a->createdTick != b->createdTick) {
                    return a->createdTick < b->createdTick;
                }
                return IdentityLess(a->identity, b->identity);
            });

        std::vector<bool> used(endpoints.size(), false);
        for (const ObjectState* main : mains) {
            using PairScore = std::tuple<
                float,
                int,
                std::uint32_t,
                std::uint32_t,
                std::size_t,
                std::size_t>;
            std::optional<PairScore> best;

            for (std::size_t i = 0; i < endpoints.size(); ++i) {
                if (used[i]) {
                    continue;
                }
                for (std::size_t j = i + 1; j < endpoints.size(); ++j) {
                    if (used[j] ||
                        SameIdentity(endpoints[i]->identity, endpoints[j]->identity)) {
                        continue;
                    }

                    const int dtA = std::abs(
                        endpoints[i]->createdTick - main->createdTick);
                    const int dtB = std::abs(
                        endpoints[j]->createdTick - main->createdTick);
                    if (dtA > kPairWindowMs || dtB > kPairWindowMs ||
                        endpoints[i]->position.DistanceSqr(endpoints[j]->position) < 1.0f) {
                        continue;
                    }

                    const Vec2 midpoint =
                        (endpoints[i]->position + endpoints[j]->position) * 0.5f;
                    const float midpointDistance =
                        midpoint.DistanceSqr(main->position);
                    if (midpointDistance >
                        kMaxMidpointDistance * kMaxMidpointDistance) {
                        continue;
                    }

                    const std::uint32_t low = std::min(
                        endpoints[i]->identity.networkId,
                        endpoints[j]->identity.networkId);
                    const std::uint32_t high = std::max(
                        endpoints[i]->identity.networkId,
                        endpoints[j]->identity.networkId);
                    const PairScore score = {
                        midpointDistance,
                        dtA + dtB,
                        low,
                        high,
                        i,
                        j,
                    };
                    if (!best || score < *best) {
                        best = score;
                    }
                }
            }

            if (!best) {
                continue;
            }

            const std::size_t i = std::get<4>(*best);
            const std::size_t j = std::get<5>(*best);
            const ObjectState* a = endpoints[i];
            const ObjectState* b = endpoints[j];
            if (IdentityLess(b->identity, a->identity)) {
                std::swap(a, b);
            }

            walls_.push_back({
                main->identity,
                a->identity,
                b->identity,
                main->level,
                main->createdTick,
                main->position,
                a->position,
                b->position,
                ResolveTeam(main, a, b, visuals),
            });
            used[i] = true;
            used[j] = true;
        }
    }
};

} // namespace SDK::YasuoWallModel
