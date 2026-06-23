#pragma once

#include "../Database/SpellDatabase.h"
#include "../SpellTypes/SkillshotCircle.h"
#include "../SpellTypes/SkillshotCone.h"
#include "../SpellTypes/SkillshotLine.h"
#include "../SpellTypes/SkillshotMissileArc.h"
#include "../SpellTypes/SkillshotMissileCircle.h"
#include "../SpellTypes/SkillshotMissileCone.h"
#include "../SpellTypes/SkillshotMissileLine.h"
#include "../SpellTypes/SkillshotRing.h"

#include "../../../Core/Game.h"
#include "../../../Events/Events.h"
#include "../../../GameObjects/GameObjects.h"
#include "../../../GameObjects/ObjectManager.h"
#include "../../../Utils/DelayAction.h"
#include "../../../Utils/Logging.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace SDK {

class Detector {
public:
    using OnDetectSkillshotH = void(*)(const std::shared_ptr<Skillshot>& skillshot);

    static void Initialize() {
        if (Initialized()) {
            return;
        }

        Initialized() = true;
        Events::AddOnProcessSpell(&Obj_AI_Base_OnProcessSpellCast);
        Events::AddOnMissileCreate(&MissileClient_OnCreate);

        // TODO(SDK parity): EnsoulSharp also listens to GameObject.OnCreate
        // for SourceObjectName spells. NightSharp currently exposes missile
        // create/delete only; wire generic object create here once available.
    }

    static bool AddOnDetectSkillshot(OnDetectSkillshotH handler) {
        Initialize();
        auto& handlers = Handlers();
        if (!handler) {
            return false;
        }
        if (std::find(handlers.begin(), handlers.end(), handler) != handlers.end()) {
            return true;
        }
        handlers.push_back(handler);
        return true;
    }

    static bool RemoveOnDetectSkillshot(OnDetectSkillshotH handler) {
        auto& handlers = Handlers();
        const auto oldSize = handlers.size();
        handlers.erase(std::remove(handlers.begin(), handlers.end(), handler), handlers.end());
        return oldSize != handlers.size();
    }

private:
    static bool& Initialized() {
        static bool initialized = false;
        return initialized;
    }

    static std::vector<OnDetectSkillshotH>& Handlers() {
        static std::vector<OnDetectSkillshotH> handlers;
        return handlers;
    }

    static AIBaseClient MakeCaster(const ::Core::Events::ObjectInfo& info) {
        if (info.NetworkId != 0 && info.NetworkId != 0xFFFFFFFFu) {
            auto caster = ObjectManager::GetUnitByNetworkId<AIBaseClient>(
                static_cast<int>(info.NetworkId));
            if (caster.IsValid()) {
                return caster;
            }
        }

        return info.Ptr != 0 ? AIBaseClient(info.Ptr, info.Type) : AIBaseClient();
    }

    static void Obj_AI_Base_OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
        const auto* spellDatabaseEntry = SpellDatabase::GetByName(args.SpellName);
        if (!spellDatabaseEntry) {
            return;
        }

        TriggerOnDetectSkillshot(
            *spellDatabaseEntry,
            MakeCaster(args.Sender),
            SkillshotDetectionType::ProcessSpell,
            args.StartPosition.To2D(),
            args.EndPosition.To2D(),
            Variables::TickCount() - Game::Ping() / 2);
    }

    static void MissileClient_OnCreate(const Events::ObjectEventArgs& args) {
        const MissileClient missile(args.Sender.Ptr);
        if (!missile.IsValid()) {
            return;
        }

        const char* missileName = args.MissileName[0] ? args.MissileName : args.SpellName;
        const auto* spellDatabaseEntry = SpellDatabase::GetByMissileName(missileName);
        if (!spellDatabaseEntry && args.SpellName[0]) {
            spellDatabaseEntry = SpellDatabase::GetByName(args.SpellName);
        }
        if (!spellDatabaseEntry) {
            return;
        }

        const SpellDatabaseEntry entry = *spellDatabaseEntry;
        Utils::DelayAction::Add(0, [missile, entry]() {
            MissileClient_OnCreate_Delayed(missile, entry);
        });
    }

    static void MissileClient_OnCreate_Delayed(MissileClient missile,
                                               const SpellDatabaseEntry& spellDatabaseEntry) {
        if (!missile.IsValid()) {
            return;
        }

        AIBaseClient caster = ObjectManager::GetUnitByNetworkId<AIBaseClient>(missile.CasterNetworkId());
        if (!caster.IsValid()) {
            return;
        }

        const int speed = std::max(1, spellDatabaseEntry.MissileSpeed);
        const int castTime =
            Variables::TickCount() -
            Game::Ping() / 2 -
            (spellDatabaseEntry.MissileDelayed ? 0 : spellDatabaseEntry.Delay) -
            static_cast<int>(
                1000.0f * missile.Position().Distance(missile.StartPosition()) /
                static_cast<float>(speed));

        TriggerOnDetectSkillshot(
            spellDatabaseEntry,
            caster,
            SkillshotDetectionType::MissileCreate,
            missile.StartPosition().To2D(),
            missile.EndPosition().To2D(),
            castTime,
            missile);
    }

    static std::shared_ptr<Skillshot> CreateSkillshot(const SpellDatabaseEntry& entry) {
        // TODO(SDK parity): port champion-specific Detector.Skillshots_* types
        // such as _ZiggsR and route them before the generic switch.
        switch (entry.SpellType) {
        case SpellType::SkillshotMissileArc:
            return std::make_shared<SkillshotMissileArc>(entry);
        case SpellType::SkillshotMissileCircle:
            return std::make_shared<SkillshotMissileCircle>(entry);
        case SpellType::SkillshotMissileLine:
            return std::make_shared<SkillshotMissileLine>(entry);
        case SpellType::SkillshotMissileCone:
            return std::make_shared<SkillshotMissileCone>(entry);
        case SpellType::SkillshotCircle:
            return std::make_shared<SkillshotCircle>(entry);
        case SpellType::SkillshotCone:
            return std::make_shared<SkillshotCone>(entry);
        case SpellType::SkillshotLine:
            return std::make_shared<SkillshotLine>(entry);
        case SpellType::SkillshotRing:
            return std::make_shared<SkillshotRing>(entry);
        default:
            return nullptr;
        }
    }

    static void TriggerOnDetectSkillshot(const SpellDatabaseEntry& spellDatabaseEntry,
                                         const AIBaseClient& caster,
                                         SkillshotDetectionType detectionType,
                                         const Vector2& start,
                                         const Vector2& end,
                                         int time,
                                         const MissileClient& missile = MissileClient()) {
        auto skillshot = CreateSkillshot(spellDatabaseEntry);
        if (!skillshot) {
            return;
        }

        skillshot->DetectionType = detectionType;
        skillshot->Caster = caster;
        skillshot->StartPosition = start;
        skillshot->EndPosition = end;
        skillshot->StartTime = time;

        if (missile.IsValid()) {
            auto missileSkillshot = std::dynamic_pointer_cast<SkillshotMissile>(skillshot);
            if (missileSkillshot) {
                missileSkillshot->Missile = missile;
            } else {
                Utils::Logging::Write()(LogLevel::Warn,
                    "Wrong SpellType for Skillshot %s, a Missile Type was expected",
                    skillshot->SData.SpellName.c_str());
            }
        }

        if (!skillshot->Process()) {
            return;
        }

        TriggerOnDetectSkillshot(skillshot);
    }

    static void TriggerOnDetectSkillshot(const std::shared_ptr<Skillshot>& skillshot) {
        for (auto handler : Handlers()) {
            if (handler) {
                handler(skillshot);
            }
        }
    }
};

} // namespace SDK
