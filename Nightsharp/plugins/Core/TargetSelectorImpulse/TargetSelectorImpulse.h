#pragma once

#include "../../../sdk/SDK.h"
#include "ImpulsePriorityData.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace Plugins::TargetSelectorImpulse {

class TargetSelectorImpulse final : public SDK::ITargetSelector {
public:
    explicit TargetSelectorImpulse(SDK::Menu* menu)
        : menu_(menu)
    {
        Ptr() = this;
        priorityMenu_ = menu_->AddSubMenu("Priority", "Priority");
        for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
            if (!hero.IsValid()) continue;
            const std::string name = hero.CharacterName();
            const std::string key = "TS_" + name;
            if (!priorityMenu_->Get<SDK::MenuSlider>(key.c_str())) {
                priorityMenu_->Add(new SDK::MenuSlider(
                    key.c_str(), name.c_str(), GetDefaultPriority(hero), 1, 5));
            }
        }

        drawingsMenu_ = menu_->AddSubMenu("Drawings", "Drawings");
        drawingsMenu_->Add(new SDK::MenuColor("SelectColor", "^ Draw Color", 0xFF0000FFu));
        drawingsMenu_->Add(new SDK::MenuBool("DrawSelect", "Draw Selected Target", true));
        drawingsMenu_->Add(new SDK::MenuBool("LightSelect", "HighLight Selected Target", true));
        menu_->Add(new SDK::MenuBool("ForceSelectTarget", "Force on Select Target", true));
        menu_->Add(new SDK::MenuBool("OnlySelectTarget", "Only Attack Select Target", false));

        const auto player = Player();
        modeKey_ = "TSMode_" + (player.IsValid() ? player.CharacterName() : std::string());
        menu_->Add(new SDK::MenuList(
            modeKey_.c_str(),
            "TS Mode",
            std::vector<std::string>{
                "Smart AD/AP", "Lowest Health", "Most Priority",
                "Near Mouse", "Near Hero"},
            0));
        Resume();
    }

    void OnLight() {
        if (!drawingsMenu_) return;
        const auto* light = drawingsMenu_->Get<SDK::MenuBool>("LightSelect");
        if (!light || !light->Value) return;
        if (!selectedTarget_.IsValid() ||
            !SDK::Extensions::IsValidTarget(selectedTarget_, FLT_MAX, true, SDK::Vector3())) return;

        // MISSING API: Render.OnRenderMouseOvers / AIHeroClient.Glow(Purple, 5, 1)
        // is documented in missapi.md. No unverified substitute is used.
    }

    void OnWndProc(SDK::Game::WndEventArgs& args) {
        const std::uint32_t msg = args.Msg;
        if (static_cast<unsigned long long>(msg) == 513ULL) {
            const SDK::Vector3 clickPosition = SDK::Game::CursorPos();
            SDK::AIHeroClient closest;
            float closestDistance = FLT_MAX;
            for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
                if (!SDK::Extensions::IsValidTarget(hero, 5000.0f, true, SDK::Vector3())) continue;
                const float distance = hero.Distance(clickPosition);
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closest = hero;
                }
            }
            if (closest.IsValid() && SDK::Game::CursorPos().Distance(closest.PreviousPosition()) <= 300.0f) {
                selectedTarget_ = closest;
                return;
            }
            selectedTarget_ = {};
        }
    }

    void OnDraw() {
        if (!drawingsMenu_) return;
        const auto* draw = drawingsMenu_->Get<SDK::MenuBool>("DrawSelect");
        if (!draw || !draw->Value) return;
        if (!selectedTarget_.IsValid() ||
            !SDK::Extensions::IsValidTarget(selectedTarget_, FLT_MAX, true, SDK::Vector3())) return;

        const auto* color = drawingsMenu_->Get<SDK::MenuColor>("SelectColor");
        SDK::Drawing::DrawCircle(
            selectedTarget_.Position(),
            selectedTarget_.BoundingRadius(),
            color ? color->Value : 0xFF0000FFu,
            10.0f,
            64,
            false);
    }

    void Dispose() {
        if (disposed_) return;
        Suspend();
        selectedTarget_ = {};
        disposed_ = true;
        if (Ptr() == this) Ptr() = nullptr;
    }

    int GetDefaultPriority(const SDK::AIHeroClient& target) const {
        return target.IsValid()
            ? TargetSelectorImpulseData::GetDefaultPriority(target.CharacterName())
            : 1;
    }

    int GetPriority(const SDK::AIHeroClient& target) const override {
        if (!target.IsValid()) return 0;
        const std::string key = "TS_" + target.CharacterName();
        if (!priorityMenu_) return GetDefaultPriority(target);
        const auto* slider = priorityMenu_->Get<SDK::MenuSlider>(key.c_str());
        return slider ? slider->Value : GetDefaultPriority(target);
    }

    SDK::AIHeroClient GetTarget(
        const std::vector<SDK::AIHeroClient>& possibleTargets,
        SDK::DamageType damageType,
        bool ignoreShields = true,
        const SDK::Vector3& checkFrom = SDK::Vector3())
    {
        (void)ignoreShields;
        std::vector<SDK::AIHeroClient> list;
        for (const auto& target : possibleTargets) {
            if (IsValidTarget(target, FLT_MAX, checkFrom)) list.push_back(target);
        }

        if (selectedTarget_.IsValid() && !selectedTarget_.IsDead()) {
            if (Bool("ForceSelectTarget", true) &&
                Contains(possibleTargets, selectedTarget_) &&
                IsValidTarget(selectedTarget_, FLT_MAX, checkFrom)) {
                return selectedTarget_;
            }
            if (Bool("OnlySelectTarget", false) &&
                IsValidTarget(selectedTarget_, FLT_MAX, checkFrom)) {
                return selectedTarget_;
            }
        }

        switch (ModeIndex()) {
        case 0:
            return MinTarget(list, [this, damageType](const SDK::AIHeroClient& target) {
                return static_cast<float>(AaIndicator(target, damageType));
            });
        case 1:
            return MinTarget(list, [this, damageType](const SDK::AIHeroClient& target) {
                return GetRealHeath(target, damageType);
            });
        case 2:
            return MinTarget(list, [this](const SDK::AIHeroClient& target) {
                return static_cast<float>(GetPriority(target));
            });
        case 3:
            return MinTarget(list, [](const SDK::AIHeroClient& target) {
                return target.Distance(SDK::Game::CursorPosRaw());
            });
        case 4:
            return MinTarget(list, [](const SDK::AIHeroClient& target) {
                return target.Distance(SDK::GameObjects::Player());
            });
        default:
            return {};
        }
    }

    SDK::AIHeroClient GetTarget(
        float range,
        SDK::DamageType damageType = SDK::DamageType::True,
        bool ignoreShields = true,
        const SDK::Vector3& checkFrom = SDK::Vector3(),
        const std::vector<SDK::AIHeroClient>* ignoreChampions = nullptr) override
    {
        (void)ignoreShields;
        static const std::vector<SDK::AIHeroClient> empty;
        const auto& ignored = ignoreChampions ? *ignoreChampions : empty;

        if (selectedTarget_.IsValid()) {
            if (Bool("ForceSelectTarget", true) && IsValidTarget(selectedTarget_, range, checkFrom)) {
                return selectedTarget_;
            }
            if (Bool("OnlySelectTarget", false) && IsValidTarget(selectedTarget_, FLT_MAX, checkFrom)) {
                return selectedTarget_;
            }
        }

        std::vector<SDK::AIHeroClient> source;
        for (const auto& target : SDK::GameObjects::EnemyHeroes()) {
            if (!IsValidTarget(target, range, checkFrom)) continue;
            bool allIgnoredDiffer = true;
            for (const auto& ignoredTarget : ignored) {
                if (!ignoredTarget.IsValid() || ignoredTarget.Compare(target)) {
                    allIgnoredDiffer = false;
                    break;
                }
            }
            if (allIgnoredDiffer) source.push_back(target);
        }

        switch (ModeIndex()) {
        case 0:
            return MinTarget(source, [this, damageType](const SDK::AIHeroClient& target) {
                return static_cast<float>(AaIndicator(target, damageType));
            });
        case 1:
            return MinTarget(source, [this, damageType](const SDK::AIHeroClient& target) {
                return GetRealHeath(target, damageType);
            });
        case 2:
            return MinTarget(source, [this](const SDK::AIHeroClient& target) {
                return static_cast<float>(GetPriority(target));
            });
        case 3:
            return MinTarget(source, [](const SDK::AIHeroClient& target) {
                return target.Distance(SDK::Game::CursorPosRaw());
            });
        case 4:
            return MinTarget(source, [](const SDK::AIHeroClient& target) {
                return target.Distance(SDK::GameObjects::Player());
            });
        default:
            return {};
        }
    }

    std::vector<SDK::AIHeroClient> GetTargets(
        float range,
        SDK::DamageType damageType = SDK::DamageType::True,
        bool ignoreShields = true,
        const SDK::Vector3& checkFrom = SDK::Vector3(),
        const std::vector<SDK::AIHeroClient>* ignoreChampions = nullptr) override
    {
        if (Bool("OnlySelectTarget", false) && selectedTarget_.IsValid() &&
            SDK::Extensions::IsValidTarget(selectedTarget_, FLT_MAX, true, SDK::Vector3())) {
            return {selectedTarget_};
        }

        auto list = GetOrderedTargetsByMode(
            range, damageType, ignoreShields, checkFrom, ignoreChampions);

        if (Bool("ForceSelectTarget", true) && selectedTarget_.IsValid() &&
            SDK::Extensions::IsValidTarget(selectedTarget_, range, true, SDK::Vector3())) {
            const auto it = std::find_if(list.begin(), list.end(), [this](const SDK::AIHeroClient& target) {
                return target.Compare(selectedTarget_);
            });
            if (it != list.end()) {
                const auto selected = *it;
                list.erase(it);
                list.insert(list.begin(), selected);
            }
        }
        return list;
    }

    std::vector<SDK::AIHeroClient> GetOrderedTargetsByMode(
        float range,
        SDK::DamageType damageType,
        bool ignoreShield,
        const SDK::Vector3& from = SDK::Vector3(),
        const std::vector<SDK::AIHeroClient>* ignoreChampions = nullptr)
    {
        (void)ignoreShield;
        static const std::vector<SDK::AIHeroClient> empty;
        const auto& ignored = ignoreChampions ? *ignoreChampions : empty;
        std::vector<SDK::AIHeroClient> source;
        for (const auto& target : SDK::GameObjects::EnemyHeroes()) {
            if (!IsValidTarget(target, range, from)) continue;
            bool include = true;
            for (const auto& ignoredTarget : ignored) {
                if (ignoredTarget.NetworkId() == target.NetworkId()) {
                    include = false;
                    break;
                }
            }
            if (include) source.push_back(target);
        }

        switch (ModeIndex()) {
        case 0:
            std::sort(source.begin(), source.end(), [this, damageType](const auto& a, const auto& b) {
                return AaIndicator(a, damageType) < AaIndicator(b, damageType);
            });
            break;
        case 1:
            std::sort(source.begin(), source.end(), [this, damageType](const auto& a, const auto& b) {
                return GetRealHeath(a, damageType) < GetRealHeath(b, damageType);
            });
            break;
        case 2:
            std::sort(source.begin(), source.end(), [this](const auto& a, const auto& b) {
                return GetPriority(a) > GetPriority(b);
            });
            break;
        case 3: {
            const auto cursor = SDK::Game::CursorPosRaw();
            std::sort(source.begin(), source.end(), [&cursor](const auto& a, const auto& b) {
                return a.Distance(cursor) < b.Distance(cursor);
            });
            break;
        }
        case 4:
            std::sort(source.begin(), source.end(), [](const auto& a, const auto& b) {
                return a.Distance(SDK::GameObjects::Player()) < b.Distance(SDK::GameObjects::Player());
            });
            break;
        }
        return source;
    }

    bool IsValidTarget(
        const SDK::AIHeroClient& hero,
        float range,
        const SDK::Vector3& from = SDK::Vector3()) const
    {
        const auto player = Player();
        const SDK::Vector3 position = !from.IsZero() && from.IsValid()
            ? from
            : player.PreviousPosition();
        if (hero.IsValid() && SDK::Extensions::IsValidTarget(hero, FLT_MAX, true, SDK::Vector3())) {
            if (hero.IsInvulnerable() || hero.IsInvulnerable()) return false;
            if (hero.IsZombie()) return false;
            if (!hero.IsTargetable()) return false;
            if (hero.HasBuff("UndyingRage") && hero.Health() <= 71.0f) return false;
            if (range > 0.0f) {
                const float distance = hero.Distance(position);
                if (static_cast<double>(distance * distance) <
                    std::pow(static_cast<double>(range + hero.BoundingRadius()), 2.0)) {
                    return true;
                }
            } else if (range < 0.0f && InCurrentAutoAttackRange(hero, 0.0f)) {
                return true;
            }
        }
        return false;
    }

    bool InCurrentAutoAttackRange(
        const SDK::AIHeroClient& target,
        float extraRange = 0.0f) const
    {
        if (!IsValidTarget(target, FLT_MAX, SDK::Vector3())) return false;
        const auto player = Player();
        if (_stricmp(player.CharacterName().c_str(), "Azir") == 0) {
            auto soldiers = SDK::GameObjects::AllyPets();
            const auto special = SDK::GameObjects::AllySpecialMinions();
            soldiers.insert(soldiers.end(), special.begin(), special.end());
            for (const auto& soldier : soldiers) {
                if (!soldier.IsValid() || soldier.IsDead()) continue;
                std::string name = soldier.CharacterName();
                std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                if (name.find("azirsoldier") == std::string::npos) continue;
                const float playerDistance = soldier.Distance(player.PreviousPosition());
                const float targetDistance = target.Distance(soldier);
                if (static_cast<double>(playerDistance * playerDistance) <= std::pow(770.0, 2.0) &&
                    static_cast<double>(targetDistance * targetDistance) <= std::pow(350.0, 2.0)) {
                    return true;
                }
            }
        }
        const float range = SDK::Utils::AutoAttack::GetRealAutoAttackRange(player, target) + extraRange;
        const float distance = target.PreviousPosition().Distance2D(player.PreviousPosition());
        return static_cast<double>(distance * distance) <= std::pow(static_cast<double>(range), 2.0);
    }

    SDK::AIHeroClient GetSelectedTarget() const override { return selectedTarget_; }
    void SetTarget(const SDK::AIHeroClient& target) override { selectedTarget_ = target; }

    void Suspend() override {
        if (suspended_) return;
        SDK::Drawing::OnDraw -= &OnDrawHandler;
        SDK::Game::OnWndProc -= &OnWndProcHandler;
        suspended_ = true;
    }

    void Resume() override {
        if (!suspended_ || disposed_) return;
        Ptr() = this;
        SDK::Game::OnWndProc += &OnWndProcHandler;
        SDK::Drawing::OnDraw += &OnDrawHandler;
        suspended_ = false;
    }

private:
    static TargetSelectorImpulse*& Ptr() {
        static TargetSelectorImpulse* instance = nullptr;
        return instance;
    }

    static SDK::AIHeroClient Player() { return SDK::GameObjects::Player(); }

    bool Bool(const char* key, bool fallback) const {
        const auto* item = menu_ ? menu_->Get<SDK::MenuBool>(key) : nullptr;
        return item ? item->Value : fallback;
    }

    int ModeIndex() const {
        const auto* item = menu_ ? menu_->Get<SDK::MenuList>(modeKey_.c_str()) : nullptr;
        return item ? item->Index : 0;
    }

    static bool Contains(
        const std::vector<SDK::AIHeroClient>& list,
        const SDK::AIHeroClient& target)
    {
        return std::any_of(list.begin(), list.end(), [&target](const auto& item) {
            return item.Compare(target);
        });
    }

    template <typename Score>
    static SDK::AIHeroClient MinTarget(
        const std::vector<SDK::AIHeroClient>& targets,
        Score score)
    {
        if (targets.empty()) return {};
        auto best = targets.front();
        float bestScore = score(best);
        for (std::size_t i = 1; i < targets.size(); ++i) {
            const float current = score(targets[i]);
            if (current < bestScore) {
                best = targets[i];
                bestScore = current;
            }
        }
        return best;
    }

    float GetRealHeath(const SDK::AIHeroClient& unit, SDK::DamageType type) const {
        float extraShield = 0.0f;
        switch (type) {
        case SDK::DamageType::Physical:
            extraShield = unit.PhysicalShield();
            break;
        case SDK::DamageType::Magical:
            extraShield = unit.MagicalShield();
            break;
        case SDK::DamageType::Mixed:
            extraShield = unit.PhysicalShield() + unit.MagicalShield();
            break;
        case SDK::DamageType::True:
        default:
            extraShield = 0.0f;
            break;
        }
        return unit.Health() + extraShield + (unit.HealthRegenRate() * 2.0f) + unit.AllShield();
    }

    int AaIndicator(
        const SDK::AIHeroClient& enemy,
        SDK::DamageType type = SDK::DamageType::Physical,
        float damage = 0.0f) const
    {
        if (damage == 0.0f) {
            switch (type) {
            case SDK::DamageType::Physical:
                damage = Player().AD();
                break;
            default:
                damage = 200.0f;
                break;
            }
        }
        const float calculated = SDK::Damage::CalculateDamage(Player(), enemy, type, damage);
        if (calculated <= 0.0f) return INT_MAX;
        const float killableAaCount = GetRealHeath(enemy, type) / calculated;
        return static_cast<int>(std::ceil(killableAaCount));
    }

    static void OnWndProcHandler(SDK::Game::WndEventArgs& args) {
        if (Ptr()) Ptr()->OnWndProc(args);
    }

    static void OnDrawHandler() {
        if (Ptr()) Ptr()->OnDraw();
    }

    SDK::Menu* menu_ = nullptr;
    SDK::Menu* priorityMenu_ = nullptr;
    SDK::Menu* drawingsMenu_ = nullptr;
    SDK::AIHeroClient selectedTarget_ = {};
    std::string modeKey_;
    bool suspended_ = true;
    bool disposed_ = false;
};

} // namespace Plugins::TargetSelectorImpulse
