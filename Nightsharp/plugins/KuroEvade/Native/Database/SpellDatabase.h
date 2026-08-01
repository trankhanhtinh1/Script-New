#pragma once
#include "SpellData.h"

#include <cmath>

namespace Plugins::KuroEvade::Database {

class SpellDatabase final {
public:
    static std::vector<SpellData> Entries;

    static void Initialize() {
        Entries.clear();
        Entries.reserve(256);
        // ===
        {
            SpellData spell;
            spell.IsGlobal = true;
            spell.DangerValue = 1;
            spell.MissileSpellName = "summonersnowball";
            spell.DisplayName = "Mark";
            spell.MissileSpeed = 1300.0f;
            spell.Radius = 60.0f;
            spell.Range = 1600.0f;
            spell.Delay = 0;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "summonersnowball";
            spell.ExtraSpellNames = { "summonerporothrow" };
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI A
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Aatrox;
            spell.DangerValue = 2;
            spell.DisplayName = "The Darkin Blade (Q1)";
            spell.Range = 650.0f;
            spell.Radius = 120.0f;
            spell.Delay = 600;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "AatroxQ1";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Trúng rìa sẽ bị hất tung
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Aatrox;
            spell.DangerValue = 2;
            spell.DisplayName = "The Darkin Blade (Q2)";
            spell.Range = 525.0f;
            spell.MultipleAngle = 60.0f;
            spell.Delay = 600;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "AatroxQ2";
            spell.Type = SkillShotType::SkillshotCone;
            spell.CrowdControl = CrowdControlType::KnockUp;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Aatrox;
            spell.DangerValue = 3;
            spell.DisplayName = "The Darkin Blade (Q3)";
            spell.Range = 450.0f;
            spell.Radius = 200.0f;
            spell.Delay = 600;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "AatroxQ3";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::KnockUp;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Aatrox;
            spell.DangerValue = 2;
            spell.MissileSpellName = "AatroxW";
            spell.DisplayName = "Infernal Chains";
            spell.MissileSpeed = 1800.0f;
            spell.Radius = 80.0f;
            spell.Range = 825.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "AatroxW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Làm chậm trước, xích kéo lại tính sau
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ahri;
            spell.DangerValue = 2;
            spell.MissileSpellName = "AhriOrbMissile";
            spell.DisplayName = "Orb of Deception";
            spell.MissileSpeed = 1750.0f;
            spell.Radius = 100.0f;
            spell.Range = 925.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "AhriOrbofDeception";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ahri;
            spell.DangerValue = 2;
            spell.MissileSpellName = "AhriOrbReturn";
            spell.DisplayName = "Orb of Deception (Return)";
            spell.MissileSpeed = 915.0f;
            spell.Radius = 100.0f;
            spell.Range = 925.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "AhriOrbofDeception2";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ahri;
            spell.DangerValue = 3;
            spell.MissileSpellName = "AhriSeduceMissile";
            spell.DisplayName = "Charm";
            spell.MissileSpeed = 1550.0f;
            spell.Radius = 60.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "AhriSeduce";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Charm; // Mê hoặc nguy hiểm cường độ cao
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Akali;
            spell.DangerValue = 1;
            spell.DisplayName = "Five Point Strike";
            spell.Range = 550.0f;
            spell.MultipleAngle = 45.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "AkaliQ";
            spell.Type = SkillShotType::SkillshotCone;
            spell.CrowdControl = CrowdControlType::Slow; // Làm chậm ở rìa chiêu
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Akali;
            spell.DangerValue = 2;
            spell.MissileSpellName = "AkaliEMis";
            spell.DisplayName = "Shuriken Flip";
            spell.MissileSpeed = 1800.0f;
            spell.Radius = 70.0f;
            spell.Range = 825.0f;
            spell.Delay = 400;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "AkaliE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Akshan;
            spell.DangerValue = 2;
            spell.MissileSpellName = "AkshanQMissile";
            spell.DisplayName = "Avengerang";
            spell.MissileSpeed = 1500.0f;
            spell.Radius = 90.0f;
            spell.Range = 850.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "AkshanQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Alistar;
            spell.DangerValue = 3;
            spell.DisplayName = "Pulverize";
            spell.Radius = 375.0f;
            spell.Range = 375.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "Pulverize";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::KnockUp;
            spell.IsDangerous = true;
            // The impact follows Alistar through the cast windup, then locks
            // naturally when this short-lived skillshot expires.
            spell.FollowCaster = true;
            spell.UseEndPosition = true;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Amumu;
            spell.DangerValue = 3;
            spell.MissileSpellName = "SadMummyBandageToss";
            spell.DisplayName = "Bandage Toss";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 80.0f;
            spell.Range = 1100.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "BandageToss";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Choáng tầm xa bám theo mục tiêu
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Amumu;
            spell.DangerValue = 4;
            spell.DisplayName = "Curse of the Sad Mummy";
            spell.Radius = 560.0f;
            spell.Range = 560.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "CurseoftheSadMummy";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Stun; // Chiêu cuối làm choáng diện rộng
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Anivia;
            spell.DangerValue = 3;
            spell.MissileSpellName = "FlashFrostSpell";
            spell.DisplayName = "Flash Frost";
            spell.MissileSpeed = 850.0f;
            spell.Radius = 110.0f;
            spell.Range = 1250.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "FlashFrostSpell";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Choáng khi kích nổ quả cầu băng
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Annie;
            spell.DangerValue = 2;
            spell.MultipleAngle = 25.0f;
            spell.DisplayName = "Incinerate";
            spell.Range = 625.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "Incinerate";
            spell.Type = SkillShotType::SkillshotCone;
            spell.CrowdControl = CrowdControlType::Stun; // Có thể gây choáng nếu có nội tại Hỏa Cuồng
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Annie;
            spell.DangerValue = 4;
            spell.DisplayName = "Summon: Tibbers";
            spell.Radius = 290.0f;
            spell.Range = 600.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "InfernalGuardian";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Stun; // Gây choáng tức thì diện rộng kèm nội tại
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Aphelios;
            spell.DangerValue = 2;
            spell.MissileSpellName = "ApheliosCalibrumQMis";
            spell.DisplayName = "Moonshot";
            spell.MissileSpeed = 1850.0f;
            spell.Radius = 60.0f;
            spell.Range = 1450.0f;
            spell.Delay = 400;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "ApheliosCalibrumQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Aphelios;
            spell.DangerValue = 4;
            spell.MissileSpellName = "ApheliosRMis";
            spell.DisplayName = "Moonlight Vigil";
            spell.MissileSpeed = 2050.0f;
            spell.Radius = 125.0f;
            spell.Range = 1300.0f;
            spell.Delay = 500;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "ApheliosR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Chiêu cuối có thể gây chậm hoặc trói tùy loại súng (ưu tiên đặt Slow cơ bản)
            spell.CollisionObjects = {
                CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyYasuoWall,
            };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.SecondaryRadius = 300.0f;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ashe;
            spell.DangerValue = 4;
            spell.MissileSpellName = "EnchantedCrystalArrow";
            spell.DisplayName = "Enchanted Crystal Arrow";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 130.0f;
            spell.Range = 25000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "EnchantedCrystalArrow";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Choáng Đại Băng Tiễn cực kỳ quan trọng đối với Evade
            // The arrow stops on the first champion, but its shatter damage
            // is centered on that champion and reaches surrounding units.
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyYasuoWall };
            spell.HasEndExplosion = true;
            spell.SecondaryRadius = 400.0f;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.EndExplosionAtUnitCenter = true;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::AurelionSol;
            spell.DangerValue = 4;
            spell.DisplayName = "Falling Star / The Skies Descend";
            spell.Radius = 350.0f;
            spell.Range = 1200.0f;
            spell.Delay = 1250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "AurelionSolR";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Stun; // Gây choáng diện rộng (hoặc hất tung nếu nâng cấp nâng cao)
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Azir;
            spell.DangerValue = 4;
            spell.MissileSpellName = "AzirSoldierRMissile";
            spell.DisplayName = "Emperor's Divide";
            spell.MissileSpeed = 1400.0f;
            spell.Radius = 500.0f;
            spell.Range = 700.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "AzirR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Đẩy lùi/Hất tung kẻ địch
            Entries.push_back(spell);
        }
        // ===
// ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI B
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Bard;
            spell.DangerValue = 2;
            spell.MissileSpellName = "BardQMissile";
            spell.DisplayName = "Cosmic Binding";
            spell.MissileSpeed = 1500.0f;
            spell.Radius = 60.0f;
            spell.Range = 850.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "BardQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::Terrain,
                CollisionObjectType::EnemyYasuoWall };
            spell.CollisionTargetLimit = 2;
            spell.CollisionInitialRange = 850.0f;
            spell.CollisionContinuationDistance = 300.0f;
            spell.CollisionContinuationRadius = 60.0f;
            spell.CollisionContinuationStopsOnSecondTarget = true;
            spell.CollisionContinuationStopsOnTerrain = true;
            spell.FixedRange = true;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Bard;
            spell.DangerValue = 4;
            spell.MissileSpellName = "BardRMissile";
            spell.DisplayName = "Tempered Fate";
            spell.MissileSpeed = 2100.0f; // Tốc độ bay theo khoảng cách, tạm để trung bình cao
            spell.Radius = 350.0f;
            spell.Range = 3400.0f;
            spell.Delay = 500; // Tăng dần theo khoảng cách
            spell.Slot = SpellSlot::R;
            spell.SpellName = "BardR";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Suppression; // Đóng băng trạng thái (Stasis giống Đồng Hồ Cát)
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Belveth;
            spell.DangerValue = 3;
            spell.DisplayName = "Above and Below (W)";
            spell.Radius = 100.0f;
            spell.Range = 660.0f;
            spell.Delay = 500;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "BelvethW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Hất tung dạng đường thẳng thẳng góc hẹp
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Blitzcrank;
            spell.DangerValue = 4;
            spell.MissileSpellName = "RocketGrabMissile";
            spell.DisplayName = "Rocket Grab";
            spell.MissileSpeed = 1800.0f;
            spell.Radius = 70.0f;
            spell.Range = 1150.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "RocketGrab";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Kéo mục tiêu về phía bản thân (coi như Khống chế mạnh)
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Brand;
            spell.DangerValue = 2;
            spell.MissileSpellName = "BrandQMissile";
            spell.DisplayName = "Sear";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 60.0f;
            spell.Range = 1050.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "BrandQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Gây choáng nếu mục tiêu đang bị bỏng nội tại
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Brand;
            spell.DangerValue = 2;
            spell.DisplayName = "Pillar of Flame";
            spell.Radius = 250.0f;
            spell.Range = 900.0f;
            spell.Delay = 625;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "BrandW";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Braum;
            spell.DangerValue = 2;
            spell.MissileSpellName = "BraumQMissile";
            spell.DisplayName = "Winter's Bite";
            spell.MissileSpeed = 1700.0f;
            spell.Radius = 65.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "BraumQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Braum;
            spell.DangerValue = 4;
            spell.MissileSpellName = "BraumRMissile";
            spell.DisplayName = "Glacial Fissure";
            spell.MissileSpeed = 1400.0f;
            spell.Radius = 115.0f;
            spell.Range = 1250.0f;
            spell.Delay = 500;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "BraumR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Hất tung diện rộng
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Briar;
            spell.DangerValue = 3;
            spell.DisplayName = "Chilling Scream (E)";
            spell.Range = 600.0f;
            spell.MultipleAngle = 40.0f;
            spell.Delay = 1000; // Delay vận sức tối đa
            spell.Slot = SpellSlot::E;
            spell.SpellName = "BriarE";
            spell.Type = SkillShotType::SkillshotCone;
            spell.CrowdControl = CrowdControlType::KnockUp; // Đẩy lùi/Hất tung, nếu va trúng tường gây Stun
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Briar;
            spell.DangerValue = 4;
            spell.MissileSpellName = "BriarRMissile";
            spell.DisplayName = "Certain Death";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 120.0f;
            spell.Range = 25000.0f; // Toàn bản đồ
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "BriarR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Chậm mục tiêu trúng trực tiếp và hoảng sợ xung quanh
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions };
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI C
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Caitlyn;
            spell.DangerValue = 2;
            spell.MissileSpellName = "CaitlynPiltoverPeacemakerMissile";
            spell.DisplayName = "Piltover Peacemaker";
            spell.MissileSpeed = 2200.0f;
            spell.Radius = 90.0f; // Thu hẹp còn 60 sau khi xuyên thấu
            spell.Range = 1300.0f;
            spell.Delay = 625;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "CaitlynPiltoverPeacemaker";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Caitlyn;
            spell.DangerValue = 2;
            spell.DisplayName = "Yordle Snap Trap";
            spell.Radius = 75.0f;
            spell.Range = 800.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "CaitlynYordleSnapTrap";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Snare; // Trói chân kẻ địch đạp bẫy
            spell.HasTrap = true;
            spell.TrapBaseName = "CaitlynTrap";
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Caitlyn;
            spell.DangerValue = 2;
            spell.MissileSpellName = "CaitlynEntrapmentMissile";
            spell.DisplayName = "90 Caliber Net";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 70.0f;
            spell.Range = 800.0f;
            spell.Delay = 150;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "CaitlynEntrapment";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Camille;
            spell.DangerValue = 2;
            spell.DisplayName = "Tactical Sweep (W)";
            spell.Range = 650.0f;
            spell.MultipleAngle = 45.0f;
            spell.Delay = 750;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "CamilleW";
            spell.Type = SkillShotType::SkillshotCone;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Camille;
            spell.DangerValue = 3;
            spell.DisplayName = "Wall Dive (E2)";
            spell.MissileSpeed = 1050.0f;
            spell.Radius = 110.0f;
            spell.Range = 800.0f; // Nhân đôi nếu hướng về phía Champion
            spell.Delay = 0;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "CamilleEDash2";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Làm choáng khi phóng trúng mục tiêu
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Cassiopeia;
            spell.DangerValue = 1;
            spell.DisplayName = "Noxious Blast";
            spell.Radius = 160.0f;
            spell.Range = 850.0f;
            spell.Delay = 650;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "CassiopeiaQ";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Cassiopeia;
            spell.DangerValue = 4;
            spell.DisplayName = "Petrifying Gaze";
            spell.Range = 825.0f;
            spell.MultipleAngle = 80.0f;
            spell.Delay = 500;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "CassiopeiaR";
            spell.Type = SkillShotType::SkillshotCone;
            spell.CrowdControl = CrowdControlType::Stun; // Hóa đá (Stun) nếu quay mặt lại, Slow nếu quay lưng
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Chogath;
            spell.DangerValue = 3;
            spell.DisplayName = "Rupture";
            spell.Radius = 250.0f;
            spell.Range = 950.0f;
            spell.Delay = 1200; // Chỉ thị vòng tròn đỏ hiển thị lâu trước khi hất tung
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "Rupture";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::KnockUp;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Chogath;
            spell.DangerValue = 2;
            spell.DisplayName = "Feral Scream";
            spell.Range = 650.0f;
            spell.MultipleAngle = 60.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "FeralScream";
            spell.Type = SkillShotType::SkillshotCone;
            spell.CrowdControl = CrowdControlType::Silence; // Gây câm lặng diện rộng
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Corki;
            spell.DangerValue = 1;
            spell.MissileSpellName = "PhosphorusBombMissile";
            spell.DisplayName = "Phosphorus Bomb";
            spell.MissileSpeed = 1000.0f;
            spell.Radius = 250.0f;
            spell.Range = 825.0f;
            spell.Delay = 300;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "PhosphorusBomb";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Corki;
            spell.DangerValue = 2;
            spell.MissileSpellName = "MissileBarrageMissile";
            spell.DisplayName = "Missile Barrage (R)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 40.0f;
            spell.Range = 1300.0f;
            spell.Delay = 175;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "MissileBarrageMissile";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.SecondaryRadius = 150.0f;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Corki;
            spell.DangerValue = 3;
            spell.MissileSpellName = "MissileBarrageMissile2";
            spell.DisplayName = "Missile Barrage - The Big One (R)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 40.0f;
            spell.Range = 1500.0f;
            spell.Delay = 175;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "MissileBarrageMissile2";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.SecondaryRadius = 300.0f;
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI D
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Darius;
            spell.DangerValue = 3;
            spell.DisplayName = "Apprehend";
            spell.Range = 535.0f;
            spell.MultipleAngle = 50.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "DariusE";
            spell.Type = SkillShotType::SkillshotCone;
            spell.CrowdControl = CrowdControlType::KnockUp; // Kéo/Hất văng lôi kéo mục tiêu
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Diana;
            spell.DangerValue = 2;
            spell.MissileSpellName = "DianaQMissile";
            spell.DisplayName = "Crescent Strike";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 185.0f; // Bán kính vụ nổ vòng tròn cuối cung bay
            spell.Range = 900.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "DianaQ";
            spell.Type = SkillShotType::SkillshotArc; // Đạn đạo dạng hình lưỡi liềm đặc biệt
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::DrMundo;
            spell.DangerValue = 1;
            spell.MissileSpellName = "DrMundoQMissile";
            spell.DisplayName = "Infected Bonesaw";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 60.0f;
            spell.Range = 1050.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "DrMundoQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Draven;
            spell.DangerValue = 2;
            spell.MissileSpellName = "DravenDoubleShotMissile";
            spell.DisplayName = "Stand Aside";
            spell.MissileSpeed = 1400.0f;
            spell.Radius = 130.0f;
            spell.Range = 1100.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "DravenDoubleShot";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Đạt hất dạt sang một bên
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Draven;
            spell.DangerValue = 3;
            spell.MissileSpellName = "DravenRCast";
            spell.DisplayName = "Whirling Death";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 160.0f;
            spell.Range = 25000.0f;
            spell.Delay = 400;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "DravenRCast";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===
// ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI E
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ekko;
            spell.DangerValue = 2;
            spell.MissileSpellName = "EkkoQMis";
            spell.DisplayName = "Timewinder (Q1)";
            spell.MissileSpeed = 1650.0f;
            spell.Radius = 60.0f;
            spell.Range = 1075.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "EkkoQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ekko;
            spell.DangerValue = 3;
            spell.DisplayName = "Parallel Convergence (W)";
            spell.Radius = 375.0f;
            spell.Range = 1600.0f;
            spell.Delay = 3750; // Delay từ lúc gọi đến lúc vùng hiển thị kích nổ
            spell.Slot = SpellSlot::W;
            spell.SpellName = "EkkoW";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Stun; // Gây choáng nếu Ekko bước vào vùng này
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Elise;
            spell.DangerValue = 3;
            spell.MissileSpellName = "EliseHumanE";
            spell.DisplayName = "Cocoon";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 55.0f;
            spell.Range = 1075.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "EliseHumanE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Kén Nhện gây choáng rất quan trọng để né
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        // Bỏ trống hoặc cập nhật nếu có dữ liệu tướng cụ thể riêng biệt trong database của bạn
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Evelynn;
            spell.DangerValue = 1;
            spell.MissileSpellName = "EvelynnQLineMis";
            spell.DisplayName = "Hate Spike (Q)";
            spell.MissileSpeed = 2400.0f;
            spell.Radius = 60.0f;
            spell.Range = 800.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "EvelynnQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Evelynn;
            spell.DangerValue = 4;
            spell.DisplayName = "Last Caress (R)";
            spell.Range = 450.0f;
            spell.MultipleAngle = 180.0f; // Quạt ngược góc rộng về phía sau
            spell.Delay = 350;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "EvelynnR";
            spell.Type = SkillShotType::SkillshotCone;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ezreal;
            spell.DangerValue = 1;
            spell.MissileSpellName = "EzrealQ";
            spell.DisplayName = "Mystic Shot";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 60.0f;
            spell.Range = 1150.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "EzrealMysticShot";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ezreal;
            spell.DangerValue = 2;
            spell.MissileSpellName = "EzrealW";
            spell.DisplayName = "Essence Flux";
            spell.MissileSpeed = 1700.0f;
            spell.Radius = 80.0f;
            spell.Range = 1150.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "EzrealEssenceFlux";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions }; // Chỉ bám vào tướng/công trình
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ezreal;
            spell.DangerValue = 3;
            spell.MissileSpellName = "EzrealR";
            spell.DisplayName = "Trueshot Barrage";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 160.0f;
            spell.Range = 25000.0f;
            spell.Delay = 1000; // Vận sức 1 giây
            spell.Slot = SpellSlot::R;
            spell.SpellName = "EzrealTrueshotBarrage";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI F
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Fiddlesticks;
            spell.DangerValue = 2;
            spell.DisplayName = "Bountiful Harvest (E)";
            spell.Radius = 80.0f; // Bán kính vạch cắt của lưỡi liềm
            spell.Range = 850.0f;
            spell.Delay = 400;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "FiddlesticksE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Silence; // Câm lặng nếu ở giữa vạch, Slow ở rìa ngoài
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Fiora;
            spell.DangerValue = 2;
            spell.MissileSpellName = "FioraWMissile";
            spell.DisplayName = "Riposte (W)";
            spell.MissileSpeed = 3200.0f;
            spell.Radius = 70.0f;
            spell.Range = 800.0f;
            spell.Delay = 750;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "FioraW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Làm chậm cơ bản, chuyển thành Stun nếu chặn được CC cứng
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Fizz;
            spell.DangerValue = 4;
            spell.MissileSpellName = "FizzRMissile";
            spell.DisplayName = "Chum the Waters (R)";
            spell.MissileSpeed = 1300.0f;
            spell.Radius = 80.0f;
            spell.Range = 1300.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "FizzR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Thả cá gây hất tung diện rộng tại điểm cuối
            spell.CollisionObjects = {
                CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyYasuoWall,
            };
            spell.HasEndExplosion = true;
            spell.SecondaryRadius = 200.0f;
            spell.EndExplosionDelay = 2000;
            spell.EndExplosionAtUnitCenter = true;
            spell.EndExplosionFollowsUnit = true;
            spell.EndExplosionMediumTravelDistance = 455.0f;
            spell.EndExplosionFarTravelDistance = 910.0f;
            spell.EndExplosionRadiusMedium = 325.0f;
            spell.EndExplosionRadiusFar = 450.0f;
            spell.ExtraEndTime = 150;
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI G
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Galio;
            spell.DangerValue = 2;
            spell.MissileSpellName = "GalioQMissileIn";
            spell.DisplayName = "Winds of War (Q)";
            spell.MissileSpeed = 1400.0f;
            spell.Radius = 120.0f;
            spell.Range = 825.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "GalioQ";
            spell.Type = SkillShotType::SkillshotCircle; // Hai luồng gió hội tụ tạo thành một vùng xoáy tròn
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Galio;
            spell.DangerValue = 3;
            spell.DisplayName = "Justice Punch (E)";
            spell.MissileSpeed = 2300.0f;
            spell.Radius = 160.0f;
            spell.Range = 650.0f;
            spell.Delay = 400; // Có bước lùi lại lấy đà trước khi lao tới
            spell.Slot = SpellSlot::E;
            spell.SpellName = "GalioE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Gnar;
            spell.DangerValue = 1;
            spell.MissileSpellName = "GnarQMissile";
            spell.DisplayName = "Boomerang Throw (Mini Q)";
            spell.MissileSpeed = 2500.0f;
            spell.Radius = 60.0f;
            spell.Range = 1125.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "GnarQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Gnar;
            spell.DangerValue = 2;
            spell.MissileSpellName = "GnarBigQMissile";
            spell.DisplayName = "Boulder Toss (Mega Q)";
            spell.MissileSpeed = 2100.0f;
            spell.Radius = 90.0f;
            spell.Range = 1150.0f;
            spell.Delay = 500;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "GnarBigQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Gnar;
            spell.DangerValue = 3;
            spell.DisplayName = "Wallop (Mega W)";
            spell.Radius = 100.0f;
            spell.Range = 600.0f;
            spell.Delay = 600;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "GnarBigW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Đập choáng diện rộng dạng đường thẳng hẹp
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Gragas;
            spell.DangerValue = 2;
            spell.MissileSpellName = "GragasQMissile";
            spell.DisplayName = "Barrel Roll";
            spell.MissileSpeed = 1000.0f;
            spell.Radius = 250.0f;
            spell.Range = 850.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "GragasQ";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Gragas;
            spell.DangerValue = 3;
            spell.DisplayName = "Body Slam";
            spell.MissileSpeed = 900.0f;
            spell.Radius = 180.0f;
            spell.Range = 600.0f;
            spell.Delay = 0;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "GragasE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Lấy thịt đè người hất tung/bị khựng lại khi trúng mục tiêu
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Gragas;
            spell.DangerValue = 4;
            spell.MissileSpellName = "GragasRMissile";
            spell.DisplayName = "Explosive Cask";
            spell.MissileSpeed = 1800.0f;
            spell.Radius = 400.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "GragasR";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::KnockUp; // Thùng rượu nổ phá vỡ đội hình hất văng kẻ địch ra xa tâm nổ
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Graves;
            spell.DangerValue = 2;
            spell.MissileSpellName = "GravesQLineMissile";
            spell.DisplayName = "End of the Line (Q)";
            spell.MissileSpeed = 3000.0f;
            spell.Radius = 40.0f;
            spell.Range = 925.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "GravesQLineSpell";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Graves;
            spell.DangerValue = 2;
            spell.MissileSpellName = "GravesClusterShotAttack";
            spell.DisplayName = "Smoke Screen (W)";
            spell.MissileSpeed = 1500.0f;
            spell.Radius = 250.0f;
            spell.Range = 950.0f;
            spell.Delay = 150;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "GravesSmokeGrenade";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Slow; // Bom khói giảm tầm nhìn và làm chậm
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Graves;
            spell.DangerValue = 3;
            spell.MissileSpellName = "GravesUltimateMissile";
            spell.DisplayName = "Collateral Damage (R)";
            spell.MissileSpeed = 2100.0f;
            spell.Radius = 100.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "GravesUltimateShot";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Gwen;
            spell.DangerValue = 3;
            spell.MissileSpellName = "GwenRMissile";
            spell.DisplayName = "Needlework (R)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 120.0f;
            spell.Range = 1350.0f;
            spell.Delay = 250; // Delay mỗi lần tái kích hoạt phóng kim khâu
            spell.Slot = SpellSlot::R;
            spell.SpellName = "GwenR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI H
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Hecarim;
            spell.DangerValue = 4;
            spell.MissileSpellName = "HecarimUltMissile";
            spell.DisplayName = "Onslaught of Shadows (R)";
            spell.MissileSpeed = 1100.0f;
            spell.Radius = 240.0f; // Bán kính hàng bóng ma càn quét
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "HecarimUlt";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Charm; // Hoảng sợ kẻ địch tại điểm cuối
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Heimerdinger;
            spell.DangerValue = 2;
            spell.MissileSpellName = "HeimerdingerWMaxAttack";
            spell.DisplayName = "Hextech Micro-Rockets (W)";
            spell.MissileSpeed = 2850.0f;
            spell.Radius = 40.0f;
            spell.Range = 1325.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "HeimerdingerW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Heimerdinger;
            spell.DangerValue = 3;
            spell.MissileSpellName = "HeimerdingerEExplosion";
            spell.DisplayName = "CH-2 Electron Storm Grenade (E)";
            spell.MissileSpeed = 1200.0f;
            spell.Radius = 135.0f;
            spell.Range = 975.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "HeimerdingerE";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Stun; // Choáng ngay tâm nổ quả lựu đạn, làm chậm vùng ngoài
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Hwei;
            spell.DangerValue = 2;
            spell.MissileSpellName = "HweiQQMissile";
            spell.DisplayName = "Subject: Disaster - Devastating Fire (QQ)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 50.0f;
            spell.Range = 800.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "HweiQQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            spell.HasEndExplosion = true;
            spell.SecondaryRadius = 200.0f;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Hwei;
            spell.DangerValue = 2;
            spell.DisplayName = "Subject: Disaster - Molten Fissure (QW)";
            spell.Radius = 100.0f; // Bán kính đường vệt lửa dội xuống tầm xa
            spell.Range = 2000.0f;
            spell.Delay = 850;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "HweiQW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Hwei;
            spell.DangerValue = 3;
            spell.MissileSpellName = "HweiEQMissile";
            spell.DisplayName = "Subject: Torment - Grim Visage (EQ)";
            spell.MissileSpeed = 1500.0f;
            spell.Radius = 60.0f;
            spell.Range = 850.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "HweiEQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Charm; // Hoảng sợ mặt nạ ma ép lùi mục tiêu về sau
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Hwei;
            spell.DangerValue = 3;
            spell.MissileSpellName = "HweiEWMissile";
            spell.DisplayName = "Subject: Torment - Gaze of the Abyss (EW)";
            spell.MissileSpeed = 1400.0f;
            spell.Radius = 180.0f; // Vùng mắt trói
            spell.Range = 850.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "HweiEW";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Snare; // Con mắt khóa trói mục tiêu đầu tiên bước vào
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Hwei;
            spell.DangerValue = 4;
            spell.MissileSpellName = "HweiRMissile";
            spell.ExtraMissileNames = { "HweiR", "Hwei_R_Mis" };
            spell.DisplayName = "Spiraling Despair (R)";
            spell.MissileSpeed = 1400.0f;
            spell.Radius = 90.0f;
            spell.Range = 1340.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "HweiR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Dính vòng xoáy tuyệt vọng tăng tiến làm chậm rồi nổ tung
            spell.CollisionObjects = {
                CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyYasuoWall,
            };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.EndExplosionAtUnitCenter = true;
            spell.EndExplosionFollowsUnit = true;
            spell.EndExplosionDetonatesOnUnitDeath = true;
            spell.EndExplosionDelay = 3000;
            spell.SecondaryRadius = 500.0f;
            Entries.push_back(spell);
        }
        // ===
// ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI I
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Illaoi;
            spell.DangerValue = 2;
            spell.DisplayName = "Tentacle Smash (Q)";
            spell.Radius = 100.0f;
            spell.Range = 850.0f;
            spell.Delay = 750;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "IllaoiQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Illaoi;
            spell.DangerValue = 3;
            spell.MissileSpellName = "IllaoiEMis";
            spell.DisplayName = "Test of Spirit (E)";
            spell.MissileSpeed = 1900.0f;
            spell.Radius = 60.0f;
            spell.Range = 900.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "IllaoiE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Kéo linh hồn, nếu phá xích hoặc chết linh hồn sẽ bị Slow/gọi xúc tu
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Irelia;
            spell.DangerValue = 2;
            spell.MissileSpellName = "IreliaEMissile";
            spell.DisplayName = "Flawless Duet (E)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 80.0f;
            spell.Range = 900.0f;
            spell.Delay = 250; // Delay từ lúc thả cây kiếm thứ 2
            spell.Slot = SpellSlot::E;
            spell.SpellName = "IreliaE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Choáng đường thẳng nối giữa 2 cây kiếm
            spell.IsSpecial = true; // Cần thuật toán tracking vị trí E1 và E2 riêng biệt
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Irelia;
            spell.DangerValue = 3;
            spell.MissileSpellName = "IreliaR";
            spell.DisplayName = "Vanguard's Edge (R)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 160.0f;
            spell.Range = 1000.0f;
            spell.Delay = 400;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "IreliaR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Làm chậm cực mạnh khi đi xuyên qua vành kiếm nổ
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ivern;
            spell.DangerValue = 3;
            spell.MissileSpellName = "IvernQ";
            spell.DisplayName = "Rootcaller (Q)";
            spell.MissileSpeed = 1300.0f;
            spell.Radius = 80.0f;
            spell.Range = 1100.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "IvernQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare; // Trói chân để đồng đội bay vào kích hoạt đòn đánh
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI J
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Janna;
            spell.DangerValue = 2;
            spell.MissileSpellName = "HowlingGaleSpell";
            spell.DisplayName = "Howling Gale (Q)";
            spell.MissileSpeed = 660.0f; // Tốc độ tăng tiến dần theo thời gian tích gió (660 - 1584)
            spell.Radius = 120.0f;
            spell.Range = 1700.0f; // Tầm xa tăng theo thời gian tụ gió
            spell.Delay = 0;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "HowlingGale";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Hất tung cực kỳ khó chịu của Janna
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::JarvanIV;
            spell.DangerValue = 2;
            spell.DisplayName = "Dragon Strike (Q)";
            spell.Radius = 70.0f;
            spell.Range = 770.0f;
            spell.Delay = 400;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "JarvanIVDragonStrike";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Trở thành hất tung nếu kéo trúng combo Hoàng Kỳ E
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Jayce;
            spell.DangerValue = 2;
            spell.MissileSpellName = "JayceShockBlastMis";
            spell.DisplayName = "Shock Blast (Q Thường)";
            spell.MissileSpeed = 1450.0f;
            spell.Radius = 70.0f;
            spell.Range = 1050.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "JayceShockBlast";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::EnemyYasuoWall };
            spell.HasEndExplosion = true;
            spell.SecondaryRadius = 175.0f;
            spell.DetectionGroup = "JayceShockBlast";
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Jayce;
            spell.DangerValue = 3;
            spell.MissileSpellName = "JayceShockBlastWallMis";
            spell.DisplayName = "Shock Blast (Q Gia Tốc Cổng E)";
            spell.MissileSpeed = 2350.0f; // Tốc độ tăng vượt bậc
            spell.Radius = 70.0f;
            spell.Range = 1600.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "JayceShockBlastCharged";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::EnemyYasuoWall };
            spell.HasEndExplosion = true;
            spell.SecondaryRadius = 250.0f;
            spell.DetectionGroup = "JayceShockBlast";
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Jhin;
            spell.DangerValue = 2;
            spell.MissileSpellName = "JhinWMissile";
            spell.DisplayName = "Deadly Flourish (W)";
            spell.MissileSpeed = 5000.0f; // Gần như tức thì
            spell.Radius = 45.0f;
            spell.Range = 2500.0f;
            spell.Delay = 750;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "JhinW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare; // Trói chân mục tiêu bị đánh dấu
            // Deadly Flourish damages minions along the line but only an
            // enemy champion terminates it. Minions are deliberately absent.
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyYasuoWall };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Jhin;
            spell.DangerValue = 3;
            spell.MissileSpellName = "JhinRShotMis";
            spell.DisplayName = "Curtain Call (R)";
            spell.MissileSpeed = 5000.0f;
            spell.Radius = 80.0f;
            spell.Range = 3500.0f;
            spell.Delay = 250; // Delay bắn phát súng
            spell.Slot = SpellSlot::R;
            spell.SpellName = "JhinRShot";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Phát súng thứ 4 làm chậm 80%
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Jinx;
            spell.DangerValue = 2;
            spell.MissileSpellName = "JinxWMissile";
            spell.DisplayName = "Zap! (W)";
            spell.MissileSpeed = 3300.0f;
            spell.Radius = 60.0f;
            spell.Range = 1500.0f;
            spell.Delay = 600; // Giảm dần theo Tốc độ đánh của Jinx
            spell.Slot = SpellSlot::W;
            spell.SpellName = "JinxW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Jinx;
            spell.DangerValue = 3;
            spell.MissileSpellName = "JinxR";
            spell.DisplayName = "Super Mega Death Rocket! (R)";
            spell.MissileSpeed = 1700.0f; // Gia tốc lên 2200 sau khi bay được quãng ngắn
            spell.Radius = 140.0f;
            spell.Range = 25000.0f;
            spell.Delay = 600;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "JinxR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.EndExplosionAtUnitCenter = true;
            spell.SecondaryRadius = 400.0f;
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI K
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Kaisa;
            spell.DangerValue = 2;
            spell.MissileSpellName = "KaisaWMissile";
            spell.DisplayName = "Void Seeker (W)";
            spell.MissileSpeed = 1750.0f;
            spell.Radius = 100.0f;
            spell.Range = 3000.0f;
            spell.Delay = 400;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "KaisaW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Karma;
            spell.DangerValue = 2;
            spell.MissileSpellName = "KarmaQMissile";
            spell.DisplayName = "Inner Flame (Q)";
            spell.MissileSpeed = 1700.0f;
            spell.Radius = 60.0f;
            spell.Range = 950.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "KarmaQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.SecondaryRadius = 280.0f;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Karma;
            spell.DangerValue = 3;
            spell.MissileSpellName = "KarmaQMissileMantra";
            spell.DisplayName = "Inner Flame (Mantra Q)";
            spell.MissileSpeed = 1700.0f;
            spell.Radius = 80.0f;
            spell.Range = 950.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "KarmaQHeavy";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Tạo thêm vùng nổ chậm làm chậm sâu hơn
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            spell.HasEndExplosion = true;
            spell.SecondaryRadius = 280.0f;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Karthus;
            spell.DangerValue = 1;
            spell.DisplayName = "Lay Waste (Q)";
            spell.Radius = 160.0f;
            spell.Range = 875.0f;
            spell.Delay = 625;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "KarthusLayWaste";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Kassadin;
            spell.DangerValue = 2;
            spell.DisplayName = "Force Pulse (E)";
            spell.Range = 600.0f;
            spell.MultipleAngle = 80.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "ForcePulse";
            spell.Type = SkillShotType::SkillshotCone;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Kayle;
            spell.DangerValue = 1;
            spell.MissileSpellName = "KayleQMis";
            spell.DisplayName = "Radiant Blast (Q)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 75.0f;
            spell.Range = 900.0f;
            spell.Delay = 264;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "KayleQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = {
                CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::EnemyYasuoWall,
            };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.SecondaryRadius = 100.0f;
            spell.EndExplosionCross = true;
            spell.EndExplosionCenterOffset = 100.0f;
            spell.EndExplosionForwardLength = 400.0f;
            spell.EndExplosionBackwardLength = 100.0f;
            spell.EndExplosionSideLength = 150.0f;
            spell.EndExplosionLongitudinalRadius = 45.0f;
            spell.EndExplosionSideRadius = 62.5f;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Kayn;
            spell.DangerValue = 2;
            spell.DisplayName = "Blade's Reach (W - Thường)";
            spell.Radius = 90.0f;
            spell.Range = 700.0f;
            spell.Delay = 550;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "KaynW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Kayn;
            spell.DangerValue = 3;
            spell.DisplayName = "Blade's Reach (W - Darkin)";
            spell.Radius = 90.0f;
            spell.Range = 700.0f;
            spell.Delay = 550;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "KaynAssW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Dạng Darkin (Đỏ) hất tung mục tiêu cực mạnh
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Kennen;
            spell.DangerValue = 1;
            spell.MissileSpellName = "KennenShurikenHurlMissile";
            spell.DisplayName = "Thundering Shuriken (Q)";
            spell.MissileSpeed = 1700.0f;
            spell.Radius = 50.0f;
            spell.Range = 1050.0f;
            spell.Delay = 175;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "KennenShurikenHurlMissile";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None; // Tích đủ 3 dấu ấn mới nổ Stun
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::KhaZix;
            spell.DangerValue = 1;
            spell.MissileSpellName = "KhazixWMissile";
            spell.DisplayName = "Void Spike (W)";
            spell.MissileSpeed = 1700.0f;
            spell.Radius = 70.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "KhazixW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.SecondaryRadius = 275.0f;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Kled;
            spell.DangerValue = 2;
            spell.MissileSpellName = "KledQMissile";
            spell.DisplayName = "Beartrap on a Rope (Q)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 45.0f;
            spell.Range = 800.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "KledQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Dính xích bị chậm, sau đó bị kéo lùi dính vết thương sâu
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyLargeMonsters,
                CollisionObjectType::EnemyYasuoWall };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::KogMaw;
            spell.DangerValue = 1;
            spell.MissileSpellName = "KogMawQVMissile";
            spell.DisplayName = "Caustic Spittle (Q)";
            spell.MissileSpeed = 1650.0f;
            spell.Radius = 70.0f;
            spell.Range = 1175.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "KogMawQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::KogMaw;
            spell.DangerValue = 2;
            spell.MissileSpellName = "KogMawVoidOozeMissile";
            spell.DisplayName = "Void Ooze (E)";
            spell.MissileSpeed = 1400.0f;
            spell.Radius = 120.0f;
            spell.Range = 1360.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "KogMawVoidOoze";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Thảm bùn làm chậm liên tục khi đứng trên đó
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::KogMaw;
            spell.DangerValue = 2;
            spell.DisplayName = "Living Artillery (R)";
            spell.Radius = 240.0f;
            spell.Range = 1300.0f; // Lên tới 1800 dựa theo cấp chiêu cuối
            spell.Delay = 600; // Pháo sinh học dội từ trên trời xuống
            spell.Slot = SpellSlot::R;
            spell.SpellName = "KogMawLivingArtillery";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::KSante;
            spell.DangerValue = 2;
            spell.DisplayName = "Ntofo Strikes (Q3)";
            spell.MissileSpeed = 1400.0f;
            spell.Radius = 100.0f;
            spell.Range = 475.0f;
            spell.Delay = 400; // Tốc độ ra đòn tỉ lệ nghịch với lượng máu cộng thêm
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "KSanteQ3";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Q3 tụ đủ stack phóng sóng năng lượng kéo hất tung mục tiêu về sau
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI L
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Leblanc;
            spell.DangerValue = 2;
            spell.MissileSpellName = "LeblancEMissile";
            spell.DisplayName = "Ethereal Chains (E)";
            spell.MissileSpeed = 1750.0f;
            spell.Radius = 55.0f;
            spell.Range = 950.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "LeblancE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare; // Trói chân sau khi giữ xích đủ thời gian
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Leblanc;
            spell.DangerValue = 3;
            spell.MissileSpellName = "LeblancREMissile";
            spell.DisplayName = "Ethereal Chains (Mô Phỏng RE)";
            spell.MissileSpeed = 1750.0f;
            spell.Radius = 55.0f;
            spell.Range = 950.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "LeblancRE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::LeeSin;
            spell.DangerValue = 2;
            spell.MissileSpellName = "BlindMonkQOne";
            spell.DisplayName = "Sonic Wave (Q1)";
            spell.MissileSpeed = 1800.0f;
            spell.Radius = 60.0f;
            spell.Range = 1200.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "BlindMonkQOne";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Leona;
            spell.DangerValue = 2;
            spell.MissileSpellName = "LeonaZenithBladeMissile";
            spell.DisplayName = "Zenith Blade (E)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 70.0f;
            spell.Range = 900.0f;
            spell.Delay = 200;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "LeonaZenithBlade";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare; // Trói nhẹ tích tắc mục tiêu cuối cùng dính chiêu để Leona bay vào
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Leona;
            spell.DangerValue = 4;
            spell.DisplayName = "Solar Flare (R)";
            spell.Radius = 320.0f; // Toàn vùng ảnh hưởng, trung tâm choáng rìa làm chậm
            spell.Range = 1200.0f;
            spell.Delay = 625;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "LeonaSolarFlare";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Stun; // Chiêu cuối nổ choáng diện rộng
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Lillia;
            spell.DangerValue = 2;
            spell.MissileSpellName = "LilliaE";
            spell.DisplayName = "Swirlseed (E Initial Lob)";
            spell.MissileSpeed = 1400.0f;
            spell.Radius = 150.0f;
            spell.Range = 700.0f;
            spell.Delay = 400;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "LilliaE";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.UseEndPosition = true;
            spell.DetectionGroup = "LilliaE";
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Lillia;
            spell.DangerValue = 3;
            spell.MissileSpellName = "LilliaERollingMissile";
            spell.DisplayName = "Swirlseed (E Rolling)";
            spell.MissileSpeed = 1150.0f;
            spell.Radius = 60.0f;
            spell.Range = 25000.0f;
            spell.Delay = 0;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "LilliaERollingMissile";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = {
                CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::Terrain,
                CollisionObjectType::EnemyYasuoWall,
            };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresCollision = true;
            spell.EndExplosionOnProjectileWall = true;
            spell.SecondaryRadius = 150.0f;
            spell.DetectionGroup = "LilliaE";
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Lissandra;
            spell.DangerValue = 1;
            spell.MissileSpellName = "LissandraQMissile";
            spell.DisplayName = "Ice Shard (Q)";
            spell.MissileSpeed = 2200.0f;
            spell.Radius = 75.0f;
            spell.Range = 725.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "LissandraQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::EnemyYasuoWall };
            spell.CollisionInitialRange = 725.0f;
            spell.CollisionContinuationRange = 950.0f;
            spell.CollisionContinuationRadius = 90.0f;
            spell.FixedRange = true;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Lissandra;
            spell.DangerValue = 2;
            spell.MissileSpellName = "LissandraEMissile";
            spell.DisplayName = "Glacial Path (E)";
            spell.MissileSpeed = 850.0f;
            spell.Radius = 125.0f;
            spell.Range = 1050.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "LissandraE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===
	// ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Locke;
            spell.DangerValue = 2;
            spell.MissileSpellName = "LockeQNailMissile"; // Tên missile đinh hồn bay ra
            spell.DisplayName = "Ritual Nails (Q)";
            spell.MissileSpeed = 1800.0f; // Tốc độ phóng đinh tương đối nhanh
            spell.Radius = 60.0f;
            spell.Range = 950.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "LockeQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Làm chậm 25% ở nhịp đầu, tăng mạnh lên 60% nếu dính 2-3 stack đinh
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Locke;
            spell.DangerValue = 5; // Độ nguy hiểm tối đa do cơ chế Execute (kết liễu) lập tức dưới ngưỡng máu
            spell.MissileSpellName = "LockeRArtifact";
            spell.DisplayName = "Purgatory (R)";
            spell.MissileSpeed = 1500.0f; // Tốc độ quăng cổ vật tế đàn ra vị trí chỉ định
            spell.Radius = 350.0f; // Vùng tròn ảnh hưởng lan rộng của xích đinh
            spell.Range = 1000.0f;
            spell.Delay = 750; // 0.5 giây bay + 0.25 giây trễ kích hoạt dựng tế đàn bẫy xích
            spell.Slot = SpellSlot::R;
            spell.SpellName = "LockeR";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Slow; // Làm chậm cực mạnh đến 99% (giảm dần) giam chân trong bẫy
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Lucian;
            spell.DangerValue = 2;
            spell.DisplayName = "Piercing Light (Q)";
            spell.Radius = 65.0f;
            spell.Range = 500.0f; // Khóa mục tiêu ở 500 nhưng tia đạn xuyên thấu đạt tới 900
            spell.Delay = 350; // Tốc độ thi triển giảm theo cấp độ tướng
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "LucianQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.IsSpecial = true; // Cần tính toán hướng bắn dựa trên mục tiêu chỉ định (Targeted-to-Line)
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Lucian;
            spell.DangerValue = 1;
            spell.MissileSpellName = "LucianWMissile";
            spell.DisplayName = "Ardent Blaze (W)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 55.0f;
            spell.Range = 900.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "LucianW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Lulu;
            spell.DangerValue = 1;
            spell.MissileSpellName = "LuluQMissile";
            spell.DisplayName = "Glitterlance (Q)";
            spell.MissileSpeed = 1450.0f;
            spell.Radius = 60.0f;
            spell.Range = 925.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "LuluQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Làm chậm 80% cực mạnh giảm dần
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Lux;
            spell.DangerValue = 3;
            // Track the functional missile. The dummy VFX missile is destroyed
            // on the first target and must never truncate the real two-hit Q.
            spell.MissileSpellName = "LuxLightBindingMis";
            spell.DisplayName = "Light Binding (Q)";
            spell.MissileSpeed = 1200.0f;
            spell.Radius = 70.0f;
            spell.Range = 1300.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "LuxLightBinding";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::EnemyYasuoWall };
            spell.CollisionTargetLimit = 2;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Lux;
            spell.DangerValue = 2;
            spell.MissileSpellName = "LuxLightStrikeKugel";
            spell.DisplayName = "Lucent Singularity (E)";
            spell.MissileSpeed = 1200.0f;
            spell.Radius = 310.0f;
            spell.Range = 1100.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "LuxLightStrikeKugel";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Slow; // Vùng làm chậm trước khi kích nổ tái kích hoạt
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Lux;
            spell.DangerValue = 4;
            spell.DisplayName = "Final Spark (R)";
            spell.Radius = 100.0f;
            spell.Range = 3400.0f;
            spell.Delay = 1000; // Vận sức tia laze dọc bản đồ trong 1 giây
            spell.Slot = SpellSlot::R;
            spell.SpellName = "LuxMaliceInhabitant";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===
// ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI M
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Malphite;
            spell.DangerValue = 5; // Độ nguy hiểm tối đa vì đây là chiêu mở giao tranh cực mạnh
            spell.MissileSpellName = "MalphiteR";
            spell.DisplayName = "Unstoppable Force (R)";
            spell.MissileSpeed = 1835.0f; // Tốc độ bay của Malphite khi húc vào
            spell.Radius = 270.0f; // CDragon latest: UFSlash castRadius = 270.0
            spell.Range = 1000.0f;
            spell.Delay = 0; // Kích hoạt gần như tức thì, phụ thuộc vào khoảng cách bay
            spell.Slot = SpellSlot::R;
            spell.SpellName = "MalphiteR";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::KnockUp; // Hất tung không thể cản phá
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Malzahar;
            spell.DangerValue = 2;
            spell.DisplayName = "Call of the Void (Q)";
            spell.Radius = 85.0f; // Độ rộng của hai cổng không gian nổ song song
            spell.Range = 900.0f;
            spell.Delay = 650; // Trễ từ lúc đặt cổng đến lúc nổ tia năng lượng
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "MalzaharQ";
            spell.Type = SkillShotType::SkillshotLine; // Xử lý toán học như một đường ngang vuông góc hướng đứng
            spell.CrowdControl = CrowdControlType::Silence; // Câm lặng diện rộng
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Maokai;
            spell.DangerValue = 2;
            spell.MissileSpellName = "MaokaiQMissile";
            spell.DisplayName = "Bramble Smash (Q)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 110.0f;
            spell.Range = 600.0f;
            spell.Delay = 375;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "MaokaiQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockBack; // Đẩy lùi kẻ địch ở gần và làm chậm kẻ địch ở xa
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Maokai;
            spell.DangerValue = 4;
            spell.DisplayName = "Nature's Grasp (R)";
            spell.MissileSpeed = 500.0f; // Bắt đầu chậm (50f) tăng tiến dần lên 850f theo khoảng cách bay
            spell.Radius = 240.0f; // Bán kính mỗi nhánh rễ cây trong bức tường rễ
            spell.Range = 3000.0f;
            spell.Delay = 500;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "MaokaiR";
            spell.Type = SkillShotType::SkillshotLine; // Xử lý hệ thống di chuyển theo cụm song song hướng thẳng
            spell.CrowdControl = CrowdControlType::Snare; // Trói chân thời gian tăng dần theo quãng đường rễ cây di chuyển
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Milio;
            spell.DangerValue = 2;
            spell.MissileSpellName = "MilioQMissile";
            spell.DisplayName = "Ultra Mega Firekick (Q)";
            spell.MissileSpeed = 1200.0f;
            spell.Radius = 60.0f;
            spell.Range = 1200.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "MilioQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockBack; // Đẩy lùi mục tiêu đầu tiên và làm chậm vùng nổ phía sau
            spell.CollisionObjects = {
                CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::EnemyYasuoWall,
            };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.SecondaryRadius = 250.0f;
            spell.EndExplosionDelay = 800;
            spell.CollisionBounceDistance = 140.0f;
            spell.CollisionBounceDistanceNonChampion = 340.0f;
            spell.EndExplosionRadiusNonChampion = 275.0f;
            spell.EndExplosionDelayNonChampion = 900;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Morgana;
            spell.DangerValue = 4; // Nguy hiểm cao do thời gian trói chân kỷ lục (tối đa 3 giây)
            spell.MissileSpellName = "DarkBindingMissile";
            spell.DisplayName = "Dark Binding (Q)";
            spell.MissileSpeed = 1200.0f; // Bay tương đối chậm, dễ né nếu ở tầm xa
            spell.Radius = 70.0f;
            spell.Range = 1300.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "DarkBindingMissile";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // Mudinho (Dr. Mundo) — REMOVED: Duplicate of // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI N
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Nami;
            spell.DangerValue = 3;
            spell.DisplayName = "Aqua Prison (Q)";
            spell.Radius = 162.5f;
            spell.Range = 875.0f;
            spell.Delay = 950; // Trễ cố định (0.95 giây bóng nước rơi xuống)
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "NamiQ";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::KnockUp; // Bong bóng giam cầm (thực tế tính là hất tung/choáng treo)
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Nami;
            spell.DangerValue = 4;
            spell.MissileSpellName = "NamiRMissile";
            spell.DisplayName = "Tidal Wave (R)";
            spell.MissileSpeed = 850.0f; // Sóng thần bay chậm nhưng độ phủ cực lớn
            spell.Radius = 375.0f; // Nửa độ rộng của ngọn sóng
            spell.Range = 2750.0f;
            spell.Delay = 500;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "NamiR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Hất nhẹ và làm chậm sâu dựa trên khoảng cách di chuyển
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Nautilus;
            spell.DangerValue = 4;
            spell.MissileSpellName = "NautilusAnchorDragMissile";
            spell.DisplayName = "Dredge Line (Q)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 90.0f; // Bản kích mỏ neo khá lớn, dễ va chạm địa hình
            spell.Range = 1150.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "NautilusAnchorDrag";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Kéo mục tiêu và Nautilus lại gần nhau kèm choáng nhẹ
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::Terrain,
                CollisionObjectType::EnemyYasuoWall }; // Va chạm mục tiêu hoặc tường
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Neeko;
            spell.DangerValue = 2;
            spell.MissileSpellName = "NeekoQMissile";
            spell.DisplayName = "Blooming Burst (Q)";
            spell.MissileSpeed = 1500.0f;
            spell.Radius = 225.0f;
            spell.Range = 800.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "NeekoQ";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::None; // Nổ 3 lần nếu hạ gục mục tiêu hoặc trúng tướng
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Neeko;
            spell.DangerValue = 3;
            spell.MissileSpellName = "NeekoEMissile";
            spell.DisplayName = "Tangle-Barbs (E)";
            spell.MissileSpeed = 1300.0f; // Tăng lên 1400 sau khi đi xuyên qua lính
            spell.Radius = 70.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "NeekoE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare; // Trói chân lâu hơn và nhanh hơn nếu xuyên qua ít nhất 1 mục tiêu
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Nidalee;
            spell.DangerValue = 3; // Sát thương cực cao nếu trúng từ xa
            spell.MissileSpellName = "JavelinToss";
            spell.DisplayName = "Javelin Toss (Q Người)";
            spell.MissileSpeed = 1300.0f;
            spell.Radius = 40.0f; // Hitbox ngọn giáo rất nhỏ
            spell.Range = 1500.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "NidaleeJavelInToss";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Nilah;
            spell.DangerValue = 1;
            spell.DisplayName = "Formless Blade (Q)";
            spell.Radius = 75.0f;
            spell.Range = 600.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "NilahQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Nocturne;
            spell.DangerValue = 2;
            spell.MissileSpellName = "NocturneDuskbringerAura";
            spell.DisplayName = "Duskbringer (Q)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 60.0f;
            spell.Range = 1200.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "NocturneDuskbringer";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::None; // Chỉ tạo vệt bóng tối gia tăng tốc độ di chuyển
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI O
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Olaf;
            spell.DangerValue = 2;
            spell.MissileSpellName = "OlafAxeThrowCast";
            spell.DisplayName = "Undertow (Q)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 90.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "OlafAxeThrowCast";
            spell.Type = SkillShotType::SkillshotLine; // Thực chất ném chỉ định tọa độ điểm rơi trên đoạn thẳng
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ornn;
            spell.DangerValue = 4;
            spell.DisplayName = "Searing Charge (E)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 175.0f;
            spell.Range = 650.0f;
            spell.Delay = 350;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "OrnnE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp;
            spell.IsDangerous = true;
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Orianna;
            spell.DangerValue = 2;
            spell.MissileSpellName = "TheBall"; // Quả cầu nội tại di chuyển
            spell.DisplayName = "Command: Attack (Q)";
            spell.MissileSpeed = 1400.0f;
            spell.Radius = 80.0f; // Bán kính trúng chiêu trên đường bay
            spell.Range = 825.0f;
            spell.Delay = 0; // Bay ngay lập tức từ vị trí hiện tại của khối cầu
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "OrianaIzuna";
            spell.Type = SkillShotType::SkillshotCircle; // Bay đến đích và nằm cố định ở đó
            spell.CrowdControl = CrowdControlType::None;
            spell.IsSpecial = true; // Logic cần update điểm StartPos bằng tọa độ thực tế của quả cầu (không phải vị trí Orianna)
            Entries.push_back(spell);
        }
        // ===

        // ==========================================
        // KHU VỰC CÁC CHAMPION CHỮ CÁI P
        // ==========================================

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Pantheon;
            spell.DangerValue = 2;
            spell.MissileSpellName = "PantheonQMissile";
            spell.DisplayName = "Comet Spear (Q Phóng)";
            spell.MissileSpeed = 2700.0f; // Phóng giáo đi rất nhanh
            spell.Radius = 60.0f;
            spell.Range = 1200.0f;
            spell.Delay = 250; // Delay vận sức nhấp thả chiêu
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "PantheonQCast";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Làm chậm nếu trúng khi Pantheon cường hóa nội tại chiến ý
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Poppy;
            spell.DangerValue = 2;
            spell.DisplayName = "Hammer Shock (Q)";
            spell.Radius = 100.0f;
            spell.Range = 430.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "PoppyQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Làm chậm vùng đất nứt 2 nhịp nổ
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Poppy;
            spell.DangerValue = 4;
            spell.MissileSpellName = "PoppyRMissile";
            spell.DisplayName = "Keeper's Verdict (R Vận Sức)";
            spell.MissileSpeed = 2500.0f;
            spell.Radius = 90.0f;
            spell.Range = 1200.0f;
            spell.Delay = 350;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "PoppyRSpell";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp; // Đập búa hất văng mục tiêu bay thẳng về tế đàn chính của họ
            // The charged shockwave stops at the first champion and raises a
            // 225-radius hammer eruption centered on that target.
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyYasuoWall };
            spell.HasEndExplosion = true;
            spell.SecondaryRadius = 225.0f;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.EndExplosionAtUnitCenter = true;
            Entries.push_back(spell);
        }
        // ===

        // ===
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Pyke;
            spell.DangerValue = 3;
            spell.MissileSpellName = "PykeQMissile";
            spell.DisplayName = "Bone Skewer (Q Kéo)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 70.0f;
            spell.Range = 1100.0f;
            spell.Delay = 200; // Tùy thuộc thời gian giữ phím gồng xích
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "PykeQCast";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockBack; // Đâm/Kéo mục tiêu một khoảng cố định ra sau lưng Pyke
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Pyke;
            spell.DangerValue = 3;
            spell.DisplayName = "Phantom Undertow (E)";
            spell.Radius = 110.0f;
            spell.Range = 550.0f; // Khoảng cách lướt của Pyke, bóng ma chạy theo sau có tầm xa linh hoạt
            spell.Delay = 1000; // Thời gian bóng ma trễ trước khi bay về phía Pyke để gây choáng
            spell.Slot = SpellSlot::E;
            spell.SpellName = "PykeE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun;
            spell.IsSpecial = true; // Yêu cầu tracking vị trí phân thân (Ghost) lướt theo bóng chính
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Pyke;
            spell.DangerValue = 5; // Chiêu cuối hồi lại ngay lập tức và chia tiền, mức cảnh báo cao nhất
            spell.DisplayName = "Death From Below (R)";
            spell.Radius = 160.0f; // Bán kính vùng tâm chữ X kết liễu
            spell.Range = 750.0f;
            spell.Delay = 750; // Trễ hoạt ảnh từ lúc lặn xuống đến lúc chém nổ chữ X
            spell.Slot = SpellSlot::R;
            spell.SpellName = "PykeR";
            spell.Type = SkillShotType::SkillshotCircle; // Được vẽ logic theo vùng đa giác hoặc hình tròn bao quanh tâm X
            spell.CrowdControl = CrowdControlType::None; // Nếu thấp máu hơn ngưỡng chết sẽ bị Execute bay màu lập tức
            Entries.push_back(spell);
        }
        // ===
// ===
        // ==========================================
        // CHỮ Q
        // ==========================================
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Quinn;
            spell.DangerValue = 2;
            spell.MissileSpellName = "QuinnQMissile";
            spell.DisplayName = "Blinding Assault (Q)";
            spell.MissileSpeed = 1550.0f;
            spell.Radius = 60.0f;
            spell.Range = 1025.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "QuinnQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Blind; // Cơ chế giảm tầm nhìn đặc biệt
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // CHỮ R
        // ==========================================
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Rakan;
            spell.DangerValue = 3;
            spell.DisplayName = "Grand Entrance (W)";
            spell.Radius = 250.0f;
            spell.Range = 600.0f;
            spell.Delay = 750; // Trễ nhảy + hất tung
            spell.Slot = SpellSlot::W;
            spell.SpellName = "RakanW";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::KnockUp;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Rell;
            spell.DangerValue = 3;
            spell.DisplayName = "Shatterstrike (Q)";
            spell.Radius = 90.0f;
            spell.Range = 685.0f;
            spell.Delay = 400;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "RellQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Choáng nhịp đâm vũ khí
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Rengar;
            spell.DangerValue = 3;
            spell.MissileSpellName = "RengarEFinal";
            spell.DisplayName = "Bola Strike (E)";
            spell.MissileSpeed = 1500.0f;
            spell.Radius = 70.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "RengarE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Hoặc Trói (Snare) nếu có 4 điểm Hung Tợn
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Riven;
            spell.DangerValue = 4;
            spell.MissileSpellName = "RivenWindSlashMissile";
            spell.DisplayName = "Wind Slash (R2)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 125.0f; // Hình nón xòe rộng nhưng tính theo góc/bán kính missile đầu
            spell.Range = 900.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "RivenIzunaBlade";
            spell.Type = SkillShotType::SkillshotLine; // Xử lý né dạng đường thẳng góc rộng
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Rumble;
            spell.DangerValue = 3;
            spell.MissileSpellName = "RumbleGrenadeMissile";
            spell.DisplayName = "Electro Harpoon (E)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 60.0f;
            spell.Range = 950.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "RumbleGrenade";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ryze;
            spell.DangerValue = 2;
            spell.MissileSpellName = "RyzeQW";
            spell.DisplayName = "Overload (Q)";
            spell.MissileSpeed = 1700.0f;
            spell.Radius = 55.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "RyzeQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // CHỮ S
        // ==========================================
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Samira;
            spell.DangerValue = 2;
            spell.MissileSpellName = "SamiraQGun";
            spell.DisplayName = "Flair (Q Shot / E-Q Explosives)";
            spell.MissileSpeed = 2600.0f;
            spell.Radius = 60.0f;
            spell.Range = 950.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "SamiraQGun";
            spell.ExtraSpellNames = { "SamiraQ" };
            spell.Type = SkillShotType::SkillshotLine;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::EnemyYasuoWall };
            spell.DetectionGroup = "SamiraQ";
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Samira;
            spell.DangerValue = 2;
            spell.DisplayName = "Flair (Q Sword Cone)";
            spell.Radius = 65.0f;
            spell.Range = 400.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "SamiraQSword";
            spell.ExtraSpellNames = { "SamiraQBufferedSword" };
            spell.Type = SkillShotType::SkillshotCone;
            spell.MultipleAngle = 50.0f;
            spell.DetectionGroup = "SamiraQ";
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Samira;
            spell.DangerValue = 2;
            spell.DisplayName = "Wild Rush (E Path)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 150.0f;
            spell.Range = 650.0f;
            spell.Delay = 0;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "SamiraE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Sejuani;
            spell.DangerValue = 4;
            spell.MissileSpellName = "SejuaniRMissile";
            spell.DisplayName = "Glacial Prison (R)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 120.0f;
            spell.Range = 1300.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "SejuaniR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions }; // Chỉ cản bởi tướng
            spell.FixedRange = true;
            spell.HasEndExplosion = true;
            spell.EndExplosionAtUnitCenter = true;
            spell.EndExplosionMinimumTravelDistance = 400.0f;
            spell.EndExplosionDuration = 1500;
            spell.SecondaryRadius = 400.0f;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Senna;
            spell.DangerValue = 3;
            spell.MissileSpellName = "SennaWMissile";
            spell.DisplayName = "Last Embrace (W)";
            spell.MissileSpeed = 1200.0f;
            spell.Radius = 60.0f;
            spell.Range = 1300.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "SennaW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            spell.HasEndExplosion = true;
            spell.EndExplosionRequiresUnitCollision = true;
            spell.EndExplosionAtUnitCenter = true;
            spell.EndExplosionFollowsUnit = true;
            spell.EndExplosionDetonatesOnUnitDeath = true;
            spell.EndExplosionDelay = 1000;
            spell.SecondaryRadius = 280.0f;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Seraphine;
            spell.DangerValue = 3;
            spell.MissileSpellName = "SeraphineEIsotopeMissile";
            spell.DisplayName = "Beat Drop (E)";
            spell.MissileSpeed = 1200.0f;
            spell.Radius = 70.0f;
            spell.Range = 1300.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "SeraphineE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Có thể nâng cấp thành Choáng hoặc Trói dựa trên nội tại
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Seraphine;
            spell.DangerValue = 5;
            spell.MissileSpellName = "SeraphineRMissile";
            spell.DisplayName = "Encore (R)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 160.0f; // Bán kính siêu rộng
            spell.Range = 2000.0f; // Tầm có thể kéo dài thêm khi trúng mục tiêu
            spell.Delay = 500;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "SeraphineR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Charm;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Shaco;
            spell.DangerValue = 2;
            spell.DisplayName = "Jack In The Box (W)";
            spell.Radius = 175.0f;
            spell.Range = 425.0f;
            spell.Delay = 2000; // Trễ tàng hình trước khi kích hoạt hoảng sợ
            spell.Slot = SpellSlot::W;
            spell.SpellName = "ShacoBox";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Fear;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Shen;
            spell.DangerValue = 3;
            spell.DisplayName = "Shadow Dash (E)";
            spell.MissileSpeed = 1200.0f; // Tốc độ lướt của Shen
            spell.Radius = 60.0f;
            spell.Range = 600.0f;
            spell.Delay = 0;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "ShenE";
            spell.Type = SkillShotType::SkillshotLine; // Xử lý vùng quét dạng Line khi thực hiện lướt
            spell.CrowdControl = CrowdControlType::Taunt;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Shyvana;
            spell.DangerValue = 3;
            spell.MissileSpellName = "ShyvanaTransformLeapNoData"; // Dạng rồng quăng E lửa lớn
            spell.DisplayName = "Flame Breath (E - Dragon Form)";
            spell.MissileSpeed = 1575.0f;
            spell.Radius = 115.0f;
            spell.Range = 975.0f;
            spell.Delay = 333;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "ShyvanaFireballDragonZoneCheck";
            spell.Type = SkillShotType::SkillshotLine;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Sion;
            spell.DangerValue = 2;
            spell.MissileSpellName = "SionEMissile";
            spell.DisplayName = "Roar of the Slayer (E)";
            spell.MissileSpeed = 2000.0f;
            spell.Radius = 80.0f;
            spell.Range = 800.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "SionE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Sivir;
            spell.DangerValue = 2;
            spell.MissileSpellName = "SivirQMissile";
            spell.DisplayName = "Boomerang Blade (Q)";
            spell.MissileSpeed = 1450.0f;
            spell.Radius = 90.0f;
            spell.Range = 1250.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "SivirQ";
            spell.Type = SkillShotType::SkillshotLine; // Cần tính toán cả đường Boomerang bay về
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Skarner;
            spell.DangerValue = 3;
            spell.MissileSpellName = "SkarnerEIsotopeMissile";
            spell.DisplayName = "Ixtal's Impact (E)";
            spell.MissileSpeed = 1200.0f;
            spell.Radius = 70.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "SkarnerE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Sona;
            spell.DangerValue = 5;
            spell.MissileSpellName = "SonaRMissile";
            spell.DisplayName = "Crescendo (R)";
            spell.MissileSpeed = 2400.0f;
            spell.Radius = 140.0f;
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "SonaR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Soraka;
            spell.DangerValue = 2;
            spell.MissileSpellName = "SorakaQMissile";
            spell.DisplayName = "Starcall (Q)";
            spell.Radius = 235.0f;
            spell.Range = 800.0f;
            spell.Delay = 300; // Tốc độ rơi phụ thuộc khoảng cách, trễ tối thiểu khoảng 300ms
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "SorakaQ";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Soraka;
            spell.DangerValue = 3;
            spell.DisplayName = "Equinox (E)";
            spell.Radius = 250.0f;
            spell.Range = 925.0f;
            spell.Delay = 1500; // Đứng yên 1.5 giây trong vùng sẽ bị trói
            spell.Slot = SpellSlot::E;
            spell.SpellName = "SorakaE";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Silence; // Im lặng tức thì khi dẫm vào vùng
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Swain;
            spell.DangerValue = 3;
            spell.MissileSpellName = "SwainEMissile";
            spell.DisplayName = "Nevermove (E)";
            spell.MissileSpeed = 935.0f; // Bay đi chậm nhưng bay về nhanh hơn (~1400)
            spell.Radius = 85.0f;
            spell.Range = 850.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "SwainE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare; // Trói khi quay về dính mục tiêu
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Sylas;
            spell.DangerValue = 3;
            spell.MissileSpellName = "SylasEMissile";
            spell.DisplayName = "Abduct (E2)";
            spell.MissileSpeed = 1600.0f;
            spell.Radius = 60.0f;
            spell.Range = 800.0f;
            spell.Delay = 0;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "SylasE2";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::EnemyYasuoWall };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Syndra;
            spell.DangerValue = 2;
            spell.DisplayName = "Dark Sphere (Q)";
            spell.Radius = 180.0f;
            spell.Range = 800.0f;
            spell.Delay = 600;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "SyndraQ";
            spell.Type = SkillShotType::SkillshotCircle;
            Entries.push_back(spell);
        }

        // ==========================================
        // CHỮ T
        // ==========================================
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::TahmKench;
            spell.DangerValue = 2;
            spell.MissileSpellName = "TahmKenchQMissile";
            spell.DisplayName = "Tongue Lash (Q)";
            spell.MissileSpeed = 2800.0f; // Roi lưỡi cực nhanh
            spell.Radius = 70.0f;
            spell.Range = 900.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "TahmKenchQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow; // Choáng nếu đủ 3 stack nội tại Khẩu Vị Độc Đáo
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Taliyah;
            spell.DangerValue = 3;
            spell.DisplayName = "Seismic Shove (W)";
            spell.Radius = 150.0f;
            spell.Range = 900.0f;
            spell.Delay = 850;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "TaliyahW";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::KnockUp; // Đẩy hất theo hướng chỉ định
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Talon;
            spell.DangerValue = 2;
            spell.MissileSpellName = "TalonWBlades";
            spell.DisplayName = "Rake (W)";
            spell.MissileSpeed = 1850.0f;
            spell.Radius = 75.0f; // Đoạn đầu của dải quạt hình nón
            spell.Range = 900.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "TalonW";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Teemo;
            spell.DangerValue = 3;
            spell.MissileSpellName = "TeemoRShroom"; // Trường hợp Teemo quăng nấm bay xa nâng cấp cấp R
            spell.DisplayName = "Noxious Trap (R)";
            spell.MissileSpeed = 1000.0f;
            spell.Radius = 120.0f; // Bán kính nổ kích hoạt khi dẫm phải
            spell.Range = 400.0f; // Tăng dần 400/650/900 theo cấp chiêu cuối
            spell.Delay = 1000; // Trễ kích hoạt tàng hình của nấm sau khi đáp xuống
            spell.Slot = SpellSlot::R;
            spell.SpellName = "TeemoR";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Thresh;
            spell.DangerValue = 4;
            spell.MissileSpellName = "ThreshQMissile";
            spell.DisplayName = "Death Sentence (Q)";
            spell.MissileSpeed = 1900.0f;
            spell.Radius = 70.0f;
            spell.Range = 1100.0f;
            spell.Delay = 500; // Hoạt ảnh quay xích mất 0.5s trước khi phóng
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "ThreshQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Stun; // Kéo giật lùi mục tiêu
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Tristana;
            spell.DangerValue = 2;
            spell.DisplayName = "Rocket Jump (W)";
            spell.MissileSpeed = 1100.0f;
            spell.Radius = 270.0f; // Vùng đáp xuống gây sát thương và làm chậm
            spell.Range = 900.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::W;
            spell.SpellName = "TristanaW";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Trundle;
            spell.DangerValue = 3;
            spell.DisplayName = "Pillar of Ice (E)";
            spell.Radius = 130.0f; // Kích thước vật thể cột băng cản địa hình
            spell.Range = 1000.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "TrundlePillar";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::KnockUp; // Hất nhẹ khi mọc lên + Làm chậm diện rộng xung quanh
            Entries.push_back(spell);
        }
        // ===
// ===
        // ==========================================
        // CHỮ U - V
        // ==========================================
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Urgot;
            spell.DangerValue = 3;
            spell.MissileSpellName = "UrgotQMissile";
            spell.DisplayName = "Corrosive Charge (Q)";
            spell.Radius = 210.0f;
            spell.Range = 800.0f;
            spell.Delay = 600; // Trễ lựu đạn nổ
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "UrgotQ";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Slow;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Urgot;
            spell.DangerValue = 5; // Độ nguy hiểm tối đa (Kết liễu mục tiêu thấp máu)
            spell.MissileSpellName = "UrgotRMissile";
            spell.DisplayName = "Fear Beyond Death (R)";
            spell.MissileSpeed = 3200.0f; // Mũi khoan bay cực nhanh
            spell.Radius = 80.0f;
            spell.Range = 2500.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "UrgotR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Varus;
            spell.DangerValue = 2;
            spell.MissileSpellName = "VarusQMissile";
            spell.DisplayName = "Piercing Arrow (Q)";
            spell.MissileSpeed = 1900.0f;
            spell.Radius = 70.0f;
            spell.Range = 1625.0f; // Tầm bắn tối đa khi gồng đủ thời gian
            spell.Delay = 0; // Trễ tung chiêu phụ thuộc vào thời điểm người chơi thả phím Q
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "VarusQ";
            spell.Type = SkillShotType::SkillshotLine;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Varus;
            spell.DangerValue = 4;
            spell.MissileSpellName = "VarusRMissile";
            spell.DisplayName = "Chain of Corruption (R)";
            spell.MissileSpeed = 1500.0f;
            spell.Radius = 120.0f;
            spell.Range = 1300.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::R;
            spell.SpellName = "VarusR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions };
            Entries.push_back(spell);
        }
        // Vayne E (Condemn) — REMOVED: Targeted/point-and-click spell, cannot be dodged
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Veigar;
            spell.DangerValue = 2;
            spell.DisplayName = "Baleful Strike (Q)";
            spell.MissileSpeed = 2200.0f;
            spell.Radius = 70.0f;
            spell.Range = 950.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.MissileSpellName = "VeigarBalefulStrikeMis";
            spell.SpellName = "VeigarBalefulStrike";
            spell.ExtraSpellNames = { "VeigarQ" };
            spell.ExtraMissileNames = { "VeigarQMis", "VeigarQMissile" };
            spell.Type = SkillShotType::SkillshotLine;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions }; // Tối đa trúng 2 mục tiêu
            spell.CollisionTargetLimit = 2;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Veigar;
            spell.DangerValue = 3;
            spell.DisplayName = "Dark Matter (W)";
            spell.Radius = 240.0f;
            spell.Range = 900.0f;
            spell.Delay = 1250; // Thiên thạch rơi khá trễ, né rất dễ nếu không bị choáng trước
            spell.Slot = SpellSlot::W;
            spell.SpellName = "VeigarW";
            spell.Type = SkillShotType::SkillshotCircle;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Velkoz;
            spell.DangerValue = 3;
            spell.MissileSpellName = "VelkozQMissile";
            spell.DisplayName = "Plasma Fission (Q)";
            spell.MissileSpeed = 1300.0f;
            spell.Radius = 50.0f;
            spell.Range = 1050.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "VelkozQ";
            spell.Type = SkillShotType::SkillshotLine; // Cần logic nâng cao xử lý tách góc vuông 90 độ (Q2) khi tái kích hoạt hoặc hết tầm sinh ra 2 tia phụ
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Velkoz;
            spell.DangerValue = 3;
            spell.MissileSpellName = "VelkozQMissileSplit";
            spell.DisplayName = "Plasma Fission (Q Split)";
            spell.MissileSpeed = 2100.0f;
            spell.Radius = 45.0f;
            spell.Range = 1100.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "VelkozQSplit";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Slow;
            spell.CollisionObjects = {
                CollisionObjectType::EnemyChampions,
                CollisionObjectType::EnemyMinions,
                CollisionObjectType::EnemyYasuoWall,
            };
            spell.FixedRange = true;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Vex;
            spell.DangerValue = 2;
            spell.DisplayName = "Mistral Pace (Q)";
            spell.MissileSpeed = 1200.0f;
            spell.Radius = 180.0f;
            spell.Range = 1200.0f;
            spell.Delay = 150;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "VexQ";
            spell.Type = SkillShotType::SkillshotLine;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Vex;
            spell.DangerValue = 3;
            spell.MissileSpellName = "VexE";
            spell.DisplayName = "Mistral Bolt (E)";
            spell.MissileSpeed = 1500.0f;
            spell.Radius = 120.0f;
            spell.Range = 1100.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "VexE";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.CrowdControl = CrowdControlType::Snare;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Viktor;
            spell.DangerValue = 2;
            spell.DisplayName = "Death Ray (E)";
            spell.MissileSpeed = 1050.0f;
            spell.Radius = 80.0f;
            spell.Range = 540.0f; // Tầm đặt điểm đầu, tia laser quét thêm một đoạn dài 500
            spell.Delay = 0;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "ViktorE";
            spell.Type = SkillShotType::SkillshotLine; // Kỹ năng dạng vector, cần lấy điểm bắt đầu (StartPos) từ dữ liệu cast thực tế thay vì từ vị trí của Viktor
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Vladimir;
            spell.DangerValue = 2;
            spell.DisplayName = "Tides of Blood (E)";
            spell.MissileSpeed = 4000.0f; // Máu bắn ra xung quanh siêu tốc
            spell.Radius = 60.0f;
            spell.Range = 600.0f;
            spell.Delay = 0; // Kích hoạt khi Vladimir thả nút gồng
            spell.Slot = SpellSlot::E;
            spell.SpellName = "VladimirE";
            spell.Type = SkillShotType::SkillshotCircle; // Phân bổ vòng tròn quanh bản thân
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // CHỮ W - X - Y - Z
        // ==========================================
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Xayah;
            spell.DangerValue = 2;
            spell.MissileSpellName = "XayahQMissile1";
            spell.DisplayName = "Double Daggers (Q)";
            spell.MissileSpeed = 2075.0f;
            spell.Radius = 45.0f;
            spell.Range = 1100.0f;
            spell.Delay = 150;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "XayahQ";
            spell.Type = SkillShotType::SkillshotLine; // Engine cần lưu giữ vị trí các lông vũ rớt trên sàn để né chiêu Triệu Hồi Lông Vũ (E)
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Xerath;
            spell.DangerValue = 3;
            spell.DisplayName = "Arcanopulse (Q)";
            spell.Radius = 70.0f;
            spell.Range = 1450.0f;
            spell.Delay = 520; // Hoạt ảnh tụ năng lượng trước khi giật sét đường thẳng
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "XerathArcanopulseChargeUp";
            spell.Type = SkillShotType::SkillshotLine;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Xerath;
            spell.DangerValue = 3;
            spell.MissileSpellName = "XerathArcaneBarrageFive"; // Phát bắn chiêu cuối
            spell.DisplayName = "Rite of the Arcane (R)";
            spell.Radius = 200.0f;
            spell.Range = 5000.0f; // Tầm bắn siêu xa tăng theo cấp kỹ năng
            spell.Delay = 630; // Thời gian từ lúc bắn tới khi pháo kích chạm đất
            spell.Slot = SpellSlot::R;
            spell.SpellName = "XerathR";
            spell.Type = SkillShotType::SkillshotCircle;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Yasuo;
            spell.DangerValue = 3;
            spell.MissileSpellName = "YasuoQ3Mis"; // Lốc xoáy
            spell.DisplayName = "Steel Tempest (Q3)";
            spell.MissileSpeed = 1200.0f;
            spell.Radius = 90.0f;
            spell.Range = 1150.0f;
            spell.Delay = 333; // Giảm dần theo Tốc độ đánh (Attack Speed)
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "YasuoQ3W";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Yone;
            spell.DangerValue = 3;
            spell.MissileSpellName = "YoneQ3Mis"; // Lốc xoáy kèm lướt của Yone
            spell.DisplayName = "Mortal Steel (Q3)";
            spell.MissileSpeed = 1500.0f;
            spell.Radius = 80.0f;
            spell.Range = 1050.0f;
            spell.Delay = 350; // Giảm dần theo Tốc độ đánh
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "YoneQ3";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Yone;
            spell.DangerValue = 5;
            spell.DisplayName = "Fate Sealed (R)";
            spell.Radius = 112.5f; // Bề rộng của dải đường quét chiêu cuối
            spell.Range = 1000.0f;
            spell.Delay = 750; // 0.75 giây gồng báo đỏ trước khi chém hất tung toàn bộ mục tiêu về tâm
            spell.Slot = SpellSlot::R;
            spell.SpellName = "YoneR";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::KnockUp;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Zeri;
            spell.DangerValue = 1;
            spell.MissileSpellName = "ZeriQMis";
            spell.DisplayName = "Burst Fire (Q)";
            spell.MissileSpeed = 2600.0f;
            spell.Radius = 40.0f;
            spell.Range = 825.0f;
            spell.Delay = 0;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "ZeriQ";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ziggs;
            spell.DangerValue = 2;
            spell.MissileSpellName = "ZiggsQSpell";
            spell.DisplayName = "Bouncing Bomb (Q - First Landing)";
            spell.MissileSpeed = 1700.0f;
            spell.Radius = 240.0f;
            spell.Range = 850.0f; // Tầm ném ban đầu, tổng tầm nảy có thể lên tới 1400
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "ZiggsQ";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ziggs;
            spell.DangerValue = 2;
            spell.MissileSpellName = "ZiggsQSpell2";
            spell.DisplayName = "Bouncing Bomb (Q - Bounce 1)";
            spell.MissileSpeed = 1700.0f;
            spell.Radius = 240.0f;
            spell.Range = 850.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "ZiggsQBounce1";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.IsSpecial = true;
            spell.DontProcess = true;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Ziggs;
            spell.DangerValue = 2;
            spell.MissileSpellName = "ZiggsQSpell3";
            spell.DisplayName = "Bouncing Bomb (Q - Bounce 2)";
            spell.MissileSpeed = 1700.0f;
            spell.Radius = 240.0f;
            spell.Range = 850.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::Q;
            spell.SpellName = "ZiggsQBounce2";
            spell.Type = SkillShotType::SkillshotCircle;
            spell.IsSpecial = true;
            spell.DontProcess = true;
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Zoe;
            spell.DangerValue = 3;
            spell.MissileSpellName = "ZoeEMis";
            spell.DisplayName = "Sleepy Trouble Bubble (E)";
            spell.MissileSpeed = 1850.0f; // Bay xa hơn nếu đi qua địa hình tường
            spell.Radius = 80.0f;
            spell.Range = 800.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "ZoeE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Sleep; // Gây buồn ngủ rồi ngủ thiếp đi
            spell.CollisionObjects = { CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            SpellData spell;
            spell.ChampionId = SDK::ChampionId::Zyra;
            spell.DangerValue = 3;
            spell.MissileSpellName = "ZyraEIsotope";
            spell.DisplayName = "Grasping Roots (E)";
            spell.MissileSpeed = 1150.0f; // Tốc độ rễ cây bò khá chậm
            spell.Radius = 70.0f;
            spell.Range = 1100.0f;
            spell.Delay = 250;
            spell.Slot = SpellSlot::E;
            spell.SpellName = "ZyraE";
            spell.Type = SkillShotType::SkillshotLine;
            spell.CrowdControl = CrowdControlType::Snare; // Trói xuyên qua cả lính lẫn tướng
            Entries.push_back(spell);
        }
        // ===
        for (SpellData& spell : Entries) {
            spell.Finalize();
        }
    }

    static const std::vector<SpellData>& Spells() {
        static bool initialized = false;
        if (!initialized) {
            Initialize();
            initialized = true;
        }
        return Entries;
    }

    static const SpellData* GetByName(const char* name) {
        if (!name || !name[0]) {
            return nullptr;
        }
        for (const SpellData& spell : Spells()) {
            if (_stricmp(spell.SpellName.c_str(), name) == 0 ||
                _stricmp(spell.MissileSpellName.c_str(), name) == 0) {
                return &spell;
            }
            for (const std::string& extra : spell.ExtraSpellNames) {
                if (_stricmp(extra.c_str(), name) == 0) {
                    return &spell;
                }
            }
            for (const std::string& extra : spell.ExtraMissileNames) {
                if (_stricmp(extra.c_str(), name) == 0) {
                    return &spell;
                }
            }
        }
        return nullptr;
    }
};

inline std::vector<SpellData> SpellDatabase::Entries;
} // namespace Plugins::KuroEvade::Database
