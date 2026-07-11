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

inline int ParseMainLevel(std::string_view name) {
    const std::string lower = LowerAscii(name);
    constexpr std::string_view prefix = "yasuo_base_w_windwall";
    if (lower.size() != prefix.size() + 1 ||
        lower.compare(0, prefix.size(), prefix) != 0) {
        return 0;
    }

    const char suffix = lower.back();
    return suffix >= '1' && suffix <= '5' ? suffix - '0' : 0;
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
};

struct WallSegment {
    Identity main = {};
    Identity endpointA = {};
    Identity endpointB = {};
    int level = 1;
    int spawnTick = 0;
    Vec2 center = {};
    Vec2 start = {};
    Vec2 end = {};

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
        Vec2 position) {
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
            });
            return;
        }

        Update(identity, tick, name, position);
    }

    void Update(
        Identity identity,
        int tick,
        std::string_view name,
        Vec2 position) {
        auto it = Find(identity);
        if (it == entries_.end()) {
            OnCreate(identity, tick, name, position);
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

    void RefreshWalls() {
        walls_.clear();

        std::vector<const ObjectState*> mains;
        std::vector<const ObjectState*> endpoints;
        for (const auto& item : entries_) {
            if (item.role == ObjectRole::Main) {
                mains.push_back(&item);
            } else if (item.role == ObjectRole::Endpoint) {
                endpoints.push_back(&item);
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
            });
            used[i] = true;
            used[j] = true;
        }
    }
};

} // namespace SDK::YasuoWallModel
