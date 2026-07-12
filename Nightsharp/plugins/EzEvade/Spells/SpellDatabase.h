#pragma once
// ============================================================================
// SpellDatabase.h — EzEvade skillshot database.
// ----------------------------------------------------------------------------
// Bố cục port 1-1 từ EzEvade C# (`Spells/SpellDatabase.cs`): 1 static list
// `Spells`, khởi tạo 1 lần trong InitSpells(). MỖI entry là 1 block `{ SpellData
// spell; ...; Spells.push_back(spell); }`.
//
// DỮ LIỆU spell lấy từ CommunityDragon `latest` + LoL Wiki (wiki.leagueoflegends.com):
//   * range        <- v1/champions/<id>.json spells[slot].range[0] (gameplay range)
//   * spellDelay   <- <alias>.bin.json  mSpell.mCastTime * 1000 (ms); null → 250
//   * projectileSpeed <- <alias>.bin.json  mMissileSpec...movementComponent.mSpeed
//   * missileName  <- mScriptName của missile
//   * radius (QUAN TRỌNG — dùng cho EVADE, không phải prediction):
//       - Line/missile: = WIDTH đầy đủ theo wiki = 2 × bin.mMissileWidth (đã verify
//         8 champ: Blitz 140=2×70, Ahri Q 200=2×100, Aatrox W 160=2×80, ...).
//         mMissileWidth (=/2) hợp prediction nhưng KHÔNG hợp evade → dùng full width.
//       - Positional/instant (không missile): lấy WIDTH wiki trực tiếp vì targeter
//         indicator trong bin không khớp (Aatrox Q1=180 chứ không phải 400, Briar E=380).
//       - Circle: = RADIUS wiki trực tiếp (Cho'Gath Q=250, Bard R=350, ...).
// ============================================================================
#include "SpellData.h"

#include <vector>

namespace EzEvade {

class SpellDatabase {
public:
    static inline std::vector<SpellData> Spells;

    static void InitSpells() {
        if (!Spells.empty()) {
            return;
        }

        // #region AllChampions
        {
            SpellData spell;
            spell.charName = "AllChampions";
            spell.dangerlevel = 1;
            spell.missileName = "summonersnowball";
            spell.name = "Mark";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 60.0f;
            spell.range = 1600.0f;
            spell.spellDelay = 0;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "summonersnowball";
            spell.extraSpellNames = { "summonerporothrow" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // #endregion AllChampions

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI A
        // ==========================================

        // #region Aatrox
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 2;
            spell.missileName = "AatroxW";
            spell.name = "Infernal Chains";
            spell.projectileSpeed = 1800.0f;   // mMissileSpec.movementComponent.mSpeed
            spell.radius = 160.0f;             // wiki width (=2×80 mMissileWidth)
            spell.range = 825.0f;              // mSpell.castRange
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::W;
            spell.spellName = "AatroxW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // Aatrox Q (The Darkin Blade): positional AoE 3-cast. Width lấy từ WIKI trực tiếp vì
        // targeter indicator trong bin không khớp hitbox (Q1 targeter 400 nhưng wiki width 180).
        // Không có missile => tức thời. Delay = AatroxQWrapperCast mCastTime 0.6.
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 3;
            spell.name = "The Darkin Blade (Cast 1)";
            spell.radius = 180.0f;             // wiki width Cast 1
            spell.range = 850.0f;              // Q1 overrideBaseRange
            spell.spellDelay = 600;            // AatroxQWrapperCast mCastTime 0.6
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "AatroxQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 3;
            spell.name = "The Darkin Blade (Cast 2)";
            spell.angle = 35.0f;               // Q2 TargeterDefinitionCone coneAngleDegrees
            spell.range = 550.0f;              // Q2 coneRange
            spell.spellDelay = 600;            // AatroxQWrapperCast mCastTime 0.6
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "AatroxQ2";
            spell.spellType = SpellType::Cone;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 3;
            spell.name = "The Darkin Blade (Cast 3)";
            spell.radius = 300.0f;             // wiki radius Cast 3 (sweetspot knockup 180)
            spell.range = 300.0f;              // Q3 TargeterDefinitionRange overrideBaseRange
            spell.spellDelay = 600;            // AatroxQWrapperCast mCastTime 0.6
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "AatroxQ3";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        // #endregion Aatrox

        // #region Ahri
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 2;
            spell.missileName = "AhriQMissile";
            spell.name = "Orb of Deception";
            spell.projectileSpeed = 2500.0f;   // AcceleratingMovement: mInitialSpeed 2500 (giảm dần tới mMinSpeed 400)
            spell.radius = 200.0f;             // wiki width (=2×100 mLineWidth)
            spell.range = 970.0f;              // champions/103.json Q range
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "AhriQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 3;
            spell.missileName = "AhriEMissile";
            spell.name = "Charm";
            spell.projectileSpeed = 1550.0f;   // FixedSpeedMovement.mSpeed
            spell.radius = 120.0f;             // wiki width (=2×60 mMissileWidth)
            spell.range = 975.0f;              // champions/103.json E range
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "AhriE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Charm;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 2;
            spell.missileName = "AhriQReturnMissile";
            spell.name = "Orb of Deception (Return)";
            spell.projectileSpeed = 2600.0f;   // AhriQReturnMissile AcceleratingMovement mMaxSpeed (tăng tốc từ 60 để bắt kịp Ahri)
            spell.radius = 200.0f;             // wiki width (=2×100, cùng orb với Q đi)
            spell.range = 970.0f;              // champions/103.json Q range (orb quay về phía Ahri)
            spell.spellDelay = 250;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "AhriQReturn";   // id orb quay về; detection key thực = missileName AhriQReturnMissile (C# gốc: "AhriOrbofDeception2")
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.isSpecial = true;            // orb quay về bám Ahri → SpecialSpells/Ahri set endPos = Ahri.ServerPosition mỗi frame
            Spells.push_back(spell);
        }
        // #endregion Ahri

        // #region Akali
        {
            SpellData spell;
            spell.charName = "Akali";
            spell.dangerlevel = 1;
            spell.name = "Five Point Strike";
            spell.projectileSpeed = 3200.0f;   // AkaliQMis0..5 FixedSpeedMovement.mSpeed
            spell.radius = 120.0f;             // wiki width (=2×60 mMissileWidth từng kunai)
            spell.range = 550.0f;              // champions/84.json Q range
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "AkaliQ";
            spell.spellType = SpellType::Cone; // quạt 5 phi tiêu
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Akali";
            spell.dangerlevel = 1;
            spell.missileName = "AkaliEMis";
            spell.name = "Shuriken Flip";
            spell.projectileSpeed = 1900.0f;   // AkaliEMis FixedSpeedMovement.mSpeed
            spell.radius = 120.0f;             // wiki width (=2×60 mMissileWidth)
            spell.range = 825.0f;              // champions/84.json E range
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "AkaliE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // #endregion Akali

        // #region Akshan
        {
            SpellData spell;
            spell.charName = "Akshan";
            spell.dangerlevel = 1;
            spell.missileName = "AkshanQMissile";
            spell.name = "Avengerang";
            spell.projectileSpeed = 1500.0f;   // AkshanQMissile FixedSpeedMovement.mSpeed (đi ra; AkshanQMissileReturn 2400)
            spell.radius = 120.0f;             // wiki width (=2×60 mLineWidth)
            spell.range = 850.0f;              // champions/166.json Q range
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "AkshanQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // #endregion Akshan

        // Alistar: không có skillshot né được (Q Pulverize = PBAoE quanh mình, W Headbutt = targeted,
        // E Trample = PBAoE, R = self-buff). Bỏ qua.

        // Ambessa (id 799): Q/W/E là melee/dash-combo (không né bằng database). Chỉ R né được.
        // #region Ambessa
        {
            SpellData spell;
            spell.charName = "Ambessa";
            spell.dangerlevel = 3;
            spell.name = "Public Execution";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // line-select tức thời (chọn champ cuối trên đường thẳng), không có missile travel
            spell.radius = 130.0f;             // wiki width (=2×65)
            spell.range = 1250.0f;             // champions/799.json R range
            spell.spellDelay = 700;            // mCastTime 0.7
            spell.spellKey = SpellSlot::R;
            spell.spellName = "AmbessaR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Suppression; // AmbessaRSuppressionDebuff
            Spells.push_back(spell);
        }
        // #endregion Ambessa

        // #region Amumu
        {
            SpellData spell;
            spell.charName = "Amumu";
            spell.dangerlevel = 3;
            spell.missileName = "SadMummyBandageToss";
            spell.name = "Bandage Toss";
            spell.projectileSpeed = 2000.0f;   // SadMummyBandageToss FixedSpeedMovement.mSpeed
            spell.radius = 160.0f;             // wiki width (=2×80 mLineWidth)
            spell.range = 1100.0f;             // champions/32.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "BandageToss";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // #endregion Amumu

        // #region Anivia
        {
            SpellData spell;
            spell.charName = "Anivia";
            spell.dangerlevel = 3;
            spell.missileName = "FlashFrost";
            spell.name = "Flash Frost";
            spell.projectileSpeed = 950.0f;    // FlashFrostSpell FixedSpeedMovement.mSpeed
            spell.radius = 220.0f;             // wiki width (=2×110 mLineWidth)
            spell.range = 1075.0f;             // champions/34.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "FlashFrost";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }
        // #endregion Anivia

        // #region Annie
        {
            SpellData spell;
            spell.charName = "Annie";
            spell.dangerlevel = 1;
            spell.name = "Incinerate";
            spell.radius = 0.0f;               // cone: dùng angle
            spell.angle = 27.5f;               // mTargeterDefinitions coneAngleDegrees
            spell.range = 690.0f;              // coneRange (bin); champions/1.json ghi 600
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::W;
            spell.spellName = "AnnieW";
            spell.spellType = SpellType::Cone;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // #endregion Annie

        // #region Aphelios
        {
            SpellData spell;
            spell.charName = "Aphelios";
            spell.dangerlevel = 2;
            spell.missileName = "ApheliosRMis";
            spell.name = "Moonlight Vigil";
            spell.projectileSpeed = 2050.0f;   // ApheliosRMis FixedSpeedMovement.mSpeed
            spell.radius = 220.0f;             // wiki width (=2×110 mLineWidth)
            spell.range = 1300.0f;             // champions/523.json R range
            spell.spellDelay = 500;            // mCastTime 0.5
            spell.spellKey = SpellSlot::R;
            spell.spellName = "ApheliosR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // #endregion Aphelios

        // #region Ashe
        // Ashe W Volley: bỏ qua (hầu như không né W của Ashe). Q self, E scout — bỏ qua. Chỉ R né.
        {
            SpellData spell;
            spell.charName = "Ashe";
            spell.dangerlevel = 3;
            spell.missileName = "EnchantedCrystalArrow";
            spell.name = "Enchanted Crystal Arrow";
            spell.projectileSpeed = 1500.0f;   // AcceleratingMovement: mInitialSpeed 1500 (tăng tới mMaxSpeed 2100)
            spell.radius = 260.0f;             // wiki width (=2×130 mMissileWidth)
            spell.range = 25000.0f;            // toàn map (castRange placeholder 25000)
            spell.fixedRange = true;
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::R;
            spell.spellName = "EnchantedCrystalArrow";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        // #endregion Ashe

        // AurelionSol (id 136): cơ chế đặc biệt, không phải missile né chuẩn.
        //   Q Breath of Light = tia beam steerable bám cursor (TargeterLine 87.5, range 750),
        //   E Singularity = zone black hole đặt chỗ (~instant), R Falling Star = PBAoE quanh mình.
        // => thuộc SpecialSpells nếu muốn hỗ trợ; không thêm vào database missile.

        // #region Aurora
        {
            SpellData spell;
            spell.charName = "Aurora";
            spell.dangerlevel = 2;
            spell.missileName = "AuroraQ";
            spell.name = "Twofold Hex";
            spell.projectileSpeed = 1600.0f;   // AuroraQ FixedSpeedMovement.mSpeed (return 2000)
            spell.radius = 180.0f;             // wiki width (=2×90 mMissileWidth)
            spell.range = 900.0f;              // champions/893.json Q range
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "AuroraQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aurora";
            spell.dangerlevel = 2;
            spell.name = "The Weirding";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // blast tức thời, không có missile
            spell.radius = 175.0f;             // wiki width trực tiếp (targeter bin 80 không khớp)
            spell.range = 825.0f;              // champions/893.json E range
            spell.spellDelay = 350;            // mCastTime 0.35
            spell.spellKey = SpellSlot::E;
            spell.spellName = "AuroraE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        // #endregion Aurora

        // Azir (id 268): W triệu hồi lính, E shield dash, R Emperor's Divide tường lính — bỏ qua.
        // Q né được nhưng là cơ chế lính (isSpecial + noProcess) — SpecialSpells/Azir tính line thật theo vị trí lính.
        // #region Azir
        {
            SpellData spell;
            spell.charName = "Azir";
            spell.dangerlevel = 2;
            spell.name = "Conquering Sands";
            spell.projectileSpeed = 1000.0f;   // AzirSoldierMissile FixedSpeedMovement.mSpeed
            spell.radius = 140.0f;             // wiki width (=2×70 AzirSoldierMissile mMissileWidth)
            spell.range = 740.0f;              // champions/268.json Q range (lệnh lính; SpecialSpells nới line theo vị trí lính)
            spell.spellDelay = 0;              // C# gốc: 0 (lính thọc gần tức thời khi lệnh)
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "AzirQWrapper";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.isSpecial = true;            // cơ chế lính → SpecialSpells/Azir tính line damage theo vị trí lính
            spell.noProcess = true;            // C# gốc: noProcess (không tạo spell từ OnProcessSpell thường)
            Spells.push_back(spell);
        }
        // #endregion Azir

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI B
        // ==========================================

        // #region Bard
        {
            SpellData spell;
            spell.charName = "Bard";
            spell.dangerlevel = 3;
            spell.missileName = "BardQMissile";
            spell.name = "Cosmic Binding";
            spell.projectileSpeed = 1500.0f;   // BardQMissile FixedSpeedMovement.mSpeed
            spell.radius = 120.0f;             // wiki width (=2×60 mMissileWidth)
            spell.range = 950.0f;              // BardQMissile castRange (BardQ + champions ghi 25000 placeholder)
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "BardQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;       // slow; stun nếu trúng target 2 / tường
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Bard";
            spell.dangerlevel = 3;
            spell.name = "Tempered Fate";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // zone đặt chỗ (thiên thạch rơi), không missile ngang
            spell.radius = 350.0f;             // wiki radius
            spell.range = 3400.0f;             // champions/432.json R range
            spell.spellDelay = 500;            // mCastTime 0.5
            spell.spellKey = SpellSlot::R;
            spell.spellName = "BardR";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Suppression; // stasis (đóng băng bất khả xâm phạm)
            Spells.push_back(spell);
        }
        // #endregion Bard

        // #region Belveth
        // Bel'Veth Q Void Surge = dash tự thân (4 charge), bỏ qua. Chỉ W né được.
        {
            SpellData spell;
            spell.charName = "Belveth";
            spell.dangerlevel = 3;
            spell.name = "Above and Below";
            spell.projectileSpeed = 300000.0f; // BelvethW FixedSpeedMovement.mSpeed (slam ~tức thời)
            spell.radius = 200.0f;             // wiki width (=2×100 mMissileWidth)
            spell.range = 715.0f;              // champions/200.json W range
            spell.spellDelay = 500;            // mCastTime 0.5
            spell.spellKey = SpellSlot::W;
            spell.spellName = "BelvethW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        // #endregion Belveth

        // #region Blitzcrank
        {
            SpellData spell;
            spell.charName = "Blitzcrank";
            spell.dangerlevel = 3;
            spell.missileName = "RocketGrabMissile";
            spell.name = "Rocket Grab";
            spell.projectileSpeed = 1800.0f;   // RocketGrabMissile FixedSpeedMovement.mSpeed
            spell.radius = 140.0f;             // wiki width (=2×70 mMissileWidth)
            spell.range = 1079.0f;             // champions/53.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "RocketGrab";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;       // grab kéo về + stun ngắn
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // #endregion Blitzcrank

        // #region Brand
        {
            SpellData spell;
            spell.charName = "Brand";
            spell.dangerlevel = 2;
            spell.missileName = "BrandQMissile";
            spell.name = "Sear";
            spell.projectileSpeed = 1600.0f;   // BrandQMissile FixedSpeedMovement.mSpeed
            spell.radius = 120.0f;             // wiki width (=2×60 mMissileWidth)
            spell.range = 1050.0f;             // champions/63.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "BrandQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;       // stun nếu target đang Ablaze
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Brand";
            spell.dangerlevel = 2;
            spell.name = "Pillar of Flame";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // AoE tròn đặt chỗ, không missile ngang
            spell.radius = 260.0f;             // wiki radius
            spell.range = 900.0f;              // champions/63.json W range
            spell.spellDelay = 250;            // mCastTime 0.25 (pillar nổ sau delay; đặt ngắn = né sớm, an toàn)
            spell.spellKey = SpellSlot::W;
            spell.spellName = "BrandW";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // #endregion Brand

        // #region Braum
        {
            SpellData spell;
            spell.charName = "Braum";
            spell.dangerlevel = 2;
            spell.missileName = "BraumQMissile";
            spell.name = "Winter's Bite";
            spell.projectileSpeed = 1700.0f;   // BraumQMissile FixedSpeedMovement.mSpeed
            spell.radius = 120.0f;             // wiki width (=2×60 mMissileWidth)
            spell.range = 1000.0f;             // champions/201.json Q range
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "BraumQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;       // slow + stack Concussive (stun sau 4 stack)
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Braum";
            spell.dangerlevel = 3;
            spell.missileName = "BraumRMissile";
            spell.name = "Glacial Fissure";
            spell.projectileSpeed = 1400.0f;   // BraumRMissile FixedSpeedMovement.mSpeed
            spell.radius = 230.0f;             // wiki width (=2×115 mMissileWidth)
            spell.range = 1250.0f;             // champions/201.json R range
            spell.spellDelay = 500;            // BraumRWrapper mCastTime 0.5
            spell.spellKey = SpellSlot::R;
            spell.spellName = "BraumR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        // #endregion Braum

        // #region Briar
        // Briar Q Head Rush / W Blood Frenzy = gap-close/dash, bỏ qua. E và R né được.
        {
            SpellData spell;
            spell.charName = "Briar";
            spell.dangerlevel = 3;
            spell.name = "Chilling Scream";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // scream tức thời, không có missile
            spell.radius = 380.0f;             // wiki width trực tiếp (targeter bin 230 không khớp)
            spell.range = 400.0f;              // champions/233.json E range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::E;
            spell.spellName = "BriarE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockBack;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Briar";
            spell.dangerlevel = 3;
            spell.missileName = "BriarR";
            spell.name = "Certain Death";
            spell.projectileSpeed = 2000.0f;   // BriarR FixedSpeedMovement.mSpeed
            spell.radius = 320.0f;             // wiki width (=2×160 mMissileWidth)
            spell.range = 12000.0f;            // toàn map (castRange 12000)
            spell.spellDelay = 1000;           // mCastTime 1.0
            spell.spellKey = SpellSlot::R;
            spell.spellName = "BriarR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Fear;
            Spells.push_back(spell);
        }
        // #endregion Briar

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI C
        // ==========================================

        // #region Caitlyn
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 2;
            spell.missileName = "CaitlynQMissile";
            spell.name = "Piltover Peacemaker";
            spell.projectileSpeed = 2200.0f;   // CaitlynQMissile FixedSpeedMovement.mSpeed
            spell.radius = 120.0f;             // wiki width (=2×60 mMissileWidth)
            spell.range = 1250.0f;             // champions/51.json Q range
            spell.spellDelay = 250;            // mCastTime null (windup scripted) → default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "CaitlynQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;       // xuyên (piercing), không collision
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 2;
            spell.missileName = "CaitlynEMissile";
            spell.name = "90 Caliber Net";
            spell.projectileSpeed = 1600.0f;   // CaitlynEMissile FixedSpeedMovement.mSpeed
            spell.radius = 140.0f;             // wiki width (=2×70 mMissileWidth)
            spell.range = 750.0f;              // champions/51.json E range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::E;
            spell.spellName = "CaitlynE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 3;
            spell.name = "Yordle Snap Trap";
            spell.radius = 75.0f;              // trap trigger radius — wiki/CDragon không ghi rõ (wiki chỉ collision 15) → giữ giá trị C# gốc 75
            spell.range = 800.0f;              // champions/51.json W range
            spell.spellKey = SpellSlot::W;
            spell.spellName = "CaitlynW";      // C# gốc: "CaitlynYordleTrap"
            spell.trapBaseName = "CaitlynTrap"; // tên object bẫy spawn (C# gốc)
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Snare;      // root khi kích hoạt
            spell.hasTrap = true;
            Spells.push_back(spell);
        }
        // Caitlyn R Ace in the Hole: targeted channel (khóa 1 champ), không phải line né chuẩn.
        // #endregion Caitlyn

        // #region Camille
        // Camille Q empower / E Hookshot dash / R targeted zone — bỏ qua. Chỉ W né được.
        {
            SpellData spell;
            spell.charName = "Camille";
            spell.dangerlevel = 2;
            spell.name = "Tactical Sweep";
            spell.range = 650.0f;              // W TargeterDefinitionCone coneRange (champions ghi 610)
            spell.angle = 35.0f;               // coneAngleDegrees
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::W;
            spell.spellName = "CamilleW";
            spell.spellType = SpellType::Cone;
            spell.ccType = CCType::Slow;       // rìa ngoài slow + true damage
            Spells.push_back(spell);
        }
        // #endregion Camille

        // #region Cassiopeia
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.dangerlevel = 2;
            spell.name = "Noxious Blast";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // AoE tròn đặt chỗ, không missile ngang
            spell.radius = 200.0f;             // wiki radius
            spell.range = 850.0f;              // champions/69.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250 (bloom sau delay)
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "CassiopeiaQ";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.dangerlevel = 3;
            spell.name = "Petrifying Gaze";
            spell.range = 825.0f;              // R coneRange
            spell.angle = 40.0f;               // coneAngleDegrees
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::R;
            spell.spellName = "CassiopeiaR";
            spell.spellType = SpellType::Cone;
            spell.ccType = CCType::Stun;       // stun nếu quay mặt về phía Cassio, else slow
            Spells.push_back(spell);
        }
        // Cassiopeia W Miasma: zone độc đặt chỗ (ground) — cơ chế multi-AoE đặc biệt.
        // #endregion Cassiopeia

        // #region Chogath
        {
            SpellData spell;
            spell.charName = "Chogath";
            spell.dangerlevel = 3;
            spell.name = "Rupture";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // AoE tròn đặt chỗ, không missile ngang
            spell.radius = 250.0f;             // wiki radius
            spell.range = 950.0f;              // champions/31.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250 (nổ sau delay ~0.6s)
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "Rupture";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Chogath";
            spell.dangerlevel = 3;
            spell.name = "Feral Scream";
            spell.range = 675.0f;              // W coneRange (champions ghi 300)
            spell.angle = 28.0f;               // coneAngleDegrees
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::W;
            spell.spellName = "FeralScream";
            spell.spellType = SpellType::Cone;
            spell.ccType = CCType::Silence;
            Spells.push_back(spell);
        }
        // #endregion Chogath

        // #region Corki
        // Corki W Valkyrie = dash trail, E Gatling Gun = cone self ngắn — bỏ qua. Q + R né được.
        {
            SpellData spell;
            spell.charName = "Corki";
            spell.dangerlevel = 2;
            spell.name = "Phosphorus Bomb";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // AoE tròn đặt chỗ, không missile ngang
            spell.radius = 275.0f;             // wiki radius
            spell.range = 825.0f;              // champions/42.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "PhosphorusBomb";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Corki";
            spell.dangerlevel = 1;
            spell.missileName = "MissileBarrageMissile";
            spell.name = "Missile Barrage";
            spell.projectileSpeed = 2000.0f;   // MissileBarrageMissile FixedSpeedMovement.mSpeed
            spell.radius = 80.0f;              // wiki width (=2×40 mMissileWidth)
            spell.range = 1225.0f;             // champions/42.json R range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::R;
            spell.spellName = "MissileBarrage";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // #endregion Corki

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI D
        // ==========================================

        // #region Darius
        // Darius W AA-empower, R execute targeted — bỏ qua. Q (ring quanh mình, isSpecial) + E né.
        {
            SpellData spell;
            spell.charName = "Darius";
            spell.dangerlevel = 2;
            spell.name = "Decimate";
            spell.radius = 425.0f;             // wiki radius (= CDragon overrideRadius; ring quanh Darius)
            spell.range = 425.0f;
            spell.spellDelay = 234;            // mCastTime 0.234 (C# gốc dùng 750)
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "DariusCleave";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            spell.isSpecial = true;            // caster-centered → SpecialSpells/Darius cập nhật startPos/endPos theo Darius mỗi frame (skip missile tracking)
            spell.defaultOff = true;           // C# gốc: defaultOff (ring quanh địch, tắt mặc định)
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Darius";
            spell.dangerlevel = 2;
            spell.name = "Apprehend";
            spell.range = 535.0f;              // E coneRange
            spell.angle = 25.0f;               // coneAngleDegrees
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "DariusAxeGrabCone";
            spell.spellType = SpellType::Cone;
            spell.ccType = CCType::KnockBack;  // kéo enemy về phía Darius
            Spells.push_back(spell);
        }
        // #endregion Darius

        // #region Diana
        // Diana W orbs / E Lunar Rush dash / R Moonfall PBAoE — bỏ qua. Chỉ Q né được.
        {
            SpellData spell;
            spell.charName = "Diana";
            spell.dangerlevel = 2;
            spell.name = "Crescent Strike";
            spell.projectileSpeed = 2100.0f;   // DianaQOuterMissile FixedSpeedMovement.mSpeed (inner 1900)
            spell.radius = 140.0f;             // wiki width (=2×70 mMissileWidth)
            spell.range = 900.0f;              // champions/131.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "DianaQ";
            spell.spellType = SpellType::Arc;  // TargeterDefinitionArc (đường cong)
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // #endregion Diana

        // #region DrMundo
        // Dr. Mundo W/E/R = self — bỏ qua. Chỉ Q né được.
        {
            SpellData spell;
            spell.charName = "DrMundo";
            spell.dangerlevel = 2;
            spell.missileName = "DrMundoQ";
            spell.name = "Infected Bonesaw";
            spell.projectileSpeed = 2000.0f;   // DrMundoQ FixedSpeedMovement.mSpeed
            spell.radius = 120.0f;             // wiki width (=2×60 mMissileWidth)
            spell.range = 975.0f;              // champions/36.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "DrMundoQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // #endregion DrMundo

        // #region Draven
        // Draven Q Spinning Axe / W Blood Rush = self — bỏ qua. E và R né được.
        {
            SpellData spell;
            spell.charName = "Draven";
            spell.dangerlevel = 2;
            spell.missileName = "DravenDoubleShotMissile";
            spell.name = "Stand Aside";
            spell.projectileSpeed = 1400.0f;   // DravenDoubleShotMissile FixedSpeedMovement.mSpeed
            spell.radius = 260.0f;             // wiki width (=2×130 mMissileWidth)
            spell.range = 1050.0f;             // champions/119.json E range
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "DravenDoubleShot";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockBack;  // đẩy + slow
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Draven";
            spell.dangerlevel = 1;
            spell.missileName = "DravenR";
            spell.name = "Whirling Death";
            spell.projectileSpeed = 2000.0f;   // DravenR AcceleratingMovement mInitialSpeed 2000 (=mMaxSpeed)
            spell.radius = 320.0f;             // wiki width (=2×160 mMissileWidth)
            spell.range = 20000.0f;            // toàn map (castRange 20000, boomerang quay về)
            spell.fixedRange = true;
            spell.spellDelay = 500;            // mCastTime 0.5
            spell.spellKey = SpellSlot::R;
            spell.spellName = "DravenRCast";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // #endregion Draven

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI E
        // ==========================================

        // #region Ekko
        // Ekko E Phase Dive = dash — bỏ qua. Q (out + return special), W, R (special) né.
        {
            SpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 2;
            spell.missileName = "EkkoQMis";
            spell.name = "Timewinder";
            spell.projectileSpeed = 1650.0f;   // EkkoQMis FixedSpeedMovement.mSpeed (đi ra)
            spell.radius = 120.0f;             // wiki width đi ra (=2×60)
            spell.range = 1075.0f;             // champions/245.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "EkkoQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 2;
            spell.missileName = "EkkoQReturn";
            spell.name = "Timewinder (Return)";
            spell.projectileSpeed = 2300.0f;   // EkkoQReturn FixedSpeedMovement.mSpeed
            spell.radius = 200.0f;             // wiki width lúc quay về (=2×100)
            spell.range = 1250.0f;             // C# gốc (orb quay về Ekko)
            spell.spellDelay = 0;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "EkkoQReturn";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.isSpecial = true;            // orb quay về bám Ekko → SpecialSpells/Ekko set endPos = Ekko mỗi frame
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 3;
            spell.name = "Parallel Convergence";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // zone đặt chỗ, không missile ngang
            spell.radius = 375.0f;             // wiki radius (= C# 375)
            spell.range = 1600.0f;             // champions/245.json W range
            spell.spellDelay = 3750;           // C# gốc: zone nổ sau ~3.75s (hoặc khi Ekko vào)
            spell.spellKey = SpellSlot::W;
            spell.spellName = "EkkoW";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 3;
            spell.missileName = "EkkoR";
            spell.name = "Chronobreak";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // teleport về vị trí quá khứ + AoE, không missile ngang
            spell.radius = 375.0f;             // wiki radius (= C# 375)
            spell.range = 1600.0f;             // C# gốc / champions
            spell.spellDelay = 250;            // C# gốc
            spell.spellKey = SpellSlot::R;
            spell.spellName = "EkkoR";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            spell.isSpecial = true;            // tâm = vị trí Ekko ~4s trước → SpecialSpells/Ekko tính vị trí return
            Spells.push_back(spell);
        }
        // #endregion Ekko

        // #region Elise
        // Elise Q Neurotoxin targeted, W Volatile Spiderling = unit homing (không phải skillshot), R transform — bỏ qua. Chỉ E né.
        {
            SpellData spell;
            spell.charName = "Elise";
            spell.dangerlevel = 3;
            spell.missileName = "EliseHumanEMissile";
            spell.name = "Cocoon";
            spell.projectileSpeed = 1600.0f;   // EliseHumanEMissile FixedSpeedMovement.mSpeed
            spell.radius = 110.0f;             // wiki width (=2×55 mLineWidth)
            spell.range = 1075.0f;             // champions/60.json E range
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "EliseHumanE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // #endregion Elise

        // #region Evelynn
        // Evelynn W Allure targeted, E Whiplash dash melee — bỏ qua. Q + R né được.
        {
            SpellData spell;
            spell.charName = "Evelynn";
            spell.dangerlevel = 1;
            spell.missileName = "EvelynnQ";
            spell.name = "Hate Spike";
            spell.projectileSpeed = 2400.0f;   // EvelynnQ FixedSpeedMovement.mSpeed
            spell.radius = 120.0f;             // wiki width đầu (=2×60)
            spell.range = 800.0f;              // champions/28.json Q range
            spell.spellDelay = 250;            // mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "EvelynnQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Evelynn";
            spell.dangerlevel = 3;
            spell.name = "Last Caress";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // burst tức thời rồi blink lùi, không missile ngang
            spell.angle = 180.0f;              // wiki: cone 180° (nửa vòng trước mặt)
            spell.range = 500.0f;              // wiki effect radius
            spell.spellDelay = 349;            // mCastTime 0.35
            spell.spellKey = SpellSlot::R;
            spell.spellName = "EvelynnR";
            spell.spellType = SpellType::Cone;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // #endregion Evelynn

        // #region Ezreal
        // Ezreal E Arcane Shift = blink — bỏ qua. Q, W, R né được.
        {
            SpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 1;
            spell.missileName = "EzrealQ";
            spell.name = "Mystic Shot";
            spell.projectileSpeed = 2000.0f;   // EzrealQ FixedSpeedMovement.mSpeed
            spell.radius = 120.0f;             // wiki width (=2×60)
            spell.range = 1150.0f;             // champions/81.json Q range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "EzrealQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;       // slow tùy item
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 1;
            spell.missileName = "EzrealW";
            spell.name = "Essence Flux";
            spell.projectileSpeed = 1700.0f;   // EzrealW FixedSpeedMovement.mSpeed
            spell.radius = 160.0f;             // wiki width (=2×80)
            spell.range = 1150.0f;             // champions/81.json W range
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::W;
            spell.spellName = "EzrealW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 2;
            spell.missileName = "EzrealR";
            spell.name = "Trueshot Barrage";
            spell.projectileSpeed = 2000.0f;   // EzrealR FixedSpeedMovement.mSpeed
            spell.radius = 320.0f;             // wiki width (=2×160)
            spell.range = 25000.0f;            // toàn map (castRange placeholder 25000)
            spell.fixedRange = true;
            spell.spellDelay = 250;            // mCastTime null → default 250
            spell.spellKey = SpellSlot::R;
            spell.spellName = "EzrealR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // #endregion Ezreal

        // #region Fiddlesticks
        // E Reap is a crescent-shaped area. CDragon exposes mLineWidth=70,
        // missileSpeed=1800 and targeter widths 400/100, but no standard cone
        // angle; the Wiki also does not publish a usable width/angle pair.
        // Defer it until the custom geometry is mapped; do not invent one.
        // Q is targeted fear, W is a self-centered drain and R is a channelled
        // self-centered AoE, so they are not standard database missiles.
        // #endregion Fiddlesticks

        // #region Fiora
        {
            SpellData spell;
            spell.charName = "Fiora";
            spell.dangerlevel = 2;
            spell.missileName = "FioraWMissile";
            spell.name = "Riposte";
            spell.projectileSpeed = 3200.0f;   // CDragon FioraWMissile.mSpell.mMissileSpec.movementComponent.mSpeed
            spell.radius = 140.0f;             // Wiki width 140 (=2x CDragon mMissileWidth 70)
            spell.range = 750.0f;              // champions/114.json W range; bin castRange=3000 is internal
            spell.spellDelay = 9.999999776482582f; // CDragon mCastTime=0.009999999776482582 * 1000
            spell.spellKey = SpellSlot::W;
            spell.spellName = "FioraW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;      // conditional stun when Riposte blocks immobilizing CC
            Spells.push_back(spell);
        }
        // #endregion Fiora

        // #region Fizz

        {
            SpellData spell;
            spell.charName = "Fizz";
            spell.dangerlevel = 4;
            spell.missileName = "FizzRMissile";
            spell.name = "Chum the Waters";
            spell.projectileSpeed = 1300.0f;   // CDragon FizzRMissile.mSpell.mMissileSpec.movementComponent.mSpeed
            spell.radius = 160.0f;             // CDragon mMissileWidth=80; full evade width=2x80
            spell.range = 1300.0f;             // champions/105.json R range; bin display override=1300
            spell.spellDelay = 250.0f;         // CDragon mCastTime=0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "FizzR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.useEndPosition = true;
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 325.0f;    // Wiki eruption radius: 200 / 325 / 450 by travel distance
            spell.isSpecial = true;            // SpecialSpells/Fizz tracks the fish and dynamic eruption radius
            Spells.push_back(spell);
        }
        // #endregion Fizz

        // #region G
        // Gangplank E/R, Garen and the Gnar/Gragas dash/self-AoE spells are
        // not standard incoming skillshots; leave them for special handling.

        // #region Galio
        {
            SpellData spell;
            spell.charName = "Galio";
            spell.dangerlevel = 2;
            spell.missileName = "GalioQMissile";
            spell.name = "Winds of War";
            spell.projectileSpeed = 1400.0f;   // CDragon GalioQMissile.mSpell.missileSpeed
            spell.radius = 120.0f;             // Wiki width 120
            spell.range = 825.0f;              // champions/3.json Q range
            spell.spellDelay = 250.0f;         // CDragon mCastTime null -> default 250
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "GalioQ";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Galio";
            spell.dangerlevel = 3;
            spell.name = "Justice Punch";
            spell.projectileSpeed = std::numeric_limits<float>::max(); // CDragon GalioE không có missile; cast line tức thời
            spell.radius = 160.0f;             // CDragon GalioE mLineWidth
            spell.range = 650.0f;              // CDragon GalioE castRangeDisplayOverride
            spell.spellDelay = 250.0f;         // CDragon mCastTime null -> default 250
            spell.spellKey = SpellSlot::E;
            spell.spellName = "GalioE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        // #endregion Galio

        // #region Gnar
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 2;
            spell.missileName = "GnarQMissile";
            spell.name = "Boomerang Throw";
            spell.projectileSpeed = 2500.0f;   // CDragon GnarQMissile.mSpell.missileSpeed
            spell.radius = 110.0f;             // Wiki Mini width 110
            spell.range = 1100.0f;             // champions/150.json Q range
            spell.spellDelay = 250.0f;         // CDragon GnarQMissile.mCastTime=0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "GnarQ";
            spell.spellType = SpellType::Line;
            spell.extraMissileNames = { "GnarQMissileReturn" };
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 2;
            spell.missileName = "GnarBigQMissile";
            spell.name = "Boulder Toss";
            spell.projectileSpeed = 2100.0f;   // CDragon GnarBigQMissile.mSpell.missileSpeed
            spell.radius = 150.0f;             // Wiki Mega width 150
            spell.range = 1100.0f;             // champions/150.json Q range
            spell.spellDelay = 500.0f;         // CDragon GnarBigQMissile.mCastTime=0.5
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "GnarBigQ";
            spell.spellType = SpellType::Line;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 3;
            spell.name = "Wallop";
            spell.radius = 200.0f;             // Wiki width 200
            spell.range = 525.0f;              // CDragon GnarBigW targeter range
            spell.spellDelay = 600.0000238418579f; // CDragon mCastTime=0.6000000238418579 * 1000
            spell.spellKey = SpellSlot::W;
            spell.spellName = "GnarBigW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }
        // #endregion Gnar

        // #region Gragas
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 2;
            spell.missileName = "GragasQMissile";
            spell.name = "Barrel Roll";
            spell.projectileSpeed = 1000.0f;   // CDragon GragasQMissile.mSpell.missileSpeed
            spell.radius = 250.0f;             // Wiki effect radius 250
            spell.range = 850.0f;              // champions/79.json Q range
            spell.spellDelay = 250.0f;         // CDragon mCastTime=0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "GragasQ";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.extraDrawHeight = 50.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 2;
            spell.name = "Barrel Roll";
            spell.radius = 250.0f;             // Wiki effect radius 250
            spell.range = 850.0f;              // champions/79.json Q range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "GragasQ";
            spell.spellType = SpellType::Circle;
            spell.extraDrawHeight = 45.0f;
            spell.hasTrap = true;              // C# trap entry; runtime troy-name mapping is separate
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 4;
            spell.missileName = "GragasRBoom";
            spell.name = "Explosive Cask";
            spell.projectileSpeed = 1800.0f;   // CDragon GragasRBoom.mSpell.missileSpeed
            spell.radius = 400.0f;             // Wiki effect radius 400
            spell.range = 1000.0f;             // champions/79.json R range
            spell.spellDelay = 250.0f;         // CDragon mCastTime=0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "GragasR";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockBack;
            Spells.push_back(spell);
        }
        // #endregion Gragas

        // #region Graves
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 3;
            spell.missileName = "GravesQLineMis";
            spell.name = "End of the Line";
            spell.projectileSpeed = 3000.0f;   // CDragon GravesQLineMis.mSpell.missileSpeed
            spell.radius = 80.0f;              // Wiki outbound width 80
            spell.range = 925.0f;              // champions/104.json Q range
            spell.spellDelay = 250.0f;         // CDragon mCastTime null -> default 250
            spell.extraEndTime = 1300.0f;      // C# detector timing
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "GravesQLineSpell";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            spell.isSpecial = true;            // SpecialSpells/Graves handles trail and return
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 2;
            spell.missileName = "GravesQReturn";
            spell.name = "End of the Line (Return)";
            spell.projectileSpeed = 1600.0f;   // CDragon GravesQReturn.mSpell.missileSpeed
            spell.radius = 200.0f;             // Wiki return width 200
            spell.range = 925.0f;              // champions/104.json Q range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "GravesQLineSpell";
            spell.spellType = SpellType::Line;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 1;
            spell.defaultOff = true;
            spell.missileName = "GravesSmokeGrenadeBoom";
            spell.name = "Smoke Screen";
            spell.projectileSpeed = 1500.0f;   // CDragon GravesSmokeGrenadeBoom.mSpell.missileSpeed
            spell.radius = 200.0f;             // Wiki effect radius 200
            spell.range = 950.0f;              // champions/104.json W range
            spell.spellDelay = 250.0f;         // CDragon mCastTime null -> default 250
            spell.spellKey = SpellSlot::W;
            spell.spellName = "GravesSmokeGrenade";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 4;
            spell.missileName = "GravesChargeShotShot";
            spell.name = "Collateral Damage";
            spell.projectileSpeed = 2100.0f;   // CDragon GravesChargeShotShot.mSpell.missileSpeed
            spell.radius = 200.0f;             // Wiki shell width 200
            spell.range = 1000.0f;             // champions/104.json R range
            spell.spellDelay = 250.0f;         // CDragon mCastTime=0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "GravesChargeShot";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 4;
            spell.missileName = "GravesChargeShotFxMissile";
            spell.name = "Collateral Damage (Explosion)";
            spell.projectileSpeed = 2000.0f;   // CDragon GravesChargeShotFxMissile.mSpell.missileSpeed
            spell.radius = 200.0f;             // Wiki explosion cone width 200
            spell.range = 1000.0f;             // champions/104.json R range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "GravesChargeShotFxMissile";
            spell.extraMissileNames = { "GravesChargeShotFxMissile2" };
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        // #endregion Graves

        // #region Gwen
        {
            SpellData spell;
            spell.charName = "Gwen";
            spell.dangerlevel = 3;
            spell.missileName = "GwenRMis";
            spell.name = "Needlework";
            spell.projectileSpeed = 1800.0f;   // CDragon GwenRMis.mSpell.mMissileSpec.mSpeed
            spell.radius = 240.0f;             // Wiki width 240 -> 0 by travel distance
            spell.range = 1200.0f;             // champions/887.json R range
            spell.spellDelay = 250.0f;         // CDragon mCastTime=0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "GwenR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        // #endregion Gwen
        // #endregion G

        // #region H
        // Hecarim R: spectral riders are the dodgeable line; the fear at the end is
        // part of the cast but is not modeled as a second self-centered spell.
        {
            SpellData spell;
            spell.charName = "Hecarim";
            spell.dangerlevel = 4;
            spell.missileName = "HecarimUltMissile";
            spell.name = "Onslaught of Shadows";
            spell.projectileSpeed = 1100.0f;   // CDragon HecarimUltMissile.missileSpeed
            spell.radius = 80.0f;              // Wiki width 80 (=2x CDragon mLineWidth 40)
            spell.range = 1650.0f;             // CDragon HecarimUltMissile range
            spell.spellDelay = 10.0f;          // CDragon HecarimUlt mCastTime 0.01
            spell.spellKey = SpellSlot::R;
            spell.spellName = "HecarimUlt";
            spell.extraMissileNames = {
                "HecarimUltMissileGrab", "HecarimUltMissileGrabEmpty",
                "HecarimUltMissileSkn4C", "HecarimUltMissileSkn4L1",
                "HecarimUltMissileSkn4L2", "HecarimUltMissileSkn4R1",
                "HecarimUltMissileSkn4R2"
            };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Fear;
            spell.usePackets = true;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }

        // Heimerdinger Q turret beams are turret-special events, not a current
        // champion spell/missile entry; Q itself is not added here.
        {
            SpellData spell;
            spell.charName = "Heimerdinger";
            spell.dangerlevel = 2;
            spell.missileName = "HeimerdingerWAttack2";
            spell.name = "Hextech Micro-Rockets";
            spell.projectileSpeed = 750.0f;    // CDragon HeimerdingerWAttack2.missileSpeed
            spell.radius = 80.0f;              // CDragon mLineWidth 40 -> full evade width
            spell.range = 1325.0f;             // champions/74.json W range
            spell.spellDelay = 250.0f;         // CDragon HeimerdingerW mCastTime 0.25
            spell.spellKey = SpellSlot::W;
            spell.spellName = "HeimerdingerW";
            spell.extraMissileNames = { "HeimerdingerWAttack2Ult" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Heimerdinger";
            spell.dangerlevel = 3;
            spell.missileName = "HeimerdingerESpell";
            spell.name = "CH-2 Electron Storm Grenade";
            spell.projectileSpeed = 1200.0f;   // CDragon HeimerdingerESpell.missileSpeed
            spell.radius = 250.0f;             // CDragon/Wiki outer effect radius
            spell.range = 970.0f;              // champions/74.json E range
            spell.spellDelay = 250.0f;         // CDragon HeimerdingerE mCastTime 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "HeimerdingerE";
            spell.extraMissileNames = {
                "HeimerdingerESpell_ult", "HeimerdingerESpell_ult2", "HeimerdingerESpell_ult3"
            };
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // Hwei W is utility; EW/EE are a homing setup and an angled jaw shape
        // without an exact Line/Circle representation in the current enum.
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 2;
            spell.name = "Devastating Fire";
            spell.projectileSpeed = 2000.0f;   // Wiki projectile speed
            spell.radius = 100.0f;             // Wiki width
            spell.range = 800.0f;              // champions/910.json QQ range
            spell.spellDelay = 250.0f;         // CDragon HweiQQ mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "HweiQQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 2;
            spell.name = "Severing Bolt";
            spell.radius = 225.0f;             // Wiki effect radius
            spell.range = 1900.0f;             // CDragon HweiQW range / Wiki target range
            spell.spellDelay = 500.0f;         // CDragon HweiQW mCastTime 0.5
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "HweiQW";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 2;
            spell.name = "Molten Fissure";
            spell.radius = 225.0f;             // Wiki width/effect radius
            spell.range = 1200.0f;             // CDragon HweiQE range / Wiki range
            spell.spellDelay = 350.0f;         // CDragon HweiQE mCastTime 0.35
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "HweiQE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 3;
            spell.name = "Grim Visage";
            spell.projectileSpeed = 1300.0f;   // Wiki projectile speed
            spell.radius = 70.0f;              // Wiki width
            spell.range = 1100.0f;             // CDragon HweiEQ range / Wiki range
            spell.spellDelay = 250.0f;         // CDragon HweiEQ mCastTime 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "HweiEQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Fear;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 4;
            spell.name = "Spiraling Despair";
            spell.projectileSpeed = 1400.0f;   // Wiki projectile speed
            spell.radius = 180.0f;             // Wiki missile width
            spell.range = 1300.0f;             // champions/910.json R range
            spell.spellDelay = 250.0f;         // CDragon HweiR mCastTime 0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "HweiR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        // #endregion H

        // #region I
        {
            SpellData spell;
            spell.charName = "Illaoi";
            spell.dangerlevel = 3;
            spell.name = "Tentacle Smash";
            spell.radius = 200.0f;             // Wiki width
            spell.range = 850.0f;              // champions/420.json Q range
            spell.spellDelay = 750.0f;         // CDragon IllaoiQ mCastTime 0.75
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "IllaoiQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Illaoi";
            spell.dangerlevel = 3;
            spell.missileName = "IllaoiEMis";
            spell.name = "Test of Spirit";
            spell.projectileSpeed = 1900.0f;   // CDragon IllaoiEMis.missileSpeed
            spell.radius = 100.0f;             // Wiki width
            spell.range = 900.0f;              // champions/420.json E range
            spell.spellDelay = 250.0f;         // CDragon IllaoiE mCastTime 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "IllaoiE";
            spell.extraMissileNames = { "IllaoiESpiritMissile" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.dangerlevel = 2;
            spell.name = "Defiant Dance (Release)";
            spell.radius = 240.0f;             // Wiki width
            spell.range = 825.0f;              // CDragon Irelia W display range
            spell.spellDelay = 250.0f;         // default for null mCastTime
            spell.spellKey = SpellSlot::W;
            spell.spellName = "IreliaW2";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.dangerlevel = 3;
            spell.missileName = "IreliaEMissile";
            spell.name = "Flawless Duet";
            spell.projectileSpeed = 2000.0f;   // CDragon IreliaEMissile.missileSpeed
            spell.radius = 180.0f;             // CDragon mLineWidth 90 -> full evade width
            spell.range = 850.0f;              // champions/39.json E range
            spell.spellDelay = 250.0f;         // convergence delay
            spell.spellKey = SpellSlot::E;
            spell.spellName = "IreliaE2";
            spell.extraMissileNames = { "IreliaEParticleMissile" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.dangerlevel = 4;
            spell.missileName = "IreliaR";
            spell.name = "Vanguard's Edge";
            spell.projectileSpeed = 2000.0f;   // CDragon IreliaR.missileSpeed
            spell.radius = 320.0f;             // Wiki width
            spell.range = 950.0f;              // champions/39.json R range
            spell.spellDelay = 250.0f;         // CDragon mCastTime null -> default 250
            spell.spellKey = SpellSlot::R;
            spell.spellName = "IreliaR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Ivern";
            spell.dangerlevel = 3;
            spell.missileName = "IvernQ";
            spell.name = "Rootcaller";
            spell.projectileSpeed = 1300.0f;   // CDragon IvernQ.missileSpeed
            spell.radius = 160.0f;             // Wiki width
            spell.range = 1125.0f;             // champions/427.json Q range
            spell.spellDelay = 250.0f;         // CDragon IvernQ mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "IvernQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // Ivern W/E/R are brush, ally shield, and pet commands; not incoming skillshots.
        // #endregion I

        // #region J
        {
            SpellData spell;
            spell.charName = "Janna";
            spell.dangerlevel = 3;
            spell.missileName = "HowlingGaleSpell";
            spell.name = "Howling Gale";
            spell.projectileSpeed = 667.67f;   // CDragon HowlingGaleSpell.missileSpeed
            spell.radius = 240.0f;             // Wiki width
            spell.range = 1700.0f;             // Wiki max travel range
            spell.spellDelay = 0.0f;           // spell is tracked when released
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "JannaQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.usePackets = true;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "JarvanIV";
            spell.dangerlevel = 2;
            spell.name = "Dragon Strike";
            spell.radius = 140.0f;             // Wiki width 136 / CDragon mLineWidth 70
            spell.range = 770.0f;              // CDragon JarvanIVDragonStrike range
            spell.spellDelay = 250.0f;         // default for null mCastTime
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "JarvanIVDragonStrike";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "JarvanIV";
            spell.dangerlevel = 1;
            spell.name = "Demacian Standard";
            spell.radius = 175.0f;             // CDragon castRadius
            spell.range = 860.0f;              // champions/59.json E range
            spell.spellDelay = 250.0f;         // default for null mCastTime
            spell.spellKey = SpellSlot::E;
            spell.spellName = "JarvanIVDemacianStandard";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "JarvanIV";
            spell.dangerlevel = 3;
            spell.name = "Cataclysm";
            spell.radius = 350.0f;             // Wiki effect radius
            spell.range = 650.0f;              // CDragon JarvanIVCataclysm range
            spell.spellDelay = 250.0f;         // default for null mCastTime
            spell.spellKey = SpellSlot::R;
            spell.spellName = "JarvanIVCataclysm";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            spell.defaultOff = true;
            spell.isSpecial = true;            // creates terrain around the cast point
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Jayce";
            spell.dangerlevel = 2;
            spell.missileName = "JayceShockBlastMis";
            spell.name = "Shock Blast";
            spell.projectileSpeed = 1450.0f;   // CDragon JayceShockBlastMis.missileSpeed
            spell.radius = 140.0f;             // Wiki width
            spell.range = 1050.0f;             // champions/126.json Q range
            spell.spellDelay = 250.0f;         // CDragon mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "JayceShockBlast";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 210.0f;    // CDragon castRadius
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jayce";
            spell.dangerlevel = 3;
            spell.missileName = "JayceShockBlastWallMis";
            spell.name = "Shock Blast (Through Acceleration Gate)";
            spell.projectileSpeed = 2350.0f;   // CDragon JayceShockBlastWallMis.missileSpeed
            spell.radius = 140.0f;             // Wiki width
            spell.range = 1600.0f;             // Wiki accelerated range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "JayceShockBlastWall";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 210.0f;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.dangerlevel = 3;
            spell.missileName = "JinxWMissile";
            spell.name = "Zap!";
            spell.projectileSpeed = 3300.0f;   // CDragon JinxWMissile.missileSpeed
            spell.radius = 120.0f;             // Wiki width
            spell.range = 1500.0f;             // CDragon JinxWMissile range
            spell.spellDelay = 600.0f;         // CDragon JinxWMissile mCastTime 0.6
            spell.spellKey = SpellSlot::W;
            spell.spellName = "JinxW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.dangerlevel = 3;
            spell.name = "Flame Chompers!";
            spell.radius = 225.0f;             // Wiki effect radius
            spell.range = 925.0f;              // CDragon/champions/222.json E range
            spell.spellDelay = 400.0f;         // Wiki landing delay
            spell.spellKey = SpellSlot::E;
            spell.spellName = "JinxE";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Snare;
            spell.hasTrap = true;
            spell.trapBaseName = "jinxmine";
            spell.updatePosition = false;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.dangerlevel = 4;
            spell.name = "Super Mega Death Rocket!";
            spell.projectileSpeed = 1700.0f;   // CDragon JinxR.missileSpeed
            spell.radius = 280.0f;             // Wiki width
            spell.range = 25000.0f;            // global line
            spell.spellDelay = 600.0f;         // CDragon JinxR mCastTime 0.6
            spell.spellKey = SpellSlot::R;
            spell.spellName = "JinxR";
            spell.extraMissileNames = { "JinxRWrapper" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.dangerlevel = 3;
            spell.name = "Deadly Flourish";
            spell.radius = 90.0f;              // Wiki width
            spell.range = 3000.0f;             // CDragon/Jhin W display range
            spell.spellDelay = 750.0f;         // CDragon JhinW mCastTime 0.75
            spell.spellKey = SpellSlot::W;
            spell.spellName = "JhinW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Snare;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.dangerlevel = 3;
            spell.missileName = "JhinRShotMis";
            spell.name = "Curtain Call";
            spell.projectileSpeed = 5000.0f;   // CDragon JhinRShotMis.missileSpeed
            spell.radius = 160.0f;             // Wiki width
            spell.range = 3500.0f;             // Wiki range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "JhinRShot";
            spell.extraMissileNames = { "JhinRShotMis4" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        // Jax has no incoming skillshot; Jhin Q/E are targeted/trap mechanics.
        // #endregion J

        // #region K
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.dangerlevel = 2;
            spell.name = "Ntofo Strikes";
            spell.radius = 100.0f;             // Wiki width
            spell.range = 450.0f;              // CDragon KSanteQ range
            spell.spellDelay = 350.0f;         // CDragon KSanteQ mCastTime 0.35
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "KSanteQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.dangerlevel = 3;
            spell.missileName = "KSanteQ3Missile";
            spell.name = "Ntofo Strikes (Q3)";
            spell.projectileSpeed = 1800.0f;   // CDragon KSanteQ3Missile.missileSpeed
            spell.radius = 120.0f;             // CDragon mLineWidth 60 -> full evade width
            spell.range = 800.0f;              // CDragon display range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "KSanteQ3";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.dangerlevel = 3;
            spell.name = "Footwork";
            spell.projectileSpeed = 1500.0f;   // CDragon KSanteW.missileSpeed
            spell.radius = 110.0f;             // CDragon mLineWidth 55 -> full evade width
            spell.range = 600.0f;              // CDragon display range
            spell.spellDelay = 250.0f;         // default for null mCastTime
            spell.spellKey = SpellSlot::W;
            spell.spellName = "KSanteW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockBack;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Kaisa";
            spell.dangerlevel = 3;
            spell.name = "Void Seeker";
            spell.projectileSpeed = 1750.0f;   // CDragon KaisaW.missileSpeed
            spell.radius = 200.0f;             // Wiki width
            spell.range = 3000.0f;             // CDragon KaisaW range
            spell.spellDelay = 400.0f;         // CDragon KaisaW mCastTime 0.4
            spell.spellKey = SpellSlot::W;
            spell.spellName = "KaisaW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Kalista";
            spell.dangerlevel = 2;
            spell.missileName = "KalistaMysticShotMissile";
            spell.name = "Pierce";
            spell.projectileSpeed = 2400.0f;   // Wiki speed
            spell.radius = 80.0f;              // Wiki width
            spell.range = 1150.0f;             // champions/429.json Q range
            spell.spellDelay = 250.0f;         // CDragon Kalista Q cast time
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "KalistaMysticShot";
            spell.extraMissileNames = { "KalistaMysticShotMisTrue" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Karma";
            spell.dangerlevel = 2;
            spell.missileName = "KarmaQMissile";
            spell.name = "Inner Flame";
            spell.projectileSpeed = 1700.0f;   // CDragon KarmaQMissile.missileSpeed
            spell.radius = 120.0f;             // Wiki width
            spell.range = 950.0f;              // CDragon KarmaQMissile range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "KarmaQ";
            spell.extraMissileNames = { "KarmaQMissileMantra" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 280.0f;    // Wiki effect radius
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Karthus";
            spell.dangerlevel = 2;
            spell.name = "Lay Waste";
            spell.radius = 160.0f;             // CDragon/Wiki effect radius
            spell.range = 875.0f;              // CDragon KarthusLayWaste range
            spell.spellDelay = 625.0f;         // Wiki detonation delay
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "KarthusLayWaste";
            spell.extraSpellNames = {
                "KarthusLayWasteA2", "KarthusLayWasteA3",
                "KarthusLayWasteDeadA1", "KarthusLayWasteDeadA2", "KarthusLayWasteDeadA3"
            };
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Kassadin";
            spell.dangerlevel = 2;
            spell.name = "Force Pulse";
            spell.radius = 0.0f;               // Cone uses angle
            spell.angle = 40.0f;               // C# targeter cone angle
            spell.range = 600.0f;              // Wiki effect range
            spell.spellDelay = 250.0f;         // CDragon ForcePulse mCastTime 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "ForcePulse";
            spell.spellType = SpellType::Cone;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kassadin";
            spell.dangerlevel = 3;
            spell.name = "Riftwalk";
            spell.radius = 270.0f;             // CDragon/Wiki effect radius
            spell.range = 500.0f;              // CDragon display range
            spell.spellDelay = 250.0f;         // CDragon RiftWalk mCastTime 0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "RiftWalk";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            spell.isSpecial = true;            // teleport landing PBAoE
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Kayle";
            spell.dangerlevel = 2;
            spell.name = "Radiant Blast";
            spell.projectileSpeed = 1600.0f;   // Wiki speed
            spell.radius = 150.0f;             // Wiki width
            spell.range = 900.0f;              // CDragon KayleQ range
            spell.spellDelay = 250.0f;         // CDragon KayleQ mCastTime 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "KayleQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Kayn";
            spell.dangerlevel = 2;
            spell.name = "Blade's Reach";
            spell.radius = 175.0f;             // CDragon mLineWidth
            spell.range = 700.0f;              // Wiki base range (Shadow Assassin extends it)
            spell.spellDelay = 550.0f;         // CDragon KaynW mCastTime 0.55
            spell.spellKey = SpellSlot::W;
            spell.spellName = "KaynW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Kennen";
            spell.dangerlevel = 2;
            spell.missileName = "KennenShurikenHurlMissile1";
            spell.name = "Thundering Shuriken";
            spell.projectileSpeed = 1700.0f;   // CDragon missile speed
            spell.radius = 100.0f;             // Wiki width
            spell.range = 1050.0f;             // CDragon missile range
            spell.spellDelay = 250.0f;         // default for null mCastTime
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "KennenQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Khazix";
            spell.dangerlevel = 2;
            spell.missileName = "KhazixWMissile";
            spell.name = "Void Spike";
            spell.projectileSpeed = 1700.0f;   // CDragon KhazixWMissile.missileSpeed
            spell.radius = 140.0f;             // Wiki width
            spell.range = 1000.0f;             // CDragon display range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::W;
            spell.spellName = "KhazixW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Khazix";
            spell.dangerlevel = 2;
            spell.name = "Void Spike Evolved";
            spell.angle = 22.0f;               // C# evolved three-way spread
            spell.isThreeWay = true;
            spell.projectileSpeed = 1700.0f;
            spell.radius = 140.0f;             // Wiki width
            spell.range = 1000.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::W;
            spell.spellName = "KhazixWLong";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Kled";
            spell.dangerlevel = 2;
            spell.angle = 5.0f;
            spell.isThreeWay = true;
            spell.missileName = "KledRiderQMissile";
            spell.name = "Pocket Pistol";
            spell.projectileSpeed = 3000.0f;   // CDragon missile speed
            spell.radius = 80.0f;              // Wiki width
            spell.range = 700.0f;              // CDragon missile range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "KledRiderQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kled";
            spell.dangerlevel = 3;
            spell.missileName = "KledQMissile";
            spell.name = "Bear Trap on a Rope";
            spell.projectileSpeed = 1600.0f;   // Wiki speed
            spell.radius = 90.0f;              // Wiki width
            spell.range = 800.0f;              // Wiki range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "KledQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.dangerlevel = 2;
            spell.missileName = "KogMawQ";
            spell.name = "Caustic Spittle";
            spell.projectileSpeed = 1650.0f;   // CDragon KogMawQ.missileSpeed
            spell.radius = 140.0f;             // Wiki width
            spell.range = 1200.0f;             // CDragon KogMawQ range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "KogMawQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.dangerlevel = 2;
            spell.missileName = "KogMawVoidOozeMissile";
            spell.name = "Void Ooze";
            spell.projectileSpeed = 1400.0f;   // CDragon KogMawVoidOozeMissile.missileSpeed
            spell.radius = 240.0f;             // Wiki width
            spell.range = 1360.0f;             // CDragon missile range
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "KogMawVoidOoze";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.dangerlevel = 3;
            spell.name = "Living Artillery";
            spell.radius = 240.0f;             // CDragon/Wiki effect radius
            spell.range = 1800.0f;             // latest CDragon rank maximum
            spell.spellDelay = 1100.0f;        // Wiki impact delay
            spell.spellKey = SpellSlot::R;
            spell.spellName = "KogMawLivingArtillery";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // Katarina, Kindred, Kled E and Kha'Zix E are targeted/self-dash mechanics; skip.
        // #endregion K

        // #region L
        {
            SpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 3;
            spell.missileName = "LeblancEMissile";
            spell.name = "Ethereal Chains";
            spell.projectileSpeed = 1750.0f;
            spell.radius = 110.0f;             // Wiki width
            spell.range = 925.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "LeblancE";
            spell.extraMissileNames = { "LeblancRE" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Snare;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 2;
            spell.name = "Distortion";
            spell.radius = 240.0f;             // Wiki effect radius
            spell.range = 600.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::W;
            spell.spellName = "LeblancW";
            spell.spellType = SpellType::Circle;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "LeeSin";
            spell.dangerlevel = 3;
            spell.missileName = "BlindMonkQOne";
            spell.name = "Sonic Wave";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 120.0f;             // Wiki width
            spell.range = 1100.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "BlindMonkQOne";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leona";
            spell.dangerlevel = 4;
            spell.name = "Solar Flare";
            spell.radius = 325.0f;             // Wiki outer effect radius
            spell.range = 1200.0f;
            spell.spellDelay = 625.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "LeonaSolarFlare";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leona";
            spell.dangerlevel = 3;
            spell.missileName = "LeonaZenithBladeMissile";
            spell.name = "Zenith Blade";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 140.0f;             // Wiki width
            spell.range = 900.0f;
            spell.spellDelay = 200.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "LeonaZenithBlade";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Snare;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lillia";
            spell.dangerlevel = 2;
            spell.name = "Blooming Blows";
            spell.radius = 485.0f;              // Wiki outer effect radius
            spell.range = 485.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "LilliaQ";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lillia";
            spell.dangerlevel = 2;
            spell.name = "Watch Out! Eep!";
            spell.radius = 250.0f;              // Wiki outer landing radius
            spell.range = 500.0f;
            spell.spellDelay = 750.0f;
            spell.spellKey = SpellSlot::W;
            spell.spellName = "LilliaW";
            spell.spellType = SpellType::Circle;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lillia";
            spell.dangerlevel = 3;
            spell.missileName = "LilliaERollingMissile";
            spell.name = "Swirlseed";
            spell.projectileSpeed = 1150.0f;
            spell.radius = 120.0f;              // Wiki width
            spell.range = 10000.0f;
            spell.spellDelay = 350.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "LilliaE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lissandra";
            spell.dangerlevel = 3;
            spell.missileName = "LissandraW";
            spell.name = "Ring of Frost";
            spell.radius = 450.0f;
            spell.range = 450.0f;
            spell.spellDelay = 125.0f;
            spell.spellKey = SpellSlot::W;
            spell.spellName = "LissandraW";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Stun;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lissandra";
            spell.dangerlevel = 2;
            spell.missileName = "LissandraQMissile";
            spell.name = "Ice Shard";
            spell.projectileSpeed = 2200.0f;
            spell.radius = 150.0f;              // Wiki width
            spell.range = 750.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "LissandraQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lissandra";
            spell.dangerlevel = 2;
            spell.missileName = "LissandraQShards";
            spell.name = "Ice Shard Extended";
            spell.projectileSpeed = 2200.0f;
            spell.radius = 180.0f;              // Wiki extended width
            spell.range = 1650.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "LissandraQShards";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lissandra";
            spell.dangerlevel = 1;
            spell.missileName = "LissandraEMissile";
            spell.name = "Glacial Path";
            spell.projectileSpeed = 850.0f;
            spell.radius = 250.0f;              // Wiki width
            spell.range = 1050.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "LissandraE";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lucian";
            spell.dangerlevel = 1;
            spell.missileName = "lucianwmissile";
            spell.name = "Ardent Blaze";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 110.0f;              // Wiki width
            spell.range = 900.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::W;
            spell.spellName = "lucianw";
            spell.extraSpellNames = { "LucianW" };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 145.0f;
            spell.spellType = SpellType::Line;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            spell.fixedRange = true;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lucian";
            spell.dangerlevel = 3;
            spell.name = "Piercing Light";
            spell.radius = 120.0f;              // Wiki width
            spell.range = 1140.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "LucianQ";
            spell.spellType = SpellType::Line;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lucian";
            spell.dangerlevel = 2;
            spell.missileName = "lucianrmissile";
            spell.name = "The Culling";
            spell.projectileSpeed = 2800.0f;
            spell.radius = 220.0f;              // Wiki width
            spell.range = 1400.0f;
            spell.spellDelay = 500.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "lucianrmis";
            spell.extraSpellNames = { "LucianR" };
            spell.extraMissileNames = { "lucianrmissileoffhand" };
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            spell.usePackets = true;
            spell.defaultOff = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lulu";
            spell.dangerlevel = 2;
            spell.missileName = "LuluQMissile";
            spell.extraMissileNames = { "LuluQMissileTwo" };
            spell.name = "Glitterlance";
            spell.projectileSpeed = 1450.0f;
            spell.radius = 120.0f;              // Wiki width
            spell.range = 925.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "LuluQ";
            spell.extraSpellNames = { "LuluQPix" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.dangerlevel = 2;
            spell.missileName = "LuxLightStrikeKugel";
            spell.name = "Lucent Singularity";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 310.0f;              // Wiki effect radius
            spell.range = 1100.0f;
            spell.spellDelay = 250.0f;
            spell.extraEndTime = 500.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "LuxLightStrikeKugel";
            spell.spellType = SpellType::Circle;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.dangerlevel = 2;
            spell.name = "Lucent Singularity";
            spell.radius = 310.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "LuxLightStrikeKugel";
            spell.spellType = SpellType::Circle;
            spell.hasTrap = true;
            spell.updatePosition = false;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.dangerlevel = 4;
            spell.name = "Final Spark";
            spell.radius = 200.0f;              // Wiki width
            spell.range = 3340.0f;
            spell.spellDelay = 1000.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "LuxMaliceCannon";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.dangerlevel = 3;
            spell.missileName = "LuxLightBindingMis";
            spell.name = "Light Binding";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 140.0f;              // Wiki width
            spell.range = 1175.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "LuxLightBinding";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Snare;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // Lee Sin R, LeBlanc R and Lissandra R are targeted/self-only; skip.
        // #endregion L

        // #region M
        {
            SpellData spell;
            spell.charName = "Malphite";
            spell.dangerlevel = 4;
            spell.missileName = "UFSlash";
            spell.name = "Unstoppable Force";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 325.0f;              // Wiki effect radius
            spell.range = 1000.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "UFSlash";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Malzahar";
            spell.dangerlevel = 2;
            spell.name = "Call of the Void";
            spell.radius = 85.0f;
            spell.range = 900.0f;
            spell.sideRadius = 400.0f;
            spell.spellDelay = 830.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "MalzaharQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Silence;
            spell.isPerpendicular = true;
            spell.fixedRange = true;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Maokai";
            spell.dangerlevel = 3;
            spell.missileName = "MaokaiQMissile";
            spell.name = "Bramble Smash";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 140.0f;              // CDragon mLineWidth 70 -> full width
            spell.range = 600.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "MaokaiQ";
            spell.extraSpellNames = { "MaokaiTrunkLine" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockBack;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Maokai";
            spell.dangerlevel = 1;
            spell.missileName = "MaokaiEMissile";
            spell.name = "Sapling Toss";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 225.0f;              // Wiki medium sapling effect radius
            spell.range = 1100.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "MaokaiE";
            spell.extraSpellNames = { "MaokaiSapling2" };
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.hasTrap = true;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Maokai";
            spell.dangerlevel = 4;
            spell.missileName = "MaokaiRMis";
            spell.name = "Nature's Grasp";
            spell.projectileSpeed = 50.0f;
            spell.radius = 240.0f;              // Wiki width
            spell.range = 3000.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "MaokaiR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Snare;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Mel";
            spell.dangerlevel = 2;
            spell.name = "Radiant Volley";
            spell.radius = 220.0f;              // Wiki primary effect radius
            spell.secondaryRadius = 100.0f;
            spell.range = 900.0f;
            spell.spellDelay = 350.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "MelQ";
            spell.spellType = SpellType::Circle;
            spell.hasEndExplosion = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Mel";
            spell.dangerlevel = 3;
            spell.missileName = "MelE";
            spell.name = "Solar Snare";
            spell.projectileSpeed = 1100.0f;
            spell.radius = 80.0f;
            spell.secondaryRadius = 260.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "MelE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Snare;
            spell.hasEndExplosion = true;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Milio";
            spell.dangerlevel = 2;
            spell.missileName = "MilioQ";
            spell.name = "Ultra Mega Fire Kick";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 60.0f;               // Wiki width
            spell.range = 1200.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "MilioQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockBack;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "MissFortune";
            spell.dangerlevel = 1;
            spell.name = "Make It Rain";
            spell.radius = 200.0f;              // Wiki effect radius
            spell.range = 1000.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "MissFortuneScattershot";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "MissFortune";
            spell.dangerlevel = 4;
            spell.missileName = "MissFortuneBullets";
            spell.name = "Bullet Time";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 40.0f;               // Wiki bullet width
            spell.range = 1400.0f;
            spell.angle = 17.0f;                // CDragon cone angle
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "MissFortuneBulletTime";
            spell.spellType = SpellType::Cone;
            spell.fixedRange = true;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Mordekaiser";
            spell.dangerlevel = 3;
            spell.name = "Obliterate";
            spell.radius = 160.0f;              // Wiki width
            spell.range = 675.0f;
            spell.spellDelay = 500.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "MordekaiserQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Mordekaiser";
            spell.dangerlevel = 3;
            spell.missileName = "MordekaiserEMissile";
            spell.name = "Death's Grasp";
            spell.projectileSpeed = 3000.0f;
            spell.radius = 200.0f;              // Wiki width
            spell.range = 700.0f;
            spell.angle = 45.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "MordekaiserSyphonOfDestruction";
            spell.extraSpellNames = { "MordekaiserE" };
            spell.spellType = SpellType::Cone;
            spell.ccType = CCType::KnockBack;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Morgana";
            spell.dangerlevel = 3;
            spell.missileName = "DarkBindingMissile";
            spell.extraMissileNames = { "MorganaQ", "Morgana_Q_Mis" };
            spell.name = "Dark Binding";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 140.0f;              // Wiki width
            spell.range = 1300.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "DarkBindingMissile";
            spell.extraSpellNames = { "MorganaQ" };
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Snare;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Morgana";
            spell.dangerlevel = 2;
            spell.name = "Tormented Soil";
            spell.radius = 280.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::W;
            spell.spellName = "TormentedSoil";
            spell.spellType = SpellType::Circle;
            spell.hasTrap = true;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        // Master Yi, Mel R, Morgana R and Mordekaiser R are targeted/self-only; skip.
        // #endregion M

        // #region N
        {
            SpellData spell;
            spell.charName = "Naafiri";
            spell.dangerlevel = 2;
            spell.missileName = "NaafiriQ";
            spell.name = "Darkin Daggers";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 50.0f;               // Wiki width
            spell.range = 900.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "NaafiriQ";
            spell.extraMissileNames = { "NaafiriQRecast" };
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nami";
            spell.dangerlevel = 3;
            spell.missileName = "namiqmissile";
            spell.name = "Aqua Prison";
            spell.projectileSpeed = 2500.0f;
            spell.radius = 200.0f;
            spell.range = 875.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "NamiQ";
            spell.extraSpellNames = { "namiq" };
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nami";
            spell.dangerlevel = 4;
            spell.missileName = "namirmissile";
            spell.name = "Tidal Wave";
            spell.projectileSpeed = 850.0f;
            spell.radius = 500.0f;              // Wiki width
            spell.range = 2750.0f;
            spell.spellDelay = 500.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "NamiR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nasus";
            spell.dangerlevel = 2;
            spell.name = "Spirit Fire";
            spell.radius = 400.0f;
            spell.range = 650.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "NasusE";
            spell.spellType = SpellType::Circle;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nautilus";
            spell.dangerlevel = 3;
            spell.missileName = "NautilusAnchorDragMissile";
            spell.name = "Dredge Line";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 180.0f;              // Wiki width
            spell.range = 1150.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "NautilusAnchorDrag";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nautilus";
            spell.dangerlevel = 1;
            spell.name = "Riptide";
            spell.radius = 350.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "NautilusSplashZone";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Neeko";
            spell.dangerlevel = 2;
            spell.name = "Blooming Burst";
            spell.radius = 250.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "NeekoQ";
            spell.spellType = SpellType::Circle;
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 250.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Neeko";
            spell.dangerlevel = 3;
            spell.missileName = "NeekoE";
            spell.name = "Tangle-Barbs";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 140.0f;              // Wiki initial width
            spell.range = 1000.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::E;
            spell.spellName = "NeekoE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Snare;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Neeko";
            spell.dangerlevel = 4;
            spell.name = "Pop Blossom";
            spell.radius = 600.0f;
            spell.range = 600.0f;
            spell.spellDelay = 600.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "NeekoR";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            spell.isSpecial = true;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nidalee";
            spell.dangerlevel = 3;
            spell.missileName = "JavelinToss";
            spell.name = "Javelin Toss";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 80.0f;               // Wiki width
            spell.range = 1500.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "JavelinToss";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nidalee";
            spell.dangerlevel = 1;
            spell.name = "Bushwhack";
            spell.radius = 100.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::W;
            spell.spellName = "Bushwhack";
            spell.spellType = SpellType::Circle;
            spell.hasTrap = true;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nilah";
            spell.dangerlevel = 4;
            spell.name = "Apotheosis";
            spell.radius = 450.0f;
            spell.range = 450.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "NilahR";
            spell.spellType = SpellType::Circle;
            spell.isSpecial = true;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nocturne";
            spell.dangerlevel = 2;
            spell.missileName = "NocturneDuskbringer";
            spell.name = "Duskbringer";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 120.0f;              // Wiki width
            spell.range = 1200.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "NocturneDuskbringer";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nunu";
            spell.dangerlevel = 3;
            spell.missileName = "NunuWSnowballMissile";
            spell.name = "Biggest Snowball Ever!";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 50.0f;               // Wiki width
            spell.range = 7500.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::W;
            spell.spellName = "NunuW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nunu";
            spell.dangerlevel = 4;
            spell.name = "Absolute Zero";
            spell.radius = 500.0f;
            spell.range = 650.0f;
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::R;
            spell.spellName = "NunuR";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.defaultOff = true;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        // Lee Sin R, Nautilus R, Naafiri W, Nilah E and Nunu Q are targeted/dash mechanics; skip.
        // #endregion N

    }
};

} // namespace EzEvade
