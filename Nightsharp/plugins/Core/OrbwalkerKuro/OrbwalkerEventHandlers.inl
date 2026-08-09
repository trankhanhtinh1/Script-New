#pragma once

using namespace ::SDK;

namespace OrbwalkerKuro {
namespace OrbwalkingDetail {

inline bool IsFlashSpellName(const char* name) {
    return name && name[0] &&
           (_stricmp(name, "flash") == 0 ||
            ::Core::Objects::ContainsInsensitive(name, "summonerflash") ||
            ::Core::Objects::ContainsInsensitive(name, "summoner_flash"));
}

inline bool IsLocalFlashSpell(
    const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return false;
    }

    const char* names[] = {
        args.SpellName,
        args.ScriptName,
        args.SpellSlotName,
        args.PayloadSpellName,
        args.MissileName,
        args.PayloadMissileName,
    };
    for (const char* name : names) {
        if (IsFlashSpellName(name)) {
            return true;
        }
    }

    if (args.Slot != static_cast<int>(SpellSlot::Summoner1) &&
        args.Slot != static_cast<int>(SpellSlot::Summoner2)) {
        return false;
    }

    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        return false;
    }
    const auto spell = player.Spellbook().GetSpell(
        static_cast<SpellSlot>(args.Slot));
    return IsFlashSpellName(spell.Name().c_str()) ||
           IsFlashSpellName(spell.ScriptName().c_str()) ||
           IsFlashSpellName(spell.IconName().c_str());
}

} // namespace OrbwalkingDetail


inline void OrbwalkerBase::OnGameUpdateStatic(const Events::GameUpdateEventArgs& args) {
    (void)args;
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnGameUpdate();
    }
}

inline void OrbwalkerBase::OnDoCastStatic(const Events::ProcessSpellEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnDoCast(args);
    }
}

inline void OrbwalkerBase::OnProcessSpellStatic(const Events::ProcessSpellEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnProcessSpell(args);
    }
}

inline void OrbwalkerBase::OnStopCastStatic(const Events::StopCastEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnStopCast(args);
    }
}

inline void OrbwalkerBase::OnMissileCreateStatic(const Events::ObjectEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnMissileCreate(args);
    }
}


inline void OrbwalkerBase::OnDrawStatic() {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnDraw();
    }
}

inline void OrbwalkerBase::ReconcileApheliosReturnMissile() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() ||
        SDK::ChampionIdFromName(player.CharacterName().c_str()) !=
            SDK::ChampionId::Aphelios) {
        context_.apheliosReturnMissileNetworkId = 0;
        return;
    }

    const int playerNetworkId = player.NetworkId();
    int observedNetworkId = 0;
    for (const auto& missile : GameObjects::Missiles()) {
        if (!missile.IsValid() ||
            missile.CasterNetworkId() != playerNetworkId) {
            continue;
        }
        const std::string name = missile.Name();
        const std::string characterName = missile.CharacterName();
        if (_stricmp(name.c_str(), "ApheliosCrescendumAttackMisIn") == 0 ||
            _stricmp(characterName.c_str(),
                     "ApheliosCrescendumAttackMisIn") == 0) {
            observedNetworkId = missile.NetworkId();
            break;
        }
    }

    const int previousNetworkId =
        context_.apheliosReturnMissileNetworkId;
    if (previousNetworkId != 0 &&
        previousNetworkId != observedNetworkId) {
        ResetAutoAttackTimerWithReason(
            "Aphelios Crescendum return missile",
            "frame-reconciled missile lifetime",
            "missile-return",
            player.CharacterName().c_str(),
            "ApheliosCrescendumAttackMisIn",
            -1,
            "ApheliosCrescendumAttackMisIn",
            "ApheliosCrescendumAttackMisIn",
            static_cast<std::uint32_t>(previousNetworkId),
            static_cast<std::uint32_t>(playerNetworkId));
    }
    context_.apheliosReturnMissileNetworkId = observedNetworkId;
}

inline void OrbwalkerBase::ReconcileRetainedObjects() {
    const auto isLive = [](const AttackableUnit& object) {
        const int networkId = static_cast<int>(object.CachedNetworkId());
        return networkId != 0 &&
               GameObjects::GetUnitByNetworkId<AttackableUnit>(networkId).IsValid();
    };

    if (!isLive(context_.forceTarget)) {
        context_.forceTarget = {};
    }
    if (!isLive(context_.lastTarget)) {
        context_.lastTarget = {};
    }
    if (!isLive(context_.laneClearMinion)) {
        context_.laneClearMinion = {};
    }
    if (!isLive(context_.cachedTarget)) {
        context_.cachedTarget = {};
        context_.cachedTargetTick = -1;
        context_.cachedShouldWaitTick = -1;
    }
    if (context_.postFlashTargetNetworkId != 0 &&
        !GameObjects::GetUnitByNetworkId<AttackableUnit>(
            context_.postFlashTargetNetworkId).IsValid()) {
        ClearPostFlashAttackGrace();
    }
}

inline void OrbwalkerBase::OnGameUpdate() {
    NightSharpPerf::ScopedTimer timer("OrbwalkerKuro::OnGameUpdate");
    ReconcileRetainedObjects();
    if (!menu_.Enabled()) {
        ClearPendingAttackState();
        ClearPostFlashAttackGrace();
        context_.activeMode = OrbwalkingMode::None;
        return;
    }

    context_.activeMode = ActiveMode();

    if (context_.activeMode == OrbwalkingMode::None) {
        ClearPendingAttackState();
        ClearPostFlashAttackGrace();
        return;
    }

    const int now = Tick();
    if (!IsPostFlashAttackGraceActive(now)) {
        ClearPostFlashAttackGrace();
    }

    if (EvadeOwnsActions(now)) {
        ExpirePendingAttack();
        // KuroEvade always owns movement here, but its danger + delayed-route
        // policy decides whether a fresh attack is allowed. This keeps the
        // menu threshold meaningful without starting a windup that the evade
        // engine would have to cancel one frame later.
        if (!EvadeBlocksAttack(now)) {

            const AttackableUnit stationaryTarget =
                CanAttack() ? GetTarget() : AttackableUnit();

            if (stationaryTarget.IsValid()) {
                Attack(stationaryTarget);
            }
        }
        return;
    }

    const Vector3 position = context_.orbwalkerPosition.IsZero() ? Game::CursorPos() : context_.orbwalkerPosition;
    const bool preservePostFlashTarget =
        IsPostFlashAttackGraceActive(now);
    const AttackableUnit target =
        (CanAttack() || preservePostFlashTarget)
            ? GetTarget()
            : AttackableUnit();
    Orbwalk(target, position);
}


inline void OrbwalkerBase::OnDoCast(const Events::ProcessSpellEventArgs& args) {
    const bool isAttack = IsLocalAutoAttack(args);
    if (!isAttack) {
        return;
    }

    const int now = Tick();

    if (!context_.pendingAttack &&
        context_.lastAutoAttackResetTick > 0 &&
        now - context_.lastAutoAttackResetTick >= 0 &&
        now - context_.lastAutoAttackResetTick <= PendingAttackTimeoutMs()) {
        return;
    }

    const bool hadPendingAttack = context_.pendingAttack;
    const auto player = GameObjects::Player();
    const bool isAzirSoldierAttack =
        OrbwalkingDetail::IsAzirSoldierAttackEvent(args) ||
        OrbwalkingDetail::IsOwnedAzirSoldierSender(player, args.Sender);

    // OnDoCast fires at the START of the windup, so an attack the orbwalker did
    // not order — a manual right-click, or a champion script issuing its own
    // attack command — is starting right now. It used to be back-dated by a full
    // windup (`now - attackWindupMs`), which is where the manual-attack cancel
    // came from: CanMove() measured that stale tick, found the windup already
    // over and let the very next orbwalk step send a move order that cancelled
    // the attack before it dealt damage, while CanAttack() went ready a windup
    // too early on top. Stamping `now` matches what the pending branch below
    // resolves to for the orbwalker's own attacks, so both kinds of attack are
    // gated from the same event at the same instant.

    const int manualAttackStartTick = now;
    const int attackStartTick = hadPendingAttack
        ? context_.pendingAttackTick
        : manualAttackStartTick;

    context_.lastAttackOrderToAnimGapMs = hadPendingAttack
        ? std::max(0, now - context_.pendingAttackTick)
        : 0;
    const AttackableUnit target = ResolveAttackTarget(args);
    if (target.IsValid()) {
        context_.lastTarget = target;
    }

    context_.pendingAttack = false;
    context_.pendingAttackTick = 0;
    context_.pendingAttackTargetNetworkId = 0;
    context_.lastAttackConfirmTick = now;
    context_.lastAutoAttackResetTick = 0;
    context_.hasConfirmedAttack = true;
    context_.attackCastComplete = false;
    if (context_.lastAttackRequiresDoCastBeforeMove) {
        context_.lastAttackDoCastComplete = true;
        context_.lastAttackDoCastWaitTick = 0;
    }

    context_.lastAutoAttackTick = attackStartTick;
    ReadAttackTimingsFromMemory(player);

    const AttackableUnit eventTarget = target.IsValid() ? target : context_.lastTarget;
    OrbwalkingActionArgs attackArgs(
        OrbwalkingType::OnAttack,
        eventTarget,
        eventTarget.IsValid() ? eventTarget.Position() : Vector3(),
        "Kuro");
    OrbwalkingDetail::FireOnAttack(attackArgs);
}

inline void OrbwalkerBase::OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    if (OrbwalkingDetail::IsLocalFlashSpell(args)) {
        const int now = Tick();
        int targetNetworkId = context_.pendingAttackTargetNetworkId;
        if (targetNetworkId <= 0 && context_.lastTarget.IsValid()) {
            targetNetworkId = context_.lastTarget.NetworkId();
        }
        if (targetNetworkId <= 0 && context_.cachedTarget.IsValid()) {
            targetNetworkId = context_.cachedTarget.NetworkId();
        }
        context_.postFlashTargetNetworkId = targetNetworkId > 0
            ? targetNetworkId
            : 0;
        context_.postFlashAttackGraceUntilTick =
            now + kPostFlashAttackGraceMs;
        context_.cachedTargetTick = -1;
    }

    const bool isAttack = IsLocalAutoAttack(args);
    const AutoAttackResetMatch resetMatch = GetLocalAutoAttackResetMatch(args);

    if (menu_.DebugLogSpellNames()) {
        NightSharpDebug::Logf(
            "[<b-cyan>OrbwalkerKuro</b-cyan>][<b-yellow>Spell</b-yellow>] "
            "spell=<magenta>%s</magenta> missile=<cyan>%s</cyan> slot=%d isAttack=%d isReset=%d",
            args.SpellName ? args.SpellName : "",
            args.MissileName ? args.MissileName : "",
            args.Slot,
            isAttack ? 1 : 0,
            resetMatch != AutoAttackResetMatch::None ? 1 : 0);
    }

    if (resetMatch != AutoAttackResetMatch::None && !isAttack) {
        const auto player = GameObjects::Player();
        const std::string championName = player.IsValid()
            ? player.CharacterName()
            : std::string(args.Sender.CharacterName);
        std::string matchTypeDetail;
        if (resetMatch == AutoAttackResetMatch::SpellName) {
            matchTypeDetail = "spell-name: " + std::string(args.SpellName ? args.SpellName : "unknown");
        } else {
            matchTypeDetail = "champion-slot: " + std::to_string(args.Slot);
        }
        const char* reason = resetMatch == AutoAttackResetMatch::SpellName
            ? "auto-attack reset spell name"
            : "champion/slot auto-attack reset";

        ResetAutoAttackTimerWithReason(
            reason,
            "OnProcessSpell",
            matchTypeDetail.c_str(),
            championName.c_str(),
            args.SpellName,
            args.Slot,
            args.Sender.Name,
            args.MissileName,
            args.Sender.NetworkId,
            args.CasterNetworkId);
        return;
    }

    if (!isAttack) {
        return;
    }

    const int now = Tick();
    const auto player = GameObjects::Player();
    const SDK::ChampionId playerChampionId = player.IsValid()
        ? SDK::ChampionIdFromName(player.CharacterName().c_str())
        : SDK::ChampionId::Unknown;
    std::string spellNameStr = args.SpellName ? args.SpellName : "";
    for (auto& c : spellNameStr) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    context_.lastAttackSpellName = spellNameStr;

    // Crit sequence tracking intentionally lives on OnProcessSpell and is not
    // gated by the orbwalker's pending/timer recovery state. At this point the
    // game has selected the concrete normal/crit attack variant.
    const bool isAzirSoldierAttack =
        OrbwalkingDetail::IsAzirSoldierAttackEvent(args) ||
        OrbwalkingDetail::IsOwnedAzirSoldierSender(player, args.Sender);
    const int critTargetNetworkId = args.Target.IsValid()
        ? static_cast<int>(args.Target.NetworkId)
        : static_cast<int>(args.TargetNetworkId);
    const bool duplicateCritProcessSpell =
        context_.lastCritProcessSpellTick >= 0 &&
        now - context_.lastCritProcessSpellTick >= 0 &&
        now - context_.lastCritProcessSpellTick <= 5 &&
        context_.lastCritProcessSpellTargetNetworkId == critTargetNetworkId;
    const bool forcedFioraCrit =
        player.IsValid() &&
        playerChampionId == SDK::ChampionId::Fiora &&
        (player.HasBuff("fiorae2") ||
         spellNameStr.find("fiorae2") != std::string::npos);
    if (player.IsValid() &&
        !isAzirSoldierAttack &&
        !forcedFioraCrit &&
        !duplicateCritProcessSpell) {
        const bool didCrit =
            FarmLogic::IsCriticalAttackName(args.SpellName) ||
            FarmLogic::IsCriticalAttackName(args.MissileName) ||
            FarmLogic::IsCriticalAttackName(args.ScriptName) ||
            FarmLogic::IsCriticalAttackName(args.SpellSlotName) ||
            FarmLogic::IsCriticalAttackName(args.PayloadSpellName) ||
            FarmLogic::IsCriticalAttackName(args.PayloadMissileName) ||
            spellNameStr.find("akshancrit") != std::string::npos;
        context_.critSequence.Observe(player.Crit(), didCrit);
        context_.lastCritProcessSpellTick = now;
        context_.lastCritProcessSpellTargetNetworkId = critTargetNetworkId;
        context_.cachedTargetTick = -1;
        context_.cachedShouldWaitTick = -1;
    }

    if (!context_.pendingAttack &&
        context_.lastAutoAttackResetTick > 0 &&
        now - context_.lastAutoAttackResetTick >= 0 &&
        now - context_.lastAutoAttackResetTick <= PendingAttackTimeoutMs()) {
        return;
    }

    const AttackableUnit target = ResolveAttackTarget(args);
    if (target.IsValid()) {
        context_.lastTarget = target;
    }

    const bool isSpecialAfterAA = IsSpecialAfterAttack(spellNameStr);
    if (isSpecialAfterAA) {
        context_.attackWindupMs = 0.0f;
        context_.attackCastComplete = true;
        context_.lastAttackDoCastComplete = true;
    } else {
        context_.attackCastComplete = false;
    }
    ReadAttackTimingsFromMemory(player);

    if (player.IsValid() && playerChampionId == SDK::ChampionId::Akshan) {
        const bool isAkshanSecondAttack =
            spellNameStr.find("akshanpassive") != std::string::npos ||
            spellNameStr.find("akshanpattack") != std::string::npos;
        if (isAkshanSecondAttack) {
            context_.isAkshanSecondShotPending = false;
            context_.isAkshanSecondShotActive = true;
            context_.lastAutoAttackTick = now;
        } else if (isAttack) {
            const int passiveMode = menu_.AkshanPassiveMode();
            if (passiveMode == 0 || passiveMode == 2) {
                context_.isAkshanSecondShotPending = true;
                context_.pendingAkshanSecondShotTick = now;
            }
        }
    }

    const AttackableUnit eventTarget = target.IsValid() ? target : context_.lastTarget;

    // Fire AfterAttack immediately ONLY for SpecialAfterAA spells (e.g. lucianpassiveshot, rengarqattack, rengarqempattack)
    if (isSpecialAfterAA) {
        if (context_.lastAfterAttackStartTick != context_.lastAutoAttackTick) {
            context_.lastAfterAttackStartTick = context_.lastAutoAttackTick;
            LogAfterAttackDebug(eventTarget);
            OrbwalkingActionArgs afterArgs(
                OrbwalkingType::AfterAttack,
                eventTarget,
                eventTarget.IsValid() ? eventTarget.Position() : Vector3(),
                "Kuro");
            OrbwalkingDetail::FireAfterAttack(afterArgs);
        }
    } else {
        // OnProcessSpell is called when the attack animation actually starts on the server.
        // attackWindupMs is the total windup duration from this animation start.
        // Therefore, the exact windup completion tick is (now + attackWindupMs).
        const int windupEndTick = now + context_.lastAttackOrderToAnimGapMs + static_cast<int>(context_.attackWindupMs);
        int delayMs = windupEndTick - now;
        if (delayMs < 0) {
            delayMs = 0;
        }
        const std::string debugAfterAttackSpellName = context_.lastAttackSpellName;
        SDK::Utils::DelayAction::Add(delayMs, [this, eventTarget, debugAfterAttackSpellName]() {
            if (eventTarget.IsValid()) {
                if (menu_.DebugLogAfterAttack()) {
                    LogAfterAttackDebug(eventTarget, &debugAfterAttackSpellName);
                }
                OrbwalkingActionArgs afterArgs(
                    OrbwalkingType::AfterAttack,
                    eventTarget,
                    eventTarget.Position(),
                    "Kuro");
                OrbwalkingDetail::FireAfterAttack(afterArgs);
            }
        });
    }
}

inline void OrbwalkerBase::OnStopCast(const Events::StopCastEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (args.Slot >= 0 && args.Slot != 64) {
        return;
    }

    const int now = Tick();
    const int pingSafety = 30 + static_cast<int>(OneWayPingMs());
    const bool stoppedPending =
        context_.pendingAttack &&
        now - context_.pendingAttackTick > pingSafety &&
        now - context_.pendingAttackTick <= PendingAttackTimeoutMs();

    ReadAttackTimingsFromMemory(GameObjects::Player());
    const int windupWindow = static_cast<int>(
        context_.attackWindupMs + MoveSafetyMs() + OneWayPingMs() + kDuplicateAttackEventMs);
    const bool stoppedWindup =
        !context_.attackCastComplete &&
        context_.lastAutoAttackTick > 0 &&
        now - context_.lastAutoAttackTick >= 0 &&
        now - context_.lastAutoAttackTick <= windupWindow;

    if (!stoppedPending && !stoppedWindup) {
        return;
    }

    const int stoppedAttackTick = stoppedPending
        ? context_.pendingAttackTick
        : context_.lastAutoAttackTick;

    ClearPendingAttackState();

    if (args.HasBeenCast || args.DestroyMissile) {
        if (stoppedPending && context_.lastAutoAttackTick <= 0) {
            context_.lastAutoAttackTick = now;
            context_.hasConfirmedAttack = true;
        }
        context_.attackCastComplete = true;
        return;
    }

    context_.lastAutoAttackTick = stoppedAttackTick > 0 ? stoppedAttackTick : now;
    context_.lastAttackConfirmTick = 0;
    context_.hasConfirmedAttack = false;
    context_.attackCastComplete = false;
    context_.attackPauseTick = std::max(context_.attackPauseTick, now + kAttackRetryDelayMs);
}

inline void OrbwalkerBase::OnMissileCreate(const Events::ObjectEventArgs& args) {
    if (!IsLocalAutoAttackMissile(args)) {
        return;
    }

    const AttackableUnit target = ResolveAttackTarget(args);
    if (target.IsValid()) {
        context_.lastTarget = target;
    }

    context_.pendingAttack = false;
    context_.pendingAttackTick = 0;
    context_.pendingAttackTargetNetworkId = 0;
    context_.hasConfirmedAttack = true;
    context_.attackCastComplete = true;

    const AttackableUnit eventTarget = target.IsValid() ? target : context_.lastTarget;
}


namespace OrbwalkingDetail {

struct AutoAttackResetSlotEntry {
    SDK::ChampionId Champion = SDK::ChampionId::Unknown;
    SpellSlot Slot;
};

inline bool IsKnownAutoAttackResetSlot(
    SDK::ChampionId championId,
    int slot
) {
    if (championId == SDK::ChampionId::Unknown) {
        return false;
    }

    static constexpr AutoAttackResetSlotEntry entries[] = {
        { SDK::ChampionId::Aatrox,     SpellSlot::E },
        { SDK::ChampionId::Ashe,       SpellSlot::Q },
        { SDK::ChampionId::Belveth,    SpellSlot::Q },
        { SDK::ChampionId::Blitzcrank, SpellSlot::E },
        { SDK::ChampionId::Briar,      SpellSlot::Q },
        { SDK::ChampionId::Briar,      SpellSlot::W },
        { SDK::ChampionId::Camille,    SpellSlot::Q },
        { SDK::ChampionId::Chogath,    SpellSlot::E },
        { SDK::ChampionId::Darius,     SpellSlot::W },
        { SDK::ChampionId::DrMundo,    SpellSlot::E },
        { SDK::ChampionId::Ekko,       SpellSlot::E },
        { SDK::ChampionId::Fiora,      SpellSlot::E },
        { SDK::ChampionId::Fizz,       SpellSlot::W },
        { SDK::ChampionId::Garen,      SpellSlot::Q },
        { SDK::ChampionId::Graves,     SpellSlot::E },
        { SDK::ChampionId::Gwen,       SpellSlot::E },
        { SDK::ChampionId::Hecarim,    SpellSlot::E },
        { SDK::ChampionId::Illaoi,     SpellSlot::W },
        { SDK::ChampionId::Jax,        SpellSlot::W },
        { SDK::ChampionId::Kaisa,      SpellSlot::R },
        { SDK::ChampionId::Kassadin,   SpellSlot::W },
        { SDK::ChampionId::Katarina,   SpellSlot::E },
        { SDK::ChampionId::Kayle,      SpellSlot::E },
        { SDK::ChampionId::Kindred,    SpellSlot::Q },
        { SDK::ChampionId::KSante,     SpellSlot::Q },
        { SDK::ChampionId::Leona,      SpellSlot::Q },

        // Lucian E resets/accelerates his attack sequence directly.
        { SDK::ChampionId::Lucian, SpellSlot::Q },
        { SDK::ChampionId::Lucian, SpellSlot::W },
        { SDK::ChampionId::Lucian, SpellSlot::E },
        { SDK::ChampionId::Lucian, SpellSlot::R },

        { SDK::ChampionId::Malphite,   SpellSlot::W },
        { SDK::ChampionId::MasterYi,   SpellSlot::W },
        { SDK::ChampionId::MonkeyKing, SpellSlot::Q },
        { SDK::ChampionId::Nasus,      SpellSlot::Q },
        { SDK::ChampionId::Nautilus,   SpellSlot::W },
        { SDK::ChampionId::Nilah,      SpellSlot::E },
        { SDK::ChampionId::Olaf,       SpellSlot::W },
        { SDK::ChampionId::Pantheon,   SpellSlot::W },
        { SDK::ChampionId::Quinn,      SpellSlot::E },
        { SDK::ChampionId::RekSai,     SpellSlot::Q },
        { SDK::ChampionId::Rell,       SpellSlot::W },
        { SDK::ChampionId::Renekton,   SpellSlot::W },
        { SDK::ChampionId::Rengar,     SpellSlot::Q },
        { SDK::ChampionId::Riven,      SpellSlot::Q },
        { SDK::ChampionId::Sejuani,    SpellSlot::E },
        { SDK::ChampionId::Sett,       SpellSlot::Q },
        { SDK::ChampionId::Shyvana,    SpellSlot::Q },
        { SDK::ChampionId::Sivir,      SpellSlot::W },

        // Every normal Sylas spell grants a Petricite Burst charge.
        { SDK::ChampionId::Sylas,      SpellSlot::Q },
        { SDK::ChampionId::Sylas,      SpellSlot::W },
        { SDK::ChampionId::Sylas,      SpellSlot::E },
        { SDK::ChampionId::Sylas,      SpellSlot::R },

        { SDK::ChampionId::Talon,      SpellSlot::Q },
        { SDK::ChampionId::Trundle,    SpellSlot::Q },

        // Entering or awakening any stance accelerates Udyr's next attacks.
        { SDK::ChampionId::Udyr,       SpellSlot::Q },
        { SDK::ChampionId::Udyr,       SpellSlot::W },
        { SDK::ChampionId::Udyr,       SpellSlot::E },
        { SDK::ChampionId::Udyr,       SpellSlot::R },

        { SDK::ChampionId::Vayne,      SpellSlot::Q },
        { SDK::ChampionId::Vi,         SpellSlot::E },
        { SDK::ChampionId::Viego,      SpellSlot::W },
        { SDK::ChampionId::Volibear,   SpellSlot::Q },
        { SDK::ChampionId::XinZhao,    SpellSlot::Q },
        { SDK::ChampionId::Yorick,     SpellSlot::Q },
        { SDK::ChampionId::Zaahen,     SpellSlot::Q },
        { SDK::ChampionId::Zac,        SpellSlot::Q },
        { SDK::ChampionId::Zeri,       SpellSlot::E },
        { SDK::ChampionId::Zoe,        SpellSlot::R },
    };

    for (const auto& entry : entries) {
        if (slot == static_cast<int>(entry.Slot) &&
            championId == entry.Champion) {
            return true;
        }
    }

    return false;
}

} // namespace OrbwalkingDetail

inline bool OrbwalkerBase::IsLocalAutoAttack(const Events::ProcessSpellEventArgs& args) const {
    const auto player = GameObjects::Player();
    if (Events::IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack) return true;
        if (args.SpellName && IsAutoAttack(args.SpellName)) return true;
        if (player.IsValid() &&
            SDK::ChampionIdFromName(player.CharacterName().c_str()) == SDK::ChampionId::Akshan) {
            if (args.SpellName) {
                std::string sName = args.SpellName;
                for (auto& c : sName) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                if (sName.find("akshanpassive") != std::string::npos ||
                    sName.find("akshanpattack") != std::string::npos ||
                    sName.find("akshanattack") != std::string::npos ||
                    sName.find("akshancrit") != std::string::npos) {
                    return true;
                }
            }
        }
        return (OrbwalkingDetail::IsAzirPlayer(player) &&
                OrbwalkingDetail::IsAzirSoldierAttackEvent(args));
    }

    return OrbwalkingDetail::IsOwnedAzirSoldierSender(player, args.Sender) &&
           (args.IsAutoAttack ||
            OrbwalkingDetail::IsAzirSoldierAttackEvent(args));
}

inline OrbwalkerBase::AutoAttackResetMatch OrbwalkerBase::GetLocalAutoAttackResetMatch(
    const Events::ProcessSpellEventArgs& args
) const {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return AutoAttackResetMatch::None;
    }
    if (IsAutoAttackReset(args.SpellName)) {
        return AutoAttackResetMatch::SpellName;
    }
    const auto player = GameObjects::Player();
    if (player.IsValid()) {
        if (IsLocalAutoAttackResetSlot(args.Sender, args.Slot)) {
            return AutoAttackResetMatch::ChampionSlot;
        }
        if (args.SpellName && args.SpellName[0]) {
            for (int slot = 0; slot < 4; ++slot) {
                if (IsLocalAutoAttackResetSlot(args.Sender, slot)) {
                    auto spell = player.GetSpell(static_cast<SpellSlot>(slot));
                    if (spell.IsValid() && _stricmp(spell.Name().c_str(), args.SpellName) == 0) {
                        return AutoAttackResetMatch::ChampionSlot;
                    }
                }
            }
        }
    }
    return AutoAttackResetMatch::None;
}

inline bool OrbwalkerBase::IsLocalAutoAttackResetSlot(const ::Core::Events::ObjectInfo& sender,
                                                      int slot) const {
    if (!Events::IsLocalPlayer(sender)) {
        return false;
    }

    SDK::ChampionId championId = SDK::ChampionId::Unknown;
    const auto player = GameObjects::Player();
    if (player.IsValid()) {
        championId = SDK::ChampionIdFromName(player.CharacterName().c_str());
    } else if (sender.CharacterName) {
        championId = SDK::ChampionIdFromName(sender.CharacterName);
    }

    return OrbwalkingDetail::IsKnownAutoAttackResetSlot(championId, slot);
}

inline bool OrbwalkerBase::IsLocalAutoAttackMissile(const Events::ObjectEventArgs& args) const {
    const auto player = GameObjects::Player();
    if (Events::IsLocalPlayer(args.Source)) {
        if (args.SpellName[0] && IsAutoAttack(args.SpellName)) return true;
        if (args.MissileName[0] && IsAutoAttack(args.MissileName)) return true;
        if (player.IsValid() &&
            SDK::ChampionIdFromName(player.CharacterName().c_str()) == SDK::ChampionId::Akshan) {
            std::string mName = args.MissileName[0] ? args.MissileName : (args.SpellName[0] ? args.SpellName : "");
            for (auto& c : mName) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            if (mName.find("akshanpassive") != std::string::npos ||
                mName.find("akshanpattack") != std::string::npos ||
                mName.find("akshanattack") != std::string::npos ||
                mName.find("akshancrit") != std::string::npos) {
                return true;
            }
        }
        return (OrbwalkingDetail::IsAzirPlayer(player) &&
                OrbwalkingDetail::IsAzirSoldierAttackMissileName(args));
    }

    return OrbwalkingDetail::IsOwnedAzirSoldierSender(player, args.Source) &&
           OrbwalkingDetail::IsAzirSoldierAttackMissileName(args);
}

inline void OrbwalkerBase::OnDraw() {
    if (!menu_.Enabled()) {
        return;
    }

    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    if (menu_.DrawAARange()) {
        DrawAutoAttackRangeFade(player);
        if (OrbwalkingDetail::IsAzirPlayer(player)) {
            DrawAzirSoldierRanges(player);
        }
    } else if (menu_.DrawAzirSoldierRanges()) {
        DrawAzirSoldierRanges(player);
    }

    if (menu_.DrawExtraHoldPosition()) {
        Drawing::DrawCircle(
            player.Position(),
            player.BoundingRadius() + static_cast<float>(menu_.MovementExtraHold()),
            0xFF800080u,
            1.5f,
            48);
    }

    if (menu_.DrawAARangeEnemy()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) {
                continue;
            }
            Drawing::DrawCircle(
                enemy.Position(),
                GetRealAutoAttackRange(enemy, player),
                0xFF00BFFFu,
                1.5f,
                64);
        }
    }

    DrawFakeVisuals();

    if (!menu_.DrawKillableMinion()) {
        return;
    }

    ReadAttackTimingsFromMemory(player);
    const int farmDelay = menu_.DelayFarm();
    const int lastHitWindowHorizonMs = std::clamp(
        static_cast<int>(OrbwalkingDetail::kLaneClearWaitCycles *
            (context_.attackDelayMs + context_.attackWindupMs)),
        500,
        2500);
    const OrbwalkingDetail::CritAttackPrediction critPrediction =
        OrbwalkingDetail::BuildCritAttackPrediction(player, context_.critSequence);

    int stableOrder = 0;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!OrbwalkingDetail::IsValidMinionTarget(minion) ||
            !OrbwalkingDetail::IsTargetWithinCurrentAttackRange(
                player, AttackableUnit(minion.Handle()), 2.0f)) {
            continue;
        }

        const float damage = OrbwalkingDetail::PredictedLastHitDamage(
            player, minion, critPrediction);
        if (damage <= 0.0f) {
            continue;
        }

        if (menu_.DrawKillableMinionFade()) {
            if (minion.Health() >= damage * 2.0f) {
                continue;
            }
            const int blue = static_cast<int>(std::clamp(255.0f - minion.Health() * 2.0f, 0.0f, 255.0f));
            Drawing::DrawCircle(
                minion.Position(),
                minion.BoundingRadius() * 2.0f,
                0xFF00FF00u | static_cast<std::uint32_t>(blue),
                1.5f,
                32);
        } else if (OrbwalkingDetail::EvaluateLastHitMinion(
                       player,
                       minion,
                       critPrediction,
                       farmDelay,
                       lastHitWindowHorizonMs,
                       stableOrder++).window.valid) {
            Drawing::DrawCircle(
                minion.Position(),
                minion.BoundingRadius() * 2.0f,
                0xFF00FF00u,
                1.5f,
                32);
        }
    }
}

inline void OrbwalkerBase::OnPlayAnimationStatic(const Events::PlayAnimationEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnPlayAnimation(args);
    }
}

inline void OrbwalkerBase::OnPlayAnimation(const Events::PlayAnimationEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Events::IsLocalPlayer(args.Sender)) {
        return;
    }

    if (SDK::ChampionIdFromName(player.CharacterName().c_str()) == SDK::ChampionId::Rengar) {
        if (_stricmp(args.Animation, "Spell5") == 0) {
            const int now = Tick();
            const float dist = context_.lastTarget.IsValid() ? player.Distance(context_.lastTarget) : 0.0f;
            const float flightDelayMs = (std::min)(dist / 1.5f, 600.0f);
            context_.lastAutoAttackTick = static_cast<int>(now - OneWayPingMs() * 0.5f + flightDelayMs);
        }
    }
}

inline void OrbwalkerBase::OnDashStatic(const Events::Dash::DashArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnDash(args);
    }
}

inline void OrbwalkerBase::OnDash(const Events::Dash::DashArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || args.NetworkId != player.NetworkId()) {
        return;
    }

    if (SDK::ChampionIdFromName(player.CharacterName().c_str()) == SDK::ChampionId::Rengar) {
        const int now = Tick();
        if (now - context_.lastRengarLeapTick < 100) {
            return;
        }
        context_.lastRengarLeapTick = now;

        const float dist = context_.lastTarget.IsValid() ? player.Distance(context_.lastTarget) : 0.0f;
        const float flightDelayMs = (std::min)(dist / 1.5f, 600.0f);
        context_.lastAutoAttackTick = static_cast<int>(now - OneWayPingMs() * 0.5f + flightDelayMs);

        const AttackableUnit eventTarget = context_.lastTarget.IsValid() ? context_.lastTarget : AttackableUnit();
        const std::string debugAfterAttackSpellName = context_.lastAttackSpellName;
        SDK::Utils::DelayAction::Add(flightDelayMs, [this, eventTarget, debugAfterAttackSpellName]() {
            if (eventTarget.IsValid()) {
                LogAfterAttackDebug(eventTarget, &debugAfterAttackSpellName);
                OrbwalkingActionArgs afterArgs(
                    OrbwalkingType::AfterAttack,
                    eventTarget,
                    eventTarget.Position(),
                    "Kuro");
                OrbwalkingDetail::FireAfterAttack(afterArgs);
            }
        });
    }
}

} // namespace OrbwalkerKuro
