#pragma once

using namespace ::SDK;

namespace OrbwalkerKuro {

inline void OrbwalkerBase::OnGameUpdateStatic(const Events::GameUpdateEventArgs& args) {
    (void)args;
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnGameUpdate();
    }
}

inline void OrbwalkerBase::OnProcessSpellStatic(const Events::ProcessSpellEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnProcessSpell(args);
    }
}

inline void OrbwalkerBase::OnDoCastStatic(const Events::ProcessSpellEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnDoCast(args);
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

inline void OrbwalkerBase::OnDebugDrawStatic() {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnDebugDraw();
    }
}

inline void OrbwalkerBase::OnGameUpdate() {
    if (!menu_.Enabled()) {
        ClearPendingAttackState();
        context_.activeMode = OrbwalkingMode::None;
        return;
    }

    context_.activeMode = ActiveMode();

    if (context_.activeMode == OrbwalkingMode::None) {
        ClearPendingAttackState();
        return;
    }

    const int now = Tick();

    while (!context_.pendingProcessSpellList.empty()) {
        if (now - context_.pendingProcessSpellList.front().processTick > 1000) {
            auto expired = context_.pendingProcessSpellList.front();
            context_.pendingProcessSpellList.erase(context_.pendingProcessSpellList.begin());

            char logMsg[224];
            std::snprintf(logMsg, sizeof(logMsg),
                "[EVENT_LOG][EXPIRED] Tick:%d | OnProcessSpell Expired (>1000ms without OnDoCast) | Spell:'%s' | TargetNetID:0x%X",
                now, expired.spellName.c_str(), expired.targetNetworkId);
            DebugPrint(logMsg);
            ::OutputDebugStringA(logMsg);
            ::OutputDebugStringA("\n");
        } else {
            break;
        }
    }

    if (EvadeOwnsActions(now)) {
        ExpirePendingAttack();
        if (menu_.CoordinateKuroEvade() &&
            Plugins::KuroCombatCoordination::Coordinator::
                AllowsStationaryAttacks(now)) {

            const AttackableUnit stationaryTarget =
                CanAttack() ? GetTarget() : AttackableUnit();

            if (stationaryTarget.IsValid()) {
                Attack(stationaryTarget);
            }
        }
        return;
    }

    const Vector3 position = context_.orbwalkerPosition.IsZero() ? Game::CursorPos() : context_.orbwalkerPosition;
    const AttackableUnit target = CanAttack() ? GetTarget() : AttackableUnit();
    Orbwalk(target, position);
}

inline void OrbwalkerBase::OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    const bool isAttack = IsLocalAutoAttack(args);
    const bool isAttackReset = IsLocalAutoAttackReset(args);

    if (isAttackReset && !isAttack) {
        const int now = Tick();
        if (now - context_.lastAutoAttackResetTick >= 150) {
            ResetAutoAttackTimer();
        }
        return;
    }

    if (!isAttack) {
        return;
    }

    const int now = Tick();

    // Log ADD event to list
    ProcessSpellLogEntry logEntry;
    logEntry.processTick = now;
    logEntry.spellName = args.SpellName ? args.SpellName : "";
    logEntry.targetNetworkId = args.TargetNetworkId;
    context_.pendingProcessSpellList.push_back(logEntry);

    char logMsg[224];
    std::snprintf(logMsg, sizeof(logMsg),
        "[EVENT_LOG][ADD] Tick:%d | OnProcessSpell | Spell:'%s' | TargetNetID:0x%X | QueueSize:%zu",
        now, logEntry.spellName.c_str(), logEntry.targetNetworkId, context_.pendingProcessSpellList.size());
    DebugPrint(logMsg);
    ::OutputDebugStringA(logMsg);
    ::OutputDebugStringA("\n");

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
    const int attackStartTick = hadPendingAttack
        ? (isAzirSoldierAttack
            ? context_.pendingAttackTick + static_cast<int>(OneWayPingMs())
            : std::max(
                context_.pendingAttackTick + static_cast<int>(OneWayPingMs()),
                now))
        : now;
    const AttackableUnit target = ResolveAttackTarget(args);
    if (target.IsValid()) {
        context_.lastTarget = target;
    }

    if (!hadPendingAttack &&
        context_.hasConfirmedAttack &&
        context_.lastAutoAttackTick > 0 &&
        now - context_.lastAttackConfirmTick <= kDuplicateAttackEventMs) {
        ReadAttackTimingsFromMemory(player);
        return;
    }

    context_.lastAutoAttackTick = attackStartTick;
    context_.lastAttackConfirmTick = now;
    context_.pendingAttack = false;
    context_.pendingAttackTick = 0;
    context_.pendingAttackTargetNetworkId = 0;
    context_.lastAutoAttackResetTick = 0;
    context_.hasConfirmedAttack = true;
    context_.attackCastComplete = false;
    ReadAttackTimingsFromMemory(player);

    if (player.IsValid() && _stricmp(player.CharacterName().c_str(), "Akshan") == 0) {
        std::string spellNameStr = args.SpellName;
        for (auto& c : spellNameStr) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
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
    OrbwalkingActionArgs attackArgs(
        OrbwalkingType::OnAttack,
        eventTarget,
        eventTarget.IsValid() ? eventTarget.Position() : Vector3(),
        "Kuro");
    OrbwalkingDetail::FireOnAttack(attackArgs);
}

inline void OrbwalkerBase::OnDoCast(const Events::ProcessSpellEventArgs& args) {
    const bool isAttack = IsLocalAutoAttack(args);
    const bool isAttackReset = IsLocalAutoAttackReset(args);
    if (!isAttack) {
        if (isAttackReset) {
            const int now = Tick();
            if (now - context_.lastAutoAttackResetTick >= 150) {
                ResetAutoAttackTimer();
            }
        }
        return;
    }

    const int now = Tick();

    // Log REMOVE event from list
    if (!context_.pendingProcessSpellList.empty()) {
        auto entry = context_.pendingProcessSpellList.front();
        context_.pendingProcessSpellList.erase(context_.pendingProcessSpellList.begin());

        int delayMs = now - entry.processTick;
        char logMsg[224];
        std::snprintf(logMsg, sizeof(logMsg),
            "[EVENT_LOG][REMOVE] Tick:%d | OnDoCast | Spell:'%s' | TargetNetID:0x%X | ProcessTick:%d (Delay:%dms) | QueueSize:%zu",
            now, entry.spellName.c_str(), entry.targetNetworkId, entry.processTick, delayMs, context_.pendingProcessSpellList.size());
        DebugPrint(logMsg);
        ::OutputDebugStringA(logMsg);
        ::OutputDebugStringA("\n");
    } else {
        char logMsg[224];
        std::snprintf(logMsg, sizeof(logMsg),
            "[EVENT_LOG][REMOVE_FAIL] Tick:%d | OnDoCast (No preceding OnProcessSpell in list!) | Spell:'%s' | TargetNetID:0x%X",
            now, args.SpellName ? args.SpellName : "", args.TargetNetworkId);
        DebugPrint(logMsg);
        ::OutputDebugStringA(logMsg);
        ::OutputDebugStringA("\n");
    }

    if (!context_.pendingAttack &&
        context_.lastAutoAttackResetTick > 0 &&
        now - context_.lastAutoAttackResetTick >= 0 &&
        now - context_.lastAutoAttackResetTick <= PendingAttackTimeoutMs()) {
        return;
    }

    ReadAttackTimingsFromMemory(GameObjects::Player());
    const int estimatedAttackStartTick = now - static_cast<int>(context_.attackWindupMs);
    const int attackStartTick = context_.pendingAttack
        ? std::max(
            context_.pendingAttackTick + static_cast<int>(OneWayPingMs()),
            estimatedAttackStartTick)
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

    if (attackStartTick > 0) {
        context_.lastAutoAttackTick = attackStartTick;
    } else if (context_.lastAutoAttackTick <= 0 || now - context_.lastAutoAttackTick > 300) {
        context_.lastAutoAttackTick = std::max(0, now - static_cast<int>(context_.attackWindupMs));
    }
    CheckAfterAttack();
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
    OrbwalkingActionArgs afterArgs(
        OrbwalkingType::AfterAttack,
        eventTarget,
        eventTarget.IsValid() ? eventTarget.Position() : Vector3(),
        "Kuro");
    OrbwalkingDetail::FireAfterAttack(afterArgs);
    context_.lastAfterAttackStartTick = context_.lastAutoAttackTick;
}

namespace OrbwalkingDetail {

struct AutoAttackResetSlotEntry {
    const char* ChampionName;
    SpellSlot Slot;
};

inline bool IsKnownAutoAttackResetSlot(
    const std::string& championName,
    int slot
) {
    if (championName.empty()) {
        return false;
    }

    static constexpr AutoAttackResetSlotEntry entries[] = {
        { "Aatrox",     SpellSlot::E },
        { "Ashe",       SpellSlot::Q },
        { "Belveth",    SpellSlot::Q },
        { "Blitzcrank", SpellSlot::E },
        { "Briar",      SpellSlot::Q },
        { "Briar",      SpellSlot::W },
        { "Camille",    SpellSlot::Q },
        { "Chogath",    SpellSlot::E },
        { "Darius",     SpellSlot::W },
        { "DrMundo",    SpellSlot::E },
        { "Ekko",       SpellSlot::E },
        { "Fiora",      SpellSlot::E },
        { "Fizz",       SpellSlot::W },
        { "Garen",      SpellSlot::Q },
        { "Graves",     SpellSlot::E },
        { "Gwen",       SpellSlot::E },
        { "Hecarim",    SpellSlot::E },
        { "Illaoi",     SpellSlot::W },
        { "Jax",        SpellSlot::W },
        { "Kaisa",      SpellSlot::R },
        { "Kassadin",   SpellSlot::W },
        { "Katarina",   SpellSlot::E },
        { "Kayle",      SpellSlot::E },
        { "Kindred",    SpellSlot::Q },
        { "KSante",     SpellSlot::Q },
        { "Leona",      SpellSlot::Q },

        // Lucian E resets/accelerates his attack sequence directly.
        { "Lucian", SpellSlot::Q },
        { "Lucian", SpellSlot::W },
        { "Lucian", SpellSlot::E },
        { "Lucian", SpellSlot::R },

        { "Malphite",   SpellSlot::W },
        { "MasterYi",   SpellSlot::W },
        { "MonkeyKing", SpellSlot::Q },
        { "Nasus",      SpellSlot::Q },
        { "Nautilus",   SpellSlot::W },
        { "Nilah",      SpellSlot::E },
        { "Olaf",       SpellSlot::W },
        { "Pantheon",   SpellSlot::W },
        { "Quinn",      SpellSlot::E },
        { "RekSai",     SpellSlot::Q },
        { "Rell",       SpellSlot::W },
        { "Renekton",   SpellSlot::W },
        { "Rengar",     SpellSlot::Q },
        { "Riven",      SpellSlot::Q },
        { "Sejuani",    SpellSlot::E },
        { "Sett",       SpellSlot::Q },
        { "Shyvana",    SpellSlot::Q },
        { "Sivir",      SpellSlot::W },

        // Every normal Sylas spell grants a Petricite Burst charge.
        { "Sylas",      SpellSlot::Q },
        { "Sylas",      SpellSlot::W },
        { "Sylas",      SpellSlot::E },
        { "Sylas",      SpellSlot::R },

        { "Talon",      SpellSlot::Q },
        { "Trundle",    SpellSlot::Q },

        // Entering or awakening any stance accelerates Udyr's next attacks.
        { "Udyr",       SpellSlot::Q },
        { "Udyr",       SpellSlot::W },
        { "Udyr",       SpellSlot::E },
        { "Udyr",       SpellSlot::R },

        { "Vayne",      SpellSlot::Q },
        { "Vi",         SpellSlot::E },
        { "Viego",      SpellSlot::W },
        { "Volibear",   SpellSlot::Q },
        { "XinZhao",    SpellSlot::Q },
        { "Yorick",     SpellSlot::Q },
        { "Zaahen",     SpellSlot::Q },
        { "Zac",        SpellSlot::Q },
        { "Zeri",       SpellSlot::E },
        { "Zoe",        SpellSlot::R },
    };

    for (const auto& entry : entries) {
        if (slot == static_cast<int>(entry.Slot) &&
            _stricmp(championName.c_str(), entry.ChampionName) == 0) {
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
        if (player.IsValid() && _stricmp(player.CharacterName().c_str(), "Akshan") == 0) {
            std::string sName = args.SpellName;
            for (auto& c : sName) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            if (sName.find("akshanpassive") != std::string::npos ||
                sName.find("akshanpattack") != std::string::npos) {
                return true;
            }
        }
        return (OrbwalkingDetail::IsAzirPlayer(player) &&
                OrbwalkingDetail::IsAzirSoldierAttackEvent(args));
    }

    return OrbwalkingDetail::IsOwnedAzirSoldierSender(player, args.Sender) &&
           (args.IsAutoAttack ||
            OrbwalkingDetail::IsAzirSoldierAttackEvent(args));
}

inline bool OrbwalkerBase::IsLocalAutoAttackReset(const Events::ProcessSpellEventArgs& args) const {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return false;
    }
    return IsAutoAttackReset(args.SpellName) ||
           IsLocalAutoAttackResetSlot(args.Sender, args.Slot);
}

inline bool OrbwalkerBase::IsLocalAutoAttackResetSlot(const ::Core::Events::ObjectInfo& sender,
                                                      int slot) const {
    if (!Events::IsLocalPlayer(sender)) {
        return false;
    }

    std::string championName;
    const auto player = GameObjects::Player();
    if (player.IsValid()) {
        championName = player.CharacterName();
    }
    if (championName.empty()) {
        championName = sender.CharacterName;
    }

    return OrbwalkingDetail::IsKnownAutoAttackResetSlot(championName, slot);
}

inline bool OrbwalkerBase::IsLocalAutoAttackMissile(const Events::ObjectEventArgs& args) const {
    const auto player = GameObjects::Player();
    if (Events::IsLocalPlayer(args.Source)) {
        return IsAutoAttack(args.SpellName) || IsAutoAttack(args.MissileName) ||
               (OrbwalkingDetail::IsAzirPlayer(player) &&
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

    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!OrbwalkingDetail::IsValidMinionTarget(minion) ||
            !OrbwalkingDetail::IsTargetWithinCurrentAttackRange(
                player, AttackableUnit(minion.Handle()), 2.0f)) {
            continue;
        }

        const float damage =
            OrbwalkingDetail::GetCurrentAutoAttackDamage(player, minion);
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
        } else if (OrbwalkingDetail::CanLastHitMinion(player, minion, menu_.DelayFarm())) {
            Drawing::DrawCircle(
                minion.Position(),
                minion.BoundingRadius() * 2.0f,
                0xFF00FF00u,
                1.5f,
                32);
        }
    }
}

inline void OrbwalkerBase::OnDebugDraw() {
    DrawLiveAttackDebugOverlay();
}

inline void OrbwalkerBase::PushDebugConsoleLine(const char* text, int tick) {
    const int index = context_.debugConsoleNextLine;
    strncpy_s(
        context_.debugConsoleLines[index].text,
        text ? text : "",
        _TRUNCATE);
    context_.debugConsoleLines[index].tick = tick;

    context_.debugConsoleNextLine =
        (context_.debugConsoleNextLine + 1) % kOrbwalkerDebugConsoleMaxLines;
    if (context_.debugConsoleLineCount < kOrbwalkerDebugConsoleMaxLines) {
        ++context_.debugConsoleLineCount;
    }
}

inline void OrbwalkerBase::DebugPrint(const char* text) {
    if (!text || !text[0]) {
        return;
    }

    const int now = static_cast<int>(::GetTickCount());
    char line[kOrbwalkerDebugConsoleLineLength] = {};
    int length = 0;

    for (const char* cursor = text;; ++cursor) {
        const char ch = *cursor;
        if (ch == '\0') {
            if (length > 0) {
                line[length] = '\0';
                PushDebugConsoleLine(line, now);
            }
            break;
        }

        if (ch == '\r' || ch == '\n') {
            line[length] = '\0';
            PushDebugConsoleLine(line, now);
            length = 0;
            if (ch == '\r' && cursor[1] == '\n') {
                ++cursor;
            }
            continue;
        }

        const unsigned char value = static_cast<unsigned char>(ch);
        line[length++] = (value >= 0x20 && value <= 0x7E) ? ch : '?';
        if (length + 1 >= kOrbwalkerDebugConsoleLineLength) {
            line[length] = '\0';
            PushDebugConsoleLine(line, now);
            length = 0;
        }
    }
}

inline void OrbwalkerBase::ClearDebugConsole() {
    for (int i = 0; i < kOrbwalkerDebugConsoleMaxLines; ++i) {
        context_.debugConsoleLines[i].text[0] = '\0';
        context_.debugConsoleLines[i].tick = 0;
    }
    context_.debugConsoleNextLine = 0;
    context_.debugConsoleLineCount = 0;
}

inline void OrbwalkerBase::DrawLiveAttackDebugOverlay() {
    if (!menu_.DrawLiveDebugConsole()) {
        return;
    }

    if (!ImGui::GetCurrentContext()) {
        return;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    if (!draw) {
        return;
    }

    const int now = static_cast<int>(::GetTickCount());
    const int visibleLines = std::max(
        kOrbwalkerDebugConsoleDefaultVisibleLines,
        std::min(menu_.DrawLiveDebugConsoleLines(), 24));
    const float fontSize = ImGui::GetFontSize();
    const float lineHeight = std::max(15.0f, fontSize + 2.0f);
    const float padding = 7.0f;
    const float headerHeight = lineHeight + 2.0f;
    float width = 720.0f;
    const Vec2 rendererSize = Drawing::GetRendererSize();
    if (rendererSize.x > 0.0f) {
        width = std::min(width, std::max(360.0f, rendererSize.x - 36.0f));
    }

    const ImVec2 pos(18.0f, 108.0f);
    const ImVec2 boxMin(pos.x - padding, pos.y - padding);
    const ImVec2 boxMax(
        boxMin.x + width,
        boxMin.y + padding * 2.0f + headerHeight + lineHeight * visibleLines);

    draw->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 145), 4.0f);
    draw->AddRect(boxMin, boxMax, IM_COL32(255, 209, 102, 160), 4.0f);

    char title[96] = {};
    _snprintf_s(
        title,
        sizeof(title),
        _TRUNCATE,
        "Orbwalker Live Debug Console  %d/%d",
        context_.debugConsoleLineCount,
        kOrbwalkerDebugConsoleMaxLines);
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 230), title);
    draw->AddText(pos, IM_COL32(255, 209, 102, 255), title);

    const int storedLines = context_.debugConsoleLineCount;
    const int linesToDraw = std::min(storedLines, visibleLines);
    const int emptyRows = visibleLines - linesToDraw;
    const int oldestIndex =
        (context_.debugConsoleNextLine - storedLines + kOrbwalkerDebugConsoleMaxLines) %
        kOrbwalkerDebugConsoleMaxLines;
    const int firstRelativeLine = storedLines - linesToDraw;

    draw->PushClipRect(
        ImVec2(boxMin.x + 4.0f, pos.y + headerHeight - 1.0f),
        ImVec2(boxMax.x - 4.0f, boxMax.y - 4.0f),
        true);

    if (storedLines == 0) {
        const ImVec2 waitingPos(pos.x, pos.y + headerHeight + lineHeight * (visibleLines - 1));
        const char* waiting = "waiting for debug output...";
        draw->AddText(ImVec2(waitingPos.x + 1.0f, waitingPos.y + 1.0f), IM_COL32(0, 0, 0, 220), waiting);
        draw->AddText(waitingPos, IM_COL32(184, 231, 255, 230), waiting);
        draw->PopClipRect();
        return;
    }

    for (int row = emptyRows; row < visibleLines; ++row) {
        const int relativeLine = firstRelativeLine + (row - emptyRows);
        const int lineIndex =
            (oldestIndex + relativeLine) % kOrbwalkerDebugConsoleMaxLines;
        const OrbwalkerDebugConsoleLine& entry = context_.debugConsoleLines[lineIndex];
        const int age = entry.tick > 0 ? now - entry.tick : 999999;
        const ImU32 color = age >= 0 && age <= 4000
            ? IM_COL32(255, 244, 204, 255)
            : IM_COL32(210, 230, 238, 225);
        const ImVec2 linePos(pos.x, pos.y + headerHeight + lineHeight * row);
        draw->AddText(ImVec2(linePos.x + 1.0f, linePos.y + 1.0f), IM_COL32(0, 0, 0, 220), entry.text);
        draw->AddText(linePos, color, entry.text);
    }

    draw->PopClipRect();
}

} // namespace OrbwalkerKuro
