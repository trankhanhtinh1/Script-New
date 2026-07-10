#pragma once
#include "SpellData.h"

#include <cmath>

namespace Plugins::KuroEvade::InternalDatabase { class SpellDatabase {
public:
    static std::vector<SpellData> Spells;

    static void Initialize() {
        // ===
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
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "summonersnowball";
            spell.extraSpellNames = { "summonerporothrow" };
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI A
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 2;
            spell.name = "The Darkin Blade (Q1)";
            spell.range = 650.0f;
            spell.radius = 120.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AatroxQ1";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Trúng rìa sẽ bị hất tung
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 2;
            spell.name = "The Darkin Blade (Q2)";
            spell.range = 525.0f;
            spell.angle = 60.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AatroxQ2";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 3;
            spell.name = "The Darkin Blade (Q3)";
            spell.range = 450.0f;
            spell.radius = 200.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AatroxQ3";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 2;
            spell.missileName = "AatroxW";
            spell.name = "Infernal Chains";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 80.0f;
            spell.range = 825.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "AatroxW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Làm chậm trước, xích kéo lại tính sau
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 2;
            spell.missileName = "AhriOrbMissile";
            spell.name = "Orb of Deception";
            spell.projectileSpeed = 1750.0f;
            spell.radius = 100.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AhriOrbofDeception";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 2;
            spell.missileName = "AhriOrbReturn";
            spell.name = "Orb of Deception (Return)";
            spell.projectileSpeed = 915.0f;
            spell.radius = 100.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AhriOrbofDeception2";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 3;
            spell.missileName = "AhriSeduceMissile";
            spell.name = "Charm";
            spell.projectileSpeed = 1550.0f;
            spell.radius = 60.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "AhriSeduce";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Charm; // Mê hoặc nguy hiểm cường độ cao
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Akali";
            spell.dangerlevel = 1;
            spell.name = "Five Point Strike";
            spell.range = 550.0f;
            spell.angle = 45.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AkaliQ";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Slow; // Làm chậm ở rìa chiêu
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Akali";
            spell.dangerlevel = 2;
            spell.missileName = "AkaliEMis";
            spell.name = "Shuriken Flip";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 70.0f;
            spell.range = 825.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "AkaliE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Akshan";
            spell.dangerlevel = 2;
            spell.missileName = "AkshanQMissile";
            spell.name = "Avengerang";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 90.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AkshanQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Alistar";
            spell.defaultOff = true;
            spell.dangerlevel = 3;
            spell.name = "Pulverize";
            spell.radius = 365.0f;
            spell.range = 365.0f;
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "Pulverize";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp; // Hất tung tại chỗ cực nhanh
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Amumu";
            spell.dangerlevel = 3;
            spell.missileName = "SadMummyBandageToss";
            spell.name = "Bandage Toss";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 80.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "BandageToss";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Choáng tầm xa bám theo mục tiêu
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Amumu";
            spell.dangerlevel = 4;
            spell.name = "Curse of the Sad Mummy";
            spell.radius = 560.0f;
            spell.range = 560.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "CurseoftheSadMummy";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun; // Chiêu cuối làm choáng diện rộng
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Anivia";
            spell.dangerlevel = 3;
            spell.missileName = "FlashFrostSpell";
            spell.name = "Flash Frost";
            spell.projectileSpeed = 850.0f;
            spell.radius = 110.0f;
            spell.range = 1250.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "FlashFrostSpell";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Choáng khi kích nổ quả cầu băng
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Annie";
            spell.dangerlevel = 2;
            spell.angle = 25.0f;
            spell.name = "Incinerate";
            spell.range = 625.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "Incinerate";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Stun; // Có thể gây choáng nếu có nội tại Hỏa Cuồng
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Annie";
            spell.dangerlevel = 4;
            spell.name = "Summon: Tibbers";
            spell.radius = 290.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "InfernalGuardian";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun; // Gây choáng tức thì diện rộng kèm nội tại
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Aphelios";
            spell.dangerlevel = 2;
            spell.missileName = "ApheliosCalibrumQMis";
            spell.name = "Moonshot";
            spell.projectileSpeed = 1850.0f;
            spell.radius = 60.0f;
            spell.range = 1450.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ApheliosCalibrumQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aphelios";
            spell.dangerlevel = 4;
            spell.missileName = "ApheliosRMissile";
            spell.name = "Moonlight Vigil";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 125.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "ApheliosR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Chiêu cuối có thể gây chậm hoặc trói tùy loại súng (ưu tiên đặt Slow cơ bản)
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Ashe";
            spell.dangerlevel = 4;
            spell.missileName = "EnchantedCrystalArrow";
            spell.name = "Enchanted Crystal Arrow";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 130.0f;
            spell.range = 25000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "EnchantedCrystalArrow";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Choáng Đại Băng Tiễn cực kỳ quan trọng đối với Evade
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "AurelionSol";
            spell.dangerlevel = 4;
            spell.name = "Falling Star / The Skies Descend";
            spell.radius = 350.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 1250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "AurelionSolR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun; // Gây choáng diện rộng (hoặc hất tung nếu nâng cấp nâng cao)
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Azir";
            spell.dangerlevel = 4;
            spell.missileName = "AzirSoldierRMissile";
            spell.name = "Emperor's Divide";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 500.0f;
            spell.range = 700.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "AzirR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Đẩy lùi/Hất tung kẻ địch
            Spells.push_back(spell);
        }
        // ===
// ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI B
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Bard";
            spell.dangerlevel = 2;
            spell.missileName = "BardQMissile";
            spell.name = "Cosmic Binding";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 60.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "BardQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Làm chậm cơ bản, trúng tường/mục tiêu thứ 2 thành Snare
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Bard";
            spell.dangerlevel = 4;
            spell.missileName = "BardRMissile";
            spell.name = "Tempered Fate";
            spell.projectileSpeed = 2100.0f; // Tốc độ bay theo khoảng cách, tạm để trung bình cao
            spell.radius = 350.0f;
            spell.range = 3400.0f;
            spell.spellDelay = 500; // Tăng dần theo khoảng cách
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "BardR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Suppression; // Đóng băng trạng thái (Stasis giống Đồng Hồ Cát)
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "BelVeth";
            spell.dangerlevel = 3;
            spell.name = "Above and Below (W)";
            spell.radius = 100.0f;
            spell.range = 660.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "BelvethW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Hất tung dạng đường thẳng thẳng góc hẹp
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Blitzcrank";
            spell.dangerlevel = 4;
            spell.missileName = "RocketGrabMissile";
            spell.name = "Rocket Grab";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 70.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "RocketGrab";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Kéo mục tiêu về phía bản thân (coi như Khống chế mạnh)
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Brand";
            spell.dangerlevel = 2;
            spell.missileName = "BrandQMissile";
            spell.name = "Sear";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 60.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "BrandQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Gây choáng nếu mục tiêu đang bị bỏng nội tại
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Brand";
            spell.dangerlevel = 2;
            spell.name = "Pillar of Flame";
            spell.radius = 250.0f;
            spell.range = 900.0f;
            spell.spellDelay = 625;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "BrandW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Braum";
            spell.dangerlevel = 2;
            spell.missileName = "BraumQMissile";
            spell.name = "Winter's Bite";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 65.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "BraumQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Braum";
            spell.dangerlevel = 4;
            spell.missileName = "BraumRMissile";
            spell.name = "Glacial Fissure";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 115.0f;
            spell.range = 1250.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "BraumR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Hất tung diện rộng
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Briar";
            spell.dangerlevel = 3;
            spell.name = "Chilling Scream (E)";
            spell.range = 600.0f;
            spell.angle = 40.0f;
            spell.spellDelay = 1000; // Delay vận sức tối đa
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "BriarE";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::KnockUp; // Đẩy lùi/Hất tung, nếu va trúng tường gây Stun
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Briar";
            spell.dangerlevel = 4;
            spell.missileName = "BriarRMissile";
            spell.name = "Certain Death";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 120.0f;
            spell.range = 25000.0f; // Toàn bản đồ
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "BriarR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Chậm mục tiêu trúng trực tiếp và hoảng sợ xung quanh
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI C
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 2;
            spell.missileName = "CaitlynPiltoverPeacemakerMissile";
            spell.name = "Piltover Peacemaker";
            spell.projectileSpeed = 2200.0f;
            spell.radius = 90.0f; // Thu hẹp còn 60 sau khi xuyên thấu
            spell.range = 1300.0f;
            spell.spellDelay = 625;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "CaitlynPiltoverPeacemaker";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 2;
            spell.name = "Yordle Snap Trap";
            spell.radius = 75.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "CaitlynYordleSnapTrap";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare; // Trói chân kẻ địch đạp bẫy
            spell.hasTrap = true;
            spell.trapBaseName = "CaitlynTrap";
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 2;
            spell.missileName = "CaitlynEntrapmentMissile";
            spell.name = "90 Caliber Net";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 70.0f;
            spell.range = 800.0f;
            spell.spellDelay = 150;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "CaitlynEntrapment";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Camille";
            spell.dangerlevel = 2;
            spell.name = "Tactical Sweep (W)";
            spell.range = 650.0f;
            spell.angle = 45.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "CamilleW";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Camille";
            spell.dangerlevel = 3;
            spell.name = "Wall Dive (E2)";
            spell.projectileSpeed = 1050.0f;
            spell.radius = 110.0f;
            spell.range = 800.0f; // Nhân đôi nếu hướng về phía Champion
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "CamilleEDash2";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Làm choáng khi phóng trúng mục tiêu
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.dangerlevel = 1;
            spell.name = "Noxious Blast";
            spell.radius = 160.0f;
            spell.range = 850.0f;
            spell.spellDelay = 650;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "CassiopeiaQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.dangerlevel = 4;
            spell.name = "Petrifying Gaze";
            spell.range = 825.0f;
            spell.angle = 80.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "CassiopeiaR";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Stun; // Hóa đá (Stun) nếu quay mặt lại, Slow nếu quay lưng
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Chogath";
            spell.dangerlevel = 3;
            spell.name = "Rupture";
            spell.radius = 250.0f;
            spell.range = 950.0f;
            spell.spellDelay = 1200; // Chỉ thị vòng tròn đỏ hiển thị lâu trước khi hất tung
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "Rupture";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Chogath";
            spell.dangerlevel = 2;
            spell.name = "Feral Scream";
            spell.range = 650.0f;
            spell.angle = 60.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "FeralScream";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Silence; // Gây câm lặng diện rộng
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Corki";
            spell.dangerlevel = 1;
            spell.missileName = "PhosphorusBombMissile";
            spell.name = "Phosphorus Bomb";
            spell.projectileSpeed = 1000.0f;
            spell.radius = 250.0f;
            spell.range = 825.0f;
            spell.spellDelay = 300;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "PhosphorusBomb";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Corki";
            spell.dangerlevel = 2;
            spell.missileName = "MissileBarrageMissile";
            spell.name = "Missile Barrage (R)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 40.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 175;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "MissileBarrageMissile";
            spell.extraMissileNames = { "MissileBarrageMissile2" }; // Tên Tên lửa cực đại
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI D
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Darius";
            spell.dangerlevel = 3;
            spell.name = "Apprehend";
            spell.range = 535.0f;
            spell.angle = 50.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "DariusE";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::KnockUp; // Kéo/Hất văng lôi kéo mục tiêu
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Diana";
            spell.dangerlevel = 2;
            spell.missileName = "DianaQMissile";
            spell.name = "Crescent Strike";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 185.0f; // Bán kính vụ nổ vòng tròn cuối cung bay
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "DianaQ";
            spell.spellType = ZDSpellType::Arc; // Đạn đạo dạng hình lưỡi liềm đặc biệt
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "DrMundo";
            spell.dangerlevel = 1;
            spell.missileName = "DrMundoQMissile";
            spell.name = "Infected Bonesaw";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 60.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "DrMundoQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Draven";
            spell.dangerlevel = 2;
            spell.missileName = "DravenDoubleShotMissile";
            spell.name = "Stand Aside";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 130.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "DravenDoubleShot";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Đạt hất dạt sang một bên
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Draven";
            spell.dangerlevel = 3;
            spell.missileName = "DravenRCast";
            spell.name = "Whirling Death";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 160.0f;
            spell.range = 25000.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "DravenRCast";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===
// ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI E
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 2;
            spell.missileName = "EkkoQMis";
            spell.name = "Timewinder (Q1)";
            spell.projectileSpeed = 1650.0f;
            spell.radius = 60.0f;
            spell.range = 1075.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "EkkoQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 3;
            spell.name = "Parallel Convergence (W)";
            spell.radius = 375.0f;
            spell.range = 1600.0f;
            spell.spellDelay = 3750; // Delay từ lúc gọi đến lúc vùng hiển thị kích nổ
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "EkkoW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun; // Gây choáng nếu Ekko bước vào vùng này
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Elise";
            spell.dangerlevel = 3;
            spell.missileName = "EliseHumanE";
            spell.name = "Cocoon";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 55.0f;
            spell.range = 1075.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "EliseHumanE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Kén Nhện gây choáng rất quan trọng để né
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        // Bỏ trống hoặc cập nhật nếu có dữ liệu tướng cụ thể riêng biệt trong database của bạn
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Evelynn";
            spell.dangerlevel = 1;
            spell.missileName = "EvelynnQLineMis";
            spell.name = "Hate Spike (Q)";
            spell.projectileSpeed = 2400.0f;
            spell.radius = 60.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "EvelynnQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Evelynn";
            spell.dangerlevel = 4;
            spell.name = "Last Caress (R)";
            spell.range = 450.0f;
            spell.angle = 180.0f; // Quạt ngược góc rộng về phía sau
            spell.spellDelay = 350;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "EvelynnR";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 1;
            spell.missileName = "EzrealQ";
            spell.name = "Mystic Shot";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 60.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "EzrealMysticShot";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 2;
            spell.missileName = "EzrealW";
            spell.name = "Essence Flux";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 80.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "EzrealEssenceFlux";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions }; // Chỉ bám vào tướng/công trình
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 3;
            spell.missileName = "EzrealR";
            spell.name = "Trueshot Barrage";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 160.0f;
            spell.range = 25000.0f;
            spell.spellDelay = 1000; // Vận sức 1 giây
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "EzrealTrueshotBarrage";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI F
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Fiddlesticks";
            spell.dangerlevel = 2;
            spell.name = "Bountiful Harvest (E)";
            spell.radius = 80.0f; // Bán kính vạch cắt của lưỡi liềm
            spell.range = 850.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "FiddlesticksE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Silence; // Câm lặng nếu ở giữa vạch, Slow ở rìa ngoài
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Fiora";
            spell.dangerlevel = 2;
            spell.missileName = "FioraWMissile";
            spell.name = "Riposte (W)";
            spell.projectileSpeed = 3200.0f;
            spell.radius = 70.0f;
            spell.range = 800.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "FioraW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Làm chậm cơ bản, chuyển thành Stun nếu chặn được CC cứng
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Fizz";
            spell.dangerlevel = 4;
            spell.missileName = "FizzMarinerDoomMissile";
            spell.name = "Chum the Waters (R)";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 120.0f; // Bán kính tăng dần theo độ xa bay được
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "FizzR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Thả cá gây hất tung diện rộng tại điểm cuối
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI G
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Galio";
            spell.dangerlevel = 2;
            spell.missileName = "GalioQMissileIn";
            spell.name = "Winds of War (Q)";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 120.0f;
            spell.range = 825.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "GalioQ";
            spell.spellType = ZDSpellType::Circular; // Hai luồng gió hội tụ tạo thành một vùng xoáy tròn
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Galio";
            spell.dangerlevel = 3;
            spell.name = "Justice Punch (E)";
            spell.projectileSpeed = 2300.0f;
            spell.radius = 160.0f;
            spell.range = 650.0f;
            spell.spellDelay = 400; // Có bước lùi lại lấy đà trước khi lao tới
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "GalioE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 1;
            spell.missileName = "GnarQMissile";
            spell.name = "Boomerang Throw (Mini Q)";
            spell.projectileSpeed = 2500.0f;
            spell.radius = 60.0f;
            spell.range = 1125.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "GnarQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 2;
            spell.missileName = "GnarBigQMissile";
            spell.name = "Boulder Toss (Mega Q)";
            spell.projectileSpeed = 2100.0f;
            spell.radius = 90.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "GnarBigQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 3;
            spell.name = "Wallop (Mega W)";
            spell.radius = 100.0f;
            spell.range = 600.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "GnarBigW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Đập choáng diện rộng dạng đường thẳng hẹp
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 2;
            spell.missileName = "GragasQMissile";
            spell.name = "Barrel Roll";
            spell.projectileSpeed = 1000.0f;
            spell.radius = 250.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "GragasQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 3;
            spell.name = "Body Slam";
            spell.projectileSpeed = 900.0f;
            spell.radius = 180.0f;
            spell.range = 600.0f;
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "GragasE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Lấy thịt đè người hất tung/bị khựng lại khi trúng mục tiêu
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 4;
            spell.missileName = "GragasRMissile";
            spell.name = "Explosive Cask";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 400.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "GragasR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp; // Thùng rượu nổ phá vỡ đội hình hất văng kẻ địch ra xa tâm nổ
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 2;
            spell.missileName = "GravesQLineMissile";
            spell.name = "End of the Line (Q)";
            spell.projectileSpeed = 3000.0f;
            spell.radius = 40.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "GravesQLineSpell";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 2;
            spell.missileName = "GravesClusterShotAttack";
            spell.name = "Smoke Screen (W)";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 250.0f;
            spell.range = 950.0f;
            spell.spellDelay = 150;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "GravesSmokeGrenade";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow; // Bom khói giảm tầm nhìn và làm chậm
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 3;
            spell.missileName = "GravesUltimateMissile";
            spell.name = "Collateral Damage (R)";
            spell.projectileSpeed = 2100.0f;
            spell.radius = 100.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "GravesUltimateShot";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Gwen";
            spell.dangerlevel = 3;
            spell.missileName = "GwenRMissile";
            spell.name = "Needlework (R)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 120.0f;
            spell.range = 1350.0f;
            spell.spellDelay = 250; // Delay mỗi lần tái kích hoạt phóng kim khâu
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "GwenR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI H
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Hecarim";
            spell.dangerlevel = 4;
            spell.missileName = "HecarimUltMissile";
            spell.name = "Onslaught of Shadows (R)";
            spell.projectileSpeed = 1100.0f;
            spell.radius = 240.0f; // Bán kính hàng bóng ma càn quét
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "HecarimUlt";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Charm; // Hoảng sợ kẻ địch tại điểm cuối
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Heimerdinger";
            spell.dangerlevel = 2;
            spell.missileName = "HeimerdingerWMaxAttack";
            spell.name = "Hextech Micro-Rockets (W)";
            spell.projectileSpeed = 2850.0f;
            spell.radius = 40.0f;
            spell.range = 1325.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "HeimerdingerW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Heimerdinger";
            spell.dangerlevel = 3;
            spell.missileName = "HeimerdingerEExplosion";
            spell.name = "CH-2 Electron Storm Grenade (E)";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 135.0f;
            spell.range = 975.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "HeimerdingerE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun; // Choáng ngay tâm nổ quả lựu đạn, làm chậm vùng ngoài
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 2;
            spell.missileName = "HweiQQMissile";
            spell.name = "Subject: Disaster - Devastating Fire (QQ)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 80.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "HweiQQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 2;
            spell.name = "Subject: Disaster - Molten Fissure (QW)";
            spell.radius = 100.0f; // Bán kính đường vệt lửa dội xuống tầm xa
            spell.range = 2000.0f;
            spell.spellDelay = 850;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "HweiQW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 3;
            spell.missileName = "HweiEQMissile";
            spell.name = "Subject: Torment - Grim Visage (EQ)";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 60.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "HweiEQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Charm; // Hoảng sợ mặt nạ ma ép lùi mục tiêu về sau
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 3;
            spell.missileName = "HweiEWMissile";
            spell.name = "Subject: Torment - Gaze of the Abyss (EW)";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 180.0f; // Vùng mắt trói
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "HweiEW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare; // Con mắt khóa trói mục tiêu đầu tiên bước vào
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 4;
            spell.missileName = "HweiRMissile";
            spell.name = "Spiraling Despair (R)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 90.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "HweiR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Dính vòng xoáy tuyệt vọng tăng tiến làm chậm rồi nổ tung
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        // ===
// ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI I
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Illaoi";
            spell.dangerlevel = 2;
            spell.name = "Tentacle Smash (Q)";
            spell.radius = 100.0f;
            spell.range = 850.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "IllaoiQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Illaoi";
            spell.dangerlevel = 3;
            spell.missileName = "IllaoiEMis";
            spell.name = "Test of Spirit (E)";
            spell.projectileSpeed = 1900.0f;
            spell.radius = 60.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "IllaoiE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Kéo linh hồn, nếu phá xích hoặc chết linh hồn sẽ bị Slow/gọi xúc tu
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.dangerlevel = 2;
            spell.missileName = "IreliaEMissile";
            spell.name = "Flawless Duet (E)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 80.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250; // Delay từ lúc thả cây kiếm thứ 2
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "IreliaE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Choáng đường thẳng nối giữa 2 cây kiếm
            spell.isSpecial = true; // Cần thuật toán tracking vị trí E1 và E2 riêng biệt
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.dangerlevel = 3;
            spell.missileName = "IreliaR";
            spell.name = "Vanguard's Edge (R)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 160.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "IreliaR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Làm chậm cực mạnh khi đi xuyên qua vành kiếm nổ
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Ivern";
            spell.dangerlevel = 3;
            spell.missileName = "IvernQ";
            spell.name = "Rootcaller (Q)";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 80.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "IvernQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare; // Trói chân để đồng đội bay vào kích hoạt đòn đánh
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI J
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Janna";
            spell.dangerlevel = 2;
            spell.missileName = "HowlingGaleSpell";
            spell.name = "Howling Gale (Q)";
            spell.projectileSpeed = 660.0f; // Tốc độ tăng tiến dần theo thời gian tích gió (660 - 1584)
            spell.radius = 120.0f;
            spell.range = 1700.0f; // Tầm xa tăng theo thời gian tụ gió
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "HowlingGale";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Hất tung cực kỳ khó chịu của Janna
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "JarvanIV";
            spell.dangerlevel = 2;
            spell.name = "Dragon Strike (Q)";
            spell.radius = 70.0f;
            spell.range = 770.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "JarvanIVDragonStrike";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Trở thành hất tung nếu kéo trúng combo Hoàng Kỳ E
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Jayce";
            spell.dangerlevel = 2;
            spell.missileName = "JayceShockBlastMis";
            spell.name = "Shock Blast (Q Thường)";
            spell.projectileSpeed = 1450.0f;
            spell.radius = 70.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "JayceShockBlast";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jayce";
            spell.dangerlevel = 3;
            spell.missileName = "JayceShockBlastWallMis";
            spell.name = "Shock Blast (Q Gia Tốc Cổng E)";
            spell.projectileSpeed = 2350.0f; // Tốc độ tăng vượt bậc
            spell.radius = 100.0f; // Bán kính vụ nổ lan rộng hơn
            spell.range = 1600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "JayceShockBlastCharged";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.dangerlevel = 2;
            spell.missileName = "JhinWMissile";
            spell.name = "Deadly Flourish (W)";
            spell.projectileSpeed = 5000.0f; // Gần như tức thì
            spell.radius = 45.0f;
            spell.range = 2500.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "JhinW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare; // Trói chân mục tiêu bị đánh dấu
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.dangerlevel = 3;
            spell.missileName = "JhinRShotMis";
            spell.name = "Curtain Call (R)";
            spell.projectileSpeed = 5000.0f;
            spell.radius = 80.0f;
            spell.range = 3500.0f;
            spell.spellDelay = 250; // Delay bắn phát súng
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "JhinRShot";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Phát súng thứ 4 làm chậm 80%
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.dangerlevel = 2;
            spell.missileName = "JinxWMissile";
            spell.name = "Zap! (W)";
            spell.projectileSpeed = 3300.0f;
            spell.radius = 60.0f;
            spell.range = 1500.0f;
            spell.spellDelay = 600; // Giảm dần theo Tốc độ đánh của Jinx
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "JinxW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.dangerlevel = 3;
            spell.missileName = "JinxR";
            spell.name = "Super Mega Death Rocket! (R)";
            spell.projectileSpeed = 1700.0f; // Gia tốc lên 2200 sau khi bay được quãng ngắn
            spell.radius = 140.0f;
            spell.range = 25000.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "JinxR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI K
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Kaisa";
            spell.dangerlevel = 2;
            spell.missileName = "KaisaWMissile";
            spell.name = "Void Seeker (W)";
            spell.projectileSpeed = 1750.0f;
            spell.radius = 100.0f;
            spell.range = 3000.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "KaisaW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Karma";
            spell.dangerlevel = 2;
            spell.missileName = "KarmaQMissile";
            spell.name = "Inner Flame (Q)";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 60.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KarmaQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Karma";
            spell.dangerlevel = 3;
            spell.missileName = "KarmaQMissileMantra";
            spell.name = "Inner Flame (Mantra Q)";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 80.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KarmaQHeavy";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Tạo thêm vùng nổ chậm làm chậm sâu hơn
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Karthus";
            spell.dangerlevel = 1;
            spell.name = "Lay Waste (Q)";
            spell.radius = 160.0f;
            spell.range = 875.0f;
            spell.spellDelay = 625;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KarthusLayWaste";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Kassadin";
            spell.dangerlevel = 2;
            spell.name = "Force Pulse (E)";
            spell.range = 600.0f;
            spell.angle = 80.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ForcePulse";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Kayle";
            spell.dangerlevel = 1;
            spell.missileName = "KayleQMissile";
            spell.name = "Radiant Blast (Q)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 75.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KayleQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Kayn";
            spell.dangerlevel = 2;
            spell.name = "Blade's Reach (W - Thường)";
            spell.radius = 90.0f;
            spell.range = 700.0f;
            spell.spellDelay = 550;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "KaynW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kayn";
            spell.dangerlevel = 3;
            spell.name = "Blade's Reach (W - Darkin)";
            spell.radius = 90.0f;
            spell.range = 700.0f;
            spell.spellDelay = 550;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "KaynAssW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Dạng Darkin (Đỏ) hất tung mục tiêu cực mạnh
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Kennen";
            spell.dangerlevel = 1;
            spell.missileName = "KennenShurikenHurlMissile";
            spell.name = "Thundering Shuriken (Q)";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 50.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 175;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KennenShurikenHurlMissile";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None; // Tích đủ 3 dấu ấn mới nổ Stun
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Khazix";
            spell.dangerlevel = 1;
            spell.missileName = "KhazixWMissile";
            spell.name = "Void Spike (W)";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 70.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "KhazixW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Kled";
            spell.dangerlevel = 2;
            spell.missileName = "KledQMissile";
            spell.name = "Beartrap on a Rope (Q)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 45.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KledQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Dính xích bị chậm, sau đó bị kéo lùi dính vết thương sâu
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.dangerlevel = 1;
            spell.missileName = "KogMawQVMissile";
            spell.name = "Caustic Spittle (Q)";
            spell.projectileSpeed = 1650.0f;
            spell.radius = 70.0f;
            spell.range = 1175.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KogMawQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.dangerlevel = 2;
            spell.missileName = "KogMawVoidOozeMissile";
            spell.name = "Void Ooze (E)";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 120.0f;
            spell.range = 1360.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "KogMawVoidOoze";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Thảm bùn làm chậm liên tục khi đứng trên đó
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.dangerlevel = 2;
            spell.name = "Living Artillery (R)";
            spell.radius = 240.0f;
            spell.range = 1300.0f; // Lên tới 1800 dựa theo cấp chiêu cuối
            spell.spellDelay = 600; // Pháo sinh học dội từ trên trời xuống
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "KogMawLivingArtillery";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.dangerlevel = 2;
            spell.name = "Ntofo Strikes (Q3)";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 100.0f;
            spell.range = 475.0f;
            spell.spellDelay = 400; // Tốc độ ra đòn tỉ lệ nghịch với lượng máu cộng thêm
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KSanteQ3";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Q3 tụ đủ stack phóng sóng năng lượng kéo hất tung mục tiêu về sau
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI L
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 2;
            spell.missileName = "LeblancEMissile";
            spell.name = "Ethereal Chains (E)";
            spell.projectileSpeed = 1750.0f;
            spell.radius = 55.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LeblancE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare; // Trói chân sau khi giữ xích đủ thời gian
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 3;
            spell.missileName = "LeblancREMissile";
            spell.name = "Ethereal Chains (Mô Phỏng RE)";
            spell.projectileSpeed = 1750.0f;
            spell.radius = 55.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "LeblancRE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "LeeSin";
            spell.dangerlevel = 2;
            spell.missileName = "BlindMonkQOne";
            spell.name = "Sonic Wave (Q1)";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 60.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "BlindMonkQOne";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Leona";
            spell.dangerlevel = 2;
            spell.missileName = "LeonaZenithBladeMissile";
            spell.name = "Zenith Blade (E)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 70.0f;
            spell.range = 900.0f;
            spell.spellDelay = 200;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LeonaZenithBlade";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare; // Trói nhẹ tích tắc mục tiêu cuối cùng dính chiêu để Leona bay vào
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leona";
            spell.dangerlevel = 4;
            spell.name = "Solar Flare (R)";
            spell.radius = 320.0f; // Toàn vùng ảnh hưởng, trung tâm choáng rìa làm chậm
            spell.range = 1200.0f;
            spell.spellDelay = 625;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "LeonaSolarFlare";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun; // Chiêu cuối nổ choáng diện rộng
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Lillia";
            spell.dangerlevel = 2;
            spell.missileName = "LilliaE-Missile";
            spell.name = "Swirlseed (E)";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 60.0f;
            spell.range = 25000.0f; // Hạt lăn vô tận cho tới khi chạm địa hình hoặc mục tiêu
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LilliaE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Lissandra";
            spell.dangerlevel = 1;
            spell.missileName = "LissandraQShards";
            spell.name = "Ice Shard (Q)";
            spell.projectileSpeed = 2200.0f;
            spell.radius = 75.0f;
            spell.range = 725.0f; // Tầm bay kéo dài thêm một chút sau khi xuyên mục tiêu đầu
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LissandraQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lissandra";
            spell.dangerlevel = 2;
            spell.missileName = "LissandraEMissile";
            spell.name = "Glacial Path (E)";
            spell.projectileSpeed = 850.0f;
            spell.radius = 125.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LissandraE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===
	// ===
        {
            SpellData spell;
            spell.charName = "Locke";
            spell.dangerlevel = 2;
            spell.missileName = "LockeQNailMissile"; // Tên missile đinh hồn bay ra
            spell.name = "Ritual Nails (Q)";
            spell.projectileSpeed = 1800.0f; // Tốc độ phóng đinh tương đối nhanh
            spell.radius = 60.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LockeQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Làm chậm 25% ở nhịp đầu, tăng mạnh lên 60% nếu dính 2-3 stack đinh
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Locke";
            spell.dangerlevel = 5; // Độ nguy hiểm tối đa do cơ chế Execute (kết liễu) lập tức dưới ngưỡng máu
            spell.missileName = "LockeRArtifact";
            spell.name = "Purgatory (R)";
            spell.projectileSpeed = 1500.0f; // Tốc độ quăng cổ vật tế đàn ra vị trí chỉ định
            spell.radius = 350.0f; // Vùng tròn ảnh hưởng lan rộng của xích đinh
            spell.range = 1000.0f;
            spell.spellDelay = 750; // 0.5 giây bay + 0.25 giây trễ kích hoạt dựng tế đàn bẫy xích
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "LockeR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow; // Làm chậm cực mạnh đến 99% (giảm dần) giam chân trong bẫy
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Lucian";
            spell.dangerlevel = 2;
            spell.name = "Piercing Light (Q)";
            spell.radius = 65.0f;
            spell.range = 500.0f; // Khóa mục tiêu ở 500 nhưng tia đạn xuyên thấu đạt tới 900
            spell.spellDelay = 350; // Tốc độ thi triển giảm theo cấp độ tướng
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LucianQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.isSpecial = true; // Cần tính toán hướng bắn dựa trên mục tiêu chỉ định (Targeted-to-Line)
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lucian";
            spell.dangerlevel = 1;
            spell.missileName = "LucianWMissile";
            spell.name = "Ardent Blaze (W)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 55.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "LucianW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Lulu";
            spell.dangerlevel = 1;
            spell.missileName = "LuluQMissile";
            spell.name = "Glitterlance (Q)";
            spell.projectileSpeed = 1450.0f;
            spell.radius = 60.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LuluQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Làm chậm 80% cực mạnh giảm dần
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.dangerlevel = 3;
            spell.missileName = "LuxLightBindingDummy";
            spell.name = "Light Binding (Q)";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 70.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LuxLightBinding";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare; // Trói tối đa 2 mục tiêu xuyên qua lính
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.dangerlevel = 2;
            spell.missileName = "LuxLightStrikeKugel";
            spell.name = "Lucent Singularity (E)";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 310.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LuxLightStrikeKugel";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow; // Vùng làm chậm trước khi kích nổ tái kích hoạt
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.dangerlevel = 4;
            spell.name = "Final Spark (R)";
            spell.radius = 100.0f;
            spell.range = 3400.0f;
            spell.spellDelay = 1000; // Vận sức tia laze dọc bản đồ trong 1 giây
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "LuxMaliceInhabitant";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===
// ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI M
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Malphite";
            spell.dangerlevel = 5; // Độ nguy hiểm tối đa vì đây là chiêu mở giao tranh cực mạnh
            spell.missileName = "MalphiteR";
            spell.name = "Unstoppable Force (R)";
            spell.projectileSpeed = 1835.0f; // Tốc độ bay của Malphite khi húc vào
            spell.radius = 270.0f; // CDragon latest: UFSlash castRadius = 270.0
            spell.range = 1000.0f;
            spell.spellDelay = 0; // Kích hoạt gần như tức thì, phụ thuộc vào khoảng cách bay
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "MalphiteR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp; // Hất tung không thể cản phá
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Malzahar";
            spell.dangerlevel = 2;
            spell.name = "Call of the Void (Q)";
            spell.radius = 85.0f; // Độ rộng của hai cổng không gian nổ song song
            spell.range = 900.0f;
            spell.spellDelay = 650; // Trễ từ lúc đặt cổng đến lúc nổ tia năng lượng
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "MalzaharQ";
            spell.spellType = ZDSpellType::Line; // Xử lý toán học như một đường ngang vuông góc hướng đứng
            spell.ccType = CCType::Silence; // Câm lặng diện rộng
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Maokai";
            spell.dangerlevel = 2;
            spell.missileName = "MaokaiQMissile";
            spell.name = "Bramble Smash (Q)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 110.0f;
            spell.range = 600.0f;
            spell.spellDelay = 375;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "MaokaiQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockBack; // Đẩy lùi kẻ địch ở gần và làm chậm kẻ địch ở xa
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Maokai";
            spell.dangerlevel = 4;
            spell.name = "Nature's Grasp (R)";
            spell.projectileSpeed = 500.0f; // Bắt đầu chậm (50f) tăng tiến dần lên 850f theo khoảng cách bay
            spell.radius = 240.0f; // Bán kính mỗi nhánh rễ cây trong bức tường rễ
            spell.range = 3000.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "MaokaiR";
            spell.spellType = ZDSpellType::Line; // Xử lý hệ thống di chuyển theo cụm song song hướng thẳng
            spell.ccType = CCType::Snare; // Trói chân thời gian tăng dần theo quãng đường rễ cây di chuyển
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Milio";
            spell.dangerlevel = 2;
            spell.missileName = "MilioQMissile";
            spell.name = "Ultra Mega Firekick (Q)";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 60.0f; // Bán kính bóng bay ban đầu, khi nổ vùng rộng ra 200f
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "MilioQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockBack; // Đẩy lùi mục tiêu đầu tiên và làm chậm vùng nổ phía sau
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Morgana";
            spell.dangerlevel = 4; // Nguy hiểm cao do thời gian trói chân kỷ lục (tối đa 3 giây)
            spell.missileName = "DarkBindingMissile";
            spell.name = "Dark Binding (Q)";
            spell.projectileSpeed = 1200.0f; // Bay tương đối chậm, dễ né nếu ở tầm xa
            spell.radius = 70.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "DarkBindingMissile";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // Mudinho (Dr. Mundo) — REMOVED: Duplicate of // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI N
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Nami";
            spell.dangerlevel = 3;
            spell.name = "Aqua Prison (Q)";
            spell.radius = 162.5f;
            spell.range = 875.0f;
            spell.spellDelay = 950; // Trễ cố định (0.95 giây bóng nước rơi xuống)
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NamiQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp; // Bong bóng giam cầm (thực tế tính là hất tung/choáng treo)
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nami";
            spell.dangerlevel = 4;
            spell.missileName = "NamiRMissile";
            spell.name = "Tidal Wave (R)";
            spell.projectileSpeed = 850.0f; // Sóng thần bay chậm nhưng độ phủ cực lớn
            spell.radius = 375.0f; // Nửa độ rộng của ngọn sóng
            spell.range = 2750.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "NamiR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Hất nhẹ và làm chậm sâu dựa trên khoảng cách di chuyển
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Nautilus";
            spell.dangerlevel = 4;
            spell.missileName = "NautilusAnchorDragMissile";
            spell.name = "Dredge Line (Q)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 90.0f; // Bản kích mỏ neo khá lớn, dễ va chạm địa hình
            spell.range = 1150.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NautilusAnchorDrag";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Kéo mục tiêu và Nautilus lại gần nhau kèm choáng nhẹ
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::Terrain }; // Va chạm tướng hoặc tường
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Neeko";
            spell.dangerlevel = 2;
            spell.missileName = "NeekoQMissile";
            spell.name = "Blooming Burst (Q)";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 225.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NeekoQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None; // Nổ 3 lần nếu hạ gục mục tiêu hoặc trúng tướng
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Neeko";
            spell.dangerlevel = 3;
            spell.missileName = "NeekoEMissile";
            spell.name = "Tangle-Barbs (E)";
            spell.projectileSpeed = 1300.0f; // Tăng lên 1400 sau khi đi xuyên qua lính
            spell.radius = 70.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "NeekoE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare; // Trói chân lâu hơn và nhanh hơn nếu xuyên qua ít nhất 1 mục tiêu
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Nidalee";
            spell.dangerlevel = 3; // Sát thương cực cao nếu trúng từ xa
            spell.missileName = "JavelinToss";
            spell.name = "Javelin Toss (Q Người)";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 40.0f; // Hitbox ngọn giáo rất nhỏ
            spell.range = 1500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NidaleeJavelInToss";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Nilah";
            spell.dangerlevel = 1;
            spell.name = "Formless Blade (Q)";
            spell.radius = 75.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NilahQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Nocturne";
            spell.dangerlevel = 2;
            spell.missileName = "NocturneDuskbringerAura";
            spell.name = "Duskbringer (Q)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 60.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NocturneDuskbringer";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None; // Chỉ tạo vệt bóng tối gia tăng tốc độ di chuyển
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI O
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Olaf";
            spell.dangerlevel = 2;
            spell.missileName = "OlafAxeThrowCast";
            spell.name = "Undertow (Q)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 90.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "OlafAxeThrowCast";
            spell.spellType = ZDSpellType::Line; // Thực chất ném chỉ định tọa độ điểm rơi trên đoạn thẳng
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Orianna";
            spell.dangerlevel = 2;
            spell.missileName = "TheBall"; // Quả cầu nội tại di chuyển
            spell.name = "Command: Attack (Q)";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 80.0f; // Bán kính trúng chiêu trên đường bay
            spell.range = 825.0f;
            spell.spellDelay = 0; // Bay ngay lập tức từ vị trí hiện tại của khối cầu
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "OrianaIzuna";
            spell.spellType = ZDSpellType::Circular; // Bay đến đích và nằm cố định ở đó
            spell.ccType = CCType::None;
            spell.isSpecial = true; // Logic cần update điểm StartPos bằng tọa độ thực tế của quả cầu (không phải vị trí Orianna)
            Spells.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI P
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.charName = "Pantheon";
            spell.dangerlevel = 2;
            spell.missileName = "PantheonQMissile";
            spell.name = "Comet Spear (Q Phóng)";
            spell.projectileSpeed = 2700.0f; // Phóng giáo đi rất nhanh
            spell.radius = 60.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 250; // Delay vận sức nhấp thả chiêu
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "PantheonQCast";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Làm chậm nếu trúng khi Pantheon cường hóa nội tại chiến ý
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Poppy";
            spell.dangerlevel = 2;
            spell.name = "Hammer Shock (Q)";
            spell.radius = 100.0f;
            spell.range = 430.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "PoppyQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Làm chậm vùng đất nứt 2 nhịp nổ
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Poppy";
            spell.dangerlevel = 4;
            spell.missileName = "PoppyRMissile";
            spell.name = "Keeper's Verdict (R Vận Sức)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 100.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 350;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "PoppyRSpell";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp; // Đập búa hất văng mục tiêu bay thẳng về tế đàn chính của họ
            Spells.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.charName = "Pyke";
            spell.dangerlevel = 3;
            spell.missileName = "PykeQMissile";
            spell.name = "Bone Skewer (Q Kéo)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 70.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 200; // Tùy thuộc thời gian giữ phím gồng xích
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "PykeQCast";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockBack; // Đâm/Kéo mục tiêu một khoảng cố định ra sau lưng Pyke
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Pyke";
            spell.dangerlevel = 3;
            spell.name = "Phantom Undertow (E)";
            spell.radius = 110.0f;
            spell.range = 550.0f; // Khoảng cách lướt của Pyke, bóng ma chạy theo sau có tầm xa linh hoạt
            spell.spellDelay = 1000; // Thời gian bóng ma trễ trước khi bay về phía Pyke để gây choáng
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "PykeE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.isSpecial = true; // Yêu cầu tracking vị trí phân thân (Ghost) lướt theo bóng chính
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Pyke";
            spell.dangerlevel = 5; // Chiêu cuối hồi lại ngay lập tức và chia tiền, mức cảnh báo cao nhất
            spell.name = "Death From Below (R)";
            spell.radius = 160.0f; // Bán kính vùng tâm chữ X kết liễu
            spell.range = 750.0f;
            spell.spellDelay = 750; // Trễ hoạt ảnh từ lúc lặn xuống đến lúc chém nổ chữ X
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "PykeR";
            spell.spellType = ZDSpellType::Circular; // Được vẽ logic theo vùng đa giác hoặc hình tròn bao quanh tâm X
            spell.ccType = CCType::None; // Nếu thấp máu hơn ngưỡng chết sẽ bị Execute bay màu lập tức
            Spells.push_back(spell);
        }
        // ===
// ===
        // ==========================================
        // CHỮ Q
        // ==========================================
        {
            SpellData spell;
            spell.charName = "Quinn";
            spell.dangerlevel = 2;
            spell.missileName = "QuinnQMissile";
            spell.name = "Blinding Assault (Q)";
            spell.projectileSpeed = 1550.0f;
            spell.radius = 60.0f;
            spell.range = 1025.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "QuinnQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Blind; // Cơ chế giảm tầm nhìn đặc biệt
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // CHỮ R
        // ==========================================
        {
            SpellData spell;
            spell.charName = "Rakan";
            spell.dangerlevel = 3;
            spell.name = "Grand Entrance (W)";
            spell.radius = 250.0f;
            spell.range = 600.0f;
            spell.spellDelay = 750; // Trễ nhảy + hất tung
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "RakanW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rell";
            spell.dangerlevel = 3;
            spell.name = "Shatterstrike (Q)";
            spell.radius = 90.0f;
            spell.range = 685.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "RellQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Choáng nhịp đâm vũ khí
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rengar";
            spell.dangerlevel = 3;
            spell.missileName = "RengarEFinal";
            spell.name = "Bola Strike (E)";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 70.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "RengarE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Hoặc Trói (Snare) nếu có 4 điểm Hung Tợn
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Riven";
            spell.dangerlevel = 4;
            spell.missileName = "RivenWindSlashMissile";
            spell.name = "Wind Slash (R2)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 125.0f; // Hình nón xòe rộng nhưng tính theo góc/bán kính missile đầu
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "RivenIzunaBlade";
            spell.spellType = ZDSpellType::Line; // Xử lý né dạng đường thẳng góc rộng
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rumble";
            spell.dangerlevel = 3;
            spell.missileName = "RumbleGrenadeMissile";
            spell.name = "Electro Harpoon (E)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 60.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "RumbleGrenade";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ryze";
            spell.dangerlevel = 2;
            spell.missileName = "RyzeQW";
            spell.name = "Overload (Q)";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 55.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "RyzeQ";
            spell.spellType = ZDSpellType::Line;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // CHỮ S
        // ==========================================
        {
            SpellData spell;
            spell.charName = "Samira";
            spell.dangerlevel = 1;
            spell.missileName = "SamiraQMissile";
            spell.name = "Flair (Q)";
            spell.projectileSpeed = 2600.0f; // Bay rất nhanh
            spell.radius = 60.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SamiraQ";
            spell.spellType = ZDSpellType::Line;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sejuani";
            spell.dangerlevel = 4;
            spell.missileName = "SejuaniRMissile";
            spell.name = "Glacial Prison (R)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 120.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "SejuaniR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions }; // Chỉ cản bởi tướng
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Senna";
            spell.dangerlevel = 3;
            spell.missileName = "SennaEMissile"; // Tên gói dữ liệu trói của W
            spell.name = "Last Embrace (W)";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 60.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "SennaW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.dangerlevel = 3;
            spell.missileName = "SeraphineEIsotopeMissile";
            spell.name = "Beat Drop (E)";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 70.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SeraphineE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Có thể nâng cấp thành Choáng hoặc Trói dựa trên nội tại
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.dangerlevel = 5;
            spell.missileName = "SeraphineRMissile";
            spell.name = "Encore (R)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 160.0f; // Bán kính siêu rộng
            spell.range = 2000.0f; // Tầm có thể kéo dài thêm khi trúng mục tiêu
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "SeraphineR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Charm;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Shaco";
            spell.dangerlevel = 2;
            spell.name = "Jack In The Box (W)";
            spell.radius = 175.0f;
            spell.range = 425.0f;
            spell.spellDelay = 2000; // Trễ tàng hình trước khi kích hoạt hoảng sợ
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "ShacoBox";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Fear;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Shen";
            spell.dangerlevel = 3;
            spell.name = "Shadow Dash (E)";
            spell.projectileSpeed = 1200.0f; // Tốc độ lướt của Shen
            spell.radius = 60.0f;
            spell.range = 600.0f;
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ShenE";
            spell.spellType = ZDSpellType::Line; // Xử lý vùng quét dạng Line khi thực hiện lướt
            spell.ccType = CCType::Taunt;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Shyvana";
            spell.dangerlevel = 3;
            spell.missileName = "ShyvanaTransformLeapNoData"; // Dạng rồng quăng E lửa lớn
            spell.name = "Flame Breath (E - Dragon Form)";
            spell.projectileSpeed = 1575.0f;
            spell.radius = 115.0f;
            spell.range = 975.0f;
            spell.spellDelay = 333;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ShyvanaFireballDragonZoneCheck";
            spell.spellType = ZDSpellType::Line;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sion";
            spell.dangerlevel = 2;
            spell.missileName = "SionEMissile";
            spell.name = "Roar of the Slayer (E)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 80.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SionE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sivir";
            spell.dangerlevel = 2;
            spell.missileName = "SivirQMissile";
            spell.name = "Boomerang Blade (Q)";
            spell.projectileSpeed = 1450.0f;
            spell.radius = 90.0f;
            spell.range = 1250.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SivirQ";
            spell.spellType = ZDSpellType::Line; // Cần tính toán cả đường Boomerang bay về
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Skarner";
            spell.dangerlevel = 3;
            spell.missileName = "SkarnerEIsotopeMissile";
            spell.name = "Ixtal's Impact (E)";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 70.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SkarnerE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sona";
            spell.dangerlevel = 5;
            spell.missileName = "SonaRMissile";
            spell.name = "Crescendo (R)";
            spell.projectileSpeed = 2400.0f;
            spell.radius = 140.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "SonaR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Soraka";
            spell.dangerlevel = 2;
            spell.missileName = "SorakaQMissile";
            spell.name = "Starcall (Q)";
            spell.radius = 235.0f;
            spell.range = 800.0f;
            spell.spellDelay = 300; // Tốc độ rơi phụ thuộc khoảng cách, trễ tối thiểu khoảng 300ms
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SorakaQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Soraka";
            spell.dangerlevel = 3;
            spell.name = "Equinox (E)";
            spell.radius = 250.0f;
            spell.range = 925.0f;
            spell.spellDelay = 1500; // Đứng yên 1.5 giây trong vùng sẽ bị trói
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SorakaE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Silence; // Im lặng tức thì khi dẫm vào vùng
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Swain";
            spell.dangerlevel = 3;
            spell.missileName = "SwainEMissile";
            spell.name = "Nevermove (E)";
            spell.projectileSpeed = 935.0f; // Bay đi chậm nhưng bay về nhanh hơn (~1400)
            spell.radius = 85.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SwainE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare; // Trói khi quay về dính mục tiêu
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sylas";
            spell.dangerlevel = 3;
            spell.missileName = "SylasEMissile";
            spell.name = "Abduct (E2)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 60.0f;
            spell.range = 800.0f;
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SylasE2";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Syndra";
            spell.dangerlevel = 2;
            spell.name = "Dark Sphere (Q)";
            spell.radius = 180.0f;
            spell.range = 800.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SyndraQ";
            spell.spellType = ZDSpellType::Circular;
            Spells.push_back(spell);
        }

        // ==========================================
        // CHỮ T
        // ==========================================
        {
            SpellData spell;
            spell.charName = "TahmKench";
            spell.dangerlevel = 2;
            spell.missileName = "TahmKenchQMissile";
            spell.name = "Tongue Lash (Q)";
            spell.projectileSpeed = 2800.0f; // Roi lưỡi cực nhanh
            spell.radius = 70.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "TahmKenchQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow; // Choáng nếu đủ 3 stack nội tại Khẩu Vị Độc Đáo
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Taliyah";
            spell.dangerlevel = 3;
            spell.name = "Seismic Shove (W)";
            spell.radius = 150.0f;
            spell.range = 900.0f;
            spell.spellDelay = 850;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "TaliyahW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp; // Đẩy hất theo hướng chỉ định
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Talon";
            spell.dangerlevel = 2;
            spell.missileName = "TalonWBlades";
            spell.name = "Rake (W)";
            spell.projectileSpeed = 1850.0f;
            spell.radius = 75.0f; // Đoạn đầu của dải quạt hình nón
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "TalonW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Teemo";
            spell.dangerlevel = 3;
            spell.missileName = "TeemoRShroom"; // Trường hợp Teemo quăng nấm bay xa nâng cấp cấp R
            spell.name = "Noxious Trap (R)";
            spell.projectileSpeed = 1000.0f;
            spell.radius = 120.0f; // Bán kính nổ kích hoạt khi dẫm phải
            spell.range = 400.0f; // Tăng dần 400/650/900 theo cấp chiêu cuối
            spell.spellDelay = 1000; // Trễ kích hoạt tàng hình của nấm sau khi đáp xuống
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "TeemoR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Thresh";
            spell.dangerlevel = 4;
            spell.missileName = "ThreshQMissile";
            spell.name = "Death Sentence (Q)";
            spell.projectileSpeed = 1900.0f;
            spell.radius = 70.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 500; // Hoạt ảnh quay xích mất 0.5s trước khi phóng
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ThreshQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun; // Kéo giật lùi mục tiêu
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Tristana";
            spell.dangerlevel = 2;
            spell.name = "Rocket Jump (W)";
            spell.projectileSpeed = 1100.0f;
            spell.radius = 270.0f; // Vùng đáp xuống gây sát thương và làm chậm
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "TristanaW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Trundle";
            spell.dangerlevel = 3;
            spell.name = "Pillar of Ice (E)";
            spell.radius = 130.0f; // Kích thước vật thể cột băng cản địa hình
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "TrundlePillar";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp; // Hất nhẹ khi mọc lên + Làm chậm diện rộng xung quanh
            Spells.push_back(spell);
        }
        // ===
// ===
        // ==========================================
        // CHỮ U - V
        // ==========================================
        {
            SpellData spell;
            spell.charName = "Urgot";
            spell.dangerlevel = 3;
            spell.missileName = "UrgotQMissile";
            spell.name = "Corrosive Charge (Q)";
            spell.radius = 210.0f;
            spell.range = 800.0f;
            spell.spellDelay = 600; // Trễ lựu đạn nổ
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "UrgotQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Urgot";
            spell.dangerlevel = 5; // Độ nguy hiểm tối đa (Kết liễu mục tiêu thấp máu)
            spell.missileName = "UrgotRMissile";
            spell.name = "Fear Beyond Death (R)";
            spell.projectileSpeed = 3200.0f; // Mũi khoan bay cực nhanh
            spell.radius = 80.0f;
            spell.range = 2500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "UrgotR";
            spell.spellType = ZDSpellType::Line;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Varus";
            spell.dangerlevel = 2;
            spell.missileName = "VarusQMissile";
            spell.name = "Piercing Arrow (Q)";
            spell.projectileSpeed = 1900.0f;
            spell.radius = 70.0f;
            spell.range = 1625.0f; // Tầm bắn tối đa khi gồng đủ thời gian
            spell.spellDelay = 0; // Trễ tung chiêu phụ thuộc vào thời điểm người chơi thả phím Q
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "VarusQ";
            spell.spellType = ZDSpellType::Line;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Varus";
            spell.dangerlevel = 4;
            spell.missileName = "VarusRMissile";
            spell.name = "Chain of Corruption (R)";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 120.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "VarusR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }
        // Vayne E (Condemn) — REMOVED: Targeted/point-and-click spell, cannot be dodged
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.dangerlevel = 2;
            spell.name = "Baleful Strike (Q)";
            spell.projectileSpeed = 2200.0f;
            spell.radius = 70.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.missileName = "VeigarBalefulStrikeMis";
            spell.spellName = "VeigarBalefulStrike";
            spell.extraSpellNames = { "VeigarQ" };
            spell.extraMissileNames = { "VeigarQMis", "VeigarQMissile" };
            spell.spellType = ZDSpellType::Line;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions }; // Tối đa trúng 2 mục tiêu
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.dangerlevel = 3;
            spell.name = "Dark Matter (W)";
            spell.radius = 240.0f;
            spell.range = 900.0f;
            spell.spellDelay = 1250; // Thiên thạch rơi khá trễ, né rất dễ nếu không bị choáng trước
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "VeigarW";
            spell.spellType = ZDSpellType::Circular;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "VelKoz";
            spell.dangerlevel = 3;
            spell.missileName = "VelkozQMissile";
            spell.name = "Plasma Fission (Q)";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 50.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "VelkozQ";
            spell.spellType = ZDSpellType::Line; // Cần logic nâng cao xử lý tách góc vuông 90 độ (Q2) khi tái kích hoạt hoặc hết tầm sinh ra 2 tia phụ
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.dangerlevel = 2;
            spell.name = "Mistral Pace (Q)";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 180.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 150;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "VexQ";
            spell.spellType = ZDSpellType::Line;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.dangerlevel = 3;
            spell.missileName = "VexE";
            spell.name = "Mistral Bolt (E)";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 120.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "VexE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Viktor";
            spell.dangerlevel = 2;
            spell.name = "Death Ray (E)";
            spell.projectileSpeed = 1050.0f;
            spell.radius = 80.0f;
            spell.range = 540.0f; // Tầm đặt điểm đầu, tia laser quét thêm một đoạn dài 500
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ViktorE";
            spell.spellType = ZDSpellType::Line; // Kỹ năng dạng vector, cần lấy điểm bắt đầu (StartPos) từ dữ liệu cast thực tế thay vì từ vị trí của Viktor
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vladimir";
            spell.dangerlevel = 2;
            spell.name = "Tides of Blood (E)";
            spell.projectileSpeed = 4000.0f; // Máu bắn ra xung quanh siêu tốc
            spell.radius = 60.0f;
            spell.range = 600.0f;
            spell.spellDelay = 0; // Kích hoạt khi Vladimir thả nút gồng
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "VladimirE";
            spell.spellType = ZDSpellType::Circular; // Phân bổ vòng tròn quanh bản thân
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // CHỮ W - X - Y - Z
        // ==========================================
        {
            SpellData spell;
            spell.charName = "Xayah";
            spell.dangerlevel = 2;
            spell.missileName = "XayahQMissile1";
            spell.name = "Double Daggers (Q)";
            spell.projectileSpeed = 2075.0f;
            spell.radius = 45.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 150;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "XayahQ";
            spell.spellType = ZDSpellType::Line; // Engine cần lưu giữ vị trí các lông vũ rớt trên sàn để né chiêu Triệu Hồi Lông Vũ (E)
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.dangerlevel = 3;
            spell.name = "Arcanopulse (Q)";
            spell.radius = 70.0f;
            spell.range = 1450.0f;
            spell.spellDelay = 520; // Hoạt ảnh tụ năng lượng trước khi giật sét đường thẳng
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "XerathArcanopulseChargeUp";
            spell.spellType = ZDSpellType::Line;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.dangerlevel = 3;
            spell.missileName = "XerathArcaneBarrageFive"; // Phát bắn chiêu cuối
            spell.name = "Rite of the Arcane (R)";
            spell.radius = 200.0f;
            spell.range = 5000.0f; // Tầm bắn siêu xa tăng theo cấp kỹ năng
            spell.spellDelay = 630; // Thời gian từ lúc bắn tới khi pháo kích chạm đất
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "XerathR";
            spell.spellType = ZDSpellType::Circular;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yasuo";
            spell.dangerlevel = 3;
            spell.missileName = "YasuoQ3Mis"; // Lốc xoáy
            spell.name = "Steel Tempest (Q3)";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 90.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 333; // Giảm dần theo Tốc độ đánh (Attack Speed)
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "YasuoQ3W";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yone";
            spell.dangerlevel = 3;
            spell.missileName = "YoneQ3Mis"; // Lốc xoáy kèm lướt của Yone
            spell.name = "Mortal Steel (Q3)";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 80.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 350; // Giảm dần theo Tốc độ đánh
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "YoneQ3";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yone";
            spell.dangerlevel = 5;
            spell.name = "Fate Sealed (R)";
            spell.radius = 112.5f; // Bề rộng của dải đường quét chiêu cuối
            spell.range = 1000.0f;
            spell.spellDelay = 750; // 0.75 giây gồng báo đỏ trước khi chém hất tung toàn bộ mục tiêu về tâm
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "YoneR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zeri";
            spell.dangerlevel = 1;
            spell.missileName = "ZeriQMis";
            spell.name = "Burst Fire (Q)";
            spell.projectileSpeed = 2600.0f;
            spell.radius = 40.0f;
            spell.range = 825.0f;
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZeriQ";
            spell.spellType = ZDSpellType::Line;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 2;
            spell.missileName = "ZiggsQBombs";
            spell.name = "Bouncing Bomb (Q)";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 150.0f; // Bán kính nổ khi chạm mục tiêu hoặc nảy hết tầm
            spell.range = 850.0f; // Tầm ném ban đầu, tổng tầm nảy có thể lên tới 1400
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZiggsQ";
            spell.spellType = ZDSpellType::Circular; // Xử lý né dạng điểm rơi nảy đoạn ngắn
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zoe";
            spell.dangerlevel = 3;
            spell.missileName = "ZoeEMis";
            spell.name = "Sleepy Trouble Bubble (E)";
            spell.projectileSpeed = 1850.0f; // Bay xa hơn nếu đi qua địa hình tường
            spell.radius = 80.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ZoeE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Sleep; // Gây buồn ngủ rồi ngủ thiếp đi
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zyra";
            spell.dangerlevel = 3;
            spell.missileName = "ZyraEIsotope";
            spell.name = "Grasping Roots (E)";
            spell.projectileSpeed = 1150.0f; // Tốc độ rễ cây bò khá chậm
            spell.radius = 70.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ZyraE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare; // Trói xuyên qua cả lính lẫn tướng
            Spells.push_back(spell);
        }
        // ===
    }
};

inline std::vector<SpellData> SpellDatabase::Spells;
} // namespace Plugins::KuroEvade::InternalDatabase

// ============================================================================
// KuroEvade Compatibility Wrapper
// ============================================================================
namespace Plugins::KuroEvade::SpellDatabase {

static inline std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

inline const std::vector<SpellDataEntry>& Spells() {
    static std::vector<SpellDataEntry> combinedData;
    static bool initialized = false;
    if (!initialized) {
        if (::Plugins::KuroEvade::InternalDatabase::SpellDatabase::Spells.empty()) {
            ::Plugins::KuroEvade::InternalDatabase::SpellDatabase::Initialize();
        }

        for (const auto& zd : ::Plugins::KuroEvade::InternalDatabase::SpellDatabase::Spells) {
            SpellDataEntry e = {};
            e.sdk.ChampionName = zd.charName;
            e.sdk.SpellName = zd.spellName;
            if (!zd.missileName.empty()) {
                e.sdk.MissileSpellName = zd.missileName;
            }
            e.sdk.DangerValue = zd.dangerlevel;
            e.sdk.Delay = zd.spellDelay;
            e.sdk.Radius = static_cast<int>(zd.radius);
            e.sdk.Range = static_cast<int>(zd.range);
            e.sdk.Width = static_cast<int>(zd.radius);
            e.sdk.MissileSpeed = (zd.projectileSpeed > 0.0f) ? static_cast<int>(zd.projectileSpeed) : 2147483647;
            e.sdk.FixedRange = zd.fixedRange;
            e.sdk.ExtraSpellNames = zd.extraSpellNames;
            e.sdk.ExtraMissileNames = zd.extraMissileNames;
            if (zd.angle > 0.0f) {
                e.sdk.Angle = static_cast<int>(std::lround(zd.angle));
                e.sdk.ArcAngle = static_cast<int>(std::lround(zd.angle));
            }
            e.UseEndPosition = zd.useEndPosition;
            e.DisplayName = zd.name;
            e.HasTrap = zd.hasTrap;
            e.TrapBaseName = zd.trapBaseName;
            e.IsSpecialIgnore = zd.noProcess;
            e.DisabledByDefault = zd.defaultOff;
            e.SecondaryRadius = static_cast<int>(std::lround(zd.secondaryRadius));
            e.ExtraEndTime = static_cast<float>(zd.extraEndTime);

            switch (zd.spellKey) {
                case ::Plugins::KuroEvade::KuroSpellSlot::Q: e.sdk.Slot = SDK::SpellSlot::Q; break;
                case ::Plugins::KuroEvade::KuroSpellSlot::W: e.sdk.Slot = SDK::SpellSlot::W; break;
                case ::Plugins::KuroEvade::KuroSpellSlot::E: e.sdk.Slot = SDK::SpellSlot::E; break;
                case ::Plugins::KuroEvade::KuroSpellSlot::R: e.sdk.Slot = SDK::SpellSlot::R; break;
                default: e.sdk.Slot = SDK::SpellSlot::Unknown; break;
            }

            switch (zd.spellType) {
                case ::Plugins::KuroEvade::KuroSpellType::Line: e.sdk.SpellType = SDK::SpellType::SkillshotLine; break;
                case ::Plugins::KuroEvade::KuroSpellType::Circular: e.sdk.SpellType = SDK::SpellType::SkillshotCircle; break;
                case ::Plugins::KuroEvade::KuroSpellType::Cone: e.sdk.SpellType = SDK::SpellType::SkillshotCone; break;
                // The imported Arc entry has no usable ArcPoly radius/angle.
                // A moving chord is conservative but, unlike a zero-angle arc,
                // remains visible and participates in collision checks.
                case ::Plugins::KuroEvade::KuroSpellType::Arc: e.sdk.SpellType = SDK::SpellType::SkillshotMissileLine; break;
            }

            for (const auto& obj : zd.collisionObjects) {
                switch (obj) {
                    case ::Plugins::KuroEvade::KuroCollisionObjectType::EnemyChampions:
                        e.sdk.CollisionObjects.push_back(SDK::CollisionableObjects::Heroes);
                        break;
                    case ::Plugins::KuroEvade::KuroCollisionObjectType::EnemyMinions:
                        e.sdk.CollisionObjects.push_back(SDK::CollisionableObjects::Minions);
                        break;
                    case ::Plugins::KuroEvade::KuroCollisionObjectType::EnemyYasuoWall:
                        e.sdk.CollisionObjects.push_back(SDK::CollisionableObjects::YasuoWall);
                        break;
                    case ::Plugins::KuroEvade::KuroCollisionObjectType::Terrain:
                        e.sdk.CollisionObjects.push_back(SDK::CollisionableObjects::Walls);
                        break;
                }
            }

            combinedData.push_back(e);
        }
        initialized = true;
    }
    return combinedData;
}

} // namespace Plugins::KuroEvade::SpellDatabase
