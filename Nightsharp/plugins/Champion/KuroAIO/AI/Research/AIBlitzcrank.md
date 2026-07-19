# AIBlitzcrank research and implementation dossier

Baseline: League of Legends / CommunityDragon PC **16.14**  
Controller: `AI/Controllers/AIBlitzcrankController.h`  
Pure policy/mechanics: `AI/Controllers/AIBlitzcrankGeometry.h`  
Regression: `tests/blitzcrank_geometry_test.cpp` (**101 scenarios**)  
Published runtime scenarios: **167**

## Coverage decision

The KuroAIO dispatcher, catalog and champion directory were checked before
implementation. KuroAIO had no Blitzcrank route, profile or controller, so the
new `AI` files do not shadow or replace an existing KuroAIO champion. Three
other local implementations were read completely:

- `SharpShooterAIO/Blitzcrank.h`;
- `OneKeyToWin/Champions/Blitzcrank.h`;
- `OneKeyToWin/OKTW_CSharp/Champions/Blitzcrank.cs`.

They supplied useful historical intent—E before an attack, R interrupt and
anti-gapclose—but their static collision/range and unconditional post-hook
chains are not reused as one-trick policy. This controller computes moving
first contact, pull value, exact E timing and R passive opportunity cost.

## Pinned live data

- [CommunityDragon 16.14 Blitzcrank bin](https://raw.communitydragon.org/16.14/game/data/characters/blitzcrank/blitzcrank.bin.json), SHA-256
  `1640678cebf937b2740ea58bd4eaed1cce96a5f73d34f9eb2bef693e2c7437de`.
- [CommunityDragon 16.14 champion JSON](https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/53.json), SHA-256
  `f1e026a3abc41bb27156a75be5abd1c7833f7706c3ea5e6966fcd18b2c19fae2`.
- Runtime names include `ManaBarrier`, `RocketGrab`, `RocketGrabMissile`,
  `Overdrive`, `PowerFist`, `PowerFistAttack` and `StaticField`.

The bin contains inactive calculations left behind by a reverted fighter
experiment. They are deliberately excluded. The reconciliation authority is:

- [Riot patch 25.08](https://www.leagueoflegends.com/en-us/news/game-updates/patch-25-08-notes/):
  Mana Barrier became 35% maximum mana and Q became 110/160/210/260/310
  with 120% AP.
- [Riot patch 25.22](https://www.leagueoflegends.com/en-us/news/game-updates/patch-25-22-notes/):
  E cooldown became 7/6.5/6/5.5/5.
- [Riot patch 13.17](https://www.leagueoflegends.com/en-sg/news/game-updates/patch-13-17-notes/):
  the fighter/jungle changes were rolled back. Current W has no 1%-maximum-HP
  non-champion damage; current E has no special non-champion multiplier.
- [Riot patch 12.19](https://www.leagueoflegends.com/en-us/news/game-updates/patch-12-19-notes/)
  remains relevant only for changes that survived the rollback, including the
  unlimited R mark backlog and passive maximum-mana ratio.

The current mechanics cross-check was the
[League of Legends Wiki Blitzcrank page](https://wiki.leagueoflegends.com/en-us/Blitzcrank).
Old Fandom values were not used when they disagreed with later Riot notes.

## Live mechanics represented

### Mana Barrier

- triggers when health crosses 30%;
- shield is 35% **maximum** mana for ten seconds;
- cooldown is 90 seconds;
- spending current mana does not weaken the shield. Mana reservation therefore
  protects future spell sequences, not a fictional current-mana shield value.

### Rocket Grab

- cast time 0.25 seconds, speed 1800 and collision radius 70;
- missile segment 1080 with the live center-only endpoint behavior through
  1115 target-center range;
- first enemy body is stunned for 0.65 seconds and pulled to 75 units in front
  of Blitzcrank;
- Blitzcrank cannot move or attack while the projectile is travelling;
- projectile walls and spell shield stop the control; if Q kills, there is no
  useful pull to score.

`ContactWithBody` solves missile and target relative motion. Every visible
enemy champion, enemy minion and jungle monster contributes its own predicted
velocity. Contacts are sorted by time rather than the order returned by the
SDK. The controller rejects a cast unless the intended champion is the first
body and separately scores the post-pull location.

### Overdrive

- five-second attack-speed buff of 30/40/50/60/70%;
- movement starts at 60/65/70/75/80%, decays to 10% by 2.9 seconds, then stays
  at 10% until the buff ends;
- the following self-slow is 30% for 1.5 seconds.

The travel helper integrates that decay. W is authorized only for a concrete
walk-up E, materially improved hook angle, post-hook contact, urgent peel,
explicit flee, or an optional long player-led roam. It does not issue movement.

### Power Fist

- five-second empowerment and real basic-attack reset;
- empowered attack total is 200% total AD + 25% AP before mitigation;
- knock-up is one second and adds 50 attack range;
- spell shield blocks knock-up but not attack damage;
- an attack-blocking zone such as Shen W blocks the attack and therefore the
  knock-up; blind/dodge state is treated separately.

The controller selects among immediate arrival E, Q-flight pre-arm,
AA-E-AA, delayed response to an interruptible escape startup, or urgent peel.
AA-E-AA is used only when the victim cannot Flash/dash out during the extra
attack window. An armed E can suppress a wrong minion attack only if the exact
hooked target will arrive before the buff expires.

### Static Field

- each attack while R is ready adds a mark;
- one pending mark per target detonates each second, with unlimited backlog;
- passive damage is 50/100/150 + 30/40/50% AP + 2% maximum mana;
- active radius is 600, cast time 0.25, silence 0.5 seconds and damage
  275/400/525 + 100% AP;
- active R destroys damage shields before damage/silence. A spell shield can
  block damage/silence but does not preserve those existing shields.

`RMarkTracker` maintains independent queues and next detonation times. Active R
is held when the next passive tick already kills, or when low payoff does not
beat attacks likely to be lost while R cools down. It is spent for verified
lethal, meaningful shield break, urgent interrupt/peel, valuable AoE, an
escape-ready caster during Q flight, or a verified R-Q line.

## One-trick decision model

### Hook pressure and pull safety

The target registry separates premium catches (carry, enchanter, artillery)
from dangerous deliveries (assassin, diver, juggernaut, engage bomb, warden).
A geometrically valid Q is rejected if it would deliver a healthy threat onto
the dynamically protected carry. Lethal, existing peel displacement, isolation
or verified spent engage cooldowns can override that gate.

Holding Q is a real decision. If the player's route and W speed can safely
reach E after the target's escape is spent, the controller keeps Q available;
the E knock-up then makes Q reliable. This encodes the high-elo pattern that
the threat of hook constrains movement more than a speculative max-range cast.

### Combo families

The implementation reconciles the current
[Mobalytics combo catalog](https://mobalytics.gg/lol/champions/blitzcrank/combos),
the detailed [Best Blitzcrank NA guide](https://www.mobafire.com/league-of-legends/build/s12-guide-how-to-play-blitz-by-the-best-blitzcrank-in-na-564760),
and current support guidance rather than storing one fixed sequence:

- **W-E-EAA-Q-AA**: safe walk-up control; Q is held until knock-up.
- **Q-E-EAA**: E is pre-armed while Q flies if Flash/instant mobility is ready.
- **Q-AA-E-AA**: real reset only when escape is unavailable.
- **R-Q-EAA**: silence first only for a mobile priority target and a verified
  clean first-body Q.
- **Q-W-EAA-R**: W maintains post-pull contact; R is conditional, never an
  unconditional combo suffix.
- manual Flash/Hexflash variants remain player-owned and are not automated.

[Skill-Capped's current support build/guide](https://www.skill-capped.com/lol/guides/builds/blitzcrank/support)
was used for brush/vision threat, W walk-up E reliability and Q preservation.
Community timing discussion also supports immediate E against escape-ready
targets and AA-E-AA only when safe:
[Blitzcrank mains AA/E timing](https://www.reddit.com/r/blitzcrankmains/comments/ge1zlj/)
and [late-game vision/peel discussion](https://www.reddit.com/r/blitzcrankmains/comments/1tvokfu/how_do_you_play_blitzcrank_in_the_late_game/).

### Pro and current-video review

The research pass located recent full-game/pro POV material rather than only
short combo clips:

- [Keria Blitzcrank support, T1 SoloQ replay, July 2026](https://www.youtube.com/watch?v=OfsItGiZSpw);
- [Keria Blitzcrank versus Bard, patch 26.3](https://www.youtube.com/watch?v=HUZjjciOP54);
- [How T1 Keria controls the map with Blitzcrank, patch 26.3](https://www.youtube.com/watch?v=55l-zLd5xk0);
- [T1 versus DK Keria POV, LCK 2025](https://www.youtube.com/watch?v=RP1Pwzql6P0);
- [KT versus T1 Keria POV, LCK 2025](https://www.youtube.com/watch?v=_JhwvW1g2oM).

The portable lessons encoded here are vision-side threat, patient hook
pressure, front-to-back peel, W-E reliability and selective micro-silence.
Pathing, ward placement and movement themselves stay with the player because
the controller does not have enough strategic information to replace them.

## Player cooperation

- Selected/locked targets receive a scoring bonus but never bypass collision,
  spell-shield or dangerous-delivery gates.
- A manual Q keeps its aim. The controller may assist its E timing only after a
  short ownership delay and only when the actual first body can be inferred.
- Manual W/E/R produces a configurable ownership window; no generic combo
  engine follows it.
- Orbwalker owns all attacks and movement. `OnBeforeAttack` only blocks a
  narrowly proven wrong E target; `OnAfterAttack` creates the true reset window.
- Movement, attack-move, Hold, Stop, warding, Flash, Hexflash and Smite are
  never issued by this controller.

## Verification and runtime telemetry

`blitzcrank_geometry_test.cpp` compiles independently under MSVC C++17 and
passes 101 assertions covering live arithmetic, moving collision, endpoint
lollipop, pull safety, hook pressure, W self-slow, E timing, R queues and mana
packages. The full `Release|x64` solution build passes after catalog and project
integration.

Runtime-only facts still require replay/live telemetry: exact production buff
aliases for R marks and W self-slow, missile delete timing under every
projectile wall, spell-shield ordering for every shield implementation, and
attack-blocker interactions. The controller uses event fallbacks and bounded
timers, and the coach overlay exposes first body, E reservation, mark queues,
barrier readiness and ownership so these facts can be audited without silently
inventing state.

## Shared-helper boundary

Blitzcrank reuses neutral prediction, projectile-wall, spell-name, buff-name,
cast-window, threat, protected-ally, gapcloser, interrupt, resource and cast
throttle helpers. During this pass, the duplicate Summoner1/Summoner2 spell
inspection in Ashe and Amumu was extracted as
`SpellInstanceContains`, `HeroHasSummonerSpellToken` and `HeroHasSmite`; all
three controllers now share it. Moving hook geometry, pull archetypes, W-E
pressure, E timing and R mark economy remain Blitzcrank-local because they are
kit semantics rather than neutral plumbing.
