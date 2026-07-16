# Missing / unmapped SDK APIs — SharpShooterAIO ports

## Kaisa

### Player.Spellbook.EvolveSpell / Player.EvolvePoints
- **C# gốc:** `Kaisa.cs:137-143` — `if (Player.EvolvePoints != 0) { Player.Spellbook.CastSpell(SpellSlot.Recall); Player.Spellbook.EvolveSpell(SpellSlot.E/Q/W); }`
- **Dùng để:** Fast-evolve keybind (Alt) — tự chọn tiến hoá Q/W/E khi có điểm evolve.
- **Đã search:** `EvolveSpell`, `EvolvePoints`, `Evolve` trong SDK/ → không có. Spellbook chỉ có `CastSpell`/`UpdateChargedSpell`.
- **Trạng thái:** BLOCKED — SDK không expose evolve. LoL hiện đại auto-evolve theo cấp nên bỏ qua an toàn.
- **Chỗ comment trong code:** `Kaisa.h` Game_OnUpdate (khối Fast-evolve).

## Samira

### R.Instance.IconUsed (style-grade meter Non..S)
- **C# gốc:** `Samira.cs:606-610` — `var RCount = R.Instance.IconUsed; return (Passive)RCount;`
- **Dùng để:** Đọc bậc style hiện tại (Non/E/D/C/B/A/S) để gate toàn bộ combo (W/E/R chỉ cast ở bậc phù hợp; R chỉ khi S).
- **Đã search:** `IconUsed`, `IconIndex` trong SDK/ + `SpellDataInstClient` → không có (chỉ có Ammo/Level/Cooldown/...).
- **Trạng thái:** BLOCKED — proxy tạm qua `GetBuffCount("samiradaredevilimpulse")` trong `GetPassiveStack()`. Cần SDK expose IconUsed để chính xác 1-1.
- **Chỗ comment trong code:** `Samira.h` GetPassiveStack().

### Packet mode (Game.OnProcessPacket, header 56)
- **C# gốc:** `Samira.cs:223-254` — `OnByPassPacket` / `OnActivePassPacket` chặn packet header 56 khi cast W (fast-W packet).
- **Dùng để:** Tối ưu W bằng packet (giảm delay). Cosmetic/optimization, không đổi logic combat.
- **Đã search:** `OnProcessPacket`, `GamePacket`, `PacketData` trong SDK/ → không có packet layer.
- **Trạng thái:** BLOCKED — bỏ packet, cast W thường. Không ảnh hưởng đúng-sai logic.
- **Chỗ comment trong code:** `Samira.h` header note.

### Player.IssueOrder(GameObjectOrder.MoveTo)
- **C# gốc:** `Samira.cs:264` — `Player.IssueOrder(GameObjectOrder.MoveTo, Game.CursorPos)` khi có buff SamiraR.
- **Dùng để:** Giữ di chuyển về con trỏ trong lúc R (Inferno Trigger) đang quay.
- **Đã search:** `IssueOrder`/`GameObjectOrder` trong SDK/ → không có; thay bằng `CoreControl::IssueMove(pos, true)` (đã xác nhận trong OrbwalkerActions).
- **Trạng thái:** RESOLVED — dùng `CoreControl::IssueMove`.
- **Chỗ comment trong code:** `Samira.h` Check().

## Thresh

### MissileManager.WillHit (helper block đạn cho friend)
- **C# gốc:** `Thresh.cs:59,142-153` — `MissileManager.Initialize()` + `Helper()` gọi `MissileManager.WillHit(allyhero)` để W (đèn lồng) chặn đạn bay tới friend.
- **Dùng để:** SafeHelp2 — nếu có đạn sắp trúng ally trong tầm W thì cast W (lantern shield) tới ally đó.
- **Đã search:** `MissileManager`, `WillHit(unit)` (missile-tracking global) trong SDK/ → không có manager theo dõi missile tới ally. `Spell::WillHit` chỉ check prediction của chính spell mình.
- **Trạng thái:** BLOCKED — bỏ nhánh missile-block trong Helper(); giữ nhánh OnProcessSpellCast (W cứu khi AA địch sắp giết ally) vì nhánh này dùng API có thật.
- **Chỗ comment trong code:** `Thresh.h` Helper().

### Player.IssueOrder(GameObjectOrder.MoveTo)
- **C# gốc:** `Thresh.cs:158,167` — `Player.IssueOrder(GameObjectOrder.MoveTo, Game.CursorPos)` trong Push/Pull.
- **Dùng để:** Giữ di chuyển về con trỏ khi giữ phím Push/Pull.
- **Đã search:** như Samira → thay bằng `CoreControl::IssueMove(pos, true)`.
- **Trạng thái:** RESOLVED — dùng `CoreControl::IssueMove`.
- **Chỗ comment trong code:** `Thresh.h` Push_Pull().

## Renata

### GetSummonerSpellDamage(SummonerSpell.Ignite)
- **C# gốc:** `Renata.cs:465` — `attacker.GetSummonerSpellDamage(SoulBound, SummonerSpell.Ignite)` trong AllyChampSaver.
- **Dùng để:** Cộng damage Ignite vào dự đoán incoming-damage để W cứu ally kịp.
- **Đã search:** `GetSummonerSpellDamage`, `SummonerSpell`, `Ignite` trong SDK/ → không có.
- **Trạng thái:** BLOCKED — bỏ term Ignite; vẫn dự đoán AA + Q/W/E/R spell damage. Ước lượng thấp hơn 1 chút khi địch có Ignite.
- **Chỗ comment trong code:** `Renata.h` AllyChampSaver::OnProcessSpellCast.

### GetFirstWallPoint(Vector2, Vector2)
- **C# gốc:** `Renata.cs:300,306,321` — `GetFirstWallPoint(a,b) == Vector2.Zero` (không tường chặn) trong GetBestBackPosition.
- **Dùng để:** Kiểm tra đường fling Q2 có bị tường chặn không.
- **Đã search:** `GetFirstWallPoint`, `FirstWall` trong SDK/ → không có; có `NavMesh::IsWallBetween(from,to,step)` (bool).
- **Trạng thái:** RESOLVED — dùng `NavMesh::IsWallBetween` (đảo nghĩa: true = có tường).
- **Chỗ comment trong code:** `Renata.h` WallBlocked().

### Vector position .IsUnderAllyTurret() / .CountAllysHerosInRangeFix()
- **C# gốc:** `Renata.cs:326,350` — gọi trên Vector2/Vector3 position (không phải unit).
- **Dùng để:** Chấm điểm fling: điểm nằm trong trụ đồng minh? quanh điểm có bao nhiêu đồng minh?
- **Đã search:** free-function `IsUnderAllyTurret(pos)` / `CountAllyHeroesInRange(pos,r)` trong SDK/ → chỉ có method trên unit.
- **Trạng thái:** RESOLVED — helper cục bộ: turret list + distance <= 900 (IsUnderAllyTurretPos), duyệt AllyHeroes + distance (CountAlliesAround).
- **Chỗ comment trong code:** `Renata.h` IsUnderAllyTurretPos/CountAlliesAround.

## Xerath

### AIBaseClient.GetStunDuration()
- **C# gốc:** `Xerath.cs:380` — `Ret.Obj.GetStunDuration()` để tránh E khi target đã bị stun đủ lâu.
- **Dùng để:** Combo E anti-waste: không E nếu target còn stun dài hơn thời gian E bay tới.
- **Đã search:** `GetStunDuration`, `StunDuration`, `CrowdControl` trong SDK/ → không có.
- **Trạng thái:** BLOCKED — bỏ sub-check stun-duration; vẫn E target hợp lệ theo hitchance. Có thể phí E khi target đã stun sẵn (hiếm).
- **Chỗ comment trong code:** `Xerath.h` Combo() (E block).

## Irelia

### Geometry.Circle(...).Points (tường/bụi cỏ tìm điểm E gap)
- **C# gốc:** `Irelia.cs:445` — `new Geometry.Circle(Player.ServerPosition, E.Range).Points.Where(x => x.IsWall() || x.IsBuilding())` để tìm tường đặt E dash.
- **Dùng để:** QLogic gap-E: chọn tường/bụi cỏ xa nhất trong tầm E để dash tới.
- **Đã search:** `Geometry::Circle`, `.Points`, `GrassObject` trong SDK/ → có `SDK/Math/Geometry.h` nhưng không expose Circle.Points sinh điểm; không có GrassObject GameObjects.
- **Trạng thái:** BLOCKED — bỏ nhánh wall/grass-finder; giữ fallback C# (E cast tại fastGapMinion.ServerPosition hoặc Player.ServerPosition) vì dùng API có thật.
- **Chỗ comment trong code:** `Irelia.h` QLogic().

### GameObjects.Get<GrassObject>()
- **C# gốc:** `Irelia.cs:440` — grass object gần nhất trong tầm E.
- **Đã search:** `GrassObject`, `Grass` trong GameObjects → chỉ có `CollisionFlags::Grass`, không có object list.
- **Trạng thái:** BLOCKED — bỏ, dùng fallback minion/player position.
- **Chỗ comment trong code:** `Irelia.h` QLogic().

### AIBaseClient.Buffs (enumerate EndTime/Type — GetStunDuration/GetPassiveDuration)
- **C# gốc:** `Irelia.cs:888,901` — duyệt `target.Buffs` lấy Max EndTime của CC/ireliamark buff.
- **Dùng để:** QLogic quyết Q ngay hay chờ (CC còn > 0 / mark còn <= 200ms).
- **Đã search:** buff-list public accessor → chỉ có `HasBuff/GetBuffCount/HasBuffOfType`, không enumerate EndTime.
- **Trạng thái:** PARTIAL — GetStunDuration thay bằng `HasBuffOfType(Stun/Snare/Knockback/Charm/Suppression)` trả 1000ms nếu có, 0 nếu không; GetPassiveDuration thay bằng `HasBuff("ireliamark")` trả ước lượng.
- **Chỗ comment trong code:** `Irelia.h` GetStunDuration()/GetPassiveDuration().

### HaveSpellShield
- **C# gốc:** `Irelia.cs:520` — lọc địch không có spell shield khi Flee E.
- **Đã search:** `HaveSpellShield`, `SpellShield` → không có accessor.
- **Trạng thái:** PARTIAL — thay bằng `HasBuffOfType(BuffType::SpellShield)`.
- **Chỗ comment trong code:** `Irelia.h` FleeLogic().

### OnPlayAnimation Process cancel (Spell1/Spell3_02 block) + UnitDodge.EvadeTarget (W dodge subsystem)
- **C# gốc:** `Irelia.cs:91-99` block animation; `Irelia.cs:1097-1475` W-dodge spell DB (block targeted CC).
- **Đã search:** `OnPlayAnimation` có event nhưng `Process` là `bool*` raw; EvadeTarget là hệ con lớn.
- **Trạng thái:** PARTIAL — animation-cancel bỏ (chỉ ảnh hưởng hiển thị múa). W-dodge subsystem: port rút gọn — OnProcessSpell địch dùng spell trong DB nhắm mình + HP thấp → W cast tới sender (block). Giữ DB spell 1-1.
- **Chỗ comment trong code:** `Irelia.h` OnEnemyProcessSpell().

## Yasuo

### GameEvent.OnGameTick
- **C# gốc:** `Yasuo.cs:94,284` — `GameEvent.OnGameTick += Game_OnTick` cho R-knockup logic tần suất tick thấp hơn OnUpdate.
- **Dùng để:** Chạy R-on-knockup logic mỗi game-tick.
- **Đã search:** `OnGameTick`, `GameTick` trong SDK/ → không có event tick riêng.
- **Trạng thái:** RESOLVED — gọi Game_OnTick() từ trong Game_OnUpdate() (RDelay tự gating để không spam).
- **Chỗ comment trong code:** `Yasuo.h` Game_OnUpdate()/Game_OnTick().

### AIHeroClient.Glow (highlight target)
- **C# gốc:** `Yasuo.cs:105` — `NearMouseTarget.Glow(Color.Purple, 5, 1)` khi E-mouse mode.
- **Dùng để:** Vẽ viền sáng quanh mục tiêu E-mouse sắp giết.
- **Đã search:** `Glow`, `Highlight` trong SDK/ → không có API render outline unit.
- **Trạng thái:** BLOCKED — bỏ Glow (chỉ ảnh hưởng hiển thị, không ảnh hưởng logic).
- **Chỗ comment trong code:** `Yasuo.h` OnRenderMouseOvers lambda (đã bỏ).

### DelayAction.Add (E fly-time AttackEnabled toggle)
- **C# gốc:** `Yasuo.cs:233` — `DelayAction.Add(FlyTime+300, () => Orbwalker.AttackEnabled = true)` bật lại AA sau khi E bay xong.
- **Dùng để:** Tắt AA trong lúc E dash, bật lại sau fly-time.
- **Đã search:** `DelayAction`, `Delay(` scheduler trong SDK/ → không có timed-callback scheduler.
- **Trạng thái:** PARTIAL — bỏ AttackEnabled toggle (Orbwalker tự xử lý windup). Giữ E cast 1-1.
- **Chỗ comment trong code:** `Yasuo.h` Game_OnDoCast().

### MissileClient.Target (đạn có nhắm mình?)
- **C# gốc:** `Yasuo.cs:1044` — `missile.Target != null && !missile.Target.IsMe` lọc đạn không nhắm mình trong EvadeTarget.
- **Dùng để:** Chỉ dodge đạn targeted nhắm chính mình.
- **Đã search:** `MissileClient::Target` accessor trong SDK/ → chỉ có CasterIndex/SpellName/MissileName, không có Target.
- **Trạng thái:** PARTIAL — bỏ điều kiện missile.Target; dựa DB-match tên đạn + menu toggle per-spell (giữ spell DB 1-1).
- **Chỗ comment trong code:** `Yasuo.h` EvadeTarget ObjSpellMissileOnCreate().

## Brand / Yone

### Me.SetSkin(int) / Me.SkinId (skin changer)
- **C# gốc:** `Brand.cs:144-153`, `Yone.cs:215-224` — `skind()`: `if (Me.SkinId != skinnu) Me.SetSkin(skinnu)`.
- **Dùng để:** Menu MenuSliderButton "Skin" đổi skin model tại chỗ (cosmetic).
- **Đã search:** `SetSkin`, `SkinId` trong SDK/ → không có; không port nào dùng.
- **Trạng thái:** BLOCKED — bỏ skind() (rỗng) + bỏ MenuSliderButton "Skin". Cosmetic thuần, không ảnh hưởng combat.
- **Chỗ comment trong code:** `Brand.h` skind()/BuildMenu(); `Yone.h` skind()/BuildMenu().

### OktwCommon.GetIncomingDamage(target)
- **C# gốc:** `Brand.cs:439-451`, `Yone.cs:420-428` — cộng damage đang bay tới target vào ngưỡng killsteal.
- **Dùng để:** KS chính xác hơn (máu target sẽ giảm do đòn đang bay).
- **Đã search:** `GetIncomingDamage`, incoming-damage predictor global trong SDK/ → không có.
- **Trạng thái:** BLOCKED — helper cục bộ trả 0. KS vẫn hoạt động theo damage spell tính tay; ước lượng thấp hơn 1 chút khi có nhiều đòn bay tới.
- **Chỗ comment trong code:** `Brand.h`/`Yone.h` GetIncomingDamage().

### GetSummonerSpellDamage(SummonerSpell.Ignite)
- **C# gốc:** `Brand.cs:186`, `Yone.cs:231` — cộng damage Ignite vào ComboDamage (draw).
- **Đã search:** `GetSummonerSpellDamage`, `SummonerSpell` trong SDK/ → không có (đã ghi ở Renata).
- **Trạng thái:** BLOCKED — bỏ term Ignite khỏi ComboDamage draw. Chỉ ảnh hưởng chỉ báo "Combo=Kill", không ảnh hưởng cast.
- **Chỗ comment trong code:** `Brand.h`/`Yone.h` ComboDamage().

## Yone

### Dead-code E-damage tracker (ListDmg/GetEDmg)
- **C# gốc:** `Yone.cs:110-156` — `ListDmg`/`DmgOnTarget`/`GetEDmg`/`AIBaseClient_OnBuffAdd` theo dõi damage cho E recast, nhưng buff name trong C# là chuỗi rỗng `""` → luôn no-op (dead code trong bản gốc).
- **Dùng để:** (định) tính true-damage E recast theo damage tích luỹ.
- **Trạng thái:** RESOLVED — port giữ cấu trúc nhưng buff rỗng nên vô hiệu như C#. E cast theo isE2() (mana>0) 1-1.
- **Chỗ comment trong code:** `Yone.h` OnBuffAdd()/GetEDmg().

## Cassiopeia

### Dash.OnDash (auto-Q khi target dash)
- **C# gốc:** `Cassiopeia.cs:114,447` — `Dash.OnDash += OnDash`; nếu địch dash trong tầm E thì Q predict Dashing.
- **Dùng để:** Auto Q chặn dash.
- **Đã search:** `Events::hook.OnDash` (Dash.h) → CÓ. `DashArgs{Unit,EndPos,...}`.
- **Trạng thái:** RESOLVED — hook `Events::hook.OnDash`, dùng `AIBaseClient(args.Unit)` + `Q.CastIfHitchanceEquals(sender, HitChance::Dash)`.
- **Chỗ comment trong code:** `Cassiopeia.h` OnDash().

### HpBarIndicator.drawDmg (damage indicator trên thanh máu)
- **C# gốc:** `Cassiopeia.cs:31,384-394` — `Indicator.drawDmg(...)` vẽ damage lên HP bar.
- **Đã search:** `HpBarIndicator`, `drawDmg` trong SDK/ → không có widget HP-bar indicator.
- **Trạng thái:** BLOCKED — bỏ DrawingOnEnd/indicator. Cosmetic, không ảnh hưởng logic.
- **Chỗ comment trong code:** `Cassiopeia.h` header note.

## LeeSin

### Ward-flash insec (Items.Item ward + Spellbook.CastSpell(Flash, pos))
- **C# gốc:** `LeeSin.cs:520-537` — Insec(): move tới sau target, delay 100ms, R rồi flash sau lưng.
- **Dùng để:** Combo insec R-flash.
- **Đã search:** flash slot qua `GetSpellSlot("SummonerFlash")` + `Spellbook().CanUseSpell` + `CastSpell(slot,pos)` → CÓ; `CoreControl::IssueMove` cho move → CÓ. `DelayAction.Add(100,...)` → không có scheduler.
- **Trạng thái:** PARTIAL — bỏ delay 100ms (cast R + flash ngay trong cùng frame sau IssueMove). Giữ điều kiện & thứ tự 1-1.
- **Chỗ comment trong code:** `LeeSin.h` Insec().
