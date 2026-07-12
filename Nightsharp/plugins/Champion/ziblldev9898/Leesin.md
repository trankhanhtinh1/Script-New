# Lee Sin - dữ liệu kỹ năng để làm plugin

## Snapshot và nguồn

- **Champion:** Lee Sin
- **Champion ID:** `64`
- **Alias:** `LeeSin`
- **CommunityDragon content version:** `16.13.7915903+branch.releases-16-13.content.release`
- **Ngày đối chiếu:** `2026-07-11`
- **Phạm vi chính:** Summoner's Rift/giá trị mặc định. Override Arena nằm ở cuối tài liệu.
- **BIN cố định theo phiên bản:** <https://raw.communitydragon.org/16.13/game/data/characters/leesin/leesin.bin.json>
- **Dữ liệu client cố định:** <https://raw.communitydragon.org/16.13/plugins/rcp-be-lol-game-data/global/default/v1/champions/64.json>
- **Metadata phiên bản:** <https://raw.communitydragon.org/16.13/content-metadata.json>
- **Nguồn rolling latest:** <https://raw.communitydragon.org/latest/game/data/characters/leesin/leesin.bin.json>
- **Patch 26.10 đối chiếu W/E/R:** <https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-10-notes/>
- **Patch 26.12 đối chiếu AD growth và Q:** <https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-12-notes/>

> CDragon thường lưu mảng 7 phần tử theo dạng `[placeholder, rank 1, rank 2, ...]`. Với kỹ năng 5 cấp phải lấy index `1..5`; với R lấy index `1..3`. Không lấy index `0` làm rank 1.

## Chỉ số nền liên quan đến plugin

| Thuộc tính | Giá trị |
|---|---:|
| Base AD cấp 1 | `66` |
| AD growth | `3.4` mỗi cấp |
| Base attack speed | `0.651` |
| Attack speed growth | `3%` mỗi cấp |
| Attack range | `125` |
| Move speed | `345` |
| Energy tối đa | `200` |
| Energy regen | `10/giây` (`50/5 giây`) |
| Base HP | `645` |
| HP growth | `108` |
| Armor | `36 + 4.5/cấp` |
| Magic resist | `32 + 2.05/cấp` |

## Ký hiệu damage

- `AD` = tổng Attack Damage hiện tại.
- `bonusAD` = Attack Damage cộng thêm, không gồm base AD và tăng trưởng base theo cấp.
- `AP` = Ability Power hiện tại.
- `missingHpPct = clamp(1 - target.Health / target.MaxHealth, 0, 1)`.
- `targetBonusHP` trong công thức R là bonus Health của **mục tiêu chính bị đá**, không phải nạn nhân phụ bị va trúng.
- Tất cả công thức dưới đây là **raw damage trước giáp/kháng phép, shield và modifier**.

## Tóm tắt nhanh

| Pha | Tên | Loại | Raw damage / hiệu ứng chính |
|---|---|---|---|
| P | Flurry | Passive | 2 đòn đánh nhận `40%` bonus AS; hoàn Energy |
| Q1 | Sonic Wave | Physical | `60/90/120/150/180 + 0.90 bonusAD` |
| Q2 | Resonating Strike | Physical | `QBase * (1 + missingHpPct)`, từ `1x` đến `2x` |
| W1 | Safeguard | Shield/Dash | `60/105/150/195/240 + 0.80 AP` shield |
| W2 | Iron Will | Self buff | `10/14/18/22/26%` Omnivamp |
| E1 | Tempest | Magic AoE | `35/60/85/110/135 + 0.90 AD` |
| E2 | Cripple | Slow | `35/45/55/65/75%`, giảm dần trong `4s` |
| R | Dragon's Rage | Physical/CC | `175/400/625 + 2.00 bonusAD` |
| R va chạm | Collateral | Physical AoE | `RDamage + 12/15/18% targetBonusHP` |

## Passive - Flurry

### CDragon object

- **Ability:** `LeeSinPassiveAbility`
- **Spell:** `LeeSinPassive`
- **Buff object:** `LeeSinPassiveBuff`
- **Tooltip legacy key:** `BlindMonkFlurry`

### Cơ chế

- Sau mỗi lần dùng kỹ năng, 2 đòn đánh thường tiếp theo trong khoảng `3s` nhận `40%` bonus attack speed.
- Đòn đầu hoàn `20/30/40` Energy tại cấp tướng `1/7/13`.
- Đòn thứ hai hoàn `10/15/20` Energy tại cấp tướng `1/7/13`.
- CDragon biểu diễn đòn thứ hai bằng `EnergyReturn = 10`, tăng thêm `5` ở cấp 7 và `5` ở cấp 13; đòn đầu nhân `FirstHitEnergyMult = 2`.
- Passive không gây thêm damage; damage của hai đòn vẫn là auto attack bình thường.
- Khi làm combo nên xen AA giữa các lần cast/recast để không tràn stack passive và giữ đủ Energy.

## Q - Sonic Wave / Resonating Strike

### Q1 - Sonic Wave

| Thuộc tính | Giá trị |
|---|---:|
| CDragon object/script | `LeeSinQOne` |
| Loại cast | Direction, line skillshot |
| Cast range raw | `1200` |
| Display range override | `1100` |
| Cast delay | `0.25s` |
| Missile speed | `1800` |
| Missile half-width/radius | `60` |
| Full width hiển thị | `120` |
| Cost | `50 Energy` |
| Cooldown | `10/9/8/7/6s` |
| Recast window | `3s` |
| Damage type | Physical |
| Collision | Dừng ở enemy đầu tiên; cần kiểm tra hero, minion và missile blocker |

### Q1 damage

```text
QBase(rank) = {60, 90, 120, 150, 180} + 0.90 * bonusAD
Q1Raw       = QBase(rank)
```

- Q1 trúng sẽ đánh dấu mục tiêu, cấp True Sight và mở Q2 trong `3s`.
- Missile không tracking target.
- Missile được chiếu/clamp tới cast range.
- Đối với NightSharp `Spell`, metadata khởi tạo phù hợp là range `1200`, delay `0.25`, width `60`, speed `1800`, collision `true`, line.
- Có thể dùng `1100` làm range combo bảo thủ, nhưng `1200` mới là `castRange` raw hiện tại.

### Q2 - Resonating Strike

| Thuộc tính | Giá trị |
|---|---:|
| CDragon object/script | `LeeSinQTwo` |
| Internal dash child | `LeeSinQTwoDash` |
| Target range | `1250` |
| Cost | `25 Energy` |
| Dash base speed data | `1350` |
| Targeting | Mục tiêu đang có Q mark |
| Damage type | Physical |

```text
missingHpPct = clamp(1 - target.Health / target.MaxHealth, 0, 1)
Q2Raw        = QBase(rank) * (1 + missingHpPct)
Q2MinRaw     = QBase(rank)
Q2MaxRaw     = 2 * QBase(rank)
```

| Rank | Q1 / Q2 minimum base | Q2 maximum base, chưa tính AD |
|---:|---:|---:|
| 1 | `60` | `120` |
| 2 | `90` | `180` |
| 3 | `120` | `240` |
| 4 | `150` | `300` |
| 5 | `180` | `360` |

- Q2 scale tuyến tính từ `0%` đến `100%` damage cộng thêm theo phần trăm máu đã mất.
- Đây **không phải** cộng thẳng một phần trăm lượng HP đã mất của mục tiêu.
- Phần trăm máu mất dùng HP, không dùng shield để tính tỷ lệ.
- Khi mô phỏng combo, phải tính HP còn lại sau Q1/E/R/AA trước rồi mới tính Q2.
- Q2 tracking mục tiêu. Nếu mục tiêu blink/đổi trạng thái khi Lee đang lao, kết quả vị trí và damage có thể khác; không dùng prediction line cho Q2.
- `DashSpeed=1350` là base value; hành vi dash thực tế có thể cộng current move speed bằng script.
- Q2 không cast được khi Lee Sin đang rooted/grounded.
- `LeeSinQTwoDash` có metadata nội bộ `range=800`, `missileSpeed=2200`; không dùng child này làm giới hạn cast Q2. Pha cast public là `LeeSinQTwo`, range `1250`.

## W - Safeguard / Iron Will

### W1 - Safeguard

| Thuộc tính | Giá trị |
|---|---:|
| CDragon object/script | `LeeSinWOne` |
| Shield buff object | `LeeSinWOneShield` |
| Dash buff object | `LeeSinWOneDash` |
| Target type raw | `TargetOrLocation` |
| Cast range | `700` |
| Dash base speed data | `1350` |
| Cost | `50 Energy` |
| Cooldown | `7s` mọi rank |
| Recast window | `3s` |
| Shield duration | `2s` |

```text
WShield(rank) = {60, 105, 150, 195, 240} + 0.80 * AP
```

- Có thể dùng lên allied champion, allied minion, ward và allied summoned unit hợp lệ; không dùng lên structure.
- Dùng lên allied champion: Lee Sin và champion đó nhận shield.
- Dùng lên minion/ward: Lee Sin vẫn nhận shield theo thay đổi hiện hành.
- Self-cast: Lee Sin nhận shield tại chỗ.
- W1 không cast được khi Lee Sin đang rooted/grounded, kể cả self-cast.
- Nếu dash bị ngắt trước khi tới nơi, không giả định shield chắc chắn đã được cấp.
- Dữ liệu raw có `missileSpeed=1500`, nhưng đây không nên được dùng thay cho tốc độ dash logic; `DashSpeed=1350` là giá trị định danh riêng và hành vi game có thể cộng move speed bằng script.
- Ward-hop nên cast lên ward object sau khi ward xuất hiện; nếu dùng position, game có cơ chế tìm ward gần điểm cast nhưng plugin vẫn nên xác nhận object hợp lệ.

### W2 - Iron Will

| Thuộc tính | Giá trị |
|---|---:|
| CDragon object/script | `LeeSinWTwo` |
| Targeting | Self |
| Cost | `25 Energy` |
| Duration | `4s` |
| Omnivamp | `10/14/18/22/26%` |

- Named value trong BIN vẫn là `LifestealAndSpellVamp`, nhưng mô tả client 16.13 gọi hiệu ứng hiện hành là **Omnivamp**.
- W2 không gây damage trực tiếp.
- Không dùng `BotData` hoặc `mCoefficient` của W2 làm công thức damage; đó không phải damage của kỹ năng.

## E - Tempest / Cripple

### E1 - Tempest

| Thuộc tính | Giá trị |
|---|---:|
| CDragon object/script | `LeeSinEOne` |
| Targeting | Self AoE |
| Radius/range | `450` |
| Cast delay | `0.25s` |
| Projectile | Không |
| Cost | `50 Energy` |
| Cooldown | `8s` mọi rank |
| Recast window | `3s` nếu E1 trúng enemy |
| Mark/reveal duration | `4s` |
| Damage type | Magic |

```text
E1Raw(rank) = {35, 60, 85, 110, 135} + 0.90 * AD
```

- Tỷ lệ của E là **90% total AD**, khác Q và R dùng bonus AD.
- E1 gây AoE quanh vị trí Lee Sin ở cuối cast time.
- E1 đánh dấu enemy trúng đòn và reveal mục tiêu không invisible lúc bị trúng.
- E1 là magic damage, do đó phải qua magic resistance thay vì armor.

### E2 - Cripple

| Thuộc tính | Giá trị |
|---|---:|
| CDragon object/script | `LeeSinETwo` |
| Debuff object | `LeeSinETwoDebuff` |
| Targeting | Self AoE/recast |
| Raw cast radius/range | `575` |
| Cost | `25 Energy` |
| Slow | `35/45/55/65/75%` |
| Slow duration | `4s`, giảm dần |
| Damage | Không |

- Chỉ các enemy đã bị E1 đánh dấu và còn trong vùng E2 mới nhận slow.
- Có khoảng khóa recast `0.1s` sau E1; không spam E2 cùng tick với E1.
- E2 không xóa reveal/mark E1 ngay lập tức.

## R - Dragon's Rage

| Thuộc tính | Giá trị |
|---|---:|
| CDragon object/script | `LeeSinR` |
| Alternate name | `LeeSinRKick` |
| Targeting | Enemy champion unit |
| Cast range | `375` |
| Cast time | `0.25s` |
| Cost | `0` |
| Cooldown | `110/85/60s` |
| Kick distance data | `800` |
| Airborne duration | Khoảng `1s` |
| Damage type | Physical |

### Damage mục tiêu chính

```text
RRaw(rank) = {175, 400, 625} + 2.00 * bonusAD
```

### Damage enemy bị mục tiêu chính va trúng

```text
RCarryPct(rank)      = {0.12, 0.15, 0.18}
RCollateralRaw       = RRaw(rank) + RCarryPct(rank) * kickedTarget.BonusHealth
```

- Mục tiêu chính chỉ nhận `RRaw`; phần bonus Health chỉ cộng vào damage lên enemy phụ bị va trúng.
- Mỗi enemy phụ tự giảm `RCollateralRaw` bằng armor của chính nó.
- Trong cast time, mục tiêu chính bị giữ/root; sau impact bị knockback tối đa `800` units trong khoảng `0.8s`. Enemy phụ trên đường bay bị knock-up khoảng `1s`.
- Hướng đá được xác định theo vector từ Lee Sin tới mục tiêu tại thời điểm impact, không nên khóa theo vector lúc bắt đầu cast.
- Dữ liệu targeter có line width raw `40` và indicator range `700`; dữ liệu gameplay kick distance là `800`. Khi tìm multi-hit nên kiểm tra segment từ vị trí mục tiêu theo hướng đá, cộng bounding radius của enemy.
- `missileSpeed=1500` trong spell resource không phải tốc độ displacement đáng tin cậy để mô phỏng đường bay.
- Từ thay đổi 26.10, cơ thể champion vẫn tiếp tục bay, gây va chạm và knock-up kể cả khi chết giữa đường bay.

## Tên spell/buff cần theo dõi

| Mục đích | Tên CDragon 16.13 |
|---|---|
| Passive spell | `LeeSinPassive` |
| Passive buff | `LeeSinPassiveBuff` |
| Q1 spell/missile/mark object | `LeeSinQOne` |
| Q range manager | `LeeSinQManager` |
| Q2 spell | `LeeSinQTwo` |
| Q2 dash child | `LeeSinQTwoDash` |
| W1 spell | `LeeSinWOne` |
| W1 shield | `LeeSinWOneShield` |
| W1 dash | `LeeSinWOneDash` |
| W2 spell/buff | `LeeSinWTwo` |
| E1 spell/mark | `LeeSinEOne` |
| E2 spell | `LeeSinETwo` |
| E2 debuff | `LeeSinETwoDebuff` |
| R spell | `LeeSinR` |
| R alternate | `LeeSinRKick` |

- Legacy SDK/script databases có thể trả alias `BlindMonkQOne`, `BlindMonkQTwo`, `BlindMonkWOne`, `BlindMonkWTwo`, `BlindMonkEOne`, `BlindMonkETwo`, `BlindMonkRKick`.
- Nên so sánh tên không phân biệt hoa thường và chấp nhận cả `LeeSin*` lẫn `BlindMonk*` cho state detection.
- Ưu tiên đọc tên spell hiện tại trong spellbook để phân biệt pha 1/pha 2; buff chỉ dùng để xác nhận target mark hoặc trạng thái dash.
- Trước khi khóa tên runtime, log một trận thực tế các trường `spell name`, `missile name` và active buff name vì CDragon object name và tên expose bởi game hook có thể khác alias legacy.

## Công thức damage triển khai trong NightSharp

### Raw helpers cần có

```text
Q1Raw(player, qRank)
Q2Raw(player, target, qRank, simulatedTargetHealth)
E1Raw(player, eRank)
RRaw(player, rRank)
RCollateralRaw(player, kickedTarget, rRank)
```

### Sau mitigation

- Q1, Q2, R và R collateral: dùng `player.CalculatePhysicalDamage(target, raw)`.
- E1: dùng `player.CalculateMagicDamage(target, raw)`.
- Lấy `AD` bằng `player.TotalAttackDamage()` hoặc `player.AD()`.
- Lấy `bonusAD` bằng `player.BonusAttackDamage()`.
- Lấy `AP` bằng `player.AP()`.
- Lấy bonus Health mục tiêu đá bằng `target.BonusHealth()`.
- Kill check cuối cùng phải xét `Health`, shield phù hợp, invulnerability, untargetable và trạng thái không thể chết.

### Mô phỏng combo

- Không cộng tất cả raw damage rồi giảm một lần vì E là magic còn Q/R/AA là physical.
- Damage Q2 phụ thuộc HP tại thời điểm Q2 gây damage; phải mô phỏng tuần tự.
- Ví dụ chuỗi `Q1 -> R -> Q2`: trừ physical damage sau mitigation của Q1, trừ R, tính lại `missingHpPct`, sau đó mới tính Q2.
- AA dùng `Damage::GetAutoAttackDamage` hoặc damage API tương đương; passive không thêm damage vào AA.

## Cảnh báo dữ liệu NightSharp hiện tại

- `sdk/Data/DamageData.h` hiện lưu Q stage mặc định với base `{30,60,90,120,150}` thay vì rank hiện hành `{60,90,120,150,180}`; đây là dấu hiệu lệch index CDragon.
- Stage Q thứ hai trong damage database cũng không mô tả đúng công thức missing-health hiện hành.
- Vì vậy không dùng mù `Q.GetDamage(target)` cho Q1/Q2 kill check; nên dùng helper riêng ở trên rồi gọi damage mitigation của SDK.
- `sdk/Data/Database.h` hiện có Q dưới tên legacy `BlindMonkQOne`, range `1100`.
- `plugins/ZDEvade/Database/SpellDatabase.h` có tên `LeeSinQOne`, range `1100`.
- Plugin Lee Sin nên giữ constants riêng theo CDragon 16.13: cast range `1200`, display/conservative range `1100`, width `60`, speed `1800`, delay `0.25`.

## Arena/Cherry overrides trong BIN

Chỉ áp dụng khi map/mode thực sự là Arena (`cherry`):

| Giá trị | Summoner's Rift mặc định | Arena |
|---|---|---|
| Q cooldown | `10/9/8/7/6` | `8.5/7.5/6.5/5.5/4.5` |
| W shield base | `60/105/150/195/240` | `70/140/210/280/350` |
| W2 Omnivamp | `10/14/18/22/26%` | `15/20/25/30/35%` |
| E damage base | `35/60/85/110/135` | `55/90/125/160/195` |
| E2 slow | `35/45/55/65/75%` | `40/50/60/70/80%` |

- Không tự áp dụng Arena override chỉ vì thấy `DataValuesModeOverride` trong JSON.
- ARAM/global mode balance modifier không nằm trong các công thức spell mặc định ở trên và phải đọc từ hệ thống map modifier riêng nếu plugin hỗ trợ.

## Checklist khi bắt đầu viết plugin

- Khởi tạo Q bằng metadata `1200 / 0.25 / 60 / 1800 / collision / Line`.
- Dùng spellbook name để xác định `Q1/Q2`, `W1/W2`, `E1/E2`.
- Q1 chỉ cast khi prediction đạt hitchance và không có collision không mong muốn.
- Q2 chỉ cast khi đúng marked target, còn trong `1250`, đủ `25 Energy`, và dash không đưa Lee vào vị trí nguy hiểm nếu logic yêu cầu.
- Quản lý Energy theo từng pha: Q `50+25`, W `50+25`, E `50+25`, R `0`.
- Xen AA để tận dụng 2 stack passive thay vì cast liên tục làm mất Energy economy.
- Ward-hop: đặt ward, chờ object hợp lệ, sau đó W1 lên ward; không cast W2 nhầm pha.
- E1 dùng self AoE `450`; E2 chỉ dùng khi có target E-mark trong vùng và sau recast lock ngắn.
- R kill damage và R multi-hit dùng hai công thức khác nhau.
- R insec phải tính hướng tại impact và kiểm tra segment đá qua enemy/đồng minh đích.
- Tự viết Q2 sequential damage simulation cho combo/killsteal.
- Hook/unhook toàn bộ event và xóa menu đúng lifecycle khi plugin load/unload.

## Damage model chuẩn cho module `Damage.h`

### Nguồn dữ liệu và quy tắc index

- CDragon là nguồn chính cho công thức gameplay; `DamageData.h` chỉ dùng khi stage và rank đã được kiểm chứng.
- CDragon array luôn có placeholder ở index `0`; rank kỹ năng lấy từ `1..5`, R lấy `1..3`.
- Không dùng mù `SDK::Damage::GetSpellDamage()` cho Q1/Q2 vì entry Lee Sin trong `DamageData.h` hiện có Q base lệch thành `{30,60,90,120,150}` và stage Q2 không mô tả đúng missing-health scaling.
- Q1/Q2 nên dùng `AbilityInput`/raw helper riêng rồi đưa raw physical vào `Damage::Calculate()`.
- E1/R cũng nên dùng helper riêng để mô phỏng tuần tự và giữ raw damage từng pha; W chỉ tạo shield/omnivamp, không tạo damage.

### Biến offensive lấy từ player

| Biến module | SDK source | Ý nghĩa |
|---|---|---|
| `level` | `source.Level()` | Cấp tướng, dùng scale lethality |
| `totalAD` | `source.TotalAttackDamage()` | Base AD hiện tại + bonus AD hiện tại |
| `baseAD` | `source.BaseAttackDamage()` | Base AD theo cấp |
| `bonusAD` | `source.BonusAttackDamage()` | AD cộng thêm; dùng cho Q/R |
| `AP` | `source.AP()` | Ability Power; dùng cho W shield |
| `lethality` | `source.Lethality()` | Flat physical penetration scale theo cấp |
| `flatArmorPen` | `source.FlatArmorPenetrationMod()` | Flat armor penetration khác lethality |
| `percentArmorPen` | `source.PercentArmorPenetrationMod()` | Multiplier còn lại của armor |
| `percentBonusArmorPen` | `source.PercentBonusArmorPenetrationMod()` | Multiplier còn lại của bonus armor |
| `flatMagicPen` | `source.FlatMagicPenetrationMod()` | Flat magic penetration |
| `magicLethality` | `source.MagicLethality()` | Flat magic penetration bổ sung |
| `percentMagicPen` | `source.PercentMagicPenetrationMod()` | Multiplier còn lại của MR |
| `percentBonusMagicPen` | `source.PercentBonusMagicPenetrationMod()` | Multiplier còn lại của bonus MR |

Công thức stat Lee Sin:

```text
baseAD(level) = 66 + 3.4 * (level - 1)
totalAD       = baseAD + bonusAD
```

### Biến defensive lấy từ target

| Biến module | SDK source | Ý nghĩa |
|---|---|---|
| `armor` | `target.Armor()` | Tổng armor hiện tại |
| `bonusArmor` | `target.BonusArmor()` | Phần bonus armor |
| `magicResist` | `target.SpellBlock()` | Tổng magic resistance hiện tại |
| `bonusMagicResist` | `target.BonusSpellBlock()` | Phần bonus magic resistance |
| `targetHealth` | `target.Health()` | HP hiện tại |
| `targetMaxHealth` | `target.MaxHealth()` | HP tối đa |
| `targetBonusHealth` | `target.BonusHealth()` | Bonus HP, dùng cho R collateral |

### Mitigation và penetration

Lethality Lee Sin được scale theo cấp:

```text
lethalityMultiplier = 0.6 + 0.4 * clamp(level, 1, 18) / 18
scaledLethality     = lethality * lethalityMultiplier
```

Physical:

```text
effectiveArmor = armor * percentArmorPen
               - bonusArmor * (1 - percentBonusArmorPen)
               - flatArmorPen
               - scaledLethality

if armor < 0:
    physicalMultiplier = 2 - 100 / (100 - armor)
else if effectiveArmor < 0:
    physicalMultiplier = 1
else:
    physicalMultiplier = 100 / (100 + effectiveArmor)

physicalDamage = floor(rawPhysical * physicalMultiplier)
```

Magical:

```text
effectiveMR = magicResist * percentMagicPen
            - bonusMagicResist * (1 - percentBonusMagicPen)
            - flatMagicPen
            - magicLethality

if magicResist < 0:
    magicMultiplier = 2 - 100 / (100 - magicResist)
else if effectiveMR < 0:
    magicMultiplier = 1
else:
    magicMultiplier = 100 / (100 + effectiveMR)

magicDamage = floor(rawMagic * magicMultiplier)
trueDamage  = floor(rawTrue)
totalDamage = physicalDamage + magicDamage + trueDamage
```

Sau mitigation, module có thể áp dụng thêm `DamageReductionMod` và `DamageMastery` của SDK. Không gộp physical và magic raw trước khi giảm vì chúng dùng hai loại kháng khác nhau.

## Biến raw damage từng kỹ năng

### Passive - Flurry

```text
passiveAS = 40%
passiveCharges = 2 sau mỗi lần cast skill
firstHitEnergy = 2 * energyReturn
secondHitEnergy = energyReturn
energyReturn = 10 + 5 nếu cấp tướng >= 7 + 5 nếu cấp tướng >= 13
```

Bảng hoàn Energy:

| Cấp tướng | Đòn 1 | Đòn 2 | Tổng |
|---:|---:|---:|---:|
| 1-6 | 20 | 10 | 30 |
| 7-12 | 30 | 15 | 45 |
| 13-18 | 40 | 20 | 60 |

Passive không có raw damage riêng. Damage mỗi đòn vẫn đi qua `CalculateAutoAttack()` với raw base là `totalAD`, cộng item/champion on-hit nếu bật `includePassives`.

### Q - Sonic Wave / Resonating Strike

```text
QBase(rank) = qBase[rank] + 0.90 * bonusAD
qBase       = { 60, 90, 120, 150, 180 }
Q1Raw       = QBase(rank)
```

| Q rank | Base | AD scaling | Q1 raw |
|---:|---:|---:|---|
| 1 | 60 | `0.90 * bonusAD` | `60 + 0.90 * bonusAD` |
| 2 | 90 | `0.90 * bonusAD` | `90 + 0.90 * bonusAD` |
| 3 | 120 | `0.90 * bonusAD` | `120 + 0.90 * bonusAD` |
| 4 | 150 | `0.90 * bonusAD` | `150 + 0.90 * bonusAD` |
| 5 | 180 | `0.90 * bonusAD` | `180 + 0.90 * bonusAD` |

Q2 dùng đúng QBase của Q1 và HP của mục tiêu ngay trước impact:

```text
missingHpPct = clamp((targetMaxHealth - targetHealth) / targetMaxHealth, 0, 1)
Q2Raw        = QBase(rank) * (1 + missingHpPct)
```

Lưu ý quan trọng: `missingHpPct` nhân toàn bộ `QBase`, trong đó `QBase` đã bao gồm phần `0.90 * bonusAD`. Không cộng trực tiếp phần trăm HP mất vào damage.

Q state machine:

```text
Q1 cast thành công -> chờ Q1 hit -> target nhận Q mark -> spellbook chuyển Q2
Q2 cast           -> tính missingHpPct tại thời điểm impact -> spellbook trở về Q1
Q1 miss            -> không được mô phỏng Q2 damage
```

Q2 không phải skillshot line; phải cast targeted vào đúng target đang có `LeeSinQOne`/`BlindMonkQOne` mark.

### W - Safeguard / Iron Will

W1 không phải damage; chỉ mô phỏng shield:

```text
WShield(rank) = shieldBase[rank] + 0.80 * AP
shieldBase    = { 60, 105, 150, 195, 240 }
```

| W rank | Shield base | AP scaling | Shield raw |
|---:|---:|---:|---|
| 1 | 60 | `0.80 * AP` | `60 + 0.80 * AP` |
| 2 | 105 | `0.80 * AP` | `105 + 0.80 * AP` |
| 3 | 150 | `0.80 * AP` | `150 + 0.80 * AP` |
| 4 | 195 | `0.80 * AP` | `195 + 0.80 * AP` |
| 5 | 240 | `0.80 * AP` | `240 + 0.80 * AP` |

```text
W1 -> shield 2s, mở W2 trong 3s
W2 -> self buff 4s, 10/14/18/22/26% omnivamp theo rank
```

W2 không cộng damage trực tiếp vào `totalDamage`; omnivamp chỉ tác động hồi phục theo damage thực tế của các đòn sau đó.

### E - Tempest / Cripple

E1 là magic damage theo **total AD**, không phải bonus AD:

```text
E1Base(rank) = { 35, 60, 85, 110, 135 }
E1Raw        = E1Base(rank) + 0.90 * totalAD
```

| E rank | Base | AD scaling | E1 raw magic |
|---:|---:|---:|---|
| 1 | 35 | `0.90 * totalAD` | `35 + 0.90 * totalAD` |
| 2 | 60 | `0.90 * totalAD` | `60 + 0.90 * totalAD` |
| 3 | 85 | `0.90 * totalAD` | `85 + 0.90 * totalAD` |
| 4 | 110 | `0.90 * totalAD` | `110 + 0.90 * totalAD` |
| 5 | 135 | `0.90 * totalAD` | `135 + 0.90 * totalAD` |

```text
E1 cast -> damage tại vùng self AoE -> target nhận E mark/reveal -> mở E2
E2 cast -> không gây damage -> áp slow 35/45/55/65/75% giảm dần trong 4s
```

E1 phải đưa vào `rawMagical`, không đưa vào `rawPhysical`.

### R - Dragon's Rage

R damage mục tiêu chính:

```text
RBase(rank) = { 175, 400, 625 }
RRaw        = RBase(rank) + 2.00 * bonusAD
```

| R rank | Base | AD scaling | Main target raw physical |
|---:|---:|---:|---|
| 1 | 175 | `2.00 * bonusAD` | `175 + 2.00 * bonusAD` |
| 2 | 400 | `2.00 * bonusAD` | `400 + 2.00 * bonusAD` |
| 3 | 625 | `2.00 * bonusAD` | `625 + 2.00 * bonusAD` |

Enemy phụ bị mục tiêu chính va trúng:

```text
carryPct(rank)      = { 0.12, 0.15, 0.18 }
RRadiusRaw          = RRaw + carryPct(rank) * kickedTarget.BonusHealth
```

- `kickedTarget.BonusHealth` là bonus HP của mục tiêu chính bị đá.
- Main target nhận `RRaw`, không cộng `carryPct * bonusHealth`.
- Mỗi enemy phụ tự giảm `RRadiusRaw` theo armor riêng.
- R không có raw magic damage.

## API mapping sang `Damage.h`

| Nhu cầu | API |
|---|---|
| Đọc toàn bộ AD/AP/pen | `ReadOffensiveStats(source)` |
| Đọc armor/MR target | `ReadDefensiveStats(target)` |
| Tính physical/magic/true từ raw | `Calculate(source, target, RawDamage)` |
| Tính bằng stat snapshot | `CalculateFromStats(offensive, defensive, raw)` |
| Tính skill từ base + AD/AP/HP ratio | `CalculateAbility(source, target, AbilityInput)` |
| Tính auto attack + passive/on-hit | `CalculateAutoAttack(source, target, includePassives)` |
| Lấy damage AA cuối | `GetAutoAttackDamage(source, target, includePassives)` |
| Lấy damage spell database | `GetSpellDamage(source, target, slot, stage)` |
| Tính một raw amount theo type | `GetDamage(source, target, type, rawAmount)` |

Q/E/R helper nên tạo `AbilityInput` như sau:

```text
Q1/Q2: Type=Physical, BonusADRatio=0.90
E1:     Type=Magical, TotalADRatio=0.90
R:      Type=Physical, BonusADRatio=2.00
W1:     không dùng Calculate damage; tự tính shield từ ShieldValue + 0.80*AP
```

Q2 phải nhân `1 + missingHpPct` trước khi đưa vào `RawDamage.Physical`.

## Mô phỏng tuần tự skill

### State cần lưu

```text
SimState {
    targetHealth
    targetMaxHealth
    targetArmor
    targetMagicResist
    targetBonusHealth
    energy
    passiveCharges
    qMarkActive
    eMarkActive
    qPhase
    wPhase
    ePhase
}
```

### Quy tắc cập nhật state

```text
cast any skill:
    passiveCharges = 2
    energy -= skillCost

successful Q1:
    qPhase = Second
    qMarkActive = true

successful Q2:
    raw = QBase(rank) * (1 + missingHpPct computed now)
    targetHealth -= physicalDamage(raw)
    qPhase = First
    qMarkActive = false

successful W1:
    selfShield = WShield(rank)
    wPhase = Second

successful W2:
    omnivampBuff = rank value
    wPhase = First

successful E1:
    targetHealth -= magicDamage(E1Raw)
    eMarkActive = true
    ePhase = Second

successful E2:
    if eMarkActive and target is in E2 area:
        apply slow
    ePhase = First
    eMarkActive = false

successful R:
    main target loses physicalDamage(RRaw)
    each collision target independently loses physicalDamage(RRadiusRaw)
```

Sau mỗi auto attack:

```text
passiveCharges = max(passiveCharges - 1, 0)
energy += 20/30/40 on first hit by champion level
energy += 10/15/20 on second hit by champion level
```

Không cộng damage của Q1, E1, R rồi mới giảm một lần. Mỗi hit phải cập nhật HP trước khi tính hit tiếp theo, đặc biệt Q2.

## Ví dụ mô phỏng số

Giả sử:

```text
champion level = 11
bonusAD        = 100
AP             = 0
totalAD        = 200
target HP      = 2000 / 2000
target armor   = 100
target MR      = 80
bonusHP target = 500
penetration    = 0
```

### Chuỗi `Q1 -> AA -> Q2 -> E1 -> R2`

| Bước | Raw | Loại | Damage sau kháng | HP còn lại |
|---|---:|---|---:|---:|
| Q1 rank 5 | `180 + 0.9*100 = 270` | Physical | `floor(270*100/200)=135` | `1865` |
| AA | `200` | Physical | `100` | `1765` |
| Q2 rank 5 | `270*(1+235/2000)=301.725` | Physical | `150` | `1615` |
| E1 rank 5 | `135 + 0.9*200 = 315` | Magic | `floor(315*100/180)=175` | `1440` |
| R rank 2 | `400 + 2*100 = 600` | Physical | `300` | `1140` |

Q2 phải dùng HP sau Q1 và AA; nếu tính Q2 từ HP `2000` sẽ sai damage.

### R collateral trong cùng ví dụ

```text
R2 main raw       = 600
carryPct rank 2   = 0.15
bonusHP kicked    = 500
collateral raw    = 600 + 0.15*500 = 675 physical
```

Nếu enemy phụ cũng có `100 armor` và không có penetration:

```text
collateral damage = floor(675 * 100 / 200) = 337
```

Damage `337` chỉ áp dụng cho enemy phụ bị va chạm; không cộng lại vào damage main target.

## Checklist cập nhật damage implementation

- Tạo `Q1Raw`, `Q2Raw`, `WShield`, `E1Raw`, `RRaw`, `RCollateralRaw` bằng rank skill hiện tại.
- Dùng `totalAD` cho E1; dùng `bonusAD` cho Q1/Q2/R.
- Luôn chia physical/magic/true trước khi mitigation.
- Tính Q2 sau khi cập nhật HP từ các hit trước.
- Dùng `target.BonusHealth()` cho R collateral.
- Không coi W2 omnivamp là damage.
- Không dùng stage Q hiện tại trong `DamageData.h` cho Q2 nếu chưa sửa database.
- Test rank 1, rank giữa và rank tối đa; test target armor/MR dương, âm và có penetration.
