# Irelia - Blade Dancer (CDragon verified 2026-07-09)

## Champion Info
- **id=39**, alias="Irelia", roles: fighter, assassin
- **Base stats**: HP 630 (+115/lvl), Mana 350 (+50/lvl), AD 65 (+3.5/lvl), Armor 36 (+4.7/lvl), MR 30 (+2.05/lvl)
- **Move speed**: 335, **Attack range**: 200 (melee), **AS**: 0.656 (+2.5%/lvl)
- **CDragon**: `game/data/characters/irelia/irelia.bin.json`

## Spell Names (runtime)
- Q: `IreliaQ`, W: `IreliaW`, E: `IreliaE`, R: `IreliaR`
- Passive: `IreliaPassive`

## Passive - Ionian Fervor (IreliaPassive)
- **Buff name**: `IreliaPassiveStacks` (stacks), `IreliaPassiveStacksMax` (max indicator)
- **Max stacks**: 4
- **Buff duration**: 6s
- **On-hit bonus damage**: 10 + 3/level + 0.2 AD (per stack, applies to all hits)
- **AS per stack**: 10-25 by char level (interpolated)
- **At max stacks**: bonus damage doubled + Q heal on minions
- **Trigger**: hitting enemy champion with spell or AA
- **Tags**: passive, cannotBeSuppressed, canCastWhileDisabled

## Q - Bladesurge (IreliaQ)
- **CastRange**: 600
- **Cooldown**: 10/10/9/8/7/6/5
- **Mana**: 15
- **Targeting**: Direction (dash to target)
- **cantCastWhileRooted**: true
- **DashSpeedBonus**: 1400

### Damage
- **Champion damage**: BaseDamage + 0.7 AD
  - BaseDamage: -15/5/25/45/65/85/105 (rank 1-5, first value is level 0)
- **Minion damage**: BaseDamage + (50 + 11/charLevel) + 0.7 AD
- **Heal**: 8/9/10/11/12/13/14% AD (HealTADCoefficient)

### Mechanics
- Dash to target unit/position
- **Q reset on kill**: if target dies, Q cooldown resets
- **Q reset on Mark**: if target has `IreliaMark` (from E or R), Q cooldown resets + mark consumed
- Tags: MoveBlock, DamageAbility, ActiveHeal, Strike, SignatureSpell

## W - Defiant Dance (IreliaW)
- **CastRange**: 775 (display 825)
- **CastRadius**: 300
- **Cooldown**: 20/20/18/16/14/12/10
- **Mana**: 70/75/80/85/90/90
- **Targeting**: Direction
- **Channel duration**: 1.5s (max)
- **ChargeTimeForMax**: 0.75s
- **Can move while channeling**: yes
- **ChannelIsInterruptedByDisables**: false
- **ChannelIsInterruptedByAttacking**: false
- **Recast**: `IreliaW2` (CD 3s, missile width 120, travelTime 0.05s)

### Defense (IreliaWDefense buff)
- **Physical DR**: 40-70 by char level + 7% bonus AD
- **Magic DR**: 50% of physical DR (MRReductionAmount=0.5)
- **Duration**: while channeling (max 1.5s)

### Damage (on release/recast)
- **Min damage** (released early): 10/20/30/40/50/60 + 0.4 AD + 0.5 AP
- **Max damage** (full charge): 30/60/90/120/150/180 + 1.2 AD + 1.5 AP
- **MaxBonusRatio**: 2.0 (max damage = min * 2 + bonus)
- **Missile width**: 100 (W1), 120 (W2)
- Tags: DamageAbility, RecastOrReplace, Parry, Target_Directional, Melee_SmallWindup, Low_Damage, AoE

## E - Flawless Duet (IreliaE)
- **CastRange**: 25000 (display 850)
- **MaxRange**: 775, **MinRange**: 50
- **Cooldown**: 16/16/14.5/13/11.5/10/10
- **Mana**: 50
- **Targeting**: Location
- **CDBetweenCast**: 0.25s (between E1 and E2)
- **Recast**: `IreliaE2` (free recast, no CD)

### Damage
- **BaseDamage**: 30/70/110/150/190/230/270
- **APRatio**: 1.0 (100% AP)
- **StunDuration**: 0.75s
- **Mark duration** (on enemy): 5s (`IreliaMark` buff)
- **Buff duration** (disarm/CC): 3.5s

### Child spells
- `IreliaEMissile`: width 90, travelTime 0.25s (first blade)
- `IreliaESecondary`: width 70, travelTime 0.1s (second blade)
- `IreliaEParticleMissile`: travelTime 0.6s (beam warning)

### Mechanics
- **Two-cast**: E1 places first blade, E2 places second blade
- Blades converge → stun + damage enemies between them
- Hit enemies get `IreliaMark` → Q reset on marked target
- Tags: ImmobilizingCC, DamageAbility, RecastOrReplace, Low_Damage, AoE

## R - Vanguard's Edge (IreliaR)
- **CastRange**: 1000 (display 950)
- **Cooldown**: 125/125/105/85
- **Mana**: 100
- **Targeting**: Direction
- **Missile width**: 160, **Missile speed**: 2000

### Damage
- **MissileDamage**: 50/125/200/275/350/425/500 + 0.7 AP
- **ZoneDamage**: same as missile damage
- **Zone duration**: 2.5s (wall of blades stays)
- **CC**: Disarm 1.5s (`IreliaRDisarm` buff)
- **Slow**: 90% for 1.5s
- **Mark duration**: 5s (`IreliaMark` on enemies hit)
- **Cooldown refund per enemy hit**: 0.5/1/1.5/2/2.5/3s

### Child spells
- `IreliaR2`: recast (travelTime 0.25s, width 150)
- `IreliaRDisarm`: disarm buff

### Mechanics
- Fires missile in direction → creates wall of blades
- Enemies hit by missile: damage + disarm + slow + mark
- Enemies passing through wall: zone damage + slow + mark
- Marked enemies → Q reset
- Tags: Ultimate, DamageAbility, AoE

## Buff Names Summary
| Buff name | Type | Description |
|---|---|---|
| IreliaPassiveStacks | Buff self | Passive stacks (max 4), 6s duration |
| IreliaPassiveStacksMax | Buff self | Indicator that passive is at max stacks |
| IreliaMark | Debuff enemy | Q reset mark from E/R, 5s duration |
| IreliaWDefense | Buff self | W damage reduction while channeling |
| IreliaRDisarm | Debuff enemy | R disarm, 1.5s |

## Combo Logic Notes
1. **Q reset**: Q cooldown resets on kill or on hitting marked target (IreliaMark from E/R)
2. **E → Q**: E stuns + marks → Q dash to marked target → Q resets → Q again
3. **R → Q**: R marks all enemies hit → Q dash chain through multiple marked targets
4. **W**: Channel for DR, release for damage. Can move while channeling. Release early for min damage, hold for max.
5. **Passive**: Build stacks with spells/AA for AS + on-hit damage. Max stacks = doubled on-hit + Q heal on minions.
6. **Q on minions**: Q heals on minion kill (at max passive stacks), useful for sustain + reset chain

## Spell Setup for SDK
```
Q = Spell(SpellSlot::Q, 600.0f);  // dash, no skillshot (target unit)
W = Spell(SpellSlot::W, 825.0f);  // direction, channel + recast
E = Spell(SpellSlot::E, 850.0f);  // location, two-cast
R = Spell(SpellSlot::R, 950.0f);  // direction line, width 160, speed 2000
R.SetSkillshot(0.25f, 160.0f, 2000.0f, false, SpellType::Line);
```

## Key Implementation Notes
- **Q is a dash, not a skillshot** — cast on target unit or position within 600 range
- **E is two-cast** — need to track E1 cast state, E2 is free recast within 0.25s
- **W is channel + recast** — need to track channel start time for damage calculation
- **R is line skillshot** — width 160, speed 2000, castTime 0.25
- **Q reset detection**: check for `IreliaMark` buff on target before Q, or track kill event
- **Passive stacks**: read `IreliaPassiveStacks` via `CoreBuffs::Enumerate` + `GetStacks`
- **Mark detection**: check `IreliaMark` on enemy via `HasBuff("IreliaMark")` or buff enumerate
