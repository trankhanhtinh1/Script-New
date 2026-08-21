#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::Katarina {

inline Menu* MenuRoot = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 625.0f };
inline Spell W{ SpellSlot::W, 340.0f };
inline Spell E{ SpellSlot::E, 775.0f };
inline Spell R{ SpellSlot::R, 550.0f };

struct Dagger {
    GameObject Unit;
    Vector3 Position;
    int CreateTick = 0;
    int NetworkId = 0;
};

inline bool Loaded = false;
inline bool UpdateR = false;
inline int LastR = 0;
inline int LastCastE = 0;
inline int LastCastQ = 0;
inline int LastCastW = 0;
inline int LastCastR = 0;
inline int LastBasicAttackTick = 0;
inline int LastMoveTick = 0;
inline AIBaseClient PendingEQTarget;
inline int PendingEQTick = 0;
inline int PendingDaggerResetUntilTick = 0;
inline int PendingDaggerHoldNetworkId = 0;
inline Vector3 PendingDaggerHoldPosition;
inline int PendingDaggerGoneTick = 0;

enum class EEQKillStealStage {
    None,
    WaitingForDagger,
    WaitingForQ
};

inline EEQKillStealStage PendingEEQStage = EEQKillStealStage::None;
inline AIBaseClient PendingEEQTarget;
inline int PendingEEQDeadlineTick = 0;
inline int PendingEEQSecondCastTick = 0;

// Ngay sau khi bấm R, trạng thái di chuyển của frame trước còn sót lại vài chục
// ms, nên phải có khoảng ân hạn trước khi coi di chuyển là hủy channel.
inline constexpr int RMoveGraceMs = 250;
inline constexpr int DaggerReadyAgeMs = 1000;
inline constexpr int DaggerWaitWindowMs = 200;
inline constexpr int DaggerPickupGraceMs = 250;
inline constexpr int MaxEEQResetWaitMs = 250;
inline std::vector<Dagger> Daggers;

static bool IsDaggerName(const std::string& name) {
    static constexpr const char* suffix = "_W_Indicator_Ally";
    return name.starts_with("Katarina_") && name.ends_with(suffix);
}

static bool IsDaggerObject(const GameObject& object) {
    return object.IsValid() && IsDaggerName(GetObjectName(object));
}

static bool HasLiveDaggerRuntimeName(const GameObject& object) {
    const uintptr_t address = object.Address();
    if (!Globals::IsValidPtr(address)) {
        return false;
    }

    // Bypass StaticStringCache: when a dagger is picked, the native object can
    // remain in AllGameObjects with the same address/NetworkId while Riot
    // clears Name/CharacterName in-place. Cached Name() would keep the old
    // indicator string and falsely report a live dagger.
    char name[96] = {};
    if (::Core::Objects::ReadName(address, name, sizeof(name)) && name[0]) {
        return IsDaggerName(name);
    }
    char characterName[96] = {};
    return ::Core::Objects::ReadCharacterName(
               address, characterName, sizeof(characterName)) &&
           characterName[0] && IsDaggerName(characterName);
}

// Trạng thái channel R được suy ra từ mốc OnProcessSpell của chính chiêu R:
//   - Không có buff "KatarinaRSound" -> chắc chắn không đang R.
//   - Có buff -> R chỉ còn sống nếu SAU tick cast R không có: Q/W/E, đánh
//     thường, hay di chuyển/lướt. Katarina bị trói suốt channel nên di chuyển
//     đồng nghĩa R đã đứt.
// Hàm chỉ đọc trạng thái, không đổi static nào ảnh hưởng kết quả, nên gọi bao
// nhiêu lần trong một frame cũng cho cùng một đáp án.
static bool HaveRBuff() {
    const auto player = Player();

    if (!player.HasBuff("KatarinaRSound")) {
        return false;
    }

    // Buff còn nhưng chưa từng bắt được event cast R (vd. load giữa channel)
    // -> thiếu mốc so sánh, coi như không đang R cho an toàn.
    if (LastCastR == 0) {
        return false;
    }

    const bool cancelled =
        LastCastQ > LastCastR ||
        LastCastW > LastCastR ||
        LastCastE > LastCastR ||
        LastBasicAttackTick > LastCastR ||
        LastMoveTick > LastCastR + RMoveGraceMs;

    const bool result = !cancelled;

    static bool lastLoggedResult = false;
    static float lastLogTime = 0.0f;
    const float gameTime = Game::Time();
    if (result != lastLoggedResult || gameTime - lastLogTime > 0.5f) {
        lastLoggedResult = result;
        lastLogTime = gameTime;
        NightSharpDebug::Logf(
            "[Katarina R Debug] R_Active=%d | HasBuff=1 | Age=%dms | dQ=%d dW=%d dE=%d dAA=%d dMove=%d (grace=%d)",
            result,
            SDK::Variables::TickCount() - LastCastR,
            LastCastQ - LastCastR,
            LastCastW - LastCastR,
            LastCastE - LastCastR,
            LastBasicAttackTick - LastCastR,
            LastMoveTick - LastCastR,
            RMoveGraceMs);
    }

    return result;
}


static bool IsOwnDagger(const Dagger& dagger) {
    return dagger.Unit.IsValid();
}

static bool IsDaggerReady(const Dagger& dagger) {
    return IsOwnDagger(dagger) &&
           SDK::Variables::TickCount() - dagger.CreateTick >= DaggerReadyAgeMs;
}

static int DaggerReadyInMs(const Dagger& dagger) {
    return std::max(
        0,
        DaggerReadyAgeMs -
            std::max(0, SDK::Variables::TickCount() - dagger.CreateTick));
}

static void PruneDaggers() {
    const int now = SDK::Variables::TickCount();
    Daggers.erase(
        std::remove_if(
            Daggers.begin(), Daggers.end(),
            [now](const Dagger& dagger) {
                return dagger.Position.IsZero() ||
                       now - dagger.CreateTick >= 5000 ||
                       !GameObjects::ContainsNetworkId(dagger.NetworkId) ||
                       !GameObjects::detail::HasLiveIdentity(dagger.Unit) ||
                       !HasLiveDaggerRuntimeName(dagger.Unit);
            }),
        Daggers.end());
}

static bool HasOwnDagger() {
    return std::any_of(Daggers.begin(), Daggers.end(), [](const Dagger& dagger) {
        return IsOwnDagger(dagger);
    });
}

static float MagicalEffectiveHealth(const AIBaseClient& target) {
    return target.Health() + target.AllShield() + target.MagicalShield();
}

static const Dagger* FindDagger(int networkId) {
    if (networkId == 0) {
        return nullptr;
    }
    const auto found = std::find_if(
        Daggers.begin(), Daggers.end(), [networkId](const Dagger& dagger) {
            return dagger.NetworkId == networkId && IsOwnDagger(dagger);
        });
    return found != Daggers.end() ? &*found : nullptr;
}

static int SpellRank(const Spell& spell, int maxRank) {
    return std::clamp(spell.Level(), 0, maxRank);
}

static float TotalAttackDamage() {
    const auto player = Player();
    return player.IsValid() ? player.TotalAttackDamage() : 0.0f;
}

static float BonusAttackSpeed() {
    const auto player = Player();
    return player.IsValid() ? std::max(0.0f, player.AttackSpeedMod() - 1.0f) : 0.0f;
}

static float MagicDamageAmp(const AIBaseClient& target);

static float PassiveDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    // Voracity / Sinister Steel: 68-240 at normal levels (257/275 at 19/20)
    // +60% bonus AD +70/80/90/100% AP based on champion level.
    static constexpr float baseDamage[] = {
        0.0f, 68.0f, 72.0f, 77.0f, 82.0f, 89.0f, 96.0f, 103.0f, 112.0f,
        121.0f, 131.0f, 142.0f, 154.0f, 166.0f, 180.0f, 194.0f, 208.0f,
        224.0f, 240.0f, 257.0f, 275.0f
    };
    const int level = std::clamp(player.Level(), 1, 20);
    float apRatio = 1.00f;
    if (level < 6) {
        apRatio = 0.70f;
    } else if (level < 11) {
        apRatio = 0.80f;
    } else if (level < 16) {
        apRatio = 0.90f;
    }

    const float raw =
        baseDamage[level] +
        apRatio * player.AP() +
        0.60f * player.BonusAttackDamage();
    return player.CalculateMagicDamage(target, raw) * MagicDamageAmp(target);
}

static float QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    static constexpr float baseDamage[] = { 0.0f, 80.0f, 115.0f, 150.0f, 185.0f, 220.0f };
    const int rank = SpellRank(Q, 5);
    if (rank <= 0) {
        return 0.0f;
    }

    return player.CalculateMagicDamage(target, baseDamage[rank] + 0.40f * player.AP()) *
           MagicDamageAmp(target);
}

// Bội số khuếch đại sát thương PHÉP từ trang bị. Áp lên sát thương SAU khi đã
// trừ kháng phép, vì các hiệu ứng này khuếch đại sát thương cuối cùng.
// Chỉ cần id gốc: bản sao ARAM/Arena đã được CoreItem::HasItemId chuẩn hóa.
static float MagicDamageAmp(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 1.0f;
    }

    float amp = 1.0f;

    // Shadowflame (4645) — Cinderbloom: sát thương phép và chuẩn "chí mạng" lên
    // mục tiêu dưới 40% máu, +20% sát thương.
    if (SDK::Items::HasItem(player, SDK::ItemId::Shadowflame) && target.HealthPercent() < 40.0f) {
        amp *= 1.20f;
    }

    // Perplexity (4015) — Giant Slayer: tối đa +15% lên tướng có max HP cao hơn
    // mình, đạt trần khi chênh lệch >= 3000.
    if (target.IsHero() && SDK::Items::HasItem(player, SDK::ItemId::Perplexity)) {
        const float diff = target.MaxHealth() - player.MaxHealth();
        if (diff > 0.0f) {
            amp *= 1.0f + 0.15f * std::min(diff / 3000.0f, 1.0f);
        }
    }

    // Lightning Braid (4013) — Chain Lightning: -20% sát thương kỹ năng.
    if (SDK::Items::HasItem(player, SDK::ItemId::Lightning_Braid)) {
        amp *= 0.80f;
    }

    // CHƯA HỖ TRỢ: Riftmaker (4633) Void Corruption (+2%/giây giao tranh, trần
    // +8%). Cần đọc số stack của buff mà mình chưa xác minh được tên buff — đoán
    // bừa sẽ cho ra số sai, nên tạm bỏ qua.

    return amp;
}

static float NonMagicAbilityDamageAmp(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 1.0f;
    }

    float amp = 1.0f;
    if (target.IsHero() && SDK::Items::HasItem(player, SDK::ItemId::Perplexity)) {
        const float diff = target.MaxHealth() - player.MaxHealth();
        if (diff > 0.0f) {
            amp *= 1.0f + 0.15f * std::min(diff / 3000.0f, 1.0f);
        }
    }
    if (SDK::Items::HasItem(player, SDK::ItemId::Lightning_Braid)) {
        amp *= 0.80f;
    }
    return amp;
}

static float EDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    static constexpr float baseDamage[] = { 0.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f };
    const int rank = SpellRank(E, 5);
    if (rank <= 0) {
        return 0.0f;
    }

    const float raw =
        baseDamage[rank] + 0.40f * TotalAttackDamage() + 0.25f * player.AP();
    return player.CalculateMagicDamage(target, raw) * MagicDamageAmp(target);
}

static float EOnHitDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    // Shunpo applies on-hit effects, but it does not add the basic attack's
    // base physical damage. GetPassiveDamage returns only those extra effects.
    return Damage::GetPassiveDamage(player, target);
}

static float DaggerPickupDamage(const AIBaseClient& target) {
    return PassiveDamage(target) +
           (target.IsHero() ? EOnHitDamage(target) : 0.0f);
}

struct MixedDamage {
    float Magical = 0.0f;
    float Physical = 0.0f;
};

static MixedDamage RDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return {};
    }

    const int rank = SpellRank(R, 3);
    if (rank <= 0) {
        return {};
    }

    static constexpr float baseMagicDamage[] = { 0.0f, 375.0f, 562.5f, 750.0f };
    const float magicDamage = player.CalculateMagicDamage(
        target,
        baseMagicDamage[rank] + 2.85f * player.AP()) * MagicDamageAmp(target);
    const float physicalDamage = player.CalculatePhysicalDamage(
        target,
        (2.40f + 7.50f * BonusAttackSpeed()) * player.BonusAttackDamage()) *
        NonMagicAbilityDamageAmp(target);

    // Giữ nguyên hệ số khoảng cách của R theo yêu cầu.
    const float distanceScale = target.DistanceToPlayer() <= 350.0f ? 1.0f : 0.60f;
    return { magicDamage * distanceScale, physicalDamage * distanceScale };
}

static bool IsRDamageLethal(const AIBaseClient& target) {
    const MixedDamage damage = RDamage(target);
    const float postTypeShields =
        std::max(0.0f, damage.Magical - target.MagicalShield()) +
        std::max(0.0f, damage.Physical - target.PhysicalShield());
    return postTypeShields >= target.Health() + target.AllShield();
}

static bool UnderTower(const Vector3& position) {
    const auto player = Player();
    const float extraRadius = player.IsValid() ? player.BoundingRadius() : 65.0f;
    const float range = 850.0f + extraRadius;
    const float rangeSqr = range * range;

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*for (const auto& turret : GameObjects::EnemyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) {
            continue;
        }
        if (turret.Position().DistanceSqr2D(position) <= rangeSqr) {
            return true;
        }
    }*/

    for (const auto& spawn : GameObjects::EnemySpawnPoints()) {
        if (spawn.IsValid() && spawn.Position().DistanceSqr2D(position) <= rangeSqr) {
            return true;
        }
    }
    return false;
}

static bool AllowDashTo(const Vector3& position) {
    if (position.IsZero() || NavMesh::IsWall(position)) {
        return false;
    }
    return Bool(MenuRoot, "Turret", false) || !UnderTower(position);
}

static bool CastE(const Vector3& position) {
    const int now = SDK::Variables::TickCount();
    if (!E.IsReady() ||
        now - LastCastE <= 250 ||
        position.IsZero() ||
        Player().Distance(position) > E.Range) {
        return false;
    }

    if (!E.Cast(position)) {
        return false;
    }

    LastCastE = SDK::Variables::TickCount();
    return true;
}

static bool CastQ(const AIBaseClient& target) {
    if (!Q.IsReady() || !ValidTarget(target, Q.Range)) {
        return false;
    }
    if (target.IsHero()) {
        const auto player = Player();
        const auto prediction = Q.GetPrediction(target);
        Vector3 castPosition = prediction.GetCastPosition();
        if (castPosition.IsZero()) {
            castPosition = target.Position();
        }
        if (!player.IsValid() || SDK::Collision::HasProjectileWallCollision(
                player.Position(), castPosition, 30.0f)) {
            return false;
        }
    }
    return Q.Cast(target) == CastStates::SuccessfullyCasted;
}

static Vector3 GetBestECastPos(
    const AIBaseClient& target,
    const Dagger& dagger,
    float maxTargetDistance = 340.0f) {
    const auto player = Player();
    const Vector3 A = player.Position();
    const Vector3 B = dagger.Position;
    const Vector3 T = target.Position();
    const float R_A = E.Range;
    const float playerRadius = player.BoundingRadius();
    const float targetRadius = target.BoundingRadius();
    const float R_B = 150.0f + playerRadius;
    const float distToDagger = T.Distance2D(B);
    Vector3 bestPos;

    if (distToDagger <= 150.0f) {
        bestPos = T.Extend(B, 10.0f);
    } else if (distToDagger <= 150.0f + targetRadius + playerRadius - 5.0f) {
        bestPos = T.Extend(B, targetRadius - 5.0f);
    } else {
        bestPos = B.Extend(T, R_B);
    }

    if (A.Distance2D(bestPos) <= R_A) {
        bestPos.y = distToDagger <= 150.0f ? T.y : B.y;
        if (T.Distance2D(bestPos) <= maxTargetDistance) {
            return bestPos;
        }
        return Vector3();
    }

    const float d = A.Distance2D(B);
    if (d > R_A + R_B) {
        return Vector3();
    }

    Vector3 P_B = B.Extend(T, R_B);
    if (A.Distance2D(P_B) <= R_A) {
        P_B.y = B.y;
        if (T.Distance2D(P_B) <= maxTargetDistance) {
            return P_B;
        }
        return Vector3();
    }

    Vector3 P_A = A.Extend(T, R_A);
    if (B.Distance2D(P_A) <= R_B) {
        P_A.y = T.y;
        if (T.Distance2D(P_A) <= maxTargetDistance) {
            return P_A;
        }
        return Vector3();
    }

    if (d > 0.0f) {
        const float a = (R_A * R_A - R_B * R_B + d * d) / (2.0f * d);
        const float h_sqr = R_A * R_A - a * a;
        if (h_sqr >= 0.0f) {
            const float h = std::sqrt(h_sqr);
            const Vector3 dir{B.x - A.x, 0.0f, B.z - A.z};
            const Vector3 normalizedDir = dir.Normalized();
            const Vector3 P_m = A + normalizedDir * a;
            const Vector3 perp{-normalizedDir.z, 0.0f, normalizedDir.x};
            Vector3 I1 = P_m + perp * h;
            Vector3 I2 = P_m - perp * h;
            I1.y = B.y;
            I2.y = B.y;
            Vector3 bestI = (T.Distance2D(I1) < T.Distance2D(I2)) ? I1 : I2;
            if (T.Distance2D(bestI) <= maxTargetDistance) {
                return bestI;
            }
        }
    }

    return Vector3();
}

inline constexpr float DaggerPickupRadius = 150.0f;
inline constexpr float PassiveHitRadius = 340.0f;
inline constexpr float ShunpoDamageRadius = 200.0f;

struct ECandidate {
    Vector3 Position;
    const Dagger* Anchor = nullptr;
    int ReadyPickups = 0;
    int PassiveHits = 0;
    bool AnchorReady = false;
    int AnchorAgeMs = 0;
    int EResetDelayMs = INT_MAX;
    float TargetDistance = FLT_MAX;
    float EstimatedDamage = 0.0f;

    bool IsValid() const {
        return Anchor != nullptr && !Position.IsZero();
    }
};

static int ExpectedShunpoReadyDelayMs(const ECandidate& candidate);

enum class DaggerReadinessFilter {
    Any,
    ReadyOnly,
    UnreadyOnly
};

static void AddECandidate(std::vector<Vector3>& candidates, Vector3 position, float height) {
    if (position.IsZero()) {
        return;
    }
    position.y = height;
    for (const auto& existing : candidates) {
        if (existing.DistanceSqr2D(position) <= 25.0f) {
            return;
        }
    }
    candidates.push_back(position);
}

static ECandidate EvaluateECandidate(const AIBaseClient& target, const Vector3& position) {
    ECandidate candidate;
    const auto player = Player();
    const float pickupRange = DaggerPickupRadius + player.BoundingRadius();
    if (!player.IsValid() || position.IsZero() ||
        player.Distance(position) > E.Range || !AllowDashTo(position)) {
        return candidate;
    }

    float closestAnchorDistance = FLT_MAX;
    for (const auto& dagger : Daggers) {
        if (!IsOwnDagger(dagger)) {
            continue;
        }
        const float distance = dagger.Position.Distance2D(position);
        if (distance > pickupRange) {
            continue;
        }
        const bool sameDistance = std::fabs(distance - closestAnchorDistance) <= 1.0f;
        const bool betterTie = sameDistance && candidate.Anchor &&
            (IsDaggerReady(dagger) != IsDaggerReady(*candidate.Anchor)
                ? IsDaggerReady(dagger)
                : dagger.CreateTick < candidate.Anchor->CreateTick);
        if (candidate.Anchor && distance >= closestAnchorDistance - 1.0f && !betterTie) {
            continue;
        }
        candidate.Anchor = &dagger;
        closestAnchorDistance = distance;
    }
    if (!candidate.Anchor) {
        return ECandidate{};
    }

    candidate.AnchorReady = IsDaggerReady(*candidate.Anchor);
    candidate.AnchorAgeMs = std::max(
        0, SDK::Variables::TickCount() - candidate.Anchor->CreateTick);
    candidate.TargetDistance = target.Position().Distance2D(position);
    for (const auto& dagger : Daggers) {
        if (!IsDaggerReady(dagger) ||
            dagger.Position.Distance2D(position) > pickupRange) {
            continue;
        }
        ++candidate.ReadyPickups;
        if (candidate.TargetDistance <= PassiveHitRadius + target.BoundingRadius()) {
            ++candidate.PassiveHits;
        }
    }

    candidate.EstimatedDamage =
        DaggerPickupDamage(target) * static_cast<float>(candidate.PassiveHits);
    if (candidate.TargetDistance <= ShunpoDamageRadius + target.BoundingRadius()) {
        candidate.EstimatedDamage += EDamage(target) + EOnHitDamage(target);
    }
    candidate.Position = position;
    candidate.EResetDelayMs = ExpectedShunpoReadyDelayMs(candidate);
    return candidate;
}

static bool IsBetterECandidate(const ECandidate& candidate, const ECandidate& current,
                               const AIBaseClient& target) {
    if (!candidate.IsValid()) {
        return false;
    }
    if (!current.IsValid()) {
        return true;
    }

    // A landed dagger has deterministic pickup/passive value. An airborne
    // dagger is only a fallback even when its raw cursor position is closer.
    if (candidate.AnchorReady != current.AnchorReady) {
        return candidate.AnchorReady;
    }
    if (candidate.EResetDelayMs != current.EResetDelayMs) {
        return candidate.EResetDelayMs < current.EResetDelayMs;
    }
    if (!candidate.AnchorReady && candidate.AnchorAgeMs != current.AnchorAgeMs) {
        return candidate.AnchorAgeMs > current.AnchorAgeMs;
    }

    const float effectiveHealth = MagicalEffectiveHealth(target);
    const bool candidateLethal = candidate.EstimatedDamage >= effectiveHealth;
    const bool currentLethal = current.EstimatedDamage >= effectiveHealth;
    if (candidateLethal != currentLethal) {
        return candidateLethal;
    }
    if (candidate.PassiveHits != current.PassiveHits) {
        return candidate.PassiveHits > current.PassiveHits;
    }
    if (candidate.ReadyPickups != current.ReadyPickups) {
        return candidate.ReadyPickups > current.ReadyPickups;
    }
    if (std::fabs(candidate.TargetDistance - current.TargetDistance) > 5.0f) {
        return candidate.TargetDistance < current.TargetDistance;
    }
    return Player().Distance(candidate.Position) < Player().Distance(current.Position);
}

static ECandidate BestECandidate(
    const AIBaseClient& target,
    float maxTargetDistance = PassiveHitRadius,
    DaggerReadinessFilter readiness = DaggerReadinessFilter::Any) {
    ECandidate best;
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return best;
    }

    const float pickupRange = DaggerPickupRadius + player.BoundingRadius();
    std::vector<Vector3> candidates;
    candidates.reserve(Daggers.size() * Daggers.size() * 2 + 2);

    for (const auto& dagger : Daggers) {
        if (!IsOwnDagger(dagger) ||
            player.Distance(dagger.Position) > E.Range + pickupRange) {
            continue;
        }
        AddECandidate(candidates, GetBestECastPos(target, dagger, maxTargetDistance),
                      dagger.Position.y);
        AddECandidate(candidates, dagger.Position, dagger.Position.y);
    }

    // Pair midpoints/intersections are important when several pickup circles
    // overlap: the old nearest-dagger rule regularly consumed only one dagger.
    for (size_t i = 0; i < Daggers.size(); ++i) {
        const auto& first = Daggers[i];
        if (!IsOwnDagger(first)) {
            continue;
        }
        for (size_t j = i + 1; j < Daggers.size(); ++j) {
            const auto& second = Daggers[j];
            if (!IsOwnDagger(second)) {
                continue;
            }
            const float distance = first.Position.Distance2D(second.Position);
            if (distance <= 0.0f || distance > pickupRange * 2.0f) {
                continue;
            }

            Vector3 midpoint = first.Position + (second.Position - first.Position) * 0.5f;
            AddECandidate(candidates, midpoint, first.Position.y);

            const Vector3 horizontalDelta{
                second.Position.x - first.Position.x,
                0.0f,
                second.Position.z - first.Position.z
            };
            const Vector3 direction = horizontalDelta.Normalized();
            const Vector3 perpendicular{-direction.z, 0.0f, direction.x};
            const float offset = std::sqrt(std::max(
                0.0f, pickupRange * pickupRange - distance * distance * 0.25f));
            AddECandidate(candidates, midpoint + perpendicular * offset, first.Position.y);
            AddECandidate(candidates, midpoint - perpendicular * offset, second.Position.y);
        }
    }

    for (const auto& candidatePosition : candidates) {
        ECandidate candidate = EvaluateECandidate(target, candidatePosition);
        if ((readiness == DaggerReadinessFilter::ReadyOnly && !candidate.AnchorReady) ||
            (readiness == DaggerReadinessFilter::UnreadyOnly && candidate.AnchorReady)) {
            continue;
        }
        if (candidate.TargetDistance > maxTargetDistance + target.BoundingRadius()) {
            continue;
        }
        if (IsBetterECandidate(candidate, best, target)) {
            best = candidate;
        }
    }
    return best;
}

static AIHeroClient BestDaggerTarget(
    float range,
    DaggerReadinessFilter readiness = DaggerReadinessFilter::Any) {
    AIHeroClient best;
    float bestScore = FLT_MAX;
    const auto player = Player();
    for (const auto& target : EnemyHeroes(range)) {
        if (!ValidHeroTarget(target, range)) {
            continue;
        }

        const ECandidate candidate = BestECandidate(target, PassiveHitRadius, readiness);
        const bool canReach = candidate.IsValid() ||
            (readiness == DaggerReadinessFilter::Any &&
             player.Distance(target.Position()) <= E.Range);

        if (!canReach) {
            continue;
        }

        const int daggerHits = candidate.PassiveHits;
        float score = MagicalEffectiveHealth(target) -
                      DaggerPickupDamage(target) * static_cast<float>(daggerHits);

        if (daggerHits > 0) {
            score -= 1000.0f;
        }

        if (!best.IsValid() || score < bestScore) {
            best = target;
            bestScore = score;
        }
    }
    return best;
}

static bool ShouldCancelRForKillsteal(const AIBaseClient& target) {
    if (!HaveRBuff()) {
        return true;
    }
    if (Bool(RMenu, "NeverCancelR", false)) {
        return false;
    }
    if (target.DistanceToPlayer() > R.Range) {
        return true;
    }
    if (SDK::Variables::TickCount() - LastR >= 1500) {
        return true;
    }
    if (Player().CountEnemyHeroesInRange(R.Range) == 0) {
        return true;
    }
    return false;
}

static bool ShouldKeepR() {
    if (!HaveRBuff()) {
        return false;
    }
    if (Bool(RMenu, "NeverCancelR", false) && Player().CountEnemyHeroesInRange(R.Range) > 0) {
        return true;
    }
    if (Player().CountEnemyHeroesInRange(R.Range) == 0) {
        return false;
    }
    if (SDK::Variables::TickCount() - LastR < 1500) {
        return true;
    }
    return false;
}

static bool DoComboE(
    AIBaseClient& outTarget,
    bool checkForDagger = true,
    bool setupQAgainstUnreadyDagger = false,
    bool* waitingForDagger = nullptr,
    bool* waitingForQuickEReset = nullptr) {
    if (waitingForDagger) {
        *waitingForDagger = false;
    }
    if (waitingForQuickEReset) {
        *waitingForQuickEReset = false;
    }
    if (HaveRBuff() && Bool(RMenu, "NeverCancelR", false)) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid() || !E.IsReady()) {
        return false;
    }

    AIHeroClient target;
    if (setupQAgainstUnreadyDagger) {
        target = BestDaggerTarget(
            E.Range + 400.0f, DaggerReadinessFilter::ReadyOnly);
        if (!ValidHeroTarget(target, E.Range + 400.0f)) {
            target = GetMagicalTarget(E.Range + 400.0f);
        }
    } else {
        target = HasOwnDagger()
            ? BestDaggerTarget(E.Range + 400.0f)
            : GetMagicalTarget(E.Range + 400.0f);
    }
    if (!ValidHeroTarget(target, E.Range + 400.0f)) {
        return false;
    }

    outTarget = AIBaseClient(target.Handle());

    const ECandidate daggerCandidate = BestECandidate(
        target,
        PassiveHitRadius,
        setupQAgainstUnreadyDagger
            ? DaggerReadinessFilter::ReadyOnly
            : DaggerReadinessFilter::Any);
    if (daggerCandidate.IsValid()) {
        const bool quickReset = setupQAgainstUnreadyDagger &&
            daggerCandidate.EResetDelayMs <= MaxEEQResetWaitMs;
        if (CastE(daggerCandidate.Position)) {
            if (quickReset) {
                PendingDaggerHoldNetworkId = daggerCandidate.Anchor->NetworkId;
                PendingDaggerHoldPosition = daggerCandidate.Anchor->Position;
                PendingDaggerResetUntilTick =
                    SDK::Variables::TickCount() + MaxEEQResetWaitMs;
                PendingDaggerGoneTick = 0;
                if (waitingForQuickEReset) {
                    *waitingForQuickEReset = true;
                }
            }
            return true;
        }
    }

    if (setupQAgainstUnreadyDagger) {
        const ECandidate unreadyCandidate = BestECandidate(
            target, PassiveHitRadius, DaggerReadinessFilter::UnreadyOnly);
        if (unreadyCandidate.IsValid()) {
            const int timeUntilReady =
                DaggerReadyInMs(*unreadyCandidate.Anchor);
            const bool passiveWillHit =
                unreadyCandidate.TargetDistance <= PassiveHitRadius + target.BoundingRadius();
            if (passiveWillHit &&
                unreadyCandidate.EResetDelayMs <= MaxEEQResetWaitMs &&
                timeUntilReady > DaggerWaitWindowMs &&
                CastE(unreadyCandidate.Position)) {
                PendingDaggerHoldNetworkId = unreadyCandidate.Anchor->NetworkId;
                PendingDaggerHoldPosition = unreadyCandidate.Anchor->Position;
                PendingDaggerResetUntilTick =
                    SDK::Variables::TickCount() + MaxEEQResetWaitMs;
                PendingDaggerGoneTick = 0;
                if (waitingForQuickEReset) {
                    *waitingForQuickEReset = true;
                }
                return true;
            }
            if (timeUntilReady <= DaggerWaitWindowMs && passiveWillHit) {
                if (waitingForDagger) {
                    *waitingForDagger = true;
                }
                return false;
            }
        }
    }

    if (checkForDagger && !Q.IsReady() && !W.IsReady() && !daggerCandidate.IsValid()) {
        return false;
    }

    Vector3 castPos = target.Position();
    Vector3 pathPos = target.Position() + target.Direction() * 100.0f;
    if (target.IsMoving()) {
        const auto waypoints = target.Path();
        if (waypoints.size() >= 2) {
            pathPos = waypoints[1];
        }
    }

    if (Q.IsReady()) {
        if (target.IsMoving()) {
            castPos = target.Position().Extend(pathPos, -10.0f);
        } else {
            castPos = target.Position();
        }
    } else if (W.IsReady()) {
        if (target.IsMoving()) {
            castPos = target.Position().Extend(pathPos, target.BoundingRadius() + 50.0f);
        } else {
            castPos = target.Position();
        }
    } else {
        castPos = target.Position();
    }

    castPos.y = target.Position().y;
    if (player.Distance(castPos) <= E.Range && AllowDashTo(castPos)) {
        return CastE(castPos);
    }

    Vector3 fallbackPos = target.Position().Extend(player.Position(), -50.0f);
    fallbackPos.y = target.Position().y;
    if (player.Distance(fallbackPos) <= E.Range && AllowDashTo(fallbackPos)) {
        return CastE(fallbackPos);
    }

    return false;
}

static bool TryEKillSteal() {
    if (!E.IsReady() || !Bool(EMenu, "EKs")) {
        return false;
    }

    for (const auto& target : EnemyHeroes(E.Range)) {
        if (!ShouldCancelRForKillsteal(target)) {
            continue;
        }
        const float damage = EDamage(target) + EOnHitDamage(target);
        if (MagicalEffectiveHealth(target) <= damage &&
            AllowDashTo(target.Position()) &&
            CastE(target.Position())) {
            return true;
        }
    }
    return false;
}

static bool TryDaggerKillSteal() {
    if (!E.IsReady() || !Bool(EMenu, "EKs")) {
        return false;
    }

    AIHeroClient bestTarget;
    ECandidate bestCandidate;
    float bestRemainingHealth = FLT_MAX;
    for (const auto& target : EnemyHeroes(E.Range + DaggerPickupRadius + PassiveHitRadius)) {
        if (!ShouldCancelRForKillsteal(target)) {
            continue;
        }

        ECandidate candidate = BestECandidate(target);
        if (!candidate.IsValid() || candidate.PassiveHits <= 0 ||
            candidate.EstimatedDamage < MagicalEffectiveHealth(target)) {
            continue;
        }

        const float remainingHealth =
            MagicalEffectiveHealth(target) - candidate.EstimatedDamage;
        if (!bestTarget.IsValid() || remainingHealth < bestRemainingHealth ||
            (std::fabs(remainingHealth - bestRemainingHealth) <= 1.0f &&
             IsBetterECandidate(candidate, bestCandidate, target))) {
            bestTarget = target;
            bestCandidate = candidate;
            bestRemainingHealth = remainingHealth;
        }
    }

    return bestTarget.IsValid() && bestCandidate.IsValid() &&
           CastE(bestCandidate.Position);
}

static bool TryQKillSteal() {
    if (!Q.IsReady() || !Bool(QMenu, "useQKS")) {
        return false;
    }

    for (const auto& target : EnemyHeroes(Q.Range)) {
        if (!ShouldCancelRForKillsteal(target)) {
            continue;
        }
        if (MagicalEffectiveHealth(target) <= QDamage(target) && CastQ(target)) {
            return true;
        }
    }
    return false;
}

static bool TryEQKillSteal() {
    if (!Q.IsReady() ||
        !E.IsReady() ||
        !Bool(QMenu, "useQKS") ||
        !Bool(EMenu, "EKs")) {
        return false;
    }

    const auto target = GetMagicalTarget(E.Range + Q.Range);
    if (!ValidHeroTarget(target, E.Range + Q.Range)) {
        return false;
    }

    if (!ShouldCancelRForKillsteal(target)) {
        return false;
    }

    ECandidate candidate = BestECandidate(target, Q.Range);
    if (candidate.IsValid() &&
        QDamage(target) + candidate.EstimatedDamage >= MagicalEffectiveHealth(target) &&
        CastE(candidate.Position)) {
        return true;
    }

    if (QDamage(target) < MagicalEffectiveHealth(target)) {
        return false;
    }

    AIBaseClient bestObject;
    float bestDistance = FLT_MAX;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidTarget(minion, E.Range) ||
            minion.Position().Distance2D(target.Position()) > Q.Range) {
            continue;
        }
        const float distance = minion.Position().Distance2D(target.Position());
        if (!bestObject.IsValid() || distance < bestDistance) {
            bestObject = AIBaseClient(minion.Handle());
            bestDistance = distance;
        }
    }

    if (bestObject.IsValid() && AllowDashTo(bestObject.Position())) {
        return CastE(bestObject.Position());
    }
    return false;
}

static void ClearEEQKillSteal() {
    PendingEEQStage = EEQKillStealStage::None;
    PendingEEQTarget = AIBaseClient();
    PendingEEQDeadlineTick = 0;
    PendingEEQSecondCastTick = 0;
    PendingDaggerHoldNetworkId = 0;
    PendingDaggerHoldPosition = Vector3();
    PendingDaggerResetUntilTick = 0;
    PendingDaggerGoneTick = 0;
}

static bool ContinueEEQKillSteal() {
    if (PendingEEQStage == EEQKillStealStage::None) {
        return false;
    }

    const int now = SDK::Variables::TickCount();
    if (!PendingEEQTarget.IsHero() ||
        !ValidTarget(PendingEEQTarget, E.Range + Q.Range) ||
        now > PendingEEQDeadlineTick) {
        ClearEEQKillSteal();
        return false;
    }

    if (PendingEEQStage == EEQKillStealStage::WaitingForDagger) {
        const Dagger* trackedDagger = FindDagger(PendingDaggerHoldNetworkId);
        if (trackedDagger && now <= PendingDaggerResetUntilTick) {
            return true;
        }

        PendingDaggerHoldNetworkId = 0;
        PendingDaggerHoldPosition = Vector3();
        PendingDaggerResetUntilTick = 0;
        PendingDaggerGoneTick = 0;

        // Read the real cooldown state after the pickup. No cooldown formula or
        // predicted reset is used here.
        if (!E.IsReady()) {
            return true;
        }
        if (!ValidTarget(PendingEEQTarget, E.Range) ||
            !AllowDashTo(PendingEEQTarget.Position())) {
            return true;
        }
        if (!CastE(PendingEEQTarget.Position())) {
            return true;
        }

        PendingEEQStage = EEQKillStealStage::WaitingForQ;
        PendingEEQSecondCastTick = now;
        PendingEEQDeadlineTick = now + 300;
        return true;
    }

    if (now - PendingEEQSecondCastTick < 30) {
        return true;
    }
    if (ValidTarget(PendingEEQTarget, Q.Range) && CastQ(PendingEEQTarget)) {
        ClearEEQKillSteal();
        return true;
    }
    if (now - PendingEEQSecondCastTick <= 300) {
        return true;
    }

    ClearEEQKillSteal();
    return false;
}

static float ShunpoDaggerCooldownReduction() {
    const int level = std::clamp(Player().Level(), 1, 18);
    const int tier = std::clamp((level - 1) / 5, 0, 3);
    return 0.78f + 0.06f * static_cast<float>(tier);
}

static float ShunpoTotalCooldownSeconds() {
    const auto instance = E.Instance();
    const float runtimeTotalCooldown = instance.IsValid() ? instance.Cooldown() : 0.0f;
    if (runtimeTotalCooldown > 0.0f && runtimeTotalCooldown < 60.0f) {
        return runtimeTotalCooldown;
    }

    static constexpr float baseCooldown[] = { 0.0f, 12.0f, 11.0f, 10.0f, 9.0f, 8.0f };
    return baseCooldown[SpellRank(E, 5)];
}

static int ExpectedShunpoReadyDelayMs(const ECandidate& candidate) {
    if (!candidate.IsValid()) {
        return INT_MAX;
    }

    // Voracity subtracts a percentage of E's TOTAL cooldown, not its current
    // remaining cooldown. Time elapsed before the dagger is retrieved continues
    // ticking normally, so E becomes ready after the larger of pickup time and
    // the residual total-cooldown fraction.
    const float totalCooldownMs = ShunpoTotalCooldownSeconds() * 1000.0f;
    if (totalCooldownMs <= 0.0f) {
        return INT_MAX;
    }

    const auto player = Player();
    const float pickupRange = DaggerPickupRadius + player.BoundingRadius();
    std::vector<int> pickupTimes;
    pickupTimes.reserve(Daggers.size());
    for (const auto& dagger : Daggers) {
        if (!IsOwnDagger(dagger) ||
            dagger.Position.Distance2D(candidate.Position) > pickupRange) {
            continue;
        }
        pickupTimes.push_back(std::max(150, DaggerReadyInMs(dagger)));
    }
    if (pickupTimes.empty()) {
        return INT_MAX;
    }

    std::sort(pickupTimes.begin(), pickupTimes.end());
    const float reductionPerDagger = ShunpoDaggerCooldownReduction();
    int retrievedDaggers = 0;
    int previousPickupTime = 0;
    for (const int pickupTime : pickupTimes) {
        if (retrievedDaggers > 0) {
            const float naturalReadyTime =
                totalCooldownMs *
                (1.0f - reductionPerDagger * static_cast<float>(retrievedDaggers));
            if (naturalReadyTime <= static_cast<float>(pickupTime)) {
                return static_cast<int>(std::ceil(std::max(
                    static_cast<float>(previousPickupTime), naturalReadyTime)));
            }
        }

        ++retrievedDaggers;
        previousPickupTime = pickupTime;
        const float remainingAfterPickup =
            totalCooldownMs - static_cast<float>(pickupTime) -
            totalCooldownMs * reductionPerDagger *
                static_cast<float>(retrievedDaggers);
        if (remainingAfterPickup <= 0.0f) {
            return pickupTime;
        }
    }

    const float readyTime =
        totalCooldownMs *
        (1.0f - reductionPerDagger * static_cast<float>(retrievedDaggers));
    return static_cast<int>(std::ceil(std::max(
        static_cast<float>(previousPickupTime), readyTime)));
}

static bool TryEEQKillSteal() {
    if (PendingEEQStage != EEQKillStealStage::None ||
        !E.IsReady() || !Q.IsReady() ||
        !Bool(EMenu, "EKs") || !Bool(QMenu, "useQKS")) {
        return false;
    }

    AIHeroClient bestTarget;
    ECandidate bestCandidate;
    float bestRemainingHealth = FLT_MAX;

    // Ready daggers finish the first part immediately. Only consider airborne
    // daggers when no ready-dagger EEQ is available.
    for (const auto readiness : {
             DaggerReadinessFilter::ReadyOnly,
             DaggerReadinessFilter::UnreadyOnly }) {
        for (const auto& target : EnemyHeroes(
                 E.Range + DaggerPickupRadius + PassiveHitRadius)) {
            if (!ShouldCancelRForKillsteal(target)) {
                continue;
            }

            ECandidate candidate = BestECandidate(
                target, PassiveHitRadius, readiness);
            if (!candidate.IsValid()) {
                continue;
            }

            const int resetDelayMs = ExpectedShunpoReadyDelayMs(candidate);
            if (resetDelayMs > MaxEEQResetWaitMs) {
                continue;
            }

            float firstEDamage = candidate.EstimatedDamage;
            if (!candidate.AnchorReady) {
                if (candidate.TargetDistance >
                    PassiveHitRadius + target.BoundingRadius()) {
                    continue;
                }
                firstEDamage += DaggerPickupDamage(target);
            } else if (candidate.PassiveHits <= 0) {
                continue;
            }

            const float totalDamage =
                firstEDamage +
                EDamage(target) + EOnHitDamage(target) +
                QDamage(target);
            if (totalDamage < MagicalEffectiveHealth(target)) {
                continue;
            }

            const float remainingHealth =
                MagicalEffectiveHealth(target) - totalDamage;
            if (!bestTarget.IsValid() || remainingHealth < bestRemainingHealth ||
                (std::fabs(remainingHealth - bestRemainingHealth) <= 1.0f &&
                 IsBetterECandidate(candidate, bestCandidate, target))) {
                bestTarget = target;
                bestCandidate = candidate;
                bestRemainingHealth = remainingHealth;
            }
        }

        if (bestTarget.IsValid()) {
            break;
        }
    }

    if (!bestTarget.IsValid() || !bestCandidate.IsValid() ||
        !CastE(bestCandidate.Position)) {
        return false;
    }

    const int now = SDK::Variables::TickCount();
    const int timeUntilReady = DaggerReadyInMs(*bestCandidate.Anchor);
    PendingEEQStage = EEQKillStealStage::WaitingForDagger;
    PendingEEQTarget = AIBaseClient(bestTarget.Handle());
    PendingEEQDeadlineTick = now + MaxEEQResetWaitMs;
    PendingDaggerHoldNetworkId = bestCandidate.Anchor->NetworkId;
    PendingDaggerHoldPosition = bestCandidate.Anchor->Position;
    PendingDaggerResetUntilTick = now + timeUntilReady + DaggerPickupGraceMs;
    PendingDaggerGoneTick = 0;
    return true;
}

static bool TryCastW() {
    if (!W.IsReady()) {
        return false;
    }

    const auto target = GetMagicalTarget(W.Range);
    if (ValidHeroTarget(target, W.Range) &&
        target.DistanceToPlayer() <= static_cast<float>(Slider(WMenu, "WRange", 300))) {
        return W.Cast();
    }
    return false;
}

static bool TryCastR() {
    if (!R.IsReady()) {
        return false;
    }

    const auto target = GetMagicalTarget(R.Range);
    if (!ValidHeroTarget(target, R.Range)) {
        return false;
    }

    if (IsRDamageLethal(target)) {
        return R.Cast();
    }

    if (Bool(RMenu, "RCombo", true)) {
        return R.Cast();
    }

    if (Player().CountEnemyHeroesInRange(R.Range) >= Slider(RMenu, "RCount", 3)) {
        return R.Cast();
    }
    return false;
}

static void EQ() {
    if (HaveRBuff() && Bool(RMenu, "NeverCancelR", false)) {
        return;
    }

    const int now = SDK::Variables::TickCount();
    bool daggerResetObserved = false;
    if (PendingDaggerHoldNetworkId != 0) {
        const Dagger* trackedDagger = FindDagger(PendingDaggerHoldNetworkId);
        const bool timedOut = now > PendingDaggerResetUntilTick;
        if (trackedDagger && !timedOut) {
            PendingDaggerGoneTick = 0;
            return;
        }

        if (!timedOut && !E.IsReady()) {
            if (PendingDaggerGoneTick == 0) {
                PendingDaggerGoneTick = now;
                return;
            }
            if (now - PendingDaggerGoneTick <= 100) {
                return;
            }
        }

        PendingDaggerHoldNetworkId = 0;
        PendingDaggerHoldPosition = Vector3();
        PendingDaggerResetUntilTick = 0;
        PendingDaggerGoneTick = 0;
        if (E.IsReady()) {
            daggerResetObserved = true;
        } else {
            PendingEQTick = now - 30;
        }
    }

    if (PendingEQTick > 0) {
        const int age = now - PendingEQTick;
        if (age < 30) {
            return;
        }

        AIBaseClient qTarget = PendingEQTarget;
        if (!ValidTarget(qTarget, Q.Range)) {
            qTarget = AIBaseClient(GetMagicalTarget(Q.Range).Handle());
        }
        if (ValidTarget(qTarget, Q.Range) && CastQ(qTarget)) {
            PendingEQTarget = AIBaseClient();
            PendingEQTick = 0;
            if (!TryCastW()) {
                (void)TryCastR();
            }
            return;
        }
        if (age <= 250) {
            return;
        }
        PendingEQTarget = AIBaseClient();
        PendingEQTick = 0;
    }

    AIBaseClient eTarget;
    bool eCasted = false;
    bool waitingForDagger = false;
    bool waitingForQuickEReset = false;
    if (E.IsReady() && DoComboE(
            eTarget,
            Bool(EMenu, "SaveEIfNoDaggers", true),
            Q.IsReady(),
            &waitingForDagger,
            &waitingForQuickEReset)) {
        eCasted = true;
    }

    if (waitingForDagger) {
        return;
    }

    if (eCasted) {
        PendingEQTarget = eTarget;
        if (!waitingForQuickEReset) {
            PendingEQTick = SDK::Variables::TickCount();
        }
        return;
    }

    if (daggerResetObserved) {
        PendingEQTick = now - 30;
        return;
    }

    if (E.IsReady(500)) {
        return;
    }

    AIBaseClient qTarget = eTarget;
    if (!ValidTarget(qTarget, Q.Range)) {
        qTarget = AIBaseClient(GetMagicalTarget(Q.Range).Handle());
    }
    (void)CastQ(qTarget);

    if (TryCastW()) {
        return;
    }
    (void)TryCastR();
}

static void QE() {
    if (HaveRBuff() && Bool(RMenu, "NeverCancelR", false)) {
        return;
    }

    const auto qTarget = GetMagicalTarget(Q.Range);
    (void)CastQ(qTarget);

    AIBaseClient eTarget;
    bool eCasted = false;
    if (E.IsReady() && DoComboE(eTarget, Bool(EMenu, "SaveEIfNoDaggers", true))) {
        eCasted = true;
    }

    if (!eCasted && E.IsReady(500)) {
        return;
    }

    if (TryCastW()) {
        return;
    }
    (void)TryCastR();
}

static void Combo() {
    if (PendingDaggerHoldNetworkId != 0) {
        EQ();
        return;
    }
    switch (List(MenuRoot, "KataComboMode", 0)) {
    case 0:
        EQ();
        break;
    case 1:
        QE();
        break;
    default:
        QE();
        EQ();
        break;
    }
}

static void Harass() {
    if (!Bool(QMenu, "AutoQ")) {
        return;
    }

    const auto target = GetMagicalTarget(Q.Range);
    (void)CastQ(target);
}

static void LastHit() {
    if (!Q.IsReady()) {
        return;
    }

    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, Q.Range) &&
            MagicalEffectiveHealth(minion) <= QDamage(minion) &&
            CastQ(minion)) {
            return;
        }
    }
}

inline constexpr float WOrbMaxWalkRange = 450.0f;
inline constexpr float WOrbPickupRadius = 150.0f;
inline constexpr float WOrbPickupSafetyMargin = 10.0f;
inline constexpr int WOrbMinApproachLeadMs = 75;
inline constexpr int WOrbMaxApproachLeadMs = 175;

struct WOrbCandidate {
    Vector3 Position;
    int TravelTimeMs = INT_MAX;
    int ReadyInMs = INT_MAX;
    int MoveStartInMs = INT_MAX;
    int PickupTimeMs = INT_MAX;
    int Pickups = 0;
    int PassiveHits = 0;
    bool Lethal = false;
    bool ResetsE = false;

    bool IsValid() const {
        return !Position.IsZero() && Pickups > 0;
    }
};

static WOrbCandidate EvaluateWOrbCandidate(
    const Vector3& position,
    const AIHeroClient& target) {
    WOrbCandidate candidate;
    const auto player = Player();
    if (!player.IsValid() || position.IsZero() ||
        player.Distance(position) > WOrbMaxWalkRange || NavMesh::IsWall(position)) {
        return candidate;
    }

    const int now = SDK::Variables::TickCount();
    const float pickupRange = WOrbPickupRadius;
    const float moveSpeed = std::max(1.0f, player.MoveSpeed());
    const int travelTimeMs = static_cast<int>(std::ceil(
        player.Distance(position) / moveSpeed * 1000.0f));
    int earliestReadyMs = INT_MAX;

    for (const auto& dagger : Daggers) {
        if (!IsOwnDagger(dagger) ||
            dagger.Position.Distance2D(position) > pickupRange) {
            continue;
        }
        const int age = std::max(0, now - dagger.CreateTick);
        const int readyIn = DaggerReadyInMs(dagger);
        const int expiresIn = std::max(0, 5000 - age);
        if (std::max(travelTimeMs, readyIn) <= expiresIn) {
            earliestReadyMs = std::min(earliestReadyMs, readyIn);
        }
    }
    if (earliestReadyMs == INT_MAX) {
        return candidate;
    }

    const int approachLeadMs = std::min(
        WOrbMaxApproachLeadMs,
        std::max(WOrbMinApproachLeadMs, Game::Ping() / 2 + 50));
    candidate.TravelTimeMs = travelTimeMs;
    candidate.ReadyInMs = earliestReadyMs;
    candidate.MoveStartInMs = std::max(
        0, earliestReadyMs - travelTimeMs - approachLeadMs);
    candidate.PickupTimeMs = std::max(travelTimeMs, earliestReadyMs);
    for (const auto& dagger : Daggers) {
        if (!IsOwnDagger(dagger) ||
            dagger.Position.Distance2D(position) > pickupRange) {
            continue;
        }
        const int age = std::max(0, now - dagger.CreateTick);
        const int readyIn = DaggerReadyInMs(dagger);
        const int expiresIn = std::max(0, 5000 - age);
        if (readyIn <= candidate.PickupTimeMs &&
            candidate.PickupTimeMs <= expiresIn) {
            ++candidate.Pickups;
        }
    }
    if (candidate.Pickups <= 0) {
        return WOrbCandidate{};
    }

    if (ValidHeroTarget(target, WOrbMaxWalkRange + PassiveHitRadius) &&
        target.Position().Distance2D(position) <=
            PassiveHitRadius + target.BoundingRadius()) {
        candidate.PassiveHits = candidate.Pickups;
        const float damage = DaggerPickupDamage(target) *
                             static_cast<float>(candidate.PassiveHits);
        candidate.Lethal = damage >= MagicalEffectiveHealth(target);
    }

    const auto eInstance = E.Instance();
    const float currentERemainingMs = eInstance.IsValid()
        ? eInstance.RemainingCooldown() * 1000.0f
        : 0.0f;
    if (currentERemainingMs > 0.0f) {
        const float remainingAtPickup = std::max(
            0.0f, currentERemainingMs - static_cast<float>(candidate.PickupTimeMs));
        const float cooldownReduction =
            ShunpoTotalCooldownSeconds() * 1000.0f *
            ShunpoDaggerCooldownReduction() *
            static_cast<float>(candidate.Pickups);
        candidate.ResetsE = remainingAtPickup - cooldownReduction <= 0.0f;
    }

    candidate.Position = position;
    return candidate;
}

static bool IsBetterWOrbCandidate(
    const WOrbCandidate& candidate,
    const WOrbCandidate& current) {
    if (!candidate.IsValid()) {
        return false;
    }
    if (!current.IsValid()) {
        return true;
    }
    if (candidate.Lethal != current.Lethal) {
        return candidate.Lethal;
    }
    if ((candidate.PassiveHits > 0) != (current.PassiveHits > 0)) {
        return candidate.PassiveHits > 0;
    }
    if (candidate.ResetsE != current.ResetsE) {
        return candidate.ResetsE;
    }
    if (candidate.PassiveHits != current.PassiveHits) {
        return candidate.PassiveHits > current.PassiveHits;
    }
    if (candidate.Pickups != current.Pickups) {
        return candidate.Pickups > current.Pickups;
    }
    return candidate.PickupTimeMs < current.PickupTimeMs;
}

static WOrbCandidate BestWOrbCandidate(const AIHeroClient& target) {
    WOrbCandidate best;
    const auto player = Player();
    const float pickupRange = WOrbPickupRadius;
    std::vector<Vector3> positions;
    positions.reserve(Daggers.size() * Daggers.size() + Daggers.size() * 2);

    for (const auto& dagger : Daggers) {
        if (!IsOwnDagger(dagger)) {
            continue;
        }
        AddECandidate(positions, dagger.Position, dagger.Position.y);
        AddECandidate(
            positions,
            dagger.Position.Extend(
                player.Position(),
                std::max(0.0f, pickupRange - WOrbPickupSafetyMargin)),
            dagger.Position.y);
        if (ValidHeroTarget(target, WOrbMaxWalkRange + PassiveHitRadius)) {
            AddECandidate(
                positions,
                dagger.Position.Extend(
                    target.Position(),
                    std::max(0.0f, pickupRange - WOrbPickupSafetyMargin)),
                dagger.Position.y);
        }
    }

    for (size_t i = 0; i < Daggers.size(); ++i) {
        if (!IsOwnDagger(Daggers[i])) {
            continue;
        }
        for (size_t j = i + 1; j < Daggers.size(); ++j) {
            if (!IsOwnDagger(Daggers[j]) ||
                Daggers[i].Position.Distance2D(Daggers[j].Position) > pickupRange * 2.0f) {
                continue;
            }
            const Vector3 midpoint =
                Daggers[i].Position +
                (Daggers[j].Position - Daggers[i].Position) * 0.5f;
            AddECandidate(positions, midpoint, Daggers[i].Position.y);
        }
    }

    for (const auto& position : positions) {
        const WOrbCandidate candidate = EvaluateWOrbCandidate(position, target);
        if (IsBetterWOrbCandidate(candidate, best)) {
            best = candidate;
        }
    }
    return best;
}

static void UpdateOrbwalkerState() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (HaveRBuff()) {
        if (!UpdateR) {
            LastR = SDK::Variables::TickCount();
        }
        UpdateR = true;
        Orbwalker::AttackEnabled(false);
        Orbwalker::MoveEnabled(false);

        if (!Bool(RMenu, "NeverCancelR", false) &&
            player.HealthPercent() <= 50.0f &&
            SDK::Variables::TickCount() - LastR > 500 &&
            SDK::Variables::TickCount() - LastR < 5000) {
            const auto target = BestDaggerTarget(E.Range + 400.0f);
            if (ValidHeroTarget(target, E.Range + 400.0f)) {
                const ECandidate candidate = BestECandidate(target);
                const float damage =
                    (Q.IsReady() ? QDamage(target) : 0.0f) +
                    (E.IsReady() ? EDamage(target) : 0.0f) +
                    DaggerPickupDamage(target) *
                        static_cast<float>(candidate.PassiveHits);
                if (MagicalEffectiveHealth(target) <= damage) {
                    Orbwalker::AttackEnabled(true);
                    Orbwalker::MoveEnabled(true);
                }
            }
        }
        return;
    }

    UpdateR = false;
    Orbwalker::MoveEnabled(true);

    if (PendingDaggerHoldNetworkId != 0) {
        const Dagger* trackedDagger = FindDagger(PendingDaggerHoldNetworkId);
        if (trackedDagger &&
            SDK::Variables::TickCount() <= PendingDaggerResetUntilTick) {
            PendingDaggerHoldPosition = trackedDagger->Position;
            Orbwalker::AttackEnabled(false);
            Orbwalker::SetOrbwalkerPosition(PendingDaggerHoldPosition);
            return;
        }
    }

    if (!Bool(WMenu, "WOrb") || !HasOwnDagger() ||
        Orbwalker::ActiveMode() != OrbwalkingMode::Combo) {
        Orbwalker::AttackEnabled(true);
        Orbwalker::SetOrbwalkerPosition({});
        return;
    }

    const auto target = GetMagicalTarget(WOrbMaxWalkRange + PassiveHitRadius);
    const WOrbCandidate best = BestWOrbCandidate(target);
    if (!best.IsValid()) {
        Orbwalker::AttackEnabled(true);
        Orbwalker::SetOrbwalkerPosition({});
        return;
    }

    // Có thể chấm điểm dao chưa rơi xong để chuẩn bị đường đi, nhưng chưa ép
    // orbwalker chạy nếu sẽ đến quá sớm. Công thức bắt đầu đi bù thời gian chạy,
    // nửa ping và một khoảng đệm nhỏ; được tính lại mỗi frame.
    if (best.MoveStartInMs > 0) {
        Orbwalker::AttackEnabled(true);
        Orbwalker::SetOrbwalkerPosition({});
        return;
    }

    // Giữ đòn đánh đang wind-up, nhưng vẫn đặt điểm di chuyển để orbwalker bước
    // vào vùng nhặt ngay sau khi đòn đánh hoàn tất.
    if (Orbwalker::IsWindingUp()) {
        Orbwalker::AttackEnabled(true);
        Orbwalker::SetOrbwalkerPosition(best.Position);
        return;
    }

    // Chỉ bỏ qua auto khi việc nhặt dao tạo lợi ích ngay: gây passive, hồi E,
    // hoặc gom được nhiều dao. Nếu chỉ là dao đơn không trúng ai thì vẫn cho
    // đánh thường và dùng vị trí này như hướng di chuyển, tránh tự khóa combat.
    const bool shouldRushDagger =
        best.PassiveHits > 0 || best.ResetsE || best.Pickups >= 2;
    Orbwalker::AttackEnabled(!shouldRushDagger);
    Orbwalker::SetOrbwalkerPosition(best.Position);
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Ghi mốc di chuyển mỗi frame để HaveRBuff() so như các hành động hủy khác
    // (sticky), thay vì hỏi trạng thái tức thời: IsMoving() chỉ true trong lúc
    // đang chạy, nếu hỏi trực tiếp thì R sẽ "sống lại" ngay khi dừng chân dù
    // channel đã đứt. Phải chạy TRƯỚC UpdateOrbwalkerState() vì hàm đó gọi
    // HaveRBuff().
    if (player.IsMoving() || player.IsDashing()) {
        LastMoveTick = SDK::Variables::TickCount();
    }

    PruneDaggers();
    UpdateOrbwalkerState();

    if (player.IsDead() || Game::IsChatOpen()) {
        return;
    }

    if (ShouldKeepR()) {
        return;
    }

    if (ContinueEEQKillSteal()) {
        return;
    }

    if (TryDaggerKillSteal() || TryEKillSteal() || TryQKillSteal() ||
        TryEQKillSteal() || TryEEQKillSteal()) {
        return;
    }

    if (Orbwalker::IsWindingUp()) {
        return;
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LastHit:
        LastHit();
        break;
    default:
        break;
    }
}

// args.Slot KHÔNG tin được. Decoder đọc slot qua ReadEventSlot(), mà
// detail::Read() zero-fill khi fail còn IsSlotValid(0) lại là true — nên mọi
// cast đọc hỏng đều im lặng thành slot 0 = Q. Đã quan sát trực tiếp ở đường
// DoCast và tái hiện ở ProcessSpell. Hệ quả: các event phụ phát ra trong lúc
// channel R bị coi là "vừa cast Q" và hủy oan R.
//
// Tên chiêu thì decode ĐÚNG, nên phân loại hành động theo tên.

// Riot đặt tên theo PascalCase nên hậu tố hợp lệ phải rỗng hoặc bắt đầu bằng ký
// tự không phải chữ thường: "KatarinaEDagger" khớp prefix "KatarinaE", còn
// "KatarinaRecall" thì không khớp "KatarinaR".
static bool SpellNameHasPrefix(const char* name, const char* prefix) {
    if (!name || !prefix) {
        return false;
    }
    const size_t prefixLen = std::strlen(prefix);
    if (_strnicmp(name, prefix, prefixLen) != 0) {
        return false;
    }
    const char suffix = name[prefixLen];
    return suffix == '\0' || suffix < 'a' || suffix > 'z';
}

static bool SpellNameContains(const char* name, const char* token) {
    if (!name || !token) {
        return false;
    }
    const size_t nameLen = std::strlen(name);
    const size_t tokenLen = std::strlen(token);
    if (tokenLen == 0 || tokenLen > nameLen) {
        return false;
    }
    for (size_t i = 0; i + tokenLen <= nameLen; ++i) {
        if (_strnicmp(name + i, token, tokenLen) == 0) {
            return true;
        }
    }
    return false;
}

// Trả về slot chuẩn hóa: 0=Q, 1=W, 2=E, 3=R, 64=đánh thường,
// -1 = event không phải hành động của người chơi (sub-spell, hiệu ứng nội bộ)
// nên KHÔNG được đụng tới trạng thái R.
static int ClassifyCast(const Events::ProcessSpellEventArgs& args) {
    const char* name = args.SpellName[0] ? args.SpellName : args.ScriptName;
    if (!name || !name[0]) {
        return args.Slot;  // không có tên -> đành tin slot thô
    }
    if (SpellNameContains(name, "BasicAttack") ||
        SpellNameContains(name, "CritAttack")) {
        return 64;
    }
    if (SpellNameHasPrefix(name, "KatarinaQ")) return static_cast<int>(SpellSlot::Q);
    if (SpellNameHasPrefix(name, "KatarinaW")) return static_cast<int>(SpellSlot::W);
    if (SpellNameHasPrefix(name, "KatarinaE")) return static_cast<int>(SpellSlot::E);
    if (SpellNameHasPrefix(name, "KatarinaR")) return static_cast<int>(SpellSlot::R);
    return -1;
}

static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }

    const char* name = args.SpellName[0] ? args.SpellName : args.ScriptName;
    const int slot = ClassifyCast(args);
    const int tick = SDK::Variables::TickCount();

    NightSharpDebug::Logf("[Katarina Cast] name='%s' rawSlot=%d -> slot=%d",
                          name ? name : "", args.Slot, slot);

    if (slot == static_cast<int>(SpellSlot::Q)) {
        LastCastQ = tick;
    } else if (slot == static_cast<int>(SpellSlot::W)) {
        LastCastW = tick;
    } else if (slot == static_cast<int>(SpellSlot::E)) {
        LastCastE = tick;
    } else if (slot == static_cast<int>(SpellSlot::R)) {
        // Mốc bắt đầu channel R — HaveRBuff() so mọi hành động khác với tick này.
        // R có thể phát nhiều event trong lúc channel, nên chỉ event ĐẦU TIÊN
        // mới mở window mới; nếu không, một repeat giữa channel sẽ đẩy mốc lên
        // và xóa mất cancel đã xảy ra trước đó.
        // Channel dài 2.5s còn hồi chiêu >10s, nên 3000ms tách sạch hai ca này.
        if (LastCastR == 0 || tick - LastCastR > 3000) {
            LastCastR = tick;
        }
    } else if (slot == 64) {
        LastBasicAttackTick = tick;
    }
}

static void OnObjectCreate(const GameObject& object) {
    if (!IsDaggerObject(object)) {
        return;
    }

    const int networkId = static_cast<int>(object.CachedNetworkId());
    const Vector3 position = object.Position();
    if (networkId == 0 || position.IsZero()) {
        return;
    }

    const bool alreadyTracked = std::any_of(
        Daggers.begin(), Daggers.end(),
        [networkId](const Dagger& dagger) {
            return dagger.NetworkId == networkId;
        });
    if (alreadyTracked) {
        return;
    }

    Daggers.push_back({
        object,
        position,
        SDK::Variables::TickCount(),
        networkId
    });
}

static void OnObjectDelete(const GameObject& object) {
    // OnDelete có thể là identity-only; CachedNetworkId() không cố resolve lại
    // địa chỉ đã bị prune và vẫn giữ đúng identity snapshot của event.
    const int networkId = static_cast<int>(object.CachedNetworkId());
    if (networkId == 0) {
        return;
    }

    const size_t oldSize = Daggers.size();
    Daggers.erase(
        std::remove_if(
            Daggers.begin(), Daggers.end(),
            [networkId](const Dagger& dagger) {
                return dagger.NetworkId == networkId;
            }),
        Daggers.end());

    if (Daggers.size() != oldSize) {
        Orbwalker::SetOrbwalkerPosition({});
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    if (Bool(DrawMenu, "DrawDaggers", false)) {
        for (const auto& dagger : Daggers) {
            if (!IsOwnDagger(dagger)) {
                continue;
            }
            const ImU32 color = IsDaggerReady(dagger) ? 0xFF00FF00u : 0xFFFFD700u;
            Render::DrawRingImGui(dagger.Position, 340.0f + 150.0f, 340.0f, color, 4);
        }
    }

    if (Bool(DrawMenu, "DrawQRange", false)) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFADFF2Fu, 1.5f, 64);
    }

    if (Game::Ping() >= 100) {
        Vector2 screen;
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char text[48] = {};
            _snprintf_s(text, sizeof(text), _TRUNCATE, "High Ping %d", Game::Ping());
            Drawing::DrawText(screen.x - 20.0f, screen.y + 20.0f, 0xFFFFFF00u, text);
        }
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.katarina", "Kuro - Katarina", true);
    MenuRoot->Add(new MenuList(
        "KataComboMode",
        "Combo Mode",
        { "E first then Q", "Q first then E", "Logic Swap Combo" },
        0))->Permashow();
    MenuRoot->Add(new MenuBool("Turret", "Combo under Turret", false));

    QMenu = MenuRoot->AddSubMenu(new Menu("Qstg", "Q Settings"));
    QMenu->Add(new MenuBool("FindBestTarget", "Find Best Target"));
    QMenu->Add(new MenuBool("AutoQ", "Auto Q"));
    QMenu->Add(new MenuBool("useQKS", "Use Q KS"));

    WMenu = MenuRoot->AddSubMenu(new Menu("Wstg", "W Settings"));
    WMenu->Add(new MenuSlider("WRange", "W Range", 300, 200, 340));
    WMenu->Add(new MenuBool("WGapcloser", "W Gapcloser"));
    WMenu->Add(new MenuBool("WOrb", "Orbwalker to Dagger"));

    EMenu = MenuRoot->AddSubMenu(new Menu("Estg", "E Settings"));
    EMenu->Add(new MenuBool("EKs", "Use E KS"));
    EMenu->Add(new MenuBool("SaveEIfNoDaggers", "Save E", true));

    RMenu = MenuRoot->AddSubMenu(new Menu("Rstg", "R Settings"));
    RMenu->Add(new MenuBool("RCombo", "Use R in combo", true));
    RMenu->Add(new MenuBool("NeverCancelR", "Never cancel R", false));
    RMenu->Add(new MenuBool("UseRIfKs", "Use R if target is killable"));
    RMenu->Add(new MenuSlider("RCount", "R Target in range", 3, 1, 5));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Drawstg", "Draw Settings"));
    DrawMenu->Add(new MenuBool("DrawDaggers", "Draw Daggers", false));
    DrawMenu->Add(new MenuBool("DrawQRange", "Draw Q Range", false));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }

    if (auto* item = MenuRoot->Get<MenuList>("KataComboMode")) {
        item->RemovePermashow();
    }

    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    QMenu = nullptr;
    WMenu = nullptr;
    EMenu = nullptr;
    RMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 625.0f);
    Q.SetTargetted(0.25f, 2000.0f);
    W = Spell(SpellSlot::W, 340.0f);
    E = Spell(SpellSlot::E, 775.0f);
    R = Spell(SpellSlot::R, 550.0f);

    BuildMenu();

    LastCastQ = 0;
    LastCastW = 0;
    LastCastE = 0;
    LastCastR = 0;
    LastBasicAttackTick = 0;
    LastMoveTick = 0;
    PendingEQTarget = AIBaseClient();
    PendingEQTick = 0;
    PendingDaggerResetUntilTick = 0;
    PendingDaggerHoldNetworkId = 0;
    PendingDaggerHoldPosition = Vector3();
    PendingDaggerGoneTick = 0;
    PendingEEQStage = EEQKillStealStage::None;
    PendingEEQTarget = AIBaseClient();
    PendingEEQDeadlineTick = 0;
    PendingEEQSecondCastTick = 0;
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpellCast;
    GameObjects::AddOnCreate(&OnObjectCreate);
    GameObjects::AddOnDelete(&OnObjectDelete);
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Katarina loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpellCast;
    GameObjects::RemoveOnCreate(&OnObjectCreate);
    GameObjects::RemoveOnDelete(&OnObjectDelete);
    Drawing::OnDraw -= &OnDraw;
    Orbwalker::AttackEnabled(true);
    Orbwalker::MoveEnabled(true);
    Orbwalker::SetOrbwalkerPosition({});

    Daggers.clear();
    LastCastQ = 0;
    LastCastW = 0;
    LastCastE = 0;
    LastCastR = 0;
    LastBasicAttackTick = 0;
    LastMoveTick = 0;
    PendingEQTarget = AIBaseClient();
    PendingEQTick = 0;
    PendingDaggerResetUntilTick = 0;
    PendingDaggerHoldNetworkId = 0;
    PendingDaggerHoldPosition = Vector3();
    PendingDaggerGoneTick = 0;
    PendingEEQStage = EEQKillStealStage::None;
    PendingEEQTarget = AIBaseClient();
    PendingEEQDeadlineTick = 0;
    PendingEEQSecondCastTick = 0;
    RemoveMenu();
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Katarina
