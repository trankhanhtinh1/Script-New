#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Core/EvadeHelper.h"
#include "sdk/EzEvade/Helpers/EvadeCommand.h"
#include "sdk/EzEvade/Utils/DelayAction.h"

namespace EzEvade {

class SpecialEvadeSpell {
public:
    static void LoadSpecialSpell(EvadeSpellData& spellData) {
        if (spellData.spellName == "EkkoEAttack") {
            spellData.useSpellFunc = UseEkkoE2;
        } else if (spellData.spellName == "EkkoR") {
            spellData.useSpellFunc = UseEkkoR;
        } else if (spellData.spellName == "EliseSpiderEInitial") {
            spellData.useSpellFunc = UseRappel;
        } else if (spellData.spellName == "Pounce") {
            spellData.useSpellFunc = UsePounce;
        } else if (spellData.spellName == "RivenTriCleave") {
            spellData.useSpellFunc = UseBrokenWings;
        }
    }

    static bool UseRappel(const EvadeSpellData& evadeSpell, bool process = true) {
        (void)process;
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        if (_stricmp(me.GetChampionName().c_str(), "Elise") != 0) {
            EvadeCommand::CastSpell(evadeSpell, me);
            return true;
        }

        SDK::SpellCaster caster(ToSdkSlot(evadeSpell.spellKey), true, SDK::HitChance::Medium);
        if (caster.IsReady()) {
            caster.Cast();
        }
        return false;
    }

    static bool UsePounce(const EvadeSpellData& evadeSpell, bool process = true) {
        (void)process;
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        if (_stricmp(me.GetChampionName().c_str(), "Nidalee") != 0) {
            auto posInfo = EvadeHelper::GetBestPositionDash(evadeSpell);
            if (!posInfo.Position.IsZero()) {
                EvadeCommand::CastSpell(evadeSpell);
                return true;
            }
        }
        return false;
    }

    static bool UseBrokenWings(const EvadeSpellData& evadeSpell, bool process = false) {
        auto posInfo = EvadeHelper::GetBestPositionDash(evadeSpell);
        if (!posInfo.Position.IsZero()) {
            EvadeCommand::MoveTo(posInfo.Position);
            DelayAction::Add(50, [evadeSpell, process]() {
                if (process) {
                    EvadeCommand::CastSpell(evadeSpell);
                }
            });
            return true;
        }
        return false;
    }

    static bool UseEkkoE2(const EvadeSpellData& evadeSpell, bool process = true) {
        (void)process;
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        if (me.HasBuff("ekkoeattackbuff")) {
            auto posInfo = EvadeHelper::GetBestPositionTargetedDash(evadeSpell);
            if (posInfo.Target.IsValid()) {
                EvadeCommand::Attack(evadeSpell, posInfo.Target);
                return true;
            }
        }
        return false;
    }

    static bool UseEkkoR(const EvadeSpellData& evadeSpell, bool process = true) {
        (void)process;
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;
        for (const auto& obj : SDK::GameObjects::AllMinions) {
            if (!obj.IsValid() || obj.IsDead()) continue;
            if (obj.GetTeam() != me.GetTeam()) continue;
            if (_stricmp(obj.GetName().c_str(), "Ekko") != 0) continue;

            Vec2 blinkPos = obj.GetServerPosition().To2D();
            if (!Position::CheckDangerousPos(blinkPos, 10.0f)) {
                EvadeCommand::CastSpell(evadeSpell);
                return true;
            }
        }
        return false;
    }

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
};

} // namespace EzEvade
