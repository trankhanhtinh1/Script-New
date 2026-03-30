#pragma once

// ============================================================================
// Events Hub — Central dispatcher for all SDK events
//
// Mirrors EnsoulSharp.SDK.Core.Events.Events (partial class)
// ALL 15 events matching 1:1 with EnsoulSharp ✅
//
// Usage for champion scripts:
//   SDK::Events::AddOnProcessSpellCast(handler);   // AIBaseClient.OnProcessSpellCast
//   SDK::Events::AddOnDoCast(handler);              // AIBaseClient.OnDoCast
//   SDK::Events::AddOnStopCast(handler);            // Spellbook.OnStopCast
//   SDK::Events::AddOnNewPath(handler);             // AIBaseClient.OnNewPath
//   SDK::Events::AddOnPlayAnimation(handler);       // AIBaseClient.OnPlayAnimation
//   SDK::Events::AddOnAssign(handler);              // GameObject.OnAssign
//   SDK::Events::AddOnDelete(handler);              // GameObject.OnDelete
//   SDK::Events::AddOnIntegerPropertyChange(handler); // GameObject.OnIntegerPropertyChange
//   SDK::Events::AddOnDash(handler);                // Events.OnDash
//   SDK::Events::AddOnStealth(handler);             // Events.OnStealth
//   SDK::Events::AddOnTeleport(handler);            // Events.OnTeleport
//   SDK::Events::AddOnTurretAttack(handler);        // Events.OnTurretAttack
//   SDK::Events::AddOnGapcloser(handler);           // Events.OnGapCloser
//   SDK::Events::AddOnInterruptableTarget(handler); // Events.OnInterruptableTarget
//   SDK::Events::AddOnLoad(handler);                // Events.OnLoad
// ============================================================================

#include "AnimationTracker.h"
#include "Dash.h"
#include "Gapcloser.h"
#include "InterruptableSpell.h"
#include "Load.h"
#include "ObjectTracker.h"
#include "PathTracker.h"
#include "PropertyTracker.h"
#include "SpellCastTracker.h"
#include "Stealth.h"
#include "Teleport.h"
#include "Turret.h"

namespace SDK::Events {

    // ── Re-export arg types ──
    using DashArgs                      = Dash::DashArgs;
    using TeleportEventArgs             = Teleport::TeleportEventArgs;
    using TurretArgs                    = Turret::TurretArgs;
    using OnStealthEventArgs            = Stealth::OnStealthEventArgs;
    using ProcessSpellCastEventArgs     = SpellCast::ProcessSpellCastEventArgs;
    using StopCastEventArgs             = SpellCast::StopCastEventArgs;
    using NewPathEventArgs              = Path::NewPathEventArgs;
    using IntegerPropertyChangeEventArgs = PropertyTracker::IntegerPropertyChangeEventArgs;
    using PlayAnimationEventArgs        = AnimationTracker::PlayAnimationEventArgs;

    // ── SpellCast (AIBaseClient.OnProcessSpellCast / OnDoCast / OnStopCast) ──
    inline bool AddOnProcessSpellCast(SpellCast::ProcessSpellCastHandler h) { return SpellCast::AddOnProcessSpellCast(h); }
    inline bool OnProcessSpellCast(SpellCast::ProcessSpellCastHandler h)    { return SpellCast::AddOnProcessSpellCast(h); }
    inline bool AddOnDoCast(SpellCast::DoCastHandler h)   { return SpellCast::AddOnDoCast(h); }
    inline bool OnDoCast(SpellCast::DoCastHandler h)      { return SpellCast::AddOnDoCast(h); }
    inline bool AddOnStopCast(SpellCast::StopCastHandler h) { return SpellCast::AddOnStopCast(h); }
    inline bool OnStopCast(SpellCast::StopCastHandler h)    { return SpellCast::AddOnStopCast(h); }

    // ── Path (AIBaseClient.OnNewPath) ──
    inline bool AddOnNewPath(Path::NewPathHandler h) { return Path::AddOnNewPath(h); }
    inline bool OnNewPath(Path::NewPathHandler h)    { return Path::AddOnNewPath(h); }

    // ── Object lifecycle (GameObject.OnAssign / OnDelete) ──
    inline bool AddOnAssign(ObjectTracker::AssignHandler h) { return ObjectTracker::AddOnAssign(h); }
    inline bool OnAssign(ObjectTracker::AssignHandler h)    { return ObjectTracker::AddOnAssign(h); }
    inline bool AddOnDelete(ObjectTracker::DeleteHandler h) { return ObjectTracker::AddOnDelete(h); }
    inline bool OnDelete(ObjectTracker::DeleteHandler h)    { return ObjectTracker::AddOnDelete(h); }

    // ── Property change (GameObject.OnIntegerPropertyChange) ──
    inline bool AddOnIntegerPropertyChange(PropertyTracker::PropertyChangeHandler h) { return PropertyTracker::AddOnIntegerPropertyChange(h); }
    inline bool OnIntegerPropertyChange(PropertyTracker::PropertyChangeHandler h)    { return PropertyTracker::AddOnIntegerPropertyChange(h); }

    // ── Animation (AIBaseClient.OnPlayAnimation) ──
    inline bool AddOnPlayAnimation(AnimationTracker::PlayAnimationHandler h) { return AnimationTracker::AddOnPlayAnimation(h); }
    inline bool OnPlayAnimation(AnimationTracker::PlayAnimationHandler h)    { return AnimationTracker::AddOnPlayAnimation(h); }

    // ── Dash ──
    inline bool AddOnDash(Dash::DashHandler h) { return Dash::AddOnDash(h); }
    inline bool OnDash(Dash::DashHandler h)    { return Dash::AddOnDash(h); }
    inline DashArgs GetDashInfo(const AIBaseClient& unit) { return Dash::GetDashInfo(unit); }
    inline bool IsDashing(const AIBaseClient& unit)       { return Dash::IsDashing(unit); }

    // ── Stealth ──
    inline bool AddOnStealth(Stealth::StealthHandler h) { return Stealth::AddOnStealth(h); }
    inline bool OnStealth(Stealth::StealthHandler h)    { return Stealth::AddOnStealth(h); }
    inline Vector3 GetLastVisiblePosition(const AIHeroClient& hero) { return Stealth::GetLastVisiblePosition(hero); }
    inline float GetStealthDuration(const AIHeroClient& hero)       { return Stealth::GetStealthDuration(hero); }

    // ── Teleport ──
    inline bool AddOnTeleport(Teleport::TeleportHandler h) { return Teleport::AddOnTeleport(h); }
    inline bool OnTeleport(Teleport::TeleportHandler h)    { return Teleport::AddOnTeleport(h); }
    inline TeleportEventArgs GetTeleportData(const AIBaseClient& obj) { return Teleport::GetTeleportData(obj); }

    // ── Turret ──
    inline bool AddOnTurretAttack(Turret::TurretHandler h) { return Turret::AddOnTurretAttack(h); }
    inline bool OnTurretAttack(Turret::TurretHandler h)    { return Turret::AddOnTurretAttack(h); }

    // ── Lifecycle ──
    inline void Initialize() {
        SpellCast::Initialize();
        Path::Initialize();
        ObjectTracker::Initialize();
        PropertyTracker::Initialize();
        AnimationTracker::Initialize();
        Gapcloser::Initialize();
        InterruptableSpell::Initialize();
        Dash::Initialize();
        Stealth::Initialize();
        Teleport::Initialize();
        Turret::Initialize();
    }

    inline void Update() {
        DispatchLoad();
        SpellCast::Update();          // Must run first — Gapcloser/Interrupter depend on cast data
        Path::Update();               // Must run before Dash
        ObjectTracker::Update();      // Track object add/remove
        PropertyTracker::Update();    // Track ActionState changes
        AnimationTracker::Update();   // Track animation changes
        Dash::Update();
        Gapcloser::Update();
        InterruptableSpell::Update();
        Stealth::Update();
        Teleport::Update();
        Turret::Update();
    }

    inline void Reset() {
        SpellCast::Reset();
        Path::Reset();
        ObjectTracker::Reset();
        PropertyTracker::Reset();
        AnimationTracker::Reset();
        Dash::Reset();
        Gapcloser::Reset();
        InterruptableSpell::Reset();
        Stealth::Reset();
        Teleport::Reset();
        Turret::Reset();
        ResetLoadHandlers();
    }

} // namespace SDK::Events
