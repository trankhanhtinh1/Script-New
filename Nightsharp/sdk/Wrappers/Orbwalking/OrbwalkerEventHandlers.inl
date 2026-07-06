#pragma once

namespace SDK {

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

inline void OrbwalkerBase::OnProcessCastSpellStatic(const Events::CastSpellEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnProcessCastSpell(args);
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

    const Vector3 position = context_.orbwalkerPosition.IsZero() ? Game::CursorPos() : context_.orbwalkerPosition;
    const AttackableUnit target = CanAttack() ? GetTarget() : AttackableUnit();
    Orbwalk(target, position);
}

inline void OrbwalkerBase::OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    CaptureLiveSpellDebug(args, "ProcessSpell");

    if (IsLocalAutoAttackResetSlot(args.Sender, args.Slot)) {
        ResetAutoAttackTimer();
        return;
    }

    const bool isAttack = IsLocalAutoAttack(args);
    const bool isAttackReset = IsLocalAutoAttackReset(args);

    if (isAttackReset) {
        ResetAutoAttackTimer();
        return;
    }

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
    const int attackStartTick = hadPendingAttack
        ? std::max(context_.pendingAttackTick + static_cast<int>(OneWayPingMs()), now)
        : now;
    const AttackableUnit target = ResolveAttackTarget(args);
    if (target.IsValid()) {
        context_.lastTarget = target;
    }

    if (!hadPendingAttack &&
        context_.hasConfirmedAttack &&
        context_.lastAutoAttackTick > 0 &&
        now - context_.lastAttackConfirmTick <= kDuplicateAttackEventMs) {
        SnapshotAttackTimings(GameObjects::Player());
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
    SnapshotAttackTimings(GameObjects::Player());

    const AttackableUnit eventTarget = target.IsValid() ? target : context_.lastTarget;
    OrbwalkingActionArgs attackArgs(
        OrbwalkingType::OnAttack,
        eventTarget,
        eventTarget.IsValid() ? eventTarget.Position() : Vector3(),
        "SDK");
    OrbwalkingDetail::FireOnAttack(attackArgs);
}

inline void OrbwalkerBase::OnProcessCastSpell(const Events::CastSpellEventArgs& args) {
    if (IsLocalAutoAttackResetSlot(args.Sender, args.Slot)) {
        ResetAutoAttackTimer();
    }
}

inline void OrbwalkerBase::OnDoCast(const Events::ProcessSpellEventArgs& args) {
    CaptureLiveSpellDebug(args, "DoCast");

    if (IsLocalAutoAttackResetSlot(args.Sender, args.Slot)) {
        ResetAutoAttackTimer();
        return;
    }

    const bool isAttackReset = IsLocalAutoAttackReset(args);
    if (!IsLocalAutoAttack(args)) {
        if (isAttackReset) {
            ResetAutoAttackTimer();
        }
        return;
    }

    const int now = Tick();
    if (!context_.pendingAttack &&
        context_.lastAutoAttackResetTick > 0 &&
        now - context_.lastAutoAttackResetTick >= 0 &&
        now - context_.lastAutoAttackResetTick <= PendingAttackTimeoutMs()) {
        return;
    }

    SnapshotAttackTimings(GameObjects::Player());
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
    context_.attackCastComplete = true;
    if (context_.lastAttackRequiresDoCastBeforeMove) {
        context_.lastAttackDoCastComplete = true;
        context_.lastAttackDoCastWaitTick = 0;
    }

    if (attackStartTick > 0) {
        context_.lastAutoAttackTick = attackStartTick;
    } else if (context_.lastAutoAttackTick <= 0 || now - context_.lastAutoAttackTick > 300) {
        context_.lastAutoAttackTick = std::max(0, now - static_cast<int>(context_.attackWindupMs));
    }

    if (context_.lastAutoAttackTick > 0 &&
        context_.lastAfterAttackStartTick != context_.lastAutoAttackTick) {
        const AttackableUnit eventTarget = target.IsValid() ? target : context_.lastTarget;
        OrbwalkingActionArgs afterArgs(
            OrbwalkingType::AfterAttack,
            eventTarget,
            eventTarget.IsValid() ? eventTarget.Position() : Vector3(),
            "SDK");
        OrbwalkingDetail::FireAfterAttack(afterArgs);
        context_.lastAfterAttackStartTick = context_.lastAutoAttackTick;
    }

    if (isAttackReset) {
        ResetAutoAttackTimer();
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
    const bool stoppedPending =
        context_.pendingAttack &&
        now - context_.pendingAttackTick <= PendingAttackTimeoutMs();

    SnapshotAttackTimings(GameObjects::Player());
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

inline bool OrbwalkerBase::IsLocalAutoAttack(const Events::ProcessSpellEventArgs& args) const {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return false;
    }
    return args.IsAutoAttack;
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
    if (!Events::IsLocalPlayer(sender) ||
        slot != static_cast<int>(SpellSlot::Q)) {
        return false;
    }

    const auto player = GameObjects::Player();
    if (player.IsValid()) {
        return player.CharacterName() == "Rengar";
    }

    return std::string(sender.CharacterName) == "Rengar";
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
        Drawing::DrawCircle(
            player.Position(),
            Utils::AutoAttack::GetRealAutoAttackRange(player),
            0xFF00BFFFu,
            1.5f,
            64);
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
                Utils::AutoAttack::GetRealAutoAttackRange(enemy, player),
                0xFF00BFFFu,
                1.5f,
                64);
        }
    }

    if (!menu_.DrawKillableMinion()) {
        return;
    }

    const float range = Utils::AutoAttack::GetRealAutoAttackRange(player) * 2.0f;
    const float rangeSqr = range * range;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!OrbwalkingDetail::IsValidMinionTarget(minion) ||
            player.Position().DistanceSqr2D(minion.Position()) > rangeSqr) {
            continue;
        }

        const float damage = Damage::GetAutoAttackDamage(player, minion);
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

inline void OrbwalkerBase::CaptureLiveSpellDebug(const Events::ProcessSpellEventArgs& args,
                                                 const char* source) {
    auto isDebugTextUsable = [](const char* value) -> bool {
        if (!value || !value[0]) {
            return false;
        }

        int length = 0;
        int questionMarks = 0;
        int lettersOrDigits = 0;
        for (; value[length] && length < 95; ++length) {
            const unsigned char ch = static_cast<unsigned char>(value[length]);
            if (ch < 0x20 || ch > 0x7E) {
                return false;
            }
            if (ch == '?') {
                ++questionMarks;
            }
            if ((ch >= '0' && ch <= '9') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= 'a' && ch <= 'z')) {
                ++lettersOrDigits;
            }
        }

        return length >= 2 &&
               length < 95 &&
               lettersOrDigits > 0 &&
               questionMarks * 2 < length;
    };

    const char* attackName = nullptr;
    const char* nameCandidates[] = {
        args.SpellName,
        args.SpellSlotName,
        args.ScriptName,
        args.MissileName,
        args.PayloadSpellName,
        args.PayloadMissileName,
    };
    for (const char* candidate : nameCandidates) {
        if (isDebugTextUsable(candidate)) {
            attackName = candidate;
            break;
        }
    }
    if (!attackName && (args.IsAutoAttack || args.Slot == 64)) {
        attackName = "BasicAttack";
    }
    if (!attackName) {
        attackName = "<invalid>";
    }

    const char* senderName = isDebugTextUsable(args.Sender.CharacterName)
        ? args.Sender.CharacterName
        : (isDebugTextUsable(args.Sender.Name) ? args.Sender.Name : "<invalid>");

    strncpy_s(
        context_.lastProcessAttackName,
        attackName,
        _TRUNCATE);
    strncpy_s(
        context_.lastProcessAttackMissileName,
        args.MissileName,
        _TRUNCATE);
    strncpy_s(
        context_.lastProcessAttackScriptName,
        args.ScriptName,
        _TRUNCATE);
    strncpy_s(
        context_.lastProcessAttackSlotName,
        args.SpellSlotName,
        _TRUNCATE);
    strncpy_s(
        context_.lastProcessAttackSource,
        source ? source : "",
        _TRUNCATE);
    strncpy_s(
        context_.lastProcessAttackSenderName,
        isDebugTextUsable(args.Sender.Name) ? args.Sender.Name : "",
        _TRUNCATE);
    strncpy_s(
        context_.lastProcessAttackSenderCharacterName,
        senderName,
        _TRUNCATE);
    context_.lastProcessAttackSlot = args.Slot;
    context_.lastProcessAttackSenderValid = args.Sender.IsValid();
    context_.lastProcessAttackIsLocal = Events::IsLocalPlayer(args.Sender);
    context_.lastProcessAttackIsAuto = args.IsAutoAttack;
    context_.lastProcessAttackNameTick = static_cast<int>(::GetTickCount());
}

inline void OrbwalkerBase::DrawLiveAttackDebugOverlay() {
    if (!ImGui::GetCurrentContext()) {
        return;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    if (!draw) {
        return;
    }

    const int now = static_cast<int>(::GetTickCount());
    const bool hasAttack = context_.lastProcessAttackNameTick > 0 &&
        now - context_.lastProcessAttackNameTick >= 0;
    const int attackAge = hasAttack ? now - context_.lastProcessAttackNameTick : 0;
    const bool freshAttack = hasAttack && attackAge <= 4000;

    char line[192] = {};
    if (hasAttack) {
        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "%s: %s (%dms)",
            context_.lastProcessAttackSource[0] ? context_.lastProcessAttackSource : "SpellEvent",
            context_.lastProcessAttackName[0] ? context_.lastProcessAttackName : "<empty>",
            attackAge);
    } else {
        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "ProcessSpell attack: waiting...");
    }

    const ImVec2 pos(18.0f, 108.0f);
    const ImVec2 boxMin(pos.x - 6.0f, pos.y - 5.0f);
    const ImVec2 boxMax(pos.x + 520.0f, pos.y + 68.0f);
    draw->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 125), 4.0f);
    draw->AddRect(boxMin, boxMax, IM_COL32(255, 209, 102, freshAttack ? 210 : 95), 4.0f);
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 220), line);
    draw->AddText(pos, freshAttack ? IM_COL32(255, 209, 102, 255) : IM_COL32(230, 230, 230, 255), line);

    char flagsLine[192] = {};
    if (hasAttack) {
        _snprintf_s(
            flagsLine,
            sizeof(flagsLine),
            _TRUNCATE,
            "slot=%d local=%d aa=%d senderValid=%d sender=%s",
            context_.lastProcessAttackSlot,
            context_.lastProcessAttackIsLocal ? 1 : 0,
            context_.lastProcessAttackIsAuto ? 1 : 0,
            context_.lastProcessAttackSenderValid ? 1 : 0,
            context_.lastProcessAttackSenderCharacterName[0]
                ? context_.lastProcessAttackSenderCharacterName
                : "<empty>");
    }

    const char* secondLine = hasAttack ? flagsLine : nullptr;
    char missileLine[192] = {};
    if (hasAttack && context_.lastProcessAttackMissileName[0]) {
        _snprintf_s(
            missileLine,
            sizeof(missileLine),
            _TRUNCATE,
            "Missile: %s",
            context_.lastProcessAttackMissileName);
    } else if (!hasAttack) {
        secondLine = "waiting for local auto attack ProcessSpell";
    }

    if (secondLine && secondLine[0]) {
        const ImVec2 linePos(18.0f, 123.0f);
        draw->AddText(ImVec2(linePos.x + 1.0f, linePos.y + 1.0f), IM_COL32(0, 0, 0, 220), secondLine);
        draw->AddText(linePos, IM_COL32(184, 231, 255, 255), secondLine);
    }

    if (hasAttack && missileLine[0]) {
        const ImVec2 missilePos(18.0f, 138.0f);
        draw->AddText(ImVec2(missilePos.x + 1.0f, missilePos.y + 1.0f), IM_COL32(0, 0, 0, 220), missileLine);
        draw->AddText(missilePos, IM_COL32(184, 231, 255, 255), missileLine);
    } else if (hasAttack && context_.lastProcessAttackScriptName[0]) {
        char scriptLine[192] = {};
        _snprintf_s(
            scriptLine,
            sizeof(scriptLine),
            _TRUNCATE,
            "Script: %s",
            context_.lastProcessAttackScriptName);
        const ImVec2 scriptPos(18.0f, 138.0f);
        draw->AddText(ImVec2(scriptPos.x + 1.0f, scriptPos.y + 1.0f), IM_COL32(0, 0, 0, 220), scriptLine);
        draw->AddText(scriptPos, IM_COL32(184, 231, 255, 255), scriptLine);
    }
}

} // namespace SDK
