#pragma once
// ============================================================================
// SpellDatabase.h — EzEvade skillshot database.
// ----------------------------------------------------------------------------
// Bố cục port 1-1 từ EzEvade C# (`Spells/SpellDatabase.cs`): 1 static list
// `Spells`, khởi tạo 1 lần trong InitSpells(). MỖI entry là 1 block `{ SpellData
// spell; ...; Spells.push_back(spell); }`.
//
// DỮ LIỆU spell lấy từ bin JSON (E:\DamageData\Database) + LoL Wiki (wiki.leagueoflegends.com):
//   * range        <- wiki hitbox dimensions (bin CastRange thường = 25000 cho direction)
//   * spellDelay   <- bin mSpell.mCastTime * 1000 (ms); null → 250
//   * projectileSpeed <- bin mMissileSpec.MovementComponent.mSpeed
//   * missileName  <- mScriptName của missile
//   * radius (QUAN TRỌNG — code dùng radius làm HALF-WIDTH cho Line, RADIUS cho Circle):
//       - Line/missile: = mLineWidth (half-width) từ bin. Code RectanglePoly và
//         InSkillShot đều dùng radius làm half-width. Wiki width = 2 × mLineWidth.
//       - Positional/instant (không missile): = wiki width ÷ 2 (half-width).
//       - Circle: = wiki radius trực tiếp (distance từ center đến edge).
//       - Cone: angle = full cone angle (degrees), range = max distance từ caster.
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
            spell.radius = 80.0f;              // mLineWidth (half-width; code dùng radius làm half-width)
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
            spell.radius = 90.0f;              // wiki width 180 ÷ 2 (half-width)
            spell.range = 625.0f;              // wiki: 625×180 rectangle
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
            spell.range = 475.0f;              // wiki: 475 front edge from Aatrox
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
            spell.range = 200.0f;              // wiki: center 200 units in front of Aatrox
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
            spell.projectileSpeed = 2500.0f;   // AcceleratingMovement mInitialSpeed 2500 (decel to mMinSpeed 400)
            spell.radius = 100.0f;             // mMissileWidth (half-width)
            spell.range = 900.0f;              // wiki range
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
            spell.projectileSpeed = 1550.0f;   // FixedSpeedMovement mSpeed
            spell.radius = 60.0f;              // mMissileWidth (half-width)
            spell.range = 1000.0f;             // wiki range
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
            spell.projectileSpeed = 2600.0f;   // AcceleratingMovement mMaxSpeed (tăng tốc từ mInitialSpeed 60)
            spell.radius = 100.0f;             // mMissileWidth (half-width, cùng orb với Q)
            spell.range = 900.0f;              // wiki range (orb quay về phía Ahri)
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
            spell.radius = 60.0f;              // mMissileWidth từng kunai (half-width)
            spell.range = 550.0f;              // bin CastRange
            spell.angle = 40.0f;               // wiki: 20° half-angle × 2 = 40° full
            spell.spellDelay = 250;            // mCastTime 0.25 (level 1; giảm theo level)
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
            spell.radius = 60.0f;              // mMissileWidth (half-width)
            spell.range = 825.0f;              // wiki target range
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
            spell.projectileSpeed = 1500.0f;   // AkshanQMissile FixedSpeedMovement.mSpeed (đi ra; return 2400)
            spell.radius = 60.0f;              // mLineWidth (half-width)
            spell.range = 850.0f;              // wiki range (850 + up to 500 per enemy hit)
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
            spell.radius = 65.0f;              // wiki width 130 ÷ 2 (half-width; bin LineWidth=15 là targeting line)
            spell.range = 1250.0f;             // wiki range
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
            spell.radius = 80.0f;              // mLineWidth (half-width)
            spell.range = 1100.0f;             // wiki range
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
            spell.radius = 110.0f;             // mLineWidth (half-width)
            spell.range = 1100.0f;             // wiki range
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
            spell.angle = 49.52f;              // wiki: 49.52° full cone angle
            spell.range = 600.0f;              // wiki effect radius = bin CastRange
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
            spell.radius = 110.0f;             // ApheliosR mLineWidth (half-width; RMis=125)
            spell.range = 1300.0f;             // wiki target range
            spell.spellDelay = 600;            // wiki cast time 0.6
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
            spell.projectileSpeed = 1500.0f;   // AcceleratingMovement mInitialSpeed 1500 (tăng tới mMaxSpeed 2100)
            spell.radius = 130.0f;             // mLineWidth (half-width)
            spell.range = 25000.0f;            // wiki: Global (castRange placeholder 25000)
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
            spell.radius = 90.0f;              // mLineWidth (half-width)
            spell.range = 900.0f;              // wiki range
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
            spell.projectileSpeed = std::numeric_limits<float>::max(); // blast tức thời (dash recoil), không missile
            spell.radius = 87.5f;             // wiki width 175 ÷ 2 (half-width)
            spell.range = 825.0f;              // wiki range
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
            spell.radius = 60.0f;              // mLineWidth (half-width)
            spell.range = 850.0f;              // wiki target range (bin CastRange 950 = missile travel)
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
            spell.projectileSpeed = 300000.0f; // BelvethW MissileSpec.Speed (slam ~tức thời)
            spell.radius = 100.0f;             // wiki width 200 ÷ 2 (half-width; bin không có LineWidth)
            spell.range = 660.0f;              // wiki: 0-660 edge range
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
            spell.radius = 70.0f;              // mLineWidth (half-width)
            spell.range = 1080.0f;             // bin CastRange (missile travel; wiki 1115 incl lollipop)
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
            spell.radius = 60.0f;              // mLineWidth (half-width)
            spell.range = 1100.0f;             // wiki range (centered) = bin CastRange
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
            spell.projectileSpeed = std::numeric_limits<float>::max(); // scream tức thời (channel + recast)
            spell.radius = 190.0f;            // wiki width 380 ÷ 2 (half-width)
            spell.range = 400.0f;              // wiki min range (charge tối đa 600)
            spell.spellDelay = 150;            // wiki recast cast time 0.15
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
            spell.projectileSpeed = 2000.0f;   // BriarR MissileSpec.mSpeed
            spell.radius = 160.0f;             // mLineWidth (half-width)
            spell.range = 12000.0f;            // wiki range (centered)
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
            spell.projectileSpeed = 2200.0f;   // CaitlynQ MissileSpec.mSpeed
            spell.radius = 60.0f;              // mLineWidth (half-width; expands to 180 sau target đầu)
            spell.range = 1300.0f;             // wiki range (centered) = bin CastRange
            spell.spellDelay = 625;            // wiki cast time 0.625
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
            spell.projectileSpeed = 1600.0f;   // CaitlynEMissile MissileSpec.mSpeed
            spell.radius = 70.0f;              // mLineWidth (half-width)
            spell.range = 800.0f;              // wiki range (centered) = bin CastRange
            spell.spellDelay = 150;            // wiki cast time 0.15
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
            spell.range = 650.0f;              // wiki: outer effect radius
            spell.angle = 70.0f;               // wiki: 70° full cone angle
            spell.spellDelay = 250;            // wiki: cast time none → default 250 (1.1s animation delay)
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
            spell.range = 850.0f;              // wiki effect radius
            spell.angle = 80.0f;               // wiki: 80° full cone angle
            spell.spellDelay = 500;            // wiki cast time 0.5
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
            spell.radius = 250.0f;             // wiki effect radius
            spell.range = 950.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 500;            // wiki cast time 0.5 (rupture nổ sau 0.627s delay riêng)
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
            spell.range = 650.0f;              // wiki target range (edge)
            spell.angle = 60.0f;               // wiki: 60° full cone angle
            spell.spellDelay = 500;            // wiki cast time 0.5
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
            spell.projectileSpeed = 2000.0f;   // MissileBarrageMissile MissileSpec.mSpeed
            spell.radius = 40.0f;              // mLineWidth (half-width)
            spell.range = 1300.0f;             // wiki target range (centered) = bin CastRange
            spell.spellDelay = 175;            // wiki cast time 0.175
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
            spell.radius = 460.0f;             // wiki outer effect radius (ring quanh Darius)
            spell.range = 460.0f;
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
            spell.range = 535.0f;              // wiki effect radius (edge)
            spell.angle = 50.0f;               // wiki: 50° full cone angle
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
            spell.projectileSpeed = 2100.0f;   // DianaQOuterMissile MissileSpec.mSpeed (inner 1900)
            spell.radius = 50.0f;              // mLineWidth 100 ÷ 2 (half-width; wiki không ghi width)
            spell.range = 900.0f;              // wiki target range = bin CastRange
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
            spell.projectileSpeed = 2000.0f;   // DrMundoQ MissileSpec.mSpeed
            spell.radius = 60.0f;              // mLineWidth (half-width)
            spell.range = 1050.0f;             // wiki target range (centered) = bin CastRange
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
            spell.projectileSpeed = 1400.0f;   // DravenDoubleShotMissile MissileSpec.mSpeed
            spell.radius = 130.0f;             // mLineWidth (half-width)
            spell.range = 1100.0f;             // wiki target range (centered) = bin CastRange
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
            spell.projectileSpeed = 2000.0f;   // DravenR MissileSpeed (boomerang toàn map)
            spell.radius = 160.0f;             // mLineWidth (half-width)
            spell.range = 20000.0f;            // wiki global (boomerang quay về)
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
            spell.projectileSpeed = 1650.0f;   // EkkoQMis MissileSpec.mSpeed (đi ra)
            spell.radius = 60.0f;              // mLineWidth (half-width; nở ra 200 lúc quay về)
            spell.range = 1100.0f;             // wiki target range = bin CastRange
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
            spell.projectileSpeed = 2300.0f;   // EkkoQReturn MissileSpec.mSpeed
            spell.radius = 100.0f;             // mLineWidth (half-width; orb nở ra lúc quay về)
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
            spell.radius = 375.0f;             // wiki effect radius
            spell.range = 1600.0f;             // C# gốc / wiki (afterimage 4s trước)
            spell.spellDelay = 500;            // wiki cast time 0.5
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
            spell.projectileSpeed = 1600.0f;   // EliseHumanEMissile MissileSpec.mSpeed
            spell.radius = 55.0f;              // mLineWidth (half-width)
            spell.range = 1100.0f;             // wiki target range (centered) = bin CastRange
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
            spell.projectileSpeed = 2400.0f;   // EvelynnQ MissileSpec.mSpeed
            spell.radius = 60.0f;              // mLineWidth (half-width; recast nở 180)
            spell.range = 800.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 300;            // wiki cast time 0.3 (initial dart)
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
            spell.projectileSpeed = 2000.0f;   // EzrealQ MissileSpec.mSpeed
            spell.radius = 60.0f;              // mLineWidth (half-width)
            spell.range = 1200.0f;             // wiki target range (centered) = bin CastRange
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
            spell.projectileSpeed = 1700.0f;   // EzrealW MissileSpec.mSpeed
            spell.radius = 60.0f;              // mLineWidth (half-width; wiki visual 160 ≠ JSON 60)
            spell.range = 1200.0f;             // wiki target range = bin CastRange
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
            spell.projectileSpeed = 2000.0f;   // EzrealR MissileSpec.mSpeed
            spell.radius = 160.0f;             // mLineWidth (half-width)
            spell.range = 25000.0f;            // wiki global (castRange placeholder)
            spell.fixedRange = true;
            spell.spellDelay = 1000;           // wiki cast time 1.0
            spell.spellKey = SpellSlot::R;
            spell.spellName = "EzrealR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // #endregion Ezreal

        // #region Fiddlesticks
        {
            SpellData spell;
            spell.charName = "Fiddlesticks";
            spell.dangerlevel = 2;
            spell.missileName = "FiddleSticksE";
            spell.name = "Reap";
            spell.projectileSpeed = 1800.0f;    // FiddleSticksE MissileSpec.mSpeed
            spell.radius = 500.0f;              // bin CastRadius (crescent AoE)
            spell.range = 850.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 400.0f;          // wiki cast time 0.4
            spell.spellKey = SpellSlot::E;
            spell.spellName = "FiddleSticksE";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Fiddlesticks";
            spell.dangerlevel = 1;              // low danger: detect landing zone to reposition
            spell.name = "Crowstorm";
            spell.radius = 600.0f;              // wiki effect radius
            spell.range = 800.0f;               // wiki target range
            spell.spellDelay = 1500.0f;         // wiki channel 1.5s
            spell.spellKey = SpellSlot::R;
            spell.spellName = "FiddleSticksR";
            spell.spellType = SpellType::Circle;
            spell.isSpecial = true;
            spell.defaultOff = true;
            Spells.push_back(spell);
        }
        // #endregion Fiddlesticks

        // #region Fiora
        {
            SpellData spell;
            spell.charName = "Fiora";
            spell.dangerlevel = 2;
            spell.missileName = "FioraWMissile";
            spell.name = "Riposte";
            spell.projectileSpeed = 3200.0f;   // FioraWMissile MissileSpec.mSpeed
            spell.radius = 70.0f;              // mLineWidth (half-width)
            spell.range = 900.0f;              // wiki target range (bin CastRange=5000 placeholder)
            spell.spellDelay = 9.999999776482582f; // CDragon mCastTime=0.01 (stab sau 0.5s poise)
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
            spell.projectileSpeed = 1300.0f;   // FizzRMissile MissileSpec.mSpeed
            spell.radius = 80.0f;              // mLineWidth (half-width; lure missile)
            spell.range = 1300.0f;             // wiki target range = bin display override
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
            spell.projectileSpeed = 1400.0f;   // GalioQMissile MissileSpec.mSpeed
            spell.radius = 150.0f;             // wiki effect radius (tornado sau khi 2 missile hội tụ)
            spell.range = 825.0f;              // wiki target range = bin display override
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
            spell.projectileSpeed = std::numeric_limits<float>::max(); // GalioE dash tức thời, không missile ngang
            spell.radius = 200.0f;             // wiki width 400 / 2 (half-width; CDragon mLineWidth=160 ≠ wiki)
            spell.range = 650.0f;              // wiki target range max = bin display override
            spell.spellDelay = 400;            // wiki cast time 0.4 (lunge step-back + dash)
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
            spell.projectileSpeed = 2500.0f;   // GnarQMissile MissileSpeed
            spell.radius = 55.0f;              // mLineWidth (half-width)
            spell.range = 1125.0f;             // wiki range = bin CastRange
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
            spell.projectileSpeed = 2100.0f;   // GnarBigQMissile MissileSpec.mSpeed
            spell.radius = 90.0f;              // mLineWidth (half-width; wiki width 180)
            spell.range = 1150.0f;             // wiki range = bin CastRange
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
            spell.radius = 100.0f;             // mLineWidth (half-width; wiki width 200)
            spell.range = 550.0f;              // wiki range = bin display override
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
            spell.projectileSpeed = 3000.0f;   // GravesQLineMis MissileSpec.mSpeed
            spell.radius = 40.0f;              // mLineWidth (half-width; wiki outbound 80)
            spell.range = 900.0f;              // wiki edge range = bin CastRange
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
            spell.projectileSpeed = 1600.0f;   // GravesQReturn MissileSpec.mSpeed
            spell.radius = 100.0f;             // mLineWidth (half-width; wiki return 200)
            spell.range = 900.0f;              // wiki edge range = bin CastRange
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
            spell.projectileSpeed = 2100.0f;   // GravesChargeShotShot MissileSpec.mSpeed
            spell.radius = 100.0f;             // mLineWidth (half-width; wiki shell 200)
            spell.range = 1100.0f;             // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 2000.0f;   // GravesChargeShotFxMissile MissileSpec.mSpeed
            spell.radius = 200.0f;             // wiki explosion cone width 200
            spell.range = 1100.0f;             // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1100.0f;   // HecarimUltMissile MissileSpec.mSpeed
            spell.radius = 40.0f;              // mLineWidth (half-width; mỗi rider 80)
            spell.range = 1510.0f;             // wiki rider travel range (bin CastRange=1650)
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
            spell.projectileSpeed = 750.0f;    // HeimerdingerWAttack2 MissileSpeed
            spell.radius = 40.0f;              // mLineWidth (half-width)
            spell.range = 1150.0f;             // wiki target range = bin display override
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
            spell.projectileSpeed = 1200.0f;   // HeimerdingerESpell MissileSpec.mSpeed
            spell.radius = 250.0f;             // wiki effect radius (outer)
            spell.range = 925.0f;              // wiki target range = bin CastRange
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
            spell.radius = 100.0f;             // wiki width 200 / 2 (half-width)
            spell.range = 803.0f;              // wiki edge range max (≈802.75)
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
            spell.projectileSpeed = 1900.0f;   // IllaoiEMis MissileSpec.mSpeed
            spell.radius = 50.0f;              // mLineWidth (half-width)
            spell.range = 950.0f;              // wiki centered range = bin CastRange
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
            spell.radius = 120.0f;             // mLineWidth (half-width; wiki width 240)
            spell.range = 895.0f;              // wiki max range (centered 775 + edge 120)
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
            spell.projectileSpeed = 2000.0f;   // IreliaEMissile MissileSpeed
            spell.radius = 90.0f;              // mLineWidth (half-width; wiki width 140 ≈ 2×70)
            spell.range = 775.0f;              // wiki target range (bin CastRange placeholder)
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
            spell.projectileSpeed = 2000.0f;   // IreliaR MissileSpec.mSpeed
            spell.radius = 160.0f;             // mLineWidth (half-width; wiki width 320)
            spell.range = 1000.0f;             // wiki target range (centered) = bin CastRange
            spell.spellDelay = 400;            // wiki cast time 0.4
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
            spell.projectileSpeed = 1300.0f;   // IvernQ MissileSpec.mSpeed
            spell.radius = 80.0f;              // mLineWidth (half-width; wiki width 160)
            spell.range = 1150.0f;             // wiki range (centered) = bin CastRange
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
            spell.projectileSpeed = 667.67f;   // HowlingGaleSpell MissileSpec.mSpeed (min charge)
            spell.radius = 120.0f;             // mLineWidth (half-width; wiki width 240)
            spell.range = 1760.0f;             // wiki max range (charge 3s)
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
            spell.radius = 70.0f;              // mLineWidth (half-width; wiki width 136)
            spell.range = 770.0f;              // bin CastRange (≈ wiki edge 785)
            spell.spellDelay = 400;            // wiki cast time 0.4
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
            spell.radius = 200.0f;             // wiki effect radius
            spell.range = 860.0f;              // wiki target range = bin display
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
            spell.projectileSpeed = 1450.0f;   // JayceShockBlastMis MissileSpec.mSpeed
            spell.radius = 70.0f;              // mLineWidth (half-width; wiki width 140)
            spell.range = 1050.0f;             // wiki edge range
            spell.spellDelay = 214;            // wiki cast time 0.2143
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
            spell.projectileSpeed = 2350.0f;   // JayceShockBlastWallMis MissileSpec.mSpeed
            spell.radius = 70.0f;              // mLineWidth (half-width; wiki width 140)
            spell.range = 1600.0f;             // wiki accelerated edge range
            spell.spellDela            y = 214;            // wiki cast time 0.2143
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
            spell.projectileSpeed = 3300.0f;   // JinxWMissile MissileSpec.mSpeed
            spell.radius = 60.0f;              // mLineWidth (half-width; wiki width 120)
            spell.range = 1500.0f;             // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1700.0f;   // JinxR MissileSpec.mSpeed
            spell.radius = 140.0f;             // mLineWidth (half-width; wiki width 280)
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
            spell.radius = 40.0f;              // mLineWidth (half-width; wiki width 80)
            spell.range = 2520.0f;             // wiki edge range (bin CastRange placeholder)
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
            spell.projectileSpeed = 5000.0f;   // JhinRShotMis MissileSpec.mSpeed
            spell.radius = 80.0f;              // mLineWidth (half-width; wiki width 160)
            spell.range = 3500.0f;             // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1750.0f;   // KaisaW MissileSpec.mSpeed
            spell.radius = 100.0f;             // mLineWidth (half-width; wiki width 200)
            spell.range = 3000.0f;             // wiki target range = bin CastRange
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
            spell.projectileSpeed = 2400.0f;   // wiki speed (bin MissileSpec 3000 là wrapper)
            spell.radius = 40.0f;              // mLineWidth (half-width; wiki width 80)
            spell.range = 1200.0f;             // wiki target range = bin CastRange
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
            spell.projectileSpeed = 1700.0f;   // KarmaQMissile MissileSpec.mSpeed
            spell.radius = 60.0f;              // mLineWidth (half-width; wiki width 120)
            spell.range = 950.0f;              // wiki target range = bin CastRange
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
            spell.angle = 78.0f;               // wiki full cone angle
            spell.range = 600.0f;              // wiki effect range
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
            spell.projectileSpeed = 1600.0f;   // KayleQ MissileSpec.mSpeed
            spell.radius = 60.0f;              // mLineWidth (half-width; wiki width 150)
            spell.range = 900.0f;              // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1700.0f;   // KennenShurikenHurlMissile1 MissileSpec.mSpeed
            spell.radius = 50.0f;              // mLineWidth (half-width; wiki width 100)
            spell.range = 1050.0f;             // wiki target range = bin CastRange
            spell.spellDelay = 175;            // wiki cast time 0.175
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
            spell.projectileSpeed = 1700.0f;   // KhazixWMissile MissileSpec.mSpeed
            spell.radius = 70.0f;              // mLineWidth (half-width; wiki width 140)
            spell.range = 1025.0f;             // wiki target range = bin CastRange
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
            spell.radius = 70.0f;              // mLineWidth (half-width; wiki width 140)
            spell.range = 1025.0f;             // wiki target range = bin CastRange
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
            spell.angle = 20.0f;               // wiki full cone angle
            spell.isThreeWay = true;
            spell.missileName = "KledRiderQMissile";
            spell.name = "Pocket Pistol";
            spell.projectileSpeed = 3000.0f;   // KledRiderQMissile MissileSpec.mSpeed
            spell.radius = 40.0f;              // mLineWidth (half-width; wiki width 80)
            spell.range = 700.0f;              // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1600.0f;   // KledQMissile MissileSpec.mSpeed
            spell.radius = 45.0f;              // mLineWidth (half-width; wiki width 90)
            spell.range = 800.0f;              // wiki centered range = bin KledQ CastRange
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
            spell.projectileSpeed = 1650.0f;   // KogMawQ MissileSpec.mSpeed
            spell.radius = 70.0f;              // mLineWidth (half-width; wiki width 140)
            spell.range = 1200.0f;             // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1400.0f;   // KogMawVoidOozeMissile MissileSpec.mSpeed
            spell.radius = 120.0f;             // mLineWidth (half-width; wiki width 240)
            spell.range = 1360.0f;             // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1750.0f;   // LeblancEMissile MissileSpec.mSpeed
            spell.radius = 55.0f;              // mLineWidth (half-width; wiki width 110)
            spell.range = 950.0f;              // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1800.0f;   // LeeSinQOne MissileSpec.mSpeed
            spell.radius = 60.0f;              // mLineWidth (half-width; wiki width 120)
            spell.range = 1200.0f;             // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 2000.0f;   // LeonaZenithBladeMissile MissileSpec.mSpeed
            spell.radius = 70.0f;              // mLineWidth (half-width; wiki width 140)
            spell.range = 900.0f;              // wiki centered range = bin CastRange
            spell.spellDelay = 250.0f;         // wiki cast time 0.25
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
            spell.projectileSpeed = 1150.0f;    // LilliaERollingMissile MissileSpec.mSpeed
            spell.radius = 85.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 10000.0f;             // global rolling
            spell.spellDelay = 400.0f;          // wiki cast time 0.4
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
            spell.radius = 275.0f;             // wiki effect radius = bin CastRadius
            spell.range = 275.0f;              // PBAoE centered on caster
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
            spell.projectileSpeed = 2200.0f;    // LissandraQMissile MissileSpec.mSpeed
            spell.radius = 75.0f;               // mLineWidth (half-width; wiki width 150)
            spell.range = 825.0f;               // wiki initial range
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
            spell.projectileSpeed = 2200.0f;    // LissandraQShards MissileSpec.mSpeed
            spell.radius = 90.0f;               // mLineWidth (half-width; wiki extended width 180)
            spell.range = 950.0f;               // wiki extended range
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
            spell.projectileSpeed = 850.0f;     // LissandraEMissile MissileSpec.mSpeed (initial)
            spell.radius = 125.0f;              // mLineWidth (half-width; wiki width 250)
            spell.range = 1025.0f;              // wiki range = bin CastRange
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
            spell.projectileSpeed = 1600.0f;    // LucianWMissile MissileSpec.mSpeed
            spell.radius = 55.0f;               // mLineWidth (half-width; wiki width 110)
            spell.range = 900.0f;               // wiki centered range = bin CastRange
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
            spell.radius = 65.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 1000.0f;              // wiki range
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
            spell.projectileSpeed = 2800.0f;    // LucianRMissile MissileSpec.mSpeed
            spell.radius = 110.0f;              // mLineWidth (half-width; wiki width 220)
            spell.range = 1200.0f;              // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1450.0f;    // LuluQMissile MissileSpec.mSpeed
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 950.0f;               // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1200.0f;    // wiki speed
            spell.radius = 310.0f;              // wiki effect radius
            spell.range = 1100.0f;              // wiki target range
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
            spell.radius = 100.0f;              // mLineWidth/2 (half-width; wiki width 200)
            spell.range = 3400.0f;              // wiki edge range
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
            spell.projectileSpeed = 1200.0f;    // LuxLightBindingMis MissileSpec.mSpeed
            spell.radius = 70.0f;               // mLineWidth (half-width; wiki width 140)
            spell.range = 1300.0f;              // wiki centered range = bin CastRange
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
            spell.dangerlevel = 2;
            spell.missileName = "MaokaiQMissile";
            spell.name = "Bramble Smash";
            spell.projectileSpeed = 1600.0f;    // MaokaiQMissile MissileSpec.mSpeed
            spell.radius = 110.0f;              // mLineWidth (half-width)
            spell.range = 600.0f;               // wiki target range
            spell.spellDelay = 300.0f;          // wiki cast time 0.3
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
            spell.missileName = "MaokaiRMis";
            spell.name = "Nature's Grasp";
            spell.projectileSpeed = 50.0f;      // wiki base speed
            spell.radius = 120.0f;              // mLineWidth (half-width; wiki width 240)
            spell.range = 3000.0f;              // wiki centered range = bin CastRange
            spell.spellDelay = 500.0f;          // wiki cast time 0.5
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
            spell.dangerlevel = 2;
            spell.missileName = "MissFortuneRicochetShot";
            spell.name = "Double Up";
            spell.projectileSpeed = 1400.0f;    // wiki speed = bin MissileSpeed
            spell.radius = 20.0f;               // targeted missile (small)
            spell.range = 550.0f;               // wiki target range
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "MissFortuneRicochetShot";
            spell.spellType = SpellType::Targeted;
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
            spell.projectileSpeed = 2000.0f;    // wiki speed
            spell.radius = 40.0f;               // wiki bullet width
            spell.range = 1450.0f;              // wiki effect radius
            spell.angle = 30.0f;                // wiki full cone angle
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
            spell.radius = 75.0f;               // wiki max width 150 / 2 (half-width)
            spell.range = 625.0f;               // wiki max range
            spell.spellDelay = 500.0f;          // wiki cast time 0.5
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
            spell.projectileSpeed = 1200.0f;    // MorganaQ MissileSpec.mSpeed
            spell.radius = 70.0f;               // mLineWidth (half-width; wiki width 140)
            spell.range = 1300.0f;              // wiki centered range = bin CastRange
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
            spell.radius = 275.0f;              // wiki effect radius
            spell.range = 900.0f;               // wiki target range
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
            spell.projectileSpeed = 2500.0f;    // NamiQMissile speed
            spell.radius = 200.0f;              // wiki effect radius = bin CastRadius
            spell.range = 850.0f;               // wiki target range = bin CastRange
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
            spell.projectileSpeed = 850.0f;     // NamiRMissile MissileSpec.mSpeed
            spell.radius = 250.0f;              // mLineWidth (half-width; wiki width 500)
            spell.range = 2750.0f;              // wiki target range = bin CastRange
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
            spell.projectileSpeed = 2000.0f;    // NautilusAnchorDragMissile MissileSpec.mSpeed
            spell.radius = 90.0f;               // mLineWidth (half-width; wiki width 180)
            spell.range = 1122.0f;              // wiki edge range
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
            spell.projectileSpeed = 1300.0f;    // NeekoE MissileSpec.mSpeed (initial)
            spell.radius = 70.0f;               // wiki initial width 140 / 2 (half-width)
            spell.range = 1000.0f;              // wiki range = bin CastRange
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
            spell.radius = 590.0f;              // wiki effect radius
            spell.range = 600.0f;               // bin CastRange
            spell.spellDelay = 1850.0f;         // wind-up 1.25s + cast time 0.6s
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
            spell.projectileSpeed = 1300.0f;    // JavelinToss MissileSpec.mSpeed
            spell.radius = 40.0f;               // mLineWidth (half-width; wiki width 80)
            spell.range = 1500.0f;              // wiki centered range = bin CastRange
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
            spell.projectileSpeed = 1600.0f;    // NocturneDuskbringer MissileSpec.mSpeed
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 1200.0f;              // wiki centered range = bin CastRange
            spell.spellDelay = 250.0f;
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "NocturneDuskbringer";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }

        // Lee Sin R, Nautilus R, Naafiri W, Nilah E and Nunu Q are targeted/dash mechanics; skip.
        // #endregion N

        // #region O
        {
            SpellData spell;
            spell.charName = "Olaf";
            spell.dangerlevel = 2;
            spell.missileName = "OlafAxeThrowCast";
            spell.name = "Undertow";
            spell.projectileSpeed = 1600.0f;    // OlafAxeThrowCast MissileSpeed
            spell.radius = 90.0f;               // mLineWidth (half-width; wiki width 180)
            spell.range = 1000.0f;              // wiki max target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "OlafAxeThrowCast";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Orianna";
            spell.dangerlevel = 2;
            spell.missileName = "OrianaRedact";
            spell.name = "Command: Attack";
            spell.projectileSpeed = 1400.0f;    // wiki speed
            spell.radius = 80.0f;               // mLineWidth (half-width; wiki width 160)
            spell.range = 825.0f;               // wiki target range
            spell.spellDelay = 250.0f;          // wiki cast time none (default)
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "OrianaRedactCommand";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Orianna";
            spell.dangerlevel = 4;
            spell.name = "Command: Shockwave";
            spell.radius = 415.0f;              // wiki effect radius
            spell.range = 415.0f;               // PBAoE centered on ball
            spell.spellDelay = 500.0f;          // wiki cast time 0.5
            spell.spellKey = SpellSlot::R;
            spell.spellName = "OrianaDetonateCommand";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Stun;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ornn";
            spell.dangerlevel = 2;
            spell.missileName = "OrnnQ";
            spell.name = "Volcanic Rupture";
            spell.projectileSpeed = 1800.0f;    // OrnnQ MissileSpec.mSpeed
            spell.radius = 65.0f;               // mLineWidth (half-width; wiki width 130)
            spell.range = 750.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "OrnnQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ornn";
            spell.dangerlevel = 4;
            spell.missileName = "OrnnRWave";
            spell.extraMissileNames = { "OrnnRWave2" };
            spell.name = "Call of the Forge God";
            spell.projectileSpeed = 450.0f;     // OrnnRWave MissileSpec.mSpeed (initial)
            spell.radius = 170.0f;              // mLineWidth 250 / 2 (half-width; wiki width 340)
            spell.range = 3000.0f;              // wiki effect radius (initial pass)
            spell.spellDelay = 500.0f;          // wiki cast time 0.5
            spell.spellKey = SpellSlot::R;
            spell.spellName = "OrnnR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Pantheon";
            spell.dangerlevel = 2;
            spell.missileName = "PantheonQMissile";
            spell.name = "Comet Spear";
            spell.projectileSpeed = 2700.0f;    // PantheonQMissile MissileSpec.mSpeed
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 1200.0f;              // wiki hurl range
            spell.spellDelay = 200.0f;          // wiki cast time 0.2
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "PantheonQMissile";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Pantheon";
            spell.dangerlevel = 4;
            spell.name = "Grand Starfall";
            spell.radius = 450.0f;              // wiki effect radius (landing)
            spell.range = 5500.0f;              // wiki target range
            spell.spellDelay = 2000.0f;         // channel 2s
            spell.spellKey = SpellSlot::R;
            spell.spellName = "PantheonRFall";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Poppy";
            spell.dangerlevel = 2;
            spell.name = "Hammer Shock";
            spell.radius = 80.0f;               // mLineWidth (half-width; wiki width 160)
            spell.range = 460.0f;               // wiki range
            spell.spellDelay = 332.0f;          // wiki cast time 0.3325
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "PoppyQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Poppy";
            spell.dangerlevel = 4;
            spell.missileName = "PoppyRMissile";
            spell.name = "Keeper's Verdict";
            spell.projectileSpeed = 2500.0f;    // PoppyRMissile MissileSpec.mSpeed
            spell.radius = 100.0f;              // mLineWidth (half-width; wiki width 180)
            spell.range = 1200.0f;              // wiki max charged range
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "PoppyR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Pyke";
            spell.dangerlevel = 3;
            spell.missileName = "PykeQRange";
            spell.name = "Bone Skewer";
            spell.projectileSpeed = 2000.0f;    // PykeQRange MissileSpec.mSpeed
            spell.radius = 70.0f;               // mLineWidth (half-width; wiki width 140)
            spell.range = 1100.0f;              // wiki max charged target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "PykeQRange";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Pyke";
            spell.dangerlevel = 4;
            spell.name = "Death from Below";
            spell.radius = 282.0f;              // wiki max effect radius
            spell.range = 750.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 500.0f;          // wiki cast time 0.5
            spell.spellKey = SpellSlot::R;
            spell.spellName = "PykeR";
            spell.spellType = SpellType::Circle;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Qiyana";
            spell.dangerlevel = 2;
            spell.missileName = "QiyanaQ_Rock";
            spell.extraMissileNames = { "QiyanaQ_Water", "QiyanaQ_Grass", "QiyanaQ_ExplosionMissile" };
            spell.name = "Elemental Wrath";
            spell.projectileSpeed = 1600.0f;    // QiyanaQ_Rock MissileSpec.mSpeed
            spell.radius = 70.0f;               // mLineWidth (half-width; wiki width 140)
            spell.range = 865.0f;               // wiki Elemental Wrath range
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "QiyanaQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Qiyana";
            spell.dangerlevel = 4;
            spell.missileName = "QiyanaRMissile";
            spell.name = "Supreme Display of Terrible Power";
            spell.projectileSpeed = 2000.0f;    // QiyanaRMissile MissileSpec.mSpeed
            spell.radius = 120.0f;              // mLineWidth (half-width)
            spell.range = 950.0f;               // wiki target range = bin QiyanaR CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "QiyanaR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Quinn";
            spell.dangerlevel = 2;
            spell.missileName = "QuinnQ";
            spell.name = "Blinding Assault";
            spell.projectileSpeed = 1550.0f;    // QuinnQ MissileSpec.mSpeed
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 1050.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "QuinnQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Blind;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rakan";
            spell.dangerlevel = 2;
            spell.missileName = "RakanQMis";
            spell.name = "Gleaming Quill";
            spell.projectileSpeed = 1850.0f;    // RakanQMis MissileSpec.mSpeed
            spell.radius = 65.0f;               // mLineWidth (half-width; wiki width 130)
            spell.range = 900.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "RakanQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rakan";
            spell.dangerlevel = 3;
            spell.name = "Grand Entrance";
            spell.radius = 275.0f;              // wiki effect radius
            spell.range = 650.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 350.0f;          // 0.35s delay after arrival
            spell.spellKey = SpellSlot::W;
            spell.spellName = "RakanW";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "RekSai";
            spell.dangerlevel = 2;
            spell.missileName = "RekSaiQBurrowedMis";
            spell.name = "Prey Seeker";
            spell.projectileSpeed = 1950.0f;    // RekSaiQBurrowedMis MissileSpec.mSpeed
            spell.radius = 65.0f;               // mLineWidth (half-width; wiki width 130)
            spell.range = 1500.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 125.0f;          // wiki cast time 0.125
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "RekSaiQBurrowed";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rell";
            spell.dangerlevel = 3;
            spell.name = "Shattering Strike";
            spell.radius = 75.0f;               // wiki width 150 / 2 (half-width)
            spell.range = 520.0f;               // wiki range
            spell.spellDelay = 400.0f;          // wiki cast time 0.4
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "RellQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rell";
            spell.dangerlevel = 3;
            spell.name = "Ferromancy: Crash Down";
            spell.radius = 200.0f;              // wiki collision radius
            spell.range = 400.0f;               // wiki max target range
            spell.spellDelay = 625.0f;          // wiki cast time 0.625
            spell.spellKey = SpellSlot::W;
            spell.spellName = "RellW";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "RenataGlasc";
            spell.dangerlevel = 2;
            spell.missileName = "RenataQ";
            spell.name = "Bug Out";
            spell.projectileSpeed = 1450.0f;    // RenataQ MissileSpec.mSpeed
            spell.radius = 70.0f;               // mLineWidth (half-width)
            spell.range = 900.0f;               // wiki range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "RenataQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "RenataGlasc";
            spell.dangerlevel = 3;
            spell.missileName = "RenataE";
            spell.name = "Loyalty Program";
            spell.projectileSpeed = 1450.0f;    // RenataE MissileSpec.mSpeed
            spell.radius = 110.0f;              // mLineWidth (half-width; wiki width 220)
            spell.range = 800.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "RenataE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "RenataGlasc";
            spell.dangerlevel = 4;
            spell.missileName = "RenataRMissile";
            spell.name = "Hostile Takeover";
            spell.projectileSpeed = 1200.0f;    // RenataR MissileSpeed
            spell.radius = 250.0f;              // mLineWidth (half-width; wiki width 500)
            spell.range = 2000.0f;              // wiki centered range = bin CastRange
            spell.spellDelay = 750.0f;          // wiki cast time 0.75
            spell.spellKey = SpellSlot::R;
            spell.spellName = "RenataR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Charm;
            spell.fixedRange = true;
            spell.angle = 14.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rengar";
            spell.dangerlevel = 2;
            spell.missileName = "RengarEMis";
            spell.extraMissileNames = { "RengarEEmpMis" };
            spell.name = "Bola Strike";
            spell.projectileSpeed = 1500.0f;    // RengarEMis MissileSpec.mSpeed
            spell.radius = 70.0f;               // mLineWidth (half-width; wiki width 140)
            spell.range = 1000.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "RengarE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rumble";
            spell.dangerlevel = 2;
            spell.missileName = "RumbleGrenadeMissile";
            spell.extraMissileNames = { "RumbleGrenadeMissileDangerZone" };
            spell.name = "Electro Harpoon";
            spell.projectileSpeed = 2000.0f;    // RumbleGrenadeMissile MissileSpec.mSpeed
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 950.0f;               // wiki range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "RumbleGrenade";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rumble";
            spell.dangerlevel = 4;
            spell.missileName = "RumbleCarpetBombMissile";
            spell.name = "The Equalizer";
            spell.projectileSpeed = 1600.0f;    // RumbleCarpetBombMissile MissileSpec.mSpeed
            spell.radius = 200.0f;              // mLineWidth (half-width; wiki width ~400)
            spell.range = 1700.0f;              // wiki target range
            spell.spellDelay = 583.0f;          // wiki cast time 0.5833
            spell.spellKey = SpellSlot::R;
            spell.spellName = "RumbleCarpetBomb";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ryze";
            spell.dangerlevel = 2;
            spell.missileName = "RyzeQ";
            spell.name = "Overload";
            spell.projectileSpeed = 1700.0f;    // RyzeQ MissileSpec.mSpeed
            spell.radius = 55.0f;               // mLineWidth (half-width; wiki width 110)
            spell.range = 1000.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "RyzeQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Samira";
            spell.dangerlevel = 2;
            spell.missileName = "SamiraQGun";
            spell.name = "Flair";
            spell.projectileSpeed = 2600.0f;    // SamiraQGun MissileSpec.mSpeed
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 950.0f;               // wiki range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "SamiraQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        {
            SpellData spell;
            spell.charName = "Sejuani";
            spell.dangerlevel = 3;
            spell.name = "Arctic Assault";
            spell.projectileSpeed = 1000.0f;    // wiki speed
            spell.radius = 75.0f;               // wiki collision radius
            spell.range = 650.0f;               // wiki target range
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "SejuaniQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sejuani";
            spell.dangerlevel = 4;
            spell.missileName = "SejuaniRMissile";
            spell.name = "Glacial Prison";
            spell.projectileSpeed = 1600.0f;    // SejuaniRMissile MissileSpec.mSpeed
            spell.radius = 120.0f;              // mLineWidth (half-width; wiki width 240)
            spell.range = 1300.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "SejuaniR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.dangerlevel = 3;
            spell.missileName = "SeraphineEMissile";
            spell.name = "Beat Drop";
            spell.projectileSpeed = 1200.0f;    // SeraphineEMissile MissileSpec.mSpeed
            spell.radius = 80.0f;               // mLineWidth (half-width; wiki width 140 - approx)
            spell.range = 1300.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "SeraphineE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.dangerlevel = 4;
            spell.missileName = "SeraphineR";
            spell.name = "Encore";
            spell.projectileSpeed = 1600.0f;    // SeraphineR MissileSpec.mSpeed
            spell.radius = 160.0f;              // half-width (wiki width 320)
            spell.range = 1300.0f;              // wiki target range
            spell.spellDelay = 500.0f;          // wiki cast time 0.5
            spell.spellKey = SpellSlot::R;
            spell.spellName = "SeraphineR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Charm;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sett";
            spell.dangerlevel = 3;
            spell.missileName = "SettWPassiveBuff";
            spell.name = "Haymaker";
            spell.radius = 90.0f;               // mLineWidth (half-width)
            spell.range = 720.0f;               // wiki circle radius
            spell.spellDelay = 750.0f;          // wiki cast time 0.75
            spell.spellKey = SpellSlot::W;
            spell.spellName = "SettW";
            spell.spellType = SpellType::Line;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sett";
            spell.dangerlevel = 2;
            spell.name = "The Show Stopper";
            spell.radius = 600.0f;              // wiki effect radius
            spell.range = 400.0f;               // wiki target range
            spell.spellKey = SpellSlot::R;
            spell.spellName = "SettR";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Shen";
            spell.dangerlevel = 3;
            spell.name = "Shadow Dash";
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki collision radius 60)
            spell.range = 600.0f;               // wiki max target range
            spell.spellKey = SpellSlot::E;
            spell.spellName = "ShenE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Taunt;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Shyvana";
            spell.dangerlevel = 2;
            spell.missileName = "ShyvanaE";
            spell.extraMissileNames = { "ShyvanaEDragon" };
            spell.name = "Flame Breath";
            spell.projectileSpeed = 1600.0f;    // ShyvanaEDragon MissileSpec.mSpeed
            spell.radius = 60.0f;               // half-width (wiki width 120)
            spell.range = 925.0f;               // wiki target range
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "ShyvanaE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Singed";
            spell.dangerlevel = 3;
            spell.missileName = "MegaAdhesive";
            spell.name = "Mega Adhesive";
            spell.projectileSpeed = 700.0f;    // MegaAdhesive MissileSpeed
            spell.radius = 265.0f;             // wiki effect radius
            spell.range = 1000.0f;             // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;         // wiki cast time 0.25
            spell.spellKey = SpellSlot::W;
            spell.spellName = "MegaAdhesive";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sion";
            spell.dangerlevel = 2;
            spell.missileName = "SionEMissile";
            spell.name = "Roar of the Slayer";
            spell.projectileSpeed = 1800.0f;    // SionEMissile MissileSpec.mSpeed
            spell.radius = 80.0f;               // mLineWidth (half-width; wiki width 160)
            spell.range = 800.0f;               // wiki range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "SionE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sion";
            spell.dangerlevel = 4;
            spell.name = "Unstoppable Onslaught";
            spell.radius = 100.0f;              // mLineWidth (half-width)
            spell.range = 7500.0f;              // bin CastRange
            spell.projectileSpeed = 1500.0f;    // bin MissileSpeed
            spell.spellKey = SpellSlot::R;
            spell.spellName = "SionR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sivir";
            spell.dangerlevel = 2;
            spell.missileName = "SivirQMissile";
            spell.extraMissileNames = { "SivirQMissileReturn" };
            spell.name = "Boomerang Blade";
            spell.projectileSpeed = 1450.0f;    // SivirQMissile MissileSpec.mSpeed
            spell.radius = 90.0f;               // mLineWidth (half-width; wiki width 180)
            spell.range = 1250.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "SivirQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Skarner";
            spell.dangerlevel = 4;
            spell.name = "Impale";
            spell.radius = 175.0f;              // half-width (wiki width 350)
            spell.range = 625.0f;               // wiki range = bin CastRange
            spell.spellDelay = 750.0f;          // wiki cast time 0.75
            spell.spellKey = SpellSlot::R;
            spell.spellName = "SkarnerR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Suppression;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Smolder";
            spell.dangerlevel = 2;
            spell.missileName = "SmolderW";
            spell.name = "Achooo!";
            spell.projectileSpeed = 2000.0f;    // wiki speed
            spell.radius = 75.0f;               // mLineWidth (half-width)
            spell.range = 1500.0f;              // wiki range = bin CastRange
            spell.spellDelay = 350.0f;          // wiki cast time 0.35
            spell.spellKey = SpellSlot::W;
            spell.spellName = "SmolderW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Smolder";
            spell.dangerlevel = 3;
            spell.missileName = "SmolderRMomMissile";
            spell.name = "MMOOOMMMM!";
            spell.projectileSpeed = 1700.0f;    // SmolderRMomMissile MissileSpec.mSpeed
            spell.radius = 125.0f;              // wiki width (half-width)
            spell.range = 4250.0f;              // wiki range = bin CastRange
            spell.spellDelay = 750.0f;          // wiki cast time 0.75
            spell.spellKey = SpellSlot::R;
            spell.spellName = "SmolderR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sona";
            spell.dangerlevel = 4;
            spell.missileName = "SonaR";
            spell.name = "Crescendo";
            spell.projectileSpeed = 2400.0f;    // SonaR MissileSpec.mSpeed
            spell.radius = 140.0f;              // mLineWidth (half-width; wiki width 280)
            spell.range = 1000.0f;              // wiki range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "SonaR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Soraka";
            spell.dangerlevel = 2;
            spell.name = "Starcall";
            spell.radius = 265.0f;              // wiki effect radius
            spell.range = 800.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "SorakaQ";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Soraka";
            spell.dangerlevel = 3;
            spell.name = "Equinox";
            spell.radius = 260.0f;              // wiki effect radius = bin CastRadius
            spell.range = 925.0f;               // wiki target range
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "SorakaE";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Silence;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Swain";
            spell.dangerlevel = 2;
            spell.name = "Vision of Empire";
            spell.radius = 325.0f;              // wiki effect radius
            spell.range = 7500.0f;              // wiki max target range
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::W;
            spell.spellName = "SwainW";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Swain";
            spell.dangerlevel = 3;
            spell.missileName = "SwainE";
            spell.extraMissileNames = { "SwainEReturnMissile" };
            spell.name = "Nevermove";
            spell.projectileSpeed = 935.0f;     // SwainE MissileSpeed
            spell.radius = 85.0f;               // mLineWidth (half-width; wiki width 180)
            spell.range = 850.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "SwainE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Root;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sylas";
            spell.dangerlevel = 2;
            spell.missileName = "SylasQ";
            spell.name = "Chain Lash";
            spell.projectileSpeed = 1800.0f;    // SylasQ MissileSpec.mSpeed
            spell.radius = 200.0f;              // wiki effect radius (explosion)
            spell.range = 775.0f;               // wiki target range
            spell.spellDelay = 400.0f;          // wiki cast time 0.4
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "SylasQ";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sylas";
            spell.dangerlevel = 3;
            spell.missileName = "SylasE2";
            spell.name = "Abduct";
            spell.projectileSpeed = 1600.0f;    // SylasE2 MissileSpeed
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 850.0f;               // bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "SylasE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Syndra";
            spell.dangerlevel = 2;
            spell.name = "Dark Sphere";
            spell.radius = 210.0f;              // wiki effect radius
            spell.range = 800.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 600.0f;          // wiki 0.6s delay
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "SyndraQ";
            spell.spellType = SpellType::Circle;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Syndra";
            spell.dangerlevel = 3;
            spell.missileName = "SyndraEMissile";
            spell.extraMissileNames = { "SyndraESphereMissile" };
            spell.name = "Scatter the Weak";
            spell.projectileSpeed = 2500.0f;    // SyndraEMissile MissileSpec.mSpeed
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 700.0f;               // wiki range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "SyndraE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Taliyah";
            spell.dangerlevel = 3;
            spell.name = "Seismic Shove";
            spell.radius = 225.0f;              // wiki effect radius
            spell.range = 900.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 1042.0f;         // wiki cast time 0.25 + 0.792 delay
            spell.spellKey = SpellSlot::W;
            spell.spellName = "TaliyahW";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Taliyah";
            spell.dangerlevel = 2;
            spell.name = "Unraveled Earth";
            spell.radius = 800.0f;              // wiki effect radius
            spell.range = 950.0f;               // bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "TaliyahE";
            spell.spellType = SpellType::Cone;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Talon";
            spell.dangerlevel = 2;
            spell.missileName = "TalonWMissileOne";
            spell.extraMissileNames = { "TalonWMissileTwo" };
            spell.name = "Rake";
            spell.projectileSpeed = 2500.0f;    // TalonWMissileOne MissileSpeed
            spell.radius = 75.0f;               // mLineWidth (half-width; wiki width 150)
            spell.range = 900.0f;               // wiki range
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::W;
            spell.spellName = "TalonW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Talon";
            spell.dangerlevel = 3;
            spell.missileName = "TalonRMisOne";
            spell.extraMissileNames = { "TalonRMisTwo" };
            spell.name = "Shadow Assault";
            spell.projectileSpeed = 2400.0f;    // TalonRMisOne MissileSpeed
            spell.radius = 140.0f;              // mLineWidth (half-width; wiki width 280)
            spell.range = 550.0f;               // wiki effect radius
            spell.spellKey = SpellSlot::R;
            spell.spellName = "TalonR";
            spell.spellType = SpellType::Line;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Taric";
            spell.dangerlevel = 3;
            spell.missileName = "TaricE";
            spell.name = "Dazzle";
            spell.projectileSpeed = 1750.0f;    // TaricE MissileSpeed
            spell.radius = 100.0f;              // mLineWidth (half-width; wiki width 140)
            spell.range = 575.0f;               // wiki range
            spell.spellDelay = 1000.0f;         // wiki 1s windup
            spell.spellKey = SpellSlot::E;
            spell.spellName = "TaricE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Teemo";
            spell.dangerlevel = 2;
            spell.missileName = "TeemoQ";
            spell.name = "Blinding Dart";
            spell.projectileSpeed = 2500.0f;    // TeemoQ MissileSpec.mSpeed
            spell.radius = 20.0f;               // targeted missile default
            spell.range = 680.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "TeemoQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Blind;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Thresh";
            spell.dangerlevel = 4;
            spell.missileName = "ThreshQMissile";
            spell.name = "Death Sentence";
            spell.projectileSpeed = 1900.0f;    // ThreshQMissile MissileSpec.mSpeed
            spell.radius = 70.0f;               // mLineWidth (half-width; wiki width 140)
            spell.range = 1100.0f;              // wiki range = bin CastRange
            spell.spellDelay = 500.0f;          // wiki cast time 0.5
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "ThreshQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Thresh";
            spell.dangerlevel = 3;
            spell.missileName = "ThreshEMissile1";
            spell.name = "Flay";
            spell.projectileSpeed = 2000.0f;    // ThreshEMissile1 MissileSpeed
            spell.radius = 110.0f;              // mLineWidth (half-width; wiki width 220)
            spell.range = 525.0f;               // wiki range
            spell.spellDelay = 389.0f;          // wiki cast time 0.3889
            spell.spellKey = SpellSlot::E;
            spell.spellName = "ThreshE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Thresh";
            spell.dangerlevel = 4;
            spell.name = "The Box";
            spell.radius = 400.0f;              // bin CastRadius (distance to walls)
            spell.range = 400.0f;               // bin CastRadius
            spell.spellDelay = 450.0f;          // wiki cast time 0.45
            spell.spellKey = SpellSlot::R;
            spell.spellName = "ThreshR";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Trundle";
            spell.dangerlevel = 2;
            spell.name = "Pillar of Ice";
            spell.radius = 225.0f;              // wiki effect radius
            spell.range = 1000.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "TrundleE";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Tryndamere";
            spell.dangerlevel = 2;
            spell.name = "Spinning Slash";
            spell.projectileSpeed = 700.0f;     // TryndamereE MissileSpeed
            spell.radius = 160.0f;              // mLineWidth (half-width)
            spell.range = 660.0f;               // wiki target range
            spell.spellKey = SpellSlot::E;
            spell.spellName = "TryndamereE";
            spell.spellType = SpellType::Line;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "TwistedFate";
            spell.dangerlevel = 2;
            spell.missileName = "WildCards";
            spell.name = "Wild Cards";
            spell.projectileSpeed = 1450.0f;    // WildCards MissileSpeed
            spell.radius = 40.0f;               // half-width (wiki width 80)
            spell.range = 1450.0f;              // wiki target range
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "WildCards";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Twitch";
            spell.dangerlevel = 2;
            spell.missileName = "TwitchVenomCaskMissile";
            spell.name = "Venom Cask";
            spell.projectileSpeed = 1400.0f;    // TwitchVenomCaskMissile MissileSpeed
            spell.radius = 300.0f;              // wiki effect radius (edge)
            spell.range = 950.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::W;
            spell.spellName = "TwitchVenomCask";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Urgot";
            spell.dangerlevel = 2;
            spell.name = "Corrosive Charge";
            spell.radius = 210.0f;              // wiki effect radius = bin CastRadius
            spell.range = 800.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 550.0f;          // wiki cast time 0.25 + 0.3s landing delay
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "UrgotQ";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Urgot";
            spell.dangerlevel = 5;
            spell.missileName = "UrgotR";
            spell.name = "Fear Beyond Death";
            spell.projectileSpeed = 3200.0f;    // UrgotR MissileSpec.mSpeed
            spell.radius = 80.0f;               // mLineWidth (half-width; wiki width 160)
            spell.range = 2500.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 500.0f;          // wiki cast time 0.5
            spell.spellKey = SpellSlot::R;
            spell.spellName = "UrgotR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Suppression;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Varus";
            spell.dangerlevel = 3;
            spell.missileName = "VarusQMissile";
            spell.name = "Piercing Arrow";
            spell.projectileSpeed = 1900.0f;    // VarusQMissile MissileSpec.mSpeed
            spell.radius = 70.0f;               // mLineWidth (half-width; wiki width 140)
            spell.range = 1600.0f;              // wiki max range
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "VarusQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Varus";
            spell.dangerlevel = 2;
            spell.missileName = "VarusEMissile";
            spell.name = "Hail of Arrows";
            spell.projectileSpeed = 1750.0f;    // VarusE MissileSpeed
            spell.radius = 300.0f;              // wiki effect radius
            spell.range = 925.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 742.0f;          // wiki cast time 0.2419 + 0.5s landing
            spell.spellKey = SpellSlot::E;
            spell.spellName = "VarusE";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Varus";
            spell.dangerlevel = 5;
            spell.missileName = "VarusRMissile";
            spell.name = "Chain of Corruption";
            spell.projectileSpeed = 1500.0f;    // VarusRMissile MissileSpec.mSpeed
            spell.radius = 120.0f;              // mLineWidth (half-width; wiki width 240)
            spell.range = 1370.0f;              // wiki range
            spell.spellDelay = 242.0f;          // wiki cast time 0.2419
            spell.spellKey = SpellSlot::R;
            spell.spellName = "VarusR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Root;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vayne";
            spell.dangerlevel = 3;
            spell.missileName = "VayneCondemnMissile";
            spell.name = "Condemn";
            spell.projectileSpeed = 2200.0f;    // VayneCondemnMissile MissileSpec.mSpeed
            spell.radius = 20.0f;               // targeted missile default
            spell.range = 550.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "VayneCondemn";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Knockback;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.dangerlevel = 2;
            spell.missileName = "VeigarBalefulStrikeMis";
            spell.name = "Baleful Strike";
            spell.projectileSpeed = 2200.0f;    // VeigarBalefulStrikeMis MissileSpec.mSpeed
            spell.radius = 70.0f;               // mLineWidth (half-width)
            spell.range = 1050.0f;              // bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "VeigarBalefulStrike";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.dangerlevel = 3;
            spell.name = "Dark Matter";
            spell.radius = 240.0f;              // wiki effect radius
            spell.range = 950.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 1221.0f;         // wiki 1.221s delay
            spell.spellKey = SpellSlot::W;
            spell.spellName = "VeigarDarkMatter";
            spell.spellType = SpellType::Circle;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.dangerlevel = 4;
            spell.name = "Event Horizon";
            spell.radius = 390.0f;              // wiki effect radius (cage)
            spell.range = 725.0f;               // wiki target range
            spell.spellDelay = 750.0f;          // wiki cast time 0.25 + 0.5s delay
            spell.spellKey = SpellSlot::E;
            spell.spellName = "VeigarEventHorizon";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Stun;
            spell.isSpecial = true;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Velkoz";
            spell.dangerlevel = 3;
            spell.missileName = "VelkozQMissile";
            spell.extraMissileNames = { "VelkozQMissileSplit" };
            spell.name = "Plasma Fission";
            spell.projectileSpeed = 1300.0f;    // VelkozQMissile MissileSpec.mSpeed
            spell.radius = 50.0f;               // mLineWidth (half-width; wiki width 100)
            spell.range = 1100.0f;              // wiki range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "VelkozQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Velkoz";
            spell.dangerlevel = 2;
            spell.missileName = "VelkozWMissile";
            spell.name = "Void Rift";
            spell.projectileSpeed = 1700.0f;    // VelkozWMissile MissileSpec.mSpeed
            spell.radius = 87.0f;               // mLineWidth (half-width; wiki width 175)
            spell.range = 1105.0f;              // wiki range
            spell.spellDelay = 250.0f;          // wiki 0.25s delay
            spell.spellKey = SpellSlot::W;
            spell.spellName = "VelkozW";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Velkoz";
            spell.dangerlevel = 3;
            spell.name = "Tectonic Disruption";
            spell.radius = 225.0f;              // wiki effect radius = bin CastRadius
            spell.range = 800.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 800.0f;          // wiki cast time 0.25 + 0.55s max delay
            spell.spellKey = SpellSlot::E;
            spell.spellName = "VelkozE";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.dangerlevel = 2;
            spell.missileName = "VexQ";
            spell.name = "Mistral Bolt";
            spell.projectileSpeed = 600.0f;     // VexQ MissileSpec.mSpeed (initial)
            spell.radius = 80.0f;               // half-width (wiki width 160 narrow)
            spell.range = 1200.0f;              // wiki range = bin CastRange
            spell.spellDelay = 150.0f;          // wiki cast time 0.15
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "VexQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.dangerlevel = 2;
            spell.missileName = "VexE";
            spell.name = "Looming Darkness";
            spell.projectileSpeed = 1300.0f;    // VexE MissileSpec.mSpeed
            spell.radius = 300.0f;              // wiki max effect radius
            spell.range = 800.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "VexE";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.dangerlevel = 5;
            spell.missileName = "VexR";
            spell.name = "Shadow Surge";
            spell.projectileSpeed = 1600.0f;    // VexR MissileSpec.mSpeed
            spell.radius = 130.0f;              // mLineWidth (half-width; wiki width 260)
            spell.range = 3000.0f;              // wiki max range
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "VexR";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vi";
            spell.dangerlevel = 3;
            spell.missileName = "ViQ";
            spell.name = "Vault Breaker";
            spell.projectileSpeed = 1500.0f;    // ViQ MissileSpeed
            spell.radius = 55.0f;               // mLineWidth (collision radius)
            spell.range = 725.0f;               // wiki max target range
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "ViQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Knockback;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Viktor";
            spell.dangerlevel = 3;
            spell.missileName = "ViktorEMissile";
            spell.extraMissileNames = { "ViktorEMissile2", "ViktorEAugMissile" };
            spell.name = "Death Ray";
            spell.projectileSpeed = 1050.0f;    // ViktorEMissile MissileSpec.mSpeed
            spell.radius = 90.0f;               // mLineWidth (half-width)
            spell.range = 600.0f;               // wiki target range 550 + beam range 500
            spell.spellKey = SpellSlot::E;
            spell.spellName = "ViktorE";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Viktor";
            spell.dangerlevel = 3;
            spell.name = "Gravity Field";
            spell.radius = 300.0f;              // bin CastRadius
            spell.range = 800.0f;               // bin CastRange
            spell.spellKey = SpellSlot::W;
            spell.spellName = "ViktorW";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Stun;
            spell.isSpecial = true;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Viktor";
            spell.dangerlevel = 4;
            spell.name = "Chaos Storm";
            spell.radius = 325.0f;              // wiki effect radius
            spell.range = 700.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::R;
            spell.spellName = "ViktorR";
            spell.spellType = SpellType::Circle;
            spell.isSpecial = true;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vladimir";
            spell.dangerlevel = 2;
            spell.missileName = "VladimirEMissile";
            spell.name = "Tides of Blood";
            spell.projectileSpeed = 4000.0f;    // VladimirEMissile MissileSpec.mSpeed
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 600.0f;               // wiki effect radius
            spell.spellKey = SpellSlot::E;
            spell.spellName = "VladimirE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vladimir";
            spell.dangerlevel = 3;
            spell.name = "Hemoplague";
            spell.radius = 375.0f;              // wiki effect radius = bin CastRadius
            spell.range = 625.0f;               // wiki target range = bin CastRange
            spell.spellKey = SpellSlot::R;
            spell.spellName = "VladimirR";
            spell.spellType = SpellType::Circle;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Volibear";
            spell.dangerlevel = 3;
            spell.name = "Sky Splitter";
            spell.radius = 325.0f;              // wiki effect radius = bin CastRadius
            spell.range = 1200.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 2000.0f;         // wiki 2s delay
            spell.spellKey = SpellSlot::E;
            spell.spellName = "VolibearE";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Volibear";
            spell.dangerlevel = 1;
            spell.name = "Stormbringer";
            spell.radius = 300.0f;              // wiki epicenter effect radius
            spell.range = 700.0f;               // wiki target range = bin CastRange
            spell.spellDelay = 1000.0f;         // wiki 1s leap duration
            spell.spellKey = SpellSlot::R;
            spell.spellName = "VolibearR";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.isSpecial = true;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Warwick";
            spell.dangerlevel = 5;
            spell.missileName = "WarwickR";
            spell.name = "Infinite Duress";
            spell.radius = 205.0f;              // wiki collision radius
            spell.range = 1100.0f;              // approx max leap range with Blood Hunt
            spell.spellDelay = 100.0f;          // wiki cast time 0.1
            spell.spellKey = SpellSlot::R;
            spell.spellName = "WarwickR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Suppression;
            spell.isSpecial = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xayah";
            spell.dangerlevel = 2;
            spell.missileName = "XayahQMissile1";
            spell.extraMissileNames = { "XayahQMissile2" };
            spell.name = "Double Daggers";
            spell.projectileSpeed = 400.0f;     // XayahQMissile1 MissileSpec.mSpeed
            spell.radius = 50.0f;               // mLineWidth (half-width; wiki width 100)
            spell.range = 1100.0f;              // wiki range = bin CastRange
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "XayahQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.dangerlevel = 3;
            spell.missileName = "XerathArcanopulse2";
            spell.name = "Arcanopulse";
            spell.projectileSpeed = 3000.0f;    // XerathArcanopulse2 MissileSpeed
            spell.radius = 72.0f;               // half-width (wiki width 145)
            spell.range = 1450.0f;              // wiki max range
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "XerathArcanopulse";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.dangerlevel = 2;
            spell.name = "Eye of Destruction";
            spell.radius = 275.0f;              // wiki effect radius
            spell.range = 1000.0f;              // wiki target range = bin CastRange
            spell.spellDelay = 778.0f;          // wiki cast time 0.25 + 0.528s delay
            spell.spellKey = SpellSlot::W;
            spell.spellName = "XerathArcaneBarrage";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.dangerlevel = 3;
            spell.missileName = "XerathMageSpearMissile";
            spell.name = "Shocking Orb";
            spell.projectileSpeed = 1400.0f;    // XerathMageSpearMissile MissileSpec.mSpeed
            spell.radius = 60.0f;               // mLineWidth (half-width; wiki width 120)
            spell.range = 1125.0f;              // wiki range
            spell.spellDelay = 250.0f;          // wiki cast time 0.25
            spell.spellKey = SpellSlot::E;
            spell.spellName = "XerathMageSpear";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            spell.collisionObjects = { CollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.dangerlevel = 4;
            spell.missileName = "XerathLocusPulse";
            spell.name = "Rite of the Arcane";
            spell.radius = 200.0f;              // wiki effect radius
            spell.range = 3200.0f;              // wiki range
            spell.spellDelay = 400.0f;          // wiki cast time 0.4
            spell.spellKey = SpellSlot::R;
            spell.spellName = "XerathLocusOfPower2";
            spell.spellType = SpellType::Circle;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yasuo";
            spell.dangerlevel = 3;
            spell.missileName = "YasuoQ3Mis";
            spell.name = "Steel Tempest (Tornado)";
            spell.projectileSpeed = 1200.0f;   // YasuoQ3Mis MissileSpec.mSpeed
            spell.radius = 45.0f;              // mLineWidth (half-width; wiki width 90)
            spell.range = 1000.0f;             // wiki tornado range
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "YasuoQ3";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yone";
            spell.dangerlevel = 3;
            spell.missileName = "YoneQ3Missile";
            spell.name = "Mortal Steel (Tornado)";
            spell.projectileSpeed = 1500.0f;   // YoneQ3Missile MissileSpec.mSpeed
            spell.radius = 45.0f;              // half-width (default)
            spell.range = 950.0f;              // bin CastRange
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "YoneQ3";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yone";
            spell.dangerlevel = 2;
            spell.name = "Spirit Cleave";
            spell.radius = 400.0f;             // cone half-angle
            spell.range = 700.0f;              // bin CastRange
            spell.spellKey = SpellSlot::W;
            spell.spellName = "YoneW";
            spell.spellType = SpellType::Cone;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yone";
            spell.dangerlevel = 5;
            spell.missileName = "YoneR";
            spell.name = "Fate Sealed";
            spell.projectileSpeed = 1500.0f;   // YoneR MissileSpeed
            spell.radius = 112.0f;             // mLineWidth (half-width; wiki width 225)
            spell.range = 1000.0f;             // wiki range = bin CastRange
            spell.spellDelay = 750.0f;         // wiki cast time 0.75
            spell.spellKey = SpellSlot::R;
            spell.spellName = "YoneR";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.isSpecial = true;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yorick";
            spell.dangerlevel = 2;
            spell.missileName = "YorickEMissile";
            spell.name = "Mourning Mist";
            spell.projectileSpeed = 1800.0f;   // YorickEMissile MissileSpec.mSpeed
            spell.radius = 40.0f;              // mLineWidth (half-width)
            spell.range = 700.0f;              // wiki range
            spell.spellKey = SpellSlot::E;
            spell.spellName = "YorickE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yuumi";
            spell.dangerlevel = 2;
            spell.missileName = "YuumiQ";
            spell.name = "Prowling Projectile";
            spell.projectileSpeed = 100.0f;    // YuumiQ MissileSpeed
            spell.radius = 32.0f;              // mLineWidth (half-width; wiki width 65)
            spell.range = 1150.0f;             // bin CastRange
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "YuumiQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.isSpecial = true;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Zac";
            spell.dangerlevel = 2;
            spell.missileName = "ZacQMissile";
            spell.name = "Stretching Strike";
            spell.projectileSpeed = 2800.0f;   // ZacQMissile MissileSpec.mSpeed
            spell.radius = 40.0f;              // mLineWidth (half-width)
            spell.range = 951.0f;              // bin CastRange
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "ZacQ";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zac";
            spell.dangerlevel = 4;
            spell.missileName = "ZacETimingMissile";
            spell.name = "Elastic Slingshot";
            spell.projectileSpeed = 1000.0f;   // ZacETimingMissile MissileSpec.mSpeed
            spell.radius = 265.0f;             // wiki effect radius
            spell.range = 1800.0f;             // wiki max target range
            spell.spellKey = SpellSlot::E;
            spell.spellName = "ZacE";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zed";
            spell.dangerlevel = 3;
            spell.missileName = "ZedQMissile";
            spell.name = "Razor Shuriken";
            spell.projectileSpeed = 1700.0f;   // ZedQMissile MissileSpec.mSpeed
            spell.radius = 25.0f;              // mLineWidth (half-width)
            spell.range = 925.0f;              // bin CastRange
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "ZedQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zeri";
            spell.dangerlevel = 1;
            spell.missileName = "ZeriQMis";
            spell.extraMissileNames = { "ZeriQMisPierce", "ZeriQMisEmpowered", "ZeriQMisEmpoweredPierce" };
            spell.name = "Burst Fire";
            spell.projectileSpeed = 2600.0f;   // ZeriQMis MissileSpec.mSpeed
            spell.radius = 30.0f;              // mLineWidth (half-width; wiki width 60)
            spell.range = 825.0f;              // bin CastRange
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "ZeriQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zeri";
            spell.dangerlevel = 3;
            spell.missileName = "ZeriW";
            spell.name = "Ultrashock Laser";
            spell.projectileSpeed = 2500.0f;   // ZeriW MissileSpec.mSpeed
            spell.radius = 20.0f;              // mLineWidth (half-width)
            spell.range = 1200.0f;             // bin CastRange
            spell.spellKey = SpellSlot::W;
            spell.spellName = "ZeriW";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Slow;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 3;
            spell.missileName = "ZiggsQSpell";
            spell.name = "Bouncing Bomb";
            spell.projectileSpeed = 1700.0f;   // ZiggsQSpell MissileSpec.mSpeed
            spell.radius = 150.0f;             // wiki effect radius
            spell.range = 1400.0f;             // wiki max range
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "ZiggsQ";
            spell.spellType = SpellType::Circle;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 5;
            spell.missileName = "ZiggsRBoomLong";
            spell.extraMissileNames = { "ZiggsRBoomMedium", "ZiggsRBoomExtraLong" };
            spell.name = "Mega Inferno Bomb";
            spell.projectileSpeed = 2250.0f;   // ZiggsRBoomLong MissileSpec.mSpeed
            spell.radius = 525.0f;             // wiki effect radius
            spell.range = 5300.0f;             // wiki range
            spell.spellKey = SpellSlot::R;
            spell.spellName = "ZiggsR";
            spell.spellType = SpellType::Circle;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zilean";
            spell.dangerlevel = 3;
            spell.missileName = "ZileanQMissile";
            spell.name = "Time Bomb";
            spell.projectileSpeed = 2000.0f;   // ZileanQMissile MissileSpeed
            spell.radius = 150.0f;             // wiki effect radius
            spell.range = 900.0f;              // wiki range = bin CastRange
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "ZileanQ";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::Stun;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zoe";
            spell.dangerlevel = 3;
            spell.missileName = "ZoeQMissile";
            spell.extraMissileNames = { "ZoeQMis2", "ZoeQRecast" };
            spell.name = "Paddle Star";
            spell.projectileSpeed = 2500.0f;   // ZoeQMis2 MissileSpeed (max)
            spell.radius = 25.0f;              // mLineWidth (half-width)
            spell.range = 1300.0f;             // wiki max range
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "ZoeQ";
            spell.spellType = SpellType::Line;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zoe";
            spell.dangerlevel = 4;
            spell.missileName = "ZoeEMis";
            spell.extraMissileNames = { "ZoeEb", "ZoeEb2", "ZoeEb3", "ZoeEb4", "ZoeEb5" };
            spell.name = "Sleep Trouble Bubble";
            spell.projectileSpeed = 1850.0f;   // ZoeE MissileSpec.mSpeed
            spell.radius = 20.0f;              // mLineWidth (half-width)
            spell.range = 2875.0f;             // wiki max range (through walls)
            spell.spellKey = SpellSlot::E;
            spell.spellName = "ZoeE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Sleep;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zyra";
            spell.dangerlevel = 2;
            spell.missileName = "ZyraQ";
            spell.name = "Deadly Spines";
            spell.projectileSpeed = 1400.0f;   // ZyraQ MissileSpec.mSpeed
            spell.radius = 140.0f;             // wiki effect radius = bin CastRadius
            spell.range = 800.0f;              // wiki range = bin CastRange
            spell.spellKey = SpellSlot::Q;
            spell.spellName = "ZyraQ";
            spell.spellType = SpellType::Circle;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zyra";
            spell.dangerlevel = 3;
            spell.missileName = "ZyraE";
            spell.name = "Grasping Roots";
            spell.projectileSpeed = 1150.0f;   // ZyraE MissileSpec.mSpeed
            spell.radius = 35.0f;              // mLineWidth (half-width)
            spell.range = 1150.0f;             // bin CastRange
            spell.spellKey = SpellSlot::E;
            spell.spellName = "ZyraE";
            spell.spellType = SpellType::Line;
            spell.ccType = CCType::Root;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zyra";
            spell.dangerlevel = 4;
            spell.name = "Stranglethorns";
            spell.radius = 500.0f;             // wiki effect radius = bin CastRadius
            spell.range = 700.0f;              // wiki range = bin CastRange
            spell.spellDelay = 2000.0f;        // wiki 2s delay
            spell.spellKey = SpellSlot::R;
            spell.spellName = "ZyraR";
            spell.spellType = SpellType::Circle;
            spell.ccType = CCType::KnockUp;
            spell.fixedRange = true;
            Spells.push_back(spell);
        }

    }
};

} // namespace EzEvade
