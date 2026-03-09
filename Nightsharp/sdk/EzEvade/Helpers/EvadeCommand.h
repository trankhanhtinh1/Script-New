#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Spells/SpellData.h"
#include "sdk/EzEvade/Helpers/EvadeRuntimeState.h"
#include "sdk/EzEvade/Helpers/Situation.h"

namespace EzEvade {

enum class EvadeOrderCommand {
    None = 0,
    MoveTo,
    Attack,
    CastSpell
};

class EvadeCommand {
public:
    EvadeOrderCommand Order = EvadeOrderCommand::None;
    Vec2 TargetPosition = Vec2();
    SDK::GameObject Target;
    float Timestamp = 0.0f;
    bool IsProcessed = false;
    const EvadeSpellData* EvadeSpell = nullptr;

    EvadeCommand()
        : Timestamp((float)SDK::Game::GetTickCount()), IsProcessed(false) {}

    static EvadeCommand LastEvadeCommand;
    static EvadeCommand LastSpellEvadeCommand;

private:
    static SDK::SpellSlotId ToSdkSlot(SpellSlotId slot) {
        switch (slot) {
        case SpellSlotId::Q: return SDK::SpellSlotId::Q;
        case SpellSlotId::W: return SDK::SpellSlotId::W;
        case SpellSlotId::E: return SDK::SpellSlotId::E;
        case SpellSlotId::R: return SDK::SpellSlotId::R;
        case SpellSlotId::F: return SDK::SpellSlotId::Summoner1;
        case SpellSlotId::T: return SDK::SpellSlotId::Summoner2;
        default: return SDK::SpellSlotId::Q;
        }
    }

    static SDK::SpellCaster MakeCaster(const EvadeSpellData& spellData) {
        return SDK::SpellCaster(ToSdkSlot(spellData.spellKey), true, SDK::HitChance::Medium);
    }

public:
    static void MoveTo(const Vec2& movePos) {
        if (!Situation::ShouldDodge()) {
            return;
        }

        LastEvadeCommand = EvadeCommand{};
        LastEvadeCommand.Order = EvadeOrderCommand::MoveTo;
        LastEvadeCommand.TargetPosition = movePos;
        LastEvadeCommand.Timestamp = (float)SDK::Game::GetTickCount();
        LastEvadeCommand.IsProcessed = false;

        const auto& player = SDK::GameObjects::Player;
        if (player.IsValid()) {
            EvadeRuntimeState::LastMoveToPosition = movePos;
            EvadeRuntimeState::LastMoveToServerPos = player.GetServerPosition().To2D();
            player.IssueOrder(SDK::OrderType::MoveTo, Vec3::From2D(movePos, player.GetPosition().y));
        }
    }

    static void Attack(const EvadeSpellData& spellData, const SDK::GameObject& target) {
        LastSpellEvadeCommand = EvadeCommand{};
        LastSpellEvadeCommand.Order = EvadeOrderCommand::Attack;
        LastSpellEvadeCommand.Target = target;
        LastSpellEvadeCommand.EvadeSpell = &spellData;
        LastSpellEvadeCommand.Timestamp = (float)SDK::Game::GetTickCount();
        LastSpellEvadeCommand.IsProcessed = false;

        const auto& player = SDK::GameObjects::Player;
        if (player.IsValid() && target.IsValid()) {
            player.IssueOrder(SDK::OrderType::AttackUnit, target);
        }
    }

    static void CastSpell(const EvadeSpellData& spellData, const SDK::GameObject& target) {
        LastSpellEvadeCommand = EvadeCommand{};
        LastSpellEvadeCommand.Order = EvadeOrderCommand::CastSpell;
        LastSpellEvadeCommand.Target = target;
        LastSpellEvadeCommand.EvadeSpell = &spellData;
        LastSpellEvadeCommand.Timestamp = (float)SDK::Game::GetTickCount();
        LastSpellEvadeCommand.IsProcessed = false;

        if (!target.IsValid()) return;
        auto caster = MakeCaster(spellData);
        caster.Cast(target);
    }

    static void CastSpell(const EvadeSpellData& spellData, const Vec2& movePos) {
        LastSpellEvadeCommand = EvadeCommand{};
        LastSpellEvadeCommand.Order = EvadeOrderCommand::CastSpell;
        LastSpellEvadeCommand.TargetPosition = movePos;
        LastSpellEvadeCommand.EvadeSpell = &spellData;
        LastSpellEvadeCommand.Timestamp = (float)SDK::Game::GetTickCount();
        LastSpellEvadeCommand.IsProcessed = false;

        const auto& player = SDK::GameObjects::Player;
        if (!player.IsValid()) return;
        auto caster = MakeCaster(spellData);
        caster.Cast(Vec3::From2D(movePos, player.GetPosition().y));
    }

    static void CastSpell(const EvadeSpellData& spellData) {
        LastSpellEvadeCommand = EvadeCommand{};
        LastSpellEvadeCommand.Order = EvadeOrderCommand::CastSpell;
        LastSpellEvadeCommand.EvadeSpell = &spellData;
        LastSpellEvadeCommand.Timestamp = (float)SDK::Game::GetTickCount();
        LastSpellEvadeCommand.IsProcessed = false;

        auto caster = MakeCaster(spellData);
        caster.Cast();
    }
};

inline EvadeCommand EvadeCommand::LastEvadeCommand{};
inline EvadeCommand EvadeCommand::LastSpellEvadeCommand{};

} // namespace EzEvade
