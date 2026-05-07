#pragma once
#include "SpellData.h"
#include <cctype>
#include <string>
#include <vector>


namespace SDK {
namespace Data {
inline std::vector<SpellData> &GetSpellDatabase() {
  static std::vector<SpellData> Spells;
  if (!Spells.empty())
    return Spells;
  Spells.reserve(1000);

  // ==== AllChampions ====
  // ==== Syndra ====
  Spells.push_back([]() {
    SpellData d;
    d.angle = 45;
    d.charName = "Syndra";
    d.dangerlevel = 3;
    d.name = "Scatter the Weak";
    d.missileName = "SyndraE";
    d.projectileSpeed = 2000;
    d.radius = 70.0f; // Patched wiki
    d.range = 850;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::E;
    d.spellName = "SyndraE";
    d.extraSpellNames = {"syndrae5"};
    d.spellType = SpellType::Line;
    d.isSpecial = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Syndra";
    d.dangerlevel = 2;
    d.missileName = "syndrawcast";
    d.name = "Force of Will";
    d.projectileSpeed = 1450;
    d.radius = 220;
    d.range = 950;
    d.spellDelay = 250;
    d.spellKey = SpellSlot::W;
    d.spellName = "syndrawcast";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Syndra";
    d.dangerlevel = 2;
    d.missileName = "SyndraQSpell";
    d.name = "Dark Sphere";
    d.radius = 210;
    d.range = 800;
    d.spellDelay = 600;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SyndraQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== TahmKench ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "TahmKench";
    d.dangerlevel = 3;
    d.missileName = "tahmkenchqmissile";
    d.name = "Tongue Lash";
    d.projectileSpeed = 2000;
    d.spellDelay = 250.0f; // Patched wiki
    d.radius = 70.0f;      // Patched wiki
    d.range = 800;
    d.spellKey = SpellSlot::Q;
    d.spellName = "TahmKenchQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    d.fixedRange = true;
    return d;
  }());

  // ==== Talon ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Talon";
    d.dangerlevel = 4;
    d.name = "Shadow Assault [Beta]";
    d.projectileSpeed = 2400;
    d.radius = 140.0f; // Patched wiki
    d.range = 550;
    d.spellKey = SpellSlot::R;
    d.spellName = "talonrmisone";
    d.extraMissileNames = {"talonrmistwo"};
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.angle = 14;
    d.charName = "Talon";
    d.dangerlevel = 3;
    d.isThreeWay = true;
    d.missileName = "talonwmissile";
    d.name = "Rake [Beta]";
    d.projectileSpeed = 2300;
    d.radius = 75.0f; // Patched wiki
    d.range = 900;
    d.spellKey = SpellSlot::W;
    d.spellName = "talonw";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.isSpecial = true;
    d.spellDelay = 250.0f; // Patched wiki
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.angle = 14;
    d.charName = "Talon";
    d.dangerlevel = 3;
    d.isThreeWay = true;
    d.name = "Rake Return [Beta]";
    d.projectileSpeed = 3000;
    d.radius = 75.0f; // Patched wiki
    d.range = 900;
    d.spellKey = SpellSlot::W;
    d.spellName = "talonwmissiletwo";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.isSpecial = true;
    d.spellDelay = 250.0f; // Patched wiki
    return d;
  }());

  // ==== Taliyah ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Taliyah";
    d.dangerlevel = 2;
    d.missileName = "TaliyahQMis";
    d.projectileSpeed = 1450;
    d.name = "Threaded Volley";
    d.radius = 100.0f; // Patched wiki
    d.range = 1000;
    d.fixedRange = true;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "TaliyahQ";
    d.spellType = SpellType::Line;
    d.defaultOff = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Taliyah";
    d.dangerlevel = 3;
    d.name = "Seismic Shove";
    d.radius = 165;
    d.range = 900;
    d.spellDelay = 250.0f; // Patched wiki
    d.extraEndTime = 1000;
    d.spellKey = SpellSlot::W;
    d.spellName = "TaliyahWVC";
    d.extraSpellNames = {"TaliyahW"};
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Taric ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Taric";
    d.dangerlevel = 2;
    d.missileName = "TaricEMissile";
    d.name = "Dazzle";
    d.radius = 70.0f; // Patched wiki
    d.range = 750;
    d.fixedRange = true;
    d.spellDelay = 1000;
    d.spellKey = SpellSlot::E;
    d.spellName = "TaricE";
    d.spellType = SpellType::Line;
    d.isSpecial = true;
    return d;
  }());

  // ==== Thresh ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Thresh";
    d.dangerlevel = 2;
    d.missileName = "ThreshQMissile";
    d.name = "Death Sentence";
    d.projectileSpeed = 1900;
    d.radius = 70.0f; // Patched wiki
    d.range = 1200;
    d.spellDelay = 500.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "ThreshQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Thresh";
    d.dangerlevel = 3;
    d.missileName = "ThreshEMissile1";
    d.name = "Flay";
    d.projectileSpeed = 2000;
    d.radius = 110.0f; // Patched wiki
    d.range = 1075;
    d.spellDelay = 389.0f; // Patched wiki
    d.spellKey = SpellSlot::E;
    d.spellName = "ThreshE";
    d.extraSpellNames = {"ThreshEFlay"};
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.usePackets = true;
    return d;
  }());

  // ==== Tristana ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Tristana";
    d.dangerlevel = 1;
    d.missileName = "RocketJump";
    d.name = "Rocket Jump";
    d.projectileSpeed = 1000;
    d.radius = 270;
    d.range = 900;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::W;
    d.spellName = "TristanaW";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Tryndamere ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Tryndamere";
    d.dangerlevel = 2;
    d.missileName = "slashCast";
    d.name = "Spinning Slash";
    d.projectileSpeed = 1300;
    d.radius = 95;
    d.range = 660;
    d.spellDelay = 0;
    d.spellKey = SpellSlot::E;
    d.spellName = "slashCast";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== TwistedFate ====
  Spells.push_back([]() {
    SpellData d;
    d.angle = 28;
    d.charName = "TwistedFate";
    d.dangerlevel = 2;
    d.isThreeWay = true;
    d.missileName = "SealFateMissile";
    d.name = "Wild Cards";
    d.projectileSpeed = 1000;
    d.radius = 40;
    d.range = 1450;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "WildCards";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Twitch ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Twitch";
    d.dangerlevel = 2;
    d.missileName = "TwitchVenomCaskMissile";
    d.name = "Venom Cask";
    d.projectileSpeed = 1400;
    d.radius = 280;
    d.range = 900;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::W;
    d.spellName = "TwitchVenomCask";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Twitch";
    d.dangerlevel = 3;
    d.missileName = "TwitchSprayandPrayAttack";
    d.name = "Spray and Pray";
    d.projectileSpeed = 4000;
    d.radius = 60.0f; // Patched wiki
    d.range = 1100;
    d.spellDelay = 250;
    d.spellKey = SpellSlot::R;
    d.spellName = "TwitchSprayandPrayAttack";
    d.spellType = SpellType::Line;
    d.isSpecial = true;
    return d;
  }());

  // ==== Varus ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Varus";
    d.dangerlevel = 2;
    d.name = "Hail of Arrows";
    d.projectileSpeed = 1500;
    d.radius = 235;
    d.range = 925;
    d.spellDelay = 241.9f; // Patched wiki
    d.spellKey = SpellSlot::E;
    d.spellName = "VarusE";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Varus";
    d.dangerlevel = 2;
    d.missileName = "varusqmissile";
    d.name = "Piercing Arrow";
    d.projectileSpeed = 1900;
    d.radius = 70.0f; // Patched wiki
    d.range = 1525;
    d.spellDelay = 0;
    d.spellKey = SpellSlot::Q;
    d.spellName = "varusq";
    d.spellType = SpellType::Line;
    d.usePackets = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Varus";
    d.dangerlevel = 3;
    d.name = "Chain of Corruption";
    d.missileName = "VarusRMissile";
    d.projectileSpeed = 1950;
    d.radius = 120.0f; // Patched wiki
    d.range = 1250;
    d.spellDelay = 241.9f; // Patched wiki
    d.spellKey = SpellSlot::R;
    d.spellName = "VarusR";
    d.spellType = SpellType::Line;
    d.collisionObjects = {
        CollisionObjectType::EnemyChampions,
    };
    d.fixedRange = true;
    return d;
  }());

  // ==== Veigar ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Veigar";
    d.dangerlevel = 2;
    d.missileName = "VeigarBalefulStrikeMis";
    d.name = "Baleful Strike";
    d.projectileSpeed = 2200;
    d.radius = 70.0f; // Patched wiki
    d.range = 950;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "VeigarBalefulStrike";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Veigar";
    d.dangerlevel = 2;
    d.missileName = "VeigarDarkMatter";
    d.name = "Dark Matter";
    d.radius = 225;
    d.range = 900;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::W;
    d.spellName = "VeigarDarkMatter";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Veigar";
    d.dangerlevel = 3;
    d.name = "Event Horizon";
    d.radius = 375;
    d.range = 700;
    d.spellDelay = 250.0f; // Patched wiki
    d.extraEndTime = 3300;
    d.spellKey = SpellSlot::E;
    d.spellName = "VeigarEventHorizon";
    d.spellType = SpellType::Circular;
    d.defaultOff = true;
    return d;
  }());

  // ==== Velkoz ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Velkoz";
    d.dangerlevel = 3;
    d.name = "Tectonic Disruption";
    d.projectileSpeed = 1500;
    d.radius = 225;
    d.range = 800;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::E;
    d.spellName = "VelkozE";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Velkoz";
    d.dangerlevel = 2;
    d.missileName = "VelkozWMissile";
    d.name = "Void Rift";
    d.projectileSpeed = 1700;
    d.radius = 90;
    d.range = 1150;
    d.extraEndTime = 1000;
    d.spellDelay = 250;
    d.spellKey = SpellSlot::W;
    d.spellName = "VelkozW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Velkoz";
    d.dangerlevel = 2;
    d.name = "Plasma Fission (Split)";
    d.projectileSpeed = 2100;
    d.radius = 50;
    d.range = 1100;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "VelkozQMissileSplit";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    d.usePackets = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Velkoz";
    d.dangerlevel = 2;
    d.missileName = "VelkozQMissile";
    d.name = "Plasma Fission";
    d.projectileSpeed = 1300;
    d.radius = 55;
    d.range = 1250;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "VelkozQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    d.fixedRange = true;
    return d;
  }());

  // ==== Vi ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Vi";
    d.dangerlevel = 3;
    d.name = "Vault Breaker";
    d.projectileSpeed = 1500;
    d.radius = 55.0f; // Patched wiki
    d.range = 775;
    d.spellKey = SpellSlot::Q;
    d.spellName = "ViQMissile";
    d.spellType = SpellType::Line;
    d.collisionObjects = {
        CollisionObjectType::EnemyChampions,
    };
    d.usePackets = true;
    return d;
  }());

  // ==== Viktor ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Viktor";
    d.dangerlevel = 3;
    d.missileName = "ViktorDeathRayMissile";
    d.name = "Death Ray";
    d.projectileSpeed = 1050;
    d.radius = 45.0f; // Patched wiki
    d.range = 815;
    d.spellKey = SpellSlot::E;
    d.spellName = "ViktorDeathRay";
    d.extraMissileNames = {
        "ViktorEAugMissile",
    };
    d.spellType = SpellType::Line;
    d.usePackets = true;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Viktor";
    d.dangerlevel = 3;
    d.name = "Death Ray Aftershock";
    d.spellDelay = 500;
    d.radius = 45.0f; // Patched wiki
    d.range = 815;
    d.spellKey = SpellSlot::E;
    d.spellName = "ViktorDeathRay3";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.usePackets = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Viktor";
    d.dangerlevel = 3;
    d.name = "Graviton Field";
    d.radius = 300;
    d.range = 625;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::W;
    d.spellName = "ViktorGravitonField";
    d.spellType = SpellType::Circular;
    d.defaultOff = true;
    return d;
  }());

  // ==== Vladimir ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Vladimir";
    d.dangerlevel = 3;
    d.missileName = "VladimirR", // mage update;
        d.name = "Hemoplague";
    d.radius = 375;
    d.range = 700;
    d.spellDelay = 250;
    d.spellKey = SpellSlot::R;
    d.spellName = "VladimirR", // mage update;
        d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Xerath ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Xerath";
    d.dangerlevel = 2;
    d.missileName = "XerathArcaneBarrage2";
    d.name = "Eye of Destruction";
    d.radius = 280;
    d.range = 1000;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::W;
    d.spellName = "XerathArcaneBarrage2";
    d.spellType = SpellType::Circular;
    d.extraDrawHeight = 45;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Xerath";
    d.dangerlevel = 2;
    d.missileName = "XerathArcanopulse2";
    d.name = "Arcanopulse";
    d.radius = 72.5f; // Patched wiki
    d.range = 1525;
    d.spellDelay = 500;
    d.spellKey = SpellSlot::Q;
    d.spellName = "XerathArcanopulse2";
    d.useEndPosition = true;
    d.spellType = SpellType::Line;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Xerath";
    d.dangerlevel = 3;
    d.name = "Rite of the Arcane";
    d.missileName = "XerathLocusPulse";
    d.radius = 200;
    d.range = 5600;
    d.spellDelay = 600;
    d.spellKey = SpellSlot::R;
    d.spellName = "xerathrmissilewrapper";
    d.extraSpellNames = {"XerathLocusPulse"};
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Xerath";
    d.dangerlevel = 3;
    d.missileName = "XerathMageSpearMissile";
    d.name = "Shocking Orb";
    d.projectileSpeed = 1600;
    d.radius = 60.0f; // Patched wiki
    d.range = 1125;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::E;
    d.spellName = "XerathMageSpear";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    d.fixedRange = true;
    return d;
  }());

  // ==== Yasuo ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yasuo";
    d.dangerlevel = 3;
    d.missileName = "YasuoQ3Mis";
    d.name = "Steel Tempest (Tornado)";
    d.projectileSpeed = 1250;
    d.radius = 90;
    d.range = 1150;
    d.spellDelay = 300;
    d.spellKey = SpellSlot::Q;
    d.spellName = "YasuoQ3W";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yasuo";
    d.dangerlevel = 2;
    d.missileName = "yasuoq";
    d.extraMissileNames = {"yasuoq2"};
    d.name = "Steel Tempest";
    d.radius = 40;
    d.range = 550;
    d.fixedRange = true;
    d.spellDelay = 400;
    d.spellKey = SpellSlot::Q;
    d.spellName = "YasuoQ";
    d.extraSpellNames = {"YasuoQ2"};
    d.spellType = SpellType::Line;
    d.invert = true;
    return d;
  }());

  // ==== Yorick ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yorick";
    d.dangerlevel = 3;
    d.name = "Dark Procession";
    d.radius = 250;
    d.range = 600;
    d.spellDelay = 500;
    d.spellKey = SpellSlot::W;
    d.extraEndTime = 1000;
    d.spellName = "YorickW";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yorick";
    d.dangerlevel = 3;
    d.missileName = "YorickEMissile";
    d.name = "Mourning Mist";
    d.projectileSpeed = 900;
    d.radius = 125;
    d.range = 700;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "YorickE";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ==== Zac ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zac";
    d.dangerlevel = 3;
    d.missileName = "ZacQ";
    d.name = "Stretching Strike";
    d.radius = 80.0f; // Patched wiki
    d.range = 550;
    d.spellDelay = 330.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "ZacQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zac";
    d.dangerlevel = 3;
    d.name = "Elastic Slingshot [Beta]";
    d.projectileSpeed = 1000;
    d.radius = 300;
    d.range = 1800;
    d.spellDelay = 250;
    d.spellKey = SpellSlot::E;
    d.spellName = "ZacE";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Zed ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zed";
    d.dangerlevel = 3;
    d.missileName = "ZedQMissile";
    d.name = "Razor Shuriken";
    d.projectileSpeed = 1700;
    d.radius = 50.0f; // Patched wiki
    d.range = 925;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "ZedQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Ziggs ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ziggs";
    d.dangerlevel = 1;
    d.missileName = "ZiggsE";
    d.name = "Hexplosive Minefield";
    d.projectileSpeed = 3000;
    d.radius = 235;
    d.range = 2000;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::E;
    d.spellName = "ZiggsE";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ziggs";
    d.dangerlevel = 2;
    d.missileName = "ZiggsW";
    d.name = "Satchel Charge";
    d.projectileSpeed = 2000;
    d.radius = 275;
    d.range = 1000;
    d.spellDelay = 250.0f; // Patched wiki
    d.extraEndTime = 1000;
    d.spellKey = SpellSlot::W;
    d.spellName = "ZiggsW";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ziggs";
    d.dangerlevel = 2;
    d.name = "Bouncing Bomb";
    d.projectileSpeed = 1700;
    d.radius = 180.0f; // Patched wiki
    d.range = 850;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "ZiggsQ";
    d.spellType = SpellType::Circular;
    d.isSpecial = true;
    d.noProcess = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ziggs";
    d.dangerlevel = 4;
    d.missileName = "ZiggsR";
    d.name = "Mega Inferno Bomb";
    d.projectileSpeed = 1550;
    d.radius = 500;
    d.range = 5300;
    d.spellDelay = 375.0f; // Patched wiki
    d.spellKey = SpellSlot::R;
    d.spellName = "ZiggsR";
    d.spellType = SpellType::Circular;
    d.defaultOff = true;
    d.isSpecial = true;
    return d;
  }());

  // ==== Zilean ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zilean";
    d.dangerlevel = 3;
    d.missileName = "ZileanQMissile";
    d.name = "Time Bomb";
    d.radius = 140.0f; // Patched wiki
    d.range = 900;
    d.extraEndTime = 1000;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "ZileanQ";
    d.spellType = SpellType::Circular;
    d.isSpecial = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zilean";
    d.dangerlevel = 3;
    d.spellName = "ZileanQ";
    d.name = "Time Bomb";
    d.radius = 140.0f; // Patched wiki
    d.range = 900;
    d.spellKey = SpellSlot::Q;
    d.spellType = SpellType::Circular;
    d.trapTroyName = "unknown";
    d.extraDrawHeight = -100;
    d.hasTrap = true;
    d.spellDelay = 250.0f; // Patched wiki
    return d;
  }());

  // ==== Zyra ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zyra";
    d.dangerlevel = 3;
    d.name = "Grasping Roots";
    d.missileName = "ZyraEMissile";
    d.projectileSpeed = 1400, // 1150;
        d.radius = 70;
    d.range = 1150;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::E;
    d.spellName = "ZyraE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zyra";
    d.dangerlevel = 2;
    d.missileName = "ZyraQ";
    d.name = "Deadly Bloom";
    d.radius = 140;
    d.range = 800;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::Q;
    d.spellName = "ZyraQ";
    d.spellType = SpellType::Line;
    d.isPerpendicular = true;
    d.secondaryRadius = 400;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zyra";
    d.dangerlevel = 4;
    d.name = "Stranglethorns";
    d.radius = 525;
    d.range = 700;
    d.extraEndTime = 2000;
    d.spellDelay = 250.0f; // Patched wiki
    d.spellKey = SpellSlot::R;
    d.spellName = "ZyraR";
    d.extraSpellNames = {"ZyraBrambleZone"};
    d.spellType = SpellType::Circular;
    d.defaultOff = true;
    return d;
  }());

  // ==== Smolder ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Smolder";
    d.dangerlevel = 1;
    d.name = "Dragon's Breath";
    d.radius = 50;
    d.range = 550;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SmolderQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Smolder";
    d.dangerlevel = 2;
    d.name = "ACHOO!";
    d.projectileSpeed = 1500;
    d.radius = 125;
    d.range = 1200;
    d.spellDelay = 350.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "SmolderW";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Aurora ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Aurora";
    d.dangerlevel = 2;
    d.name = "Spirit Stream";
    d.projectileSpeed = 2000;
    d.radius = 60;
    d.range = 900;
    d.spellDelay = 150.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "AuroraQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Aurora";
    d.dangerlevel = 2;
    d.name = "Across the Veil";
    d.projectileSpeed = 1800;
    d.radius = 150;
    d.range = 850;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "AuroraE";
    d.spellType = SpellType::Cone;
    return d;
  }());

  // ==== Ambessa ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ambessa";
    d.dangerlevel = 2;
    d.name = "Axe Sweep";
    d.radius = 180;
    d.range = 450;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "AmbessaQ1";
    d.spellType = SpellType::Arc;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ambessa";
    d.dangerlevel = 4;
    d.name = "Assassin's Domain";
    d.projectileSpeed = 2000;
    d.radius = 120;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "AmbessaR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Mel ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Mel";
    d.dangerlevel = 2;
    d.name = "Morning Light";
    d.projectileSpeed = 3800;
    d.radius = 70;
    d.range = 1400;
    d.spellDelay = 400.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "MelQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Mel";
    d.dangerlevel = 4;
    d.name = "Ray of Dawn";
    d.projectileSpeed = 5000;
    d.radius = 250;
    d.range = 3000;
    d.spellDelay = 1000.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "MelR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Zaahen ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zaahen";
    d.dangerlevel = 2;
    d.name = "Void Surge";
    d.projectileSpeed = 2200;
    d.radius = 80;
    d.range = 1050;
    d.spellDelay = 200.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "ZaahenQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Akali ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Akali";
    d.dangerlevel = 2;
    d.name = "Five Point Strike";
    d.radius = 140;
    d.range = 500;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "AkaliQ";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Akali";
    d.dangerlevel = 3;
    d.name = "Shuriken Flip";
    d.projectileSpeed = 1800;
    d.radius = 60;
    d.range = 825;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "AkaliE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Akali";
    d.dangerlevel = 4;
    d.name = "Perfect Execution (R1)";
    d.projectileSpeed = 1800;
    d.radius = 100;
    d.range = 675;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "AkaliR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== AurelionSol ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "AurelionSol";
    d.dangerlevel = 2;
    d.name = "Breath of Light";
    d.radius = 90;
    d.range = 750;
    d.spellDelay = 0.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "AurelionSolQ";
    d.spellType = SpellType::Line;
    d.isSpecial = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "AurelionSol";
    d.dangerlevel = 3;
    d.name = "Singularity";
    d.radius = 275;
    d.range = 750;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "AurelionSolE";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "AurelionSol";
    d.dangerlevel = 4;
    d.name = "Falling Star";
    d.radius = 600;
    d.range = 1250;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "AurelionSolR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Belveth ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Belveth";
    d.dangerlevel = 2;
    d.name = "Void Surge";
    d.radius = 100;
    d.range = 400;
    d.spellDelay = 0.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "BelvethQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Belveth";
    d.dangerlevel = 3;
    d.name = "Above and Below";
    d.radius = 200;
    d.range = 660;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "BelvethW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Briar ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Briar";
    d.dangerlevel = 2;
    d.name = "Head Rush";
    d.projectileSpeed = 1600;
    d.radius = 100;
    d.range = 300;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "BriarW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Briar";
    d.dangerlevel = 4;
    d.name = "Certain Death";
    d.projectileSpeed = 2000;
    d.radius = 160;
    d.range = 10000;
    d.spellDelay = 1000.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "BriarR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Fiddlesticks ==== (Reworked 2020)
  // Q - Terrify (AoE fear when cast from unseen/fog)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Fiddlesticks";
    d.dangerlevel = 3;
    d.name = "Terrify";
    d.radius = 600;
    d.range = 575;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "FiddlesticksQ";
    d.spellType = SpellType::Circular;
    d.isSpecial = true;
    return d;
  }());

  // W - Bountiful Harvest (AoE drain)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Fiddlesticks";
    d.dangerlevel = 2;
    d.name = "Bountiful Harvest";
    d.radius = 600;
    d.range = 650;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "FiddlesticksW";
    d.spellType = SpellType::Circular;
    d.defaultOff = true;
    return d;
  }());

  // E - Reap (cone)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Fiddlesticks";
    d.dangerlevel = 3;
    d.name = "Reap";
    d.radius = 200;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "FiddlesticksE";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Fiddlesticks";
    d.dangerlevel = 4;
    d.name = "Crowstorm";
    d.radius = 600;
    d.range = 800;
    d.spellDelay = 1500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "FiddlesticksR";
    d.spellType = SpellType::Circular;
    d.fixedRange = true;
    return d;
  }());

  // ==== Galio ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Galio";
    d.dangerlevel = 2;
    d.name = "Winds of War";
    d.projectileSpeed = 1400;
    d.radius = 150;
    d.range = 825;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "GalioQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Galio";
    d.dangerlevel = 3;
    d.name = "Justice Punch";
    d.projectileSpeed = 2300;
    d.radius = 160;
    d.range = 650;
    d.spellDelay = 400.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "GalioE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Galio";
    d.dangerlevel = 4;
    d.name = "Hero's Entrance";
    d.radius = 650;
    d.range = 5500;
    d.spellDelay = 2750.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "GalioR";
    d.spellType = SpellType::Circular;
    d.isSpecial = true;
    return d;
  }());

  // ==== Gwen ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Gwen";
    d.dangerlevel = 3;
    d.name = "Needlework (R)";
    d.projectileSpeed = 1800;
    d.radius = 60;
    d.range = 1200;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "GwenR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Hwei ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Hwei";
    d.dangerlevel = 2;
    d.name = "Devastating Fire";
    d.projectileSpeed = 1600;
    d.radius = 70;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "HweiQQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Hwei";
    d.dangerlevel = 2;
    d.name = "Severing Bolt";
    d.radius = 250;
    d.range = 900;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "HweiQW";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Hwei";
    d.dangerlevel = 2;
    d.name = "Molten Fissure";
    d.radius = 100;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "HweiQE";
    d.spellType = SpellType::Line;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Hwei";
    d.dangerlevel = 3;
    d.name = "Grim Visage";
    d.projectileSpeed = 1400;
    d.radius = 70;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "HweiEQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Hwei";
    d.dangerlevel = 4;
    d.name = "Spiraling Despair";
    d.projectileSpeed = 1200;
    d.radius = 200;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "HweiR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Irelia ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Irelia";
    d.dangerlevel = 3;
    d.name = "Flawless Duet";
    d.projectileSpeed = 2000;
    d.radius = 70;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "IreliaE";
    d.spellType = SpellType::Line;
    d.isSpecial = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Irelia";
    d.dangerlevel = 4;
    d.name = "Vanguard's Edge";
    d.projectileSpeed = 2000;
    d.radius = 160;
    d.range = 1000;
    d.spellDelay = 400.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "IreliaR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== KSante ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "KSante";
    d.dangerlevel = 2;
    d.name = "Ntofo Strikes (Q)";
    d.radius = 75;
    d.range = 465;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "KSanteQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "KSante";
    d.dangerlevel = 3;
    d.name = "Ntofo Strikes (Q3)";
    d.projectileSpeed = 1600;
    d.radius = 120;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "KSanteQ3";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "KSante";
    d.dangerlevel = 3;
    d.name = "Footwork";
    d.radius = 100;
    d.range = 475;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "KSanteW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "KSante";
    d.dangerlevel = 4;
    d.name = "All Out";
    d.radius = 200;
    d.range = 700;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "KSanteR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Lillia ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lillia";
    d.dangerlevel = 2;
    d.name = "Blooming Blows";
    d.radius = 225;
    d.range = 485;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "LilliaQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lillia";
    d.dangerlevel = 2;
    d.name = "Watch Out! Eep!";
    d.radius = 100;
    d.range = 500;
    d.spellDelay = 750.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "LilliaW";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lillia";
    d.dangerlevel = 3;
    d.name = "Swirlseed";
    d.projectileSpeed = 1400;
    d.radius = 60;
    d.range = 10000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "LilliaE";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Milio ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Milio";
    d.dangerlevel = 2;
    d.name = "Ultra Mega Fire Kick";
    d.projectileSpeed = 1200;
    d.radius = 60;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "MilioQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Mordekaiser ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Mordekaiser";
    d.dangerlevel = 3;
    d.name = "Obliterate";
    d.radius = 150;
    d.range = 625;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "MordekaiserQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Mordekaiser";
    d.dangerlevel = 3;
    d.name = "Death's Grasp";
    d.radius = 100;
    d.range = 900;
    d.spellDelay = 600.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "MordekaiserE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Naafiri ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Naafiri";
    d.dangerlevel = 2;
    d.name = "Darkin Daggers";
    d.projectileSpeed = 1700;
    d.radius = 65;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "NaafiriQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Naafiri";
    d.dangerlevel = 3;
    d.name = "Eviscerate";
    d.projectileSpeed = 3000;
    d.radius = 85;
    d.range = 750;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "NaafiriW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Nilah ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nilah";
    d.dangerlevel = 3;
    d.name = "Slipstream";
    d.projectileSpeed = 2200;
    d.radius = 100;
    d.range = 550;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "NilahE";
    d.spellType = SpellType::Line;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nilah";
    d.dangerlevel = 4;
    d.name = "Apotheosis";
    d.radius = 450;
    d.range = 450;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "NilahR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Pantheon ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Pantheon";
    d.dangerlevel = 3;
    d.name = "Comet Spear (Tap)";
    d.projectileSpeed = 2700;
    d.radius = 60;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "PantheonQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Pantheon";
    d.dangerlevel = 3;
    d.name = "Comet Spear (Hold)";
    d.projectileSpeed = 2700;
    d.radius = 60;
    d.range = 1200;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "PantheonQMissile";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.missileName = "PantheonQMissile";
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Pantheon";
    d.dangerlevel = 4;
    d.name = "Grand Starfall";
    d.radius = 700;
    d.range = 5500;
    d.spellDelay = 2200.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "PantheonRFall";
    d.spellType = SpellType::Circular;
    d.isSpecial = true;
    return d;
  }());

  // ==== Rell ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Rell";
    d.dangerlevel = 3;
    d.name = "Shattering Strike";
    d.radius = 100;
    d.range = 685;
    d.spellDelay = 350.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "RellQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Rell";
    d.dangerlevel = 4;
    d.name = "Magnet Storm";
    d.radius = 450;
    d.range = 375;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "RellR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Renata ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Renata";
    d.dangerlevel = 2;
    d.name = "Handshake";
    d.projectileSpeed = 1450;
    d.radius = 70;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "RenataQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Renata";
    d.dangerlevel = 3;
    d.name = "Loyalty Program";
    d.projectileSpeed = 1450;
    d.radius = 220;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "RenataE";
    d.spellType = SpellType::Line;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Renata";
    d.dangerlevel = 4;
    d.name = "Hostile Takeover";
    d.projectileSpeed = 650;
    d.radius = 250;
    d.range = 2500;
    d.spellDelay = 750.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "RenataR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Samira ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Samira";
    d.dangerlevel = 2;
    d.name = "Flair";
    d.projectileSpeed = 2600;
    d.radius = 60;
    d.range = 950;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SamiraQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Samira";
    d.dangerlevel = 4;
    d.name = "Inferno Trigger";
    d.radius = 600;
    d.range = 600;
    d.spellDelay = 0.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "SamiraR";
    d.spellType = SpellType::Circular;
    d.fixedRange = true;
    return d;
  }());

  // ==== Seraphine ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Seraphine";
    d.dangerlevel = 2;
    d.name = "High Note";
    d.projectileSpeed = 1200;
    d.radius = 175;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SeraphineQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Seraphine";
    d.dangerlevel = 3;
    d.name = "Beat Drop";
    d.projectileSpeed = 1200;
    d.radius = 70;
    d.range = 1300;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "SeraphineE";
    d.spellType = SpellType::Line;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Seraphine";
    d.dangerlevel = 4;
    d.name = "Encore";
    d.projectileSpeed = 1600;
    d.radius = 160;
    d.range = 1200;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "SeraphineR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Sett ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sett";
    d.dangerlevel = 2;
    d.name = "Knuckle Down";
    d.radius = 100;
    d.range = 300;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SettQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sett";
    d.dangerlevel = 3;
    d.name = "Haymaker";
    d.radius = 200;
    d.range = 790;
    d.spellDelay = 750.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "SettW";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sett";
    d.dangerlevel = 4;
    d.name = "The Show Stopper";
    d.radius = 300;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "SettR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Skarner ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Skarner";
    d.dangerlevel = 2;
    d.name = "Shattered Earth";
    d.radius = 350;
    d.range = 700;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SkarnerQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Skarner";
    d.dangerlevel = 3;
    d.name = "Ixtal's Impact";
    d.projectileSpeed = 1500;
    d.radius = 85;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "SkarnerE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Skarner";
    d.dangerlevel = 4;
    d.name = "Impale";
    d.radius = 100;
    d.range = 350;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "SkarnerR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Swain ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Swain";
    d.dangerlevel = 2;
    d.name = "Vision of Empire";
    d.radius = 325;
    d.range = 3500;
    d.spellDelay = 1500.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "SwainW";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Swain";
    d.dangerlevel = 3;
    d.name = "Nevermove";
    d.projectileSpeed = 1800;
    d.radius = 85;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "SwainE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Urgot ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Urgot";
    d.dangerlevel = 2;
    d.name = "Corrosive Charge";
    d.radius = 210;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "UrgotQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Urgot";
    d.dangerlevel = 3;
    d.name = "Disdain";
    d.projectileSpeed = 1540;
    d.radius = 100;
    d.range = 475;
    d.spellDelay = 450.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "UrgotE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Urgot";
    d.dangerlevel = 4;
    d.name = "Fear Beyond Death";
    d.projectileSpeed = 3200;
    d.radius = 80;
    d.range = 2500;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "UrgotR";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ==== Vex ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Vex";
    d.dangerlevel = 2;
    d.name = "Mistral Bolt";
    d.projectileSpeed = 3200;
    d.radius = 80;
    d.range = 1200;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "VexQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Vex";
    d.dangerlevel = 3;
    d.name = "Looming Darkness";
    d.radius = 200;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "VexW";
    d.spellType = SpellType::Circular;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Vex";
    d.dangerlevel = 3;
    d.name = "Shadow Surge";
    d.projectileSpeed = 1600;
    d.radius = 130;
    d.range = 2000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "VexR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Viego ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Viego";
    d.dangerlevel = 2;
    d.name = "Blade of the Ruined King";
    d.radius = 125;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "ViegoQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Viego";
    d.dangerlevel = 3;
    d.name = "Spectral Maw";
    d.projectileSpeed = 1300;
    d.radius = 65;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "ViegoW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Viego";
    d.dangerlevel = 3;
    d.name = "Harrowed Path";
    d.radius = 300;
    d.range = 500;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "ViegoR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Volibear ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Volibear";
    d.dangerlevel = 2;
    d.name = "Sky Splitter";
    d.radius = 300;
    d.range = 1200;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "VolibearE";
    d.spellType = SpellType::Circular;
    d.extraEndTime = 2000;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Volibear";
    d.dangerlevel = 4;
    d.name = "Stormbringer";
    d.radius = 300;
    d.range = 700;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "VolibearR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Yone ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yone";
    d.dangerlevel = 2;
    d.name = "Mortal Steel (Q1/Q2)";
    d.radius = 80;
    d.range = 475;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "YoneQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yone";
    d.dangerlevel = 3;
    d.name = "Mortal Steel (Q3 Dash)";
    d.projectileSpeed = 1500;
    d.radius = 160;
    d.range = 1050;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "YoneQ3";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yone";
    d.dangerlevel = 2;
    d.name = "Spirit Cleave";
    d.radius = 200;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "YoneW";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yone";
    d.dangerlevel = 4;
    d.name = "Fate Sealed";
    d.projectileSpeed = 3000;
    d.radius = 112;
    d.range = 1000;
    d.spellDelay = 750.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "YoneR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Zeri ====
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zeri";
    d.dangerlevel = 2;
    d.name = "Burst Fire";
    d.projectileSpeed = 2600;
    d.radius = 40;
    d.range = 825;
    d.spellDelay = 0.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "ZeriQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zeri";
    d.dangerlevel = 2;
    d.name = "Ultrashock Laser";
    d.projectileSpeed = 2500;
    d.radius = 40;
    d.range = 1200;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "ZeriW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zeri";
    d.dangerlevel = 3;
    d.name = "Spark Surge";
    d.projectileSpeed = 2000;
    d.radius = 80;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "ZeriE";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== DrMundo ==== (Reworked 2021)
  // Q - Infected Bonesaw (line missile, collides with first enemy hit)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "DrMundo";
    d.dangerlevel = 2;
    d.missileName = "DrMundoQ";
    d.name = "Infected Bonesaw";
    d.projectileSpeed = 2000;
    d.radius = 60;
    d.range = 1050;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "DrMundoQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Evelynn ==== (Reworked 2017)
  // Q1 - Hate Spike (initial cast — line missile)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Evelynn";
    d.dangerlevel = 2;
    d.missileName = "EvelynnQ";
    d.name = "Hate Spike";
    d.projectileSpeed = 2200;
    d.radius = 60;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "EvelynnQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // R - Last Caress (blink + cone damage behind)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Evelynn";
    d.dangerlevel = 4;
    d.name = "Last Caress";
    d.radius = 350;
    d.range = 450;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "EvelynnR";
    d.spellType = SpellType::Cone;
    return d;
  }());

  // ==== Nunu ==== (Reworked 2018)
  // W - Biggest Snowball Ever! (rolling line skillshot)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nunu";
    d.dangerlevel = 3;
    d.name = "Biggest Snowball Ever!";
    d.projectileSpeed = 600;
    d.radius = 200;
    d.range = 7500;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "NunuW";
    d.spellType = SpellType::Line;
    d.isSpecial = true;
    return d;
  }());

  // R - Absolute Zero (channeled AoE circle)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nunu";
    d.dangerlevel = 4;
    d.name = "Absolute Zero";
    d.radius = 650;
    d.range = 0;
    d.spellDelay = 3000.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "NunuR";
    d.spellType = SpellType::Circular;
    d.isSpecial = true;
    d.defaultOff = true;
    return d;
  }());

  // ==== Ryze ==== (Multiple reworks, current version)
  // Q - Overload (line missile)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ryze";
    d.dangerlevel = 2;
    d.missileName = "RyzeQ";
    d.name = "Overload";
    d.projectileSpeed = 1700;
    d.radius = 55;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "RyzeQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Warwick ==== (Reworked 2017)
  // R - Infinite Duress (long-range dash skillshot)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Warwick";
    d.dangerlevel = 4;
    d.name = "Infinite Duress";
    d.projectileSpeed = 1800;
    d.radius = 80;
    d.range = 3000;
    d.spellDelay = 100.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "WarwickR";
    d.spellType = SpellType::Line;
    d.fixedRange = false;
    d.isSpecial = true;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ==== Swain ==== (Q was missing — Death's Hand, cone)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Swain";
    d.dangerlevel = 2;
    d.name = "Death's Hand";
    d.radius = 125;
    d.range = 725;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SwainQ";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  // ==== XinZhao ====
  // W - Wind Becomes Lightning (thrust — line skillshot)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "XinZhao";
    d.dangerlevel = 2;
    d.name = "Wind Becomes Lightning";
    d.radius = 80;
    d.range = 900;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "XinZhaoW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // R - Crescent Guard (circular knockback)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "XinZhao";
    d.dangerlevel = 3;
    d.name = "Crescent Guard";
    d.radius = 450;
    d.range = 450;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "XinZhaoR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Camille ====
  // E - Hookshot (wall dash → stun line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Camille";
    d.dangerlevel = 3;
    d.name = "Hookshot";
    d.projectileSpeed = 1900;
    d.radius = 60;
    d.range = 800;
    d.spellDelay = 100.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "CamilleE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.isSpecial = true;
    return d;
  }());

  // W - Tactical Sweep (cone)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Camille";
    d.dangerlevel = 2;
    d.name = "Tactical Sweep";
    d.radius = 200;
    d.range = 610;
    d.spellDelay = 750.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "CamilleW";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  // ==== Kayn ====
  // W - Blade's Reach (line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Kayn";
    d.dangerlevel = 2;
    d.name = "Blade's Reach";
    d.radius = 80;
    d.range = 700;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "KaynW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Pyke ====
  // Q - Bone Skewer (hold — line skillshot)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Pyke";
    d.dangerlevel = 3;
    d.missileName = "PykeQMelee";
    d.name = "Bone Skewer";
    d.projectileSpeed = 2000;
    d.radius = 70;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "PykeQRange";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // E - Phantom Undertow (dash → returning stun line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Pyke";
    d.dangerlevel = 3;
    d.name = "Phantom Undertow";
    d.projectileSpeed = 3000;
    d.radius = 110;
    d.range = 550;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "PykeE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.isSpecial = true;
    return d;
  }());

  // R - Death from Below (X-shaped AoE)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Pyke";
    d.dangerlevel = 4;
    d.name = "Death from Below";
    d.radius = 250;
    d.range = 750;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "PykeR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Qiyana ====
  // Q - Elemental Wrath (line skillshot)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Qiyana";
    d.dangerlevel = 2;
    d.name = "Elemental Wrath";
    d.projectileSpeed = 1600;
    d.radius = 60;
    d.range = 1025;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "QiyanaQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // R - Supreme Display of Talent (line knockback)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Qiyana";
    d.dangerlevel = 4;
    d.name = "Supreme Display of Talent";
    d.projectileSpeed = 2000;
    d.radius = 190;
    d.range = 950;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "QiyanaR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Senna ====
  // Q - Piercing Darkness (line through units)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Senna";
    d.dangerlevel = 1;
    d.name = "Piercing Darkness";
    d.radius = 60;
    d.range = 1300;
    d.spellDelay = 400.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SennaQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.defaultOff = true;
    return d;
  }());

  // W - Last Embrace (line → root zone)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Senna";
    d.dangerlevel = 3;
    d.missileName = "SennaW";
    d.name = "Last Embrace";
    d.projectileSpeed = 1150;
    d.radius = 60;
    d.range = 1300;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "SennaW";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // R - Dawning Shadow (global line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Senna";
    d.dangerlevel = 3;
    d.name = "Dawning Shadow";
    d.projectileSpeed = 20000;
    d.radius = 180;
    d.range = 25000;
    d.spellDelay = 1000.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "SennaR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Diana ====
  // Q - Crescent Strike (arc skillshot)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Diana";
    d.dangerlevel = 2;
    d.name = "Crescent Strike";
    d.projectileSpeed = 1900;
    d.radius = 100;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "DianaQ";
    d.spellType = SpellType::Arc;
    d.isSpecial = true;
    return d;
  }());

  // R - Moonfall (AoE pull then slam)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Diana";
    d.dangerlevel = 4;
    d.name = "Moonfall";
    d.radius = 475;
    d.range = 475;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "DianaR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Ornn ====
  // Q - Volcanic Rupture (line + pillar)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ornn";
    d.dangerlevel = 2;
    d.name = "Volcanic Rupture";
    d.projectileSpeed = 1800;
    d.radius = 65;
    d.range = 800;
    d.spellDelay = 300.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "OrnnQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // E - Searing Charge (dash line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ornn";
    d.dangerlevel = 3;
    d.name = "Searing Charge";
    d.projectileSpeed = 1650;
    d.radius = 150;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "OrnnE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // R - Call of the Forge God (line → returns)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ornn";
    d.dangerlevel = 4;
    d.name = "Call of the Forge God";
    d.projectileSpeed = 1650;
    d.radius = 200;
    d.range = 2500;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "OrnnR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.isSpecial = true;
    return d;
  }());

  // ==== Zoe ====
  // Q - Paddle Star (line → redirect)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zoe";
    d.dangerlevel = 3;
    d.missileName = "ZoeQMissile";
    d.name = "Paddle Star";
    d.projectileSpeed = 1200;
    d.radius = 50;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "ZoeQ";
    d.spellType = SpellType::Line;
    d.isSpecial = true;
    return d;
  }());

  // E - Sleepy Trouble Bubble (line → AoE trap)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Zoe";
    d.dangerlevel = 4;
    d.missileName = "ZoeEMissile";
    d.name = "Sleepy Trouble Bubble";
    d.projectileSpeed = 1700;
    d.radius = 60;
    d.range = 800;
    d.spellDelay = 300.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "ZoeE";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Neeko ====
  // Q - Blooming Burst (circle → explosions)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Neeko";
    d.dangerlevel = 2;
    d.name = "Blooming Burst";
    d.projectileSpeed = 2000;
    d.radius = 200;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "NeekoQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // E - Tangle-Barbs (line root)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Neeko";
    d.dangerlevel = 3;
    d.missileName = "NeekoE";
    d.name = "Tangle-Barbs";
    d.projectileSpeed = 1300;
    d.radius = 70;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "NeekoE";
    d.spellType = SpellType::Line;
    return d;
  }());

  // R - Pop Blossom (circle stun)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Neeko";
    d.dangerlevel = 4;
    d.name = "Pop Blossom";
    d.radius = 600;
    d.range = 600;
    d.spellDelay = 1250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "NeekoR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Sylas ====
  // Q - Chain Lash (line + circle detonation)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sylas";
    d.dangerlevel = 2;
    d.name = "Chain Lash";
    d.projectileSpeed = 1750;
    d.radius = 70;
    d.range = 775;
    d.spellDelay = 400.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SylasQ";
    d.spellType = SpellType::Line;
    d.hasEndExplosion = true;
    d.secondaryRadius = 200;
    d.fixedRange = true;
    return d;
  }());

  // E2 - Abscond/Abduct (line skillshot dash)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sylas";
    d.dangerlevel = 3;
    d.name = "Abscond / Abduct";
    d.projectileSpeed = 1800;
    d.radius = 60;
    d.range = 850;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "SylasE2";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ==== Rakan ====
  // Q - Gleaming Quill (line missile)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Rakan";
    d.dangerlevel = 1;
    d.name = "Gleaming Quill";
    d.projectileSpeed = 1850;
    d.radius = 65;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "RakanQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // W - Grand Entrance (dash → circle knockup)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Rakan";
    d.dangerlevel = 3;
    d.name = "Grand Entrance";
    d.projectileSpeed = 1700;
    d.radius = 275;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "RakanW";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Jhin ====
  // W - Deadly Flourish (long line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Jhin";
    d.dangerlevel = 3;
    d.name = "Deadly Flourish";
    d.projectileSpeed = 5000;
    d.radius = 40;
    d.range = 2550;
    d.spellDelay = 750.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "JhinW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // E - Captive Audience (trap — circle)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Jhin";
    d.dangerlevel = 1;
    d.name = "Captive Audience";
    d.radius = 130;
    d.range = 750;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "JhinE";
    d.spellType = SpellType::Circular;
    d.hasTrap = true;
    d.defaultOff = true;
    return d;
  }());

  // R - Curtain Call (long range line shots)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Jhin";
    d.dangerlevel = 3;
    d.missileName = "JhinRShotMis";
    d.name = "Curtain Call";
    d.projectileSpeed = 5000;
    d.radius = 80;
    d.range = 3500;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "JhinRShot";
    d.spellType = SpellType::Line;
    d.isSpecial = true;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ==== Kindred ====
  // Q - Dance of Arrows (not really evadable, skip)
  // W - Wolf's Frenzy (zone, not evadable)
  // E - Mounting Dread (targeted, not evadable)

  // ==== Illaoi ====
  // Q - Tentacle Smash (line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Illaoi";
    d.dangerlevel = 2;
    d.name = "Tentacle Smash";
    d.radius = 100;
    d.range = 850;
    d.spellDelay = 750.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "IllaoiQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // E - Test of Spirit (line — pulls spirit)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Illaoi";
    d.dangerlevel = 3;
    d.name = "Test of Spirit";
    d.projectileSpeed = 1900;
    d.radius = 50;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "IllaoiE";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // R - Leap of Faith (circle slam)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Illaoi";
    d.dangerlevel = 4;
    d.name = "Leap of Faith";
    d.radius = 450;
    d.range = 450;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "IllaoiR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Kled ====
  // Q - Bear Trap on a Rope (line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Kled";
    d.dangerlevel = 2;
    d.name = "Bear Trap on a Rope";
    d.projectileSpeed = 1600;
    d.radius = 45;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "KledQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ==== Xayah ====
  // Q - Double Daggers (line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Xayah";
    d.dangerlevel = 1;
    d.name = "Double Daggers";
    d.projectileSpeed = 2075;
    d.radius = 45;
    d.range = 1100;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "XayahQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // E - Bladecaller (recall feathers — cone)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Xayah";
    d.dangerlevel = 3;
    d.name = "Bladecaller";
    d.radius = 100;
    d.range = 2000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "XayahE";
    d.spellType = SpellType::Line;
    d.isSpecial = true;
    return d;
  }());

  // R - Featherstorm (cone + untargetable)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Xayah";
    d.dangerlevel = 3;
    d.name = "Featherstorm";
    d.radius = 200;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "XayahR";
    d.spellType = SpellType::Cone;
    return d;
  }());

  // ==== Aphelios ====
  // R - Moonlight Vigil (line → AoE based on gun)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Aphelios";
    d.dangerlevel = 4;
    d.missileName = "ApheliosR";
    d.name = "Moonlight Vigil";
    d.projectileSpeed = 1600;
    d.radius = 125;
    d.range = 1300;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "ApheliosR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ================================================================
  //  A-F Champions (missing from legacy database)
  // ================================================================

  // ==== Aatrox ==== (Reworked 2018)
  // Q1 - The Darkin Blade (1st cast — wide rectangle)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Aatrox";
    d.dangerlevel = 3;
    d.name = "The Darkin Blade (Q1)";
    d.radius = 200;
    d.range = 625;
    d.spellDelay = 600.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "AatroxQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.isSpecial = true;
    return d;
  }());

  // Q2 - The Darkin Blade (2nd cast)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Aatrox";
    d.dangerlevel = 3;
    d.name = "The Darkin Blade (Q2)";
    d.radius = 150;
    d.range = 500;
    d.spellDelay = 600.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "AatroxQ2";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    d.isSpecial = true;
    return d;
  }());

  // Q3 - The Darkin Blade (3rd cast — circular slam)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Aatrox";
    d.dangerlevel = 4;
    d.name = "The Darkin Blade (Q3)";
    d.radius = 300;
    d.range = 200;
    d.spellDelay = 600.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "AatroxQ3";
    d.spellType = SpellType::Circular;
    d.isSpecial = true;
    return d;
  }());

  // W - Infernal Chains
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Aatrox";
    d.dangerlevel = 2;
    d.missileName = "AatroxW";
    d.name = "Infernal Chains";
    d.projectileSpeed = 1800;
    d.radius = 80;
    d.range = 825;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "AatroxW";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Ahri ====
  // Q - Orb of Deception
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ahri";
    d.dangerlevel = 2;
    d.missileName = "AhriOrbMissile";
    d.name = "Orb of Deception";
    d.projectileSpeed = 2500;
    d.radius = 100;
    d.range = 880;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "AhriOrbofDeception";
    d.spellType = SpellType::Line;
    return d;
  }());

  // E - Charm
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ahri";
    d.dangerlevel = 3;
    d.missileName = "AhriSeduceMissile";
    d.name = "Charm";
    d.projectileSpeed = 1550;
    d.radius = 60;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "AhriSeduce";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Amumu ====
  // Q - Bandage Toss
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Amumu";
    d.dangerlevel = 3;
    d.missileName = "SadMummyBandageToss";
    d.name = "Bandage Toss";
    d.projectileSpeed = 2000;
    d.radius = 80;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "BandageToss";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // R - Curse of the Sad Mummy
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Amumu";
    d.dangerlevel = 4;
    d.name = "Curse of the Sad Mummy";
    d.radius = 550;
    d.range = 550;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "CurseoftheSadMummy";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Anivia ====
  // Q - Flash Frost
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Anivia";
    d.dangerlevel = 3;
    d.missileName = "FlashFrostMissile";
    d.name = "Flash Frost";
    d.projectileSpeed = 850;
    d.radius = 110;
    d.range = 1075;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "FlashFrostSpell";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Annie ====
  // W - Incinerate (cone)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Annie";
    d.dangerlevel = 2;
    d.name = "Incinerate";
    d.radius = 250;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "AnnieW";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  // R - Summon: Tibbers
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Annie";
    d.dangerlevel = 4;
    d.name = "Summon: Tibbers";
    d.radius = 290;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "AnnieR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Ashe ====
  // W - Volley (cone of arrows)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ashe";
    d.dangerlevel = 1;
    d.missileName = "AsheShotMissile";
    d.name = "Volley";
    d.projectileSpeed = 2000;
    d.radius = 20;
    d.range = 1200;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "Volley";
    d.spellType = SpellType::Cone;
    d.isSpecial = true;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // R - Enchanted Crystal Arrow (global line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ashe";
    d.dangerlevel = 4;
    d.missileName = "EnchantedCrystalArrow";
    d.name = "Enchanted Crystal Arrow";
    d.projectileSpeed = 1600;
    d.radius = 130;
    d.range = 25000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "EnchantedCrystalArrow";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ==== Azir ====
  // Q - Conquering Sands
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Azir";
    d.dangerlevel = 2;
    d.name = "Conquering Sands";
    d.projectileSpeed = 1600;
    d.radius = 70;
    d.range = 740;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "AzirQ";
    d.spellType = SpellType::Line;
    d.isSpecial = true;
    return d;
  }());

  // R - Emperor's Divide (wall push)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Azir";
    d.dangerlevel = 4;
    d.name = "Emperor's Divide";
    d.projectileSpeed = 1400;
    d.radius = 250;
    d.range = 500;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "AzirR";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Bard ====
  // Q - Cosmic Binding
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Bard";
    d.dangerlevel = 3;
    d.missileName = "BardQMissile";
    d.name = "Cosmic Binding";
    d.projectileSpeed = 1500;
    d.radius = 60;
    d.range = 950;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "BardQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // R - Tempered Fate
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Bard";
    d.dangerlevel = 3;
    d.name = "Tempered Fate";
    d.radius = 350;
    d.range = 3400;
    d.spellDelay = 650.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "BardR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Blitzcrank ====
  // Q - Rocket Grab
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Blitzcrank";
    d.dangerlevel = 4;
    d.missileName = "RocketGrabMissile";
    d.name = "Rocket Grab";
    d.projectileSpeed = 1800;
    d.radius = 70;
    d.range = 1150;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "RocketGrab";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // R - Static Field (circle)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Blitzcrank";
    d.dangerlevel = 3;
    d.name = "Static Field";
    d.radius = 600;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "StaticField";
    d.spellType = SpellType::Circular;
    d.defaultOff = true;
    return d;
  }());

  // ==== Brand ====
  // Q - Sear
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Brand";
    d.dangerlevel = 3;
    d.missileName = "BrandQMissile";
    d.name = "Sear";
    d.projectileSpeed = 1600;
    d.radius = 60;
    d.range = 1050;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "BrandQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // W - Pillar of Flame
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Brand";
    d.dangerlevel = 2;
    d.name = "Pillar of Flame";
    d.radius = 250;
    d.range = 900;
    d.spellDelay = 625.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "BrandW";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Braum ====
  // Q - Winter's Bite
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Braum";
    d.dangerlevel = 2;
    d.missileName = "BraumQMissile";
    d.name = "Winter's Bite";
    d.projectileSpeed = 1700;
    d.radius = 60;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "BraumQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // R - Glacial Fissure
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Braum";
    d.dangerlevel = 4;
    d.name = "Glacial Fissure";
    d.projectileSpeed = 1125;
    d.radius = 115;
    d.range = 1250;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "BraumRWrapper";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Caitlyn ====
  // Q - Piltover Peacemaker
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Caitlyn";
    d.dangerlevel = 2;
    d.missileName = "CaitlynPiltoverPeacemaker";
    d.name = "Piltover Peacemaker";
    d.projectileSpeed = 2200;
    d.radius = 90;
    d.range = 1250;
    d.spellDelay = 625.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "CaitlynPiltoverPeacemaker";
    d.spellType = SpellType::Line;
    return d;
  }());

  // W - Yordle Snap Trap
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Caitlyn";
    d.dangerlevel = 1;
    d.name = "Yordle Snap Trap";
    d.radius = 75;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "CaitlynW";
    d.spellType = SpellType::Circular;
    d.hasTrap = true;
    d.defaultOff = true;
    return d;
  }());

  // E - 90 Caliber Net
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Caitlyn";
    d.dangerlevel = 1;
    d.missileName = "CaitlynEntrapmentMissile";
    d.name = "90 Caliber Net";
    d.projectileSpeed = 1600;
    d.radius = 70;
    d.range = 800;
    d.spellDelay = 150.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "CaitlynEntrapment";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Cassiopeia ====
  // Q - Noxious Blast
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Cassiopeia";
    d.dangerlevel = 2;
    d.name = "Noxious Blast";
    d.radius = 200;
    d.range = 850;
    d.spellDelay = 750.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "CassiopeiaQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // W - Miasma
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Cassiopeia";
    d.dangerlevel = 2;
    d.name = "Miasma";
    d.projectileSpeed = 2500;
    d.radius = 160;
    d.range = 700;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "CassiopeiaW";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // R - Petrifying Gaze (cone)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Cassiopeia";
    d.dangerlevel = 4;
    d.name = "Petrifying Gaze";
    d.radius = 250;
    d.range = 825;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "CassiopeiaR";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  // ==== ChoGath ====
  // Q - Rupture
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Chogath";
    d.dangerlevel = 3;
    d.name = "Rupture";
    d.radius = 250;
    d.range = 950;
    d.spellDelay = 1200.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "Rupture";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // W - Feral Scream (cone)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Chogath";
    d.dangerlevel = 2;
    d.name = "Feral Scream";
    d.radius = 250;
    d.range = 650;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "FeralScream";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  // ==== Corki ====
  // Q - Phosphorus Bomb
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Corki";
    d.dangerlevel = 2;
    d.missileName = "PhosphorusBombMissile";
    d.name = "Phosphorus Bomb";
    d.projectileSpeed = 1000;
    d.radius = 250;
    d.range = 825;
    d.spellDelay = 300.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "PhosphorusBomb";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // R - Missile Barrage
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Corki";
    d.dangerlevel = 2;
    d.missileName = "MissileBarrageMissile";
    d.name = "Missile Barrage";
    d.projectileSpeed = 2000;
    d.radius = 40;
    d.range = 1300;
    d.spellDelay = 175.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "MissileBarrage";
    d.extraMissileNames = {"MissileBarrageMissile2"};
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Darius ====
  // Q - Decimate (circle around self)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Darius";
    d.dangerlevel = 2;
    d.name = "Decimate";
    d.radius = 425;
    d.range = 425;
    d.spellDelay = 750.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "DariusCleave";
    d.spellType = SpellType::Circular;
    d.defaultOff = true;
    return d;
  }());

  // E - Apprehend (cone pull)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Darius";
    d.dangerlevel = 3;
    d.name = "Apprehend";
    d.radius = 200;
    d.range = 535;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "DariusAxeGrabCone";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  // ==== Draven ====
  // E - Stand Aside
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Draven";
    d.dangerlevel = 3;
    d.missileName = "DravenDoubleShotMissile";
    d.name = "Stand Aside";
    d.projectileSpeed = 1400;
    d.radius = 130;
    d.range = 1050;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "DravenDoubleShot";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // R - Whirling Death (global line)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Draven";
    d.dangerlevel = 4;
    d.missileName = "DravenRCast";
    d.name = "Whirling Death";
    d.projectileSpeed = 2000;
    d.radius = 160;
    d.range = 25000;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "DravenRCast";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Ekko ====
  // Q - Timewinder
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ekko";
    d.dangerlevel = 2;
    d.missileName = "EkkoQMis";
    d.name = "Timewinder";
    d.projectileSpeed = 1650;
    d.radius = 60;
    d.range = 1075;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "EkkoQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  // W - Parallel Convergence (delayed circle)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ekko";
    d.dangerlevel = 3;
    d.name = "Parallel Convergence";
    d.radius = 400;
    d.range = 1600;
    d.spellDelay = 3750.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "EkkoW";
    d.spellType = SpellType::Circular;
    d.isSpecial = true;
    d.defaultOff = true;
    return d;
  }());

  // ==== Elise ====
  // E - Cocoon (human form)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Elise";
    d.dangerlevel = 3;
    d.missileName = "EliseHumanE";
    d.name = "Cocoon";
    d.projectileSpeed = 1600;
    d.radius = 55;
    d.range = 1075;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "EliseHumanE";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Ezreal ====
  // Q - Mystic Shot
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ezreal";
    d.dangerlevel = 1;
    d.missileName = "EzrealMysticShotMissile";
    d.name = "Mystic Shot";
    d.projectileSpeed = 2000;
    d.radius = 60;
    d.range = 1150;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "EzrealMysticShot";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // W - Essence Flux
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ezreal";
    d.dangerlevel = 1;
    d.missileName = "EzrealW";
    d.name = "Essence Flux";
    d.projectileSpeed = 1700;
    d.radius = 60;
    d.range = 1150;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "EzrealW";
    d.spellType = SpellType::Line;
    return d;
  }());

  // R - Trueshot Barrage (global)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ezreal";
    d.dangerlevel = 3;
    d.missileName = "EzrealR";
    d.name = "Trueshot Barrage";
    d.projectileSpeed = 2000;
    d.radius = 160;
    d.range = 25000;
    d.spellDelay = 1000.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "EzrealR";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Fiora ====
  // W - Riposte
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Fiora";
    d.dangerlevel = 2;
    d.name = "Riposte";
    d.projectileSpeed = 3200;
    d.radius = 70;
    d.range = 750;
    d.spellDelay = 750.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "FioraW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Fizz ====
  // R - Chum the Waters
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Fizz";
    d.dangerlevel = 4;
    d.missileName = "FizzRMissile";
    d.name = "Chum the Waters";
    d.projectileSpeed = 1300;
    d.radius = 120;
    d.range = 1300;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "FizzR";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ================================================================
  //  G-L Champions (missing from legacy database)
  // ================================================================

  // ==== Gangplank ====
  // E - Powder Keg (explosions - circle)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Gangplank";
    d.dangerlevel = 3;
    d.name = "Powder Keg";
    d.radius = 345; // Explosion radius
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "GangplankE";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // R - Cannon Barrage
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Gangplank";
    d.dangerlevel = 4;
    d.name = "Cannon Barrage";
    d.radius = 525; // Actually 600 outer, 200 inner waves
    d.range = 25000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "GangplankR";
    d.spellType = SpellType::Circular;
    d.defaultOff = true;
    return d;
  }());

  // ==== Gnar ====
  // Q - Boomerang Throw (Mini) / Boulder Toss (Mega)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Gnar";
    d.dangerlevel = 2;
    d.missileName = "GnarQMissile"; // Boomerang
    d.name = "Boomerang Throw";
    d.projectileSpeed = 2500;
    d.radius = 55;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "GnarQ";
    d.extraSpellNames = {"GnarBigQ"}; // Mega boulder
    d.extraMissileNames = {"GnarBigQMissile"};
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // W - Wallop (Mega)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Gnar";
    d.dangerlevel = 3;
    d.name = "Wallop";
    d.radius = 150;
    d.range = 600;
    d.spellDelay = 600.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "GnarBigW";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // R - GNAR! (Mega)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Gnar";
    d.dangerlevel = 4;
    d.name = "GNAR!";
    d.radius = 475;
    d.range = 475;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "GnarR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Gragas ====
  // Q - Barrel Roll
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Gragas";
    d.dangerlevel = 2;
    d.name = "Barrel Roll";
    d.projectileSpeed = 1000;
    d.radius = 250;
    d.range = 850;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "GragasQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // E - Body Slam
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Gragas";
    d.dangerlevel = 3;
    d.name = "Body Slam";
    d.projectileSpeed = 900;
    d.radius = 50;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "GragasE";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    d.fixedRange = true;
    return d;
  }());

  // R - Explosive Cask
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Gragas";
    d.dangerlevel = 4;
    d.missileName = "GragasRBoom";
    d.name = "Explosive Cask";
    d.projectileSpeed = 1000;
    d.radius = 400;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "GragasR";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Graves ====
  // Q - End of the Line
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Graves";
    d.dangerlevel = 2;
    d.missileName = "GravesQLineSpell";
    d.name = "End of the Line";
    d.projectileSpeed = 3000;
    d.radius = 40;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "GravesQLineSpell";
    d.spellType = SpellType::Line;
    d.isSpecial = true; // Returns
    return d;
  }());

  // W - Smoke Grenade
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Graves";
    d.dangerlevel = 1;
    d.missileName = "GravesSmokeGrenadeBoom";
    d.name = "Smoke Grenade";
    d.projectileSpeed = 1500;
    d.radius = 250;
    d.range = 950;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "GravesSmokeGrenade";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // R - Collateral Damage
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Graves";
    d.dangerlevel = 4;
    d.missileName = "GravesChargeShot";
    d.name = "Collateral Damage";
    d.projectileSpeed = 2100;
    d.radius = 100;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "GravesChargeShot";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Hecarim ====
  // R - Onslaught of Shadows
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Hecarim";
    d.dangerlevel = 4;
    d.name = "Onslaught of Shadows";
    d.projectileSpeed = 1000;
    d.radius = 300;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "HecarimUlt";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Heimerdinger ====
  // W - Hextech Micro-Rockets
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Heimerdinger";
    d.dangerlevel = 2;
    d.missileName = "HeimerdingerWAttack2";
    d.name = "Hextech Micro-Rockets";
    d.projectileSpeed = 1200;
    d.radius = 70;
    d.range = 1325;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "HeimerdingerW";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // E - CH-2 Electron Storm Grenade
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Heimerdinger";
    d.dangerlevel = 3;
    d.missileName = "HeimerdingerEAttack2";
    d.name = "CH-2 Electron Storm Grenade";
    d.projectileSpeed = 1200;
    d.radius = 250;
    d.range = 975;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "HeimerdingerE";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Ivern ====
  // Q - Rootcaller
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Ivern";
    d.dangerlevel = 3;
    d.missileName = "IvernQ";
    d.name = "Rootcaller";
    d.projectileSpeed = 1300;
    d.radius = 80;
    d.range = 1130;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "IvernQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Janna ====
  // Q - Howling Gale
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Janna";
    d.dangerlevel = 3;
    d.missileName = "HowlingGaleSpell";
    d.name = "Howling Gale";
    d.projectileSpeed = 900;
    d.radius = 120;
    d.range = 1750; // Variable range/speed but this is max
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "HowlingGale";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== JarvanIV ====
  // Q - Dragon Strike
  Spells.push_back([]() {
    SpellData d;
    d.charName = "JarvanIV";
    d.dangerlevel = 2;
    d.name = "Dragon Strike";
    d.radius = 70;
    d.range = 770;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "JarvanIVDragonStrike";
    d.spellType = SpellType::Line;
    return d;
  }());

  // E - Demacian Standard
  Spells.push_back([]() {
    SpellData d;
    d.charName = "JarvanIV";
    d.dangerlevel = 1;
    d.name = "Demacian Standard";
    d.radius = 150;
    d.range = 860;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "JarvanIVDemacianStandard";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Jayce ====
  // Q - Shock Blast (Ranged)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Jayce";
    d.dangerlevel = 2;
    d.missileName = "JayceShockBlastMis";
    d.name = "Shock Blast";
    d.projectileSpeed = 1450;
    d.radius = 70;
    d.range = 1050; // Without gate
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "JayceShockBlast";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Jinx ====
  // W - Zap!
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Jinx";
    d.dangerlevel = 2;
    d.missileName = "JinxWMissile";
    d.name = "Zap!";
    d.projectileSpeed = 3300;
    d.radius = 60;
    d.range = 1500;
    d.spellDelay = 600.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "JinxW";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    d.fixedRange = true;
    return d;
  }());

  // E - Flame Chompers!
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Jinx";
    d.dangerlevel = 3;
    d.name = "Flame Chompers!";
    d.radius = 120;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "JinxE";
    d.spellType = SpellType::Circular;
    d.hasTrap = true;
    d.defaultOff = true;
    return d;
  }());

  // R - Super Mega Death Rocket!
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Jinx";
    d.dangerlevel = 4;
    d.missileName = "JinxR";
    d.name = "Super Mega Death Rocket!";
    d.projectileSpeed = 1700;
    d.radius = 140; // Collision radius is 140
    d.range = 25000;
    d.spellDelay = 600.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "JinxR";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ==== Kalista ====
  // Q - Pierce
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Kalista";
    d.dangerlevel = 2;
    d.missileName = "KalistaMysticShot"; // KalistaQ
    d.name = "Pierce";
    d.projectileSpeed = 2400;
    d.radius = 40;
    d.range = 1150;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "KalistaMysticShot";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Karma ====
  // Q - Inner Flame
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Karma";
    d.dangerlevel = 2;
    d.missileName = "KarmaQMissile";
    d.name = "Inner Flame";
    d.projectileSpeed = 1700;
    d.radius = 60;
    d.range = 950;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "KarmaQ";
    d.extraMissileNames = {"KarmaQMissileMantra"};
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Karthus ====
  // Q - Lay Waste
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Karthus";
    d.dangerlevel = 1;
    d.name = "Lay Waste";
    d.radius = 160;
    d.range = 875;
    d.spellDelay = 625.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "KarthusLayWasteA1";
    d.extraSpellNames = {"KarthusLayWasteA2", "KarthusLayWasteA3",
                         "KarthusLayWasteD1", "KarthusLayWasteD2",
                         "KarthusLayWasteD3"};
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Kassadin ====
  // E - Force Pulse
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Kassadin";
    d.dangerlevel = 2;
    d.name = "Force Pulse";
    d.radius = 280;
    d.range = 400;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "ForcePulse";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  // R - Riftwalk
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Kassadin";
    d.dangerlevel = 3;
    d.name = "Riftwalk";
    d.radius = 150;
    d.range = 500;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "RiftWalk";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Kayle ====
  // Q - Radiant Blast
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Kayle";
    d.dangerlevel = 2;
    d.missileName = "KayleQ";
    d.name = "Radiant Blast";
    d.projectileSpeed = 1600;
    d.radius = 75;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "KayleQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Kennen ====
  // Q - Thundering Shuriken
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Kennen";
    d.dangerlevel = 2;
    d.missileName = "KennenShurikenHurlMissile1";
    d.name = "Thundering Shuriken";
    d.projectileSpeed = 1700;
    d.radius = 50;
    d.range = 1050;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "KennenShurikenHurlMissile1";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Kha'Zix ====
  // W - Void Spike
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Khazix";
    d.dangerlevel = 2;
    d.missileName = "KhazixW";
    d.name = "Void Spike";
    d.projectileSpeed = 1700;
    d.radius = 70;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "KhazixW";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Kog'Maw ====
  // E - Void Ooze
  Spells.push_back([]() {
    SpellData d;
    d.charName = "KogMaw";
    d.dangerlevel = 2;
    d.missileName = "KogMawVoidOozeMissile";
    d.name = "Void Ooze";
    d.projectileSpeed = 1400;
    d.radius = 120;
    d.range = 1360;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "KogMawVoidOoze";
    d.spellType = SpellType::Line;
    return d;
  }());

  // R - Living Artillery
  Spells.push_back([]() {
    SpellData d;
    d.charName = "KogMaw";
    d.dangerlevel = 2;
    d.name = "Living Artillery";
    d.radius = 240;
    d.range = 1800; // max range
    d.spellDelay = 600.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "KogMawLivingArtillery";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== LeBlanc ====
  // E - Ethereal Chains
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Leblanc";
    d.dangerlevel = 3;
    d.missileName = "LeblancE";
    d.name = "Ethereal Chains";
    d.projectileSpeed = 1750;
    d.radius = 55;
    d.range = 925;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "LeblancE";
    d.extraSpellNames = {"LeblancRE"};
    d.extraMissileNames = {"LeblancRE"};
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Lee Sin ====
  // Q - Sonic Wave
  Spells.push_back([]() {
    SpellData d;
    d.charName = "LeeSin";
    d.dangerlevel = 3;
    d.missileName = "BlindMonkQOne";
    d.name = "Sonic Wave";
    d.projectileSpeed = 1800;
    d.radius = 60;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "BlindMonkQOne";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Leona ====
  // E - Zenith Blade
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Leona";
    d.dangerlevel = 3;
    d.missileName = "LeonaZenithBladeMissile";
    d.name = "Zenith Blade";
    d.projectileSpeed = 2000;
    d.radius = 70;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "LeonaZenithBlade";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // R - Solar Flare
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Leona";
    d.dangerlevel = 4;
    d.name = "Solar Flare";
    d.radius = 250;
    d.range = 1200;
    d.spellDelay = 625.0f; // Delay
    d.spellKey = SpellSlot::R;
    d.spellName = "LeonaSolarFlare";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Lissandra ====
  // Q - Ice Shard
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lissandra";
    d.dangerlevel = 2;
    d.missileName = "LissandraQMissile";
    d.name = "Ice Shard";
    d.projectileSpeed = 2200;
    d.radius = 75;
    d.range = 825; // Shatters to 825
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "LissandraQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  // E - Glacial Path
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lissandra";
    d.dangerlevel = 2;
    d.missileName = "LissandraEMissile";
    d.name = "Glacial Path";
    d.projectileSpeed = 850;
    d.radius = 125;
    d.range = 1050;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "LissandraE";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Lucian ====
  // Q - Piercing Light
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lucian";
    d.dangerlevel = 2;
    d.name = "Piercing Light";
    d.radius = 65;
    d.range = 1000;        // Total length
    d.spellDelay = 400.0f; // scales with level but typical
    d.spellKey = SpellSlot::Q;
    d.spellName = "LucianQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // W - Ardent Blaze
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lucian";
    d.dangerlevel = 1;
    d.missileName = "LucianWMissile";
    d.name = "Ardent Blaze";
    d.projectileSpeed = 1600;
    d.radius = 55;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "LucianW";
    d.spellType = SpellType::Line;
    return d;
  }());

  // R - The Culling
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lucian";
    d.dangerlevel = 3;
    d.missileName = "LucianRMissile";
    d.name = "The Culling";
    d.projectileSpeed = 2600;
    d.radius = 110;
    d.range = 1200;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "LucianR";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    d.defaultOff = true;
    return d;
  }());

  // ==== Lulu ====
  // Q - Glitterlance
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lulu";
    d.dangerlevel = 2;
    d.missileName = "LuluQMissile";
    d.name = "Glitterlance";
    d.projectileSpeed = 1450;
    d.radius = 60;
    d.range = 925;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "LuluQ";
    d.extraMissileNames = {"LuluQMissileTwo"};
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Lux ====
  // Q - Light Binding
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lux";
    d.dangerlevel = 3;
    d.missileName = "LuxLightBindingDummy";
    d.name = "Light Binding";
    d.projectileSpeed = 1200;
    d.radius = 70;
    d.range = 1175;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "LuxLightBinding";
    d.spellType = SpellType::Line;
    return d;
  }());

  // E - Lucent Singularity
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lux";
    d.dangerlevel = 2;
    d.missileName = "LuxLightStrikeKugel";
    d.name = "Lucent Singularity";
    d.projectileSpeed = 1200;
    d.radius = 310;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "LuxLightStrikeKugel";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // R - Final Spark
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Lux";
    d.dangerlevel = 4;
    d.name = "Final Spark";
    d.radius = 150;
    d.range = 3340;
    d.spellDelay = 1000.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "LuxMaliceCannon";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ================================================================
  //  M-R Champions (missing from legacy database + supplementary)
  // ================================================================

  // ==== Malphite ====
  // R - Unstoppable Force
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Malphite";
    d.dangerlevel = 4;
    d.name = "Unstoppable Force";
    d.projectileSpeed =
        1500; // Actually variable based on movement speed, but baseline
    d.radius = 300;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "UFSlash";
    d.spellType = SpellType::Circular;
    d.fixedRange = true;
    return d;
  }());

  // ==== Malzahar ====
  // Q - Call of the Void
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Malzahar";
    d.dangerlevel = 2;
    d.name = "Call of the Void";
    d.radius = 85;
    d.range = 900;
    d.spellDelay = 400.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "MalzaharQ";
    d.spellType = SpellType::Line;
    d.isPerpendicular = true;
    d.secondaryRadius = 400; // Total width
    d.fixedRange = true;
    return d;
  }());

  // ==== Maokai ====
  // Q - Bramble Smash
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Maokai";
    d.dangerlevel = 2;
    d.name = "Bramble Smash";
    d.radius = 110;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "MaokaiQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // E - Sapling Toss
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Maokai";
    d.dangerlevel = 1;
    d.missileName = "MaokaiSaplingMissile";
    d.name = "Sapling Toss";
    d.projectileSpeed = 1500;
    d.radius = 225;
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "MaokaiE";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Miss Fortune ====
  // Q - Double Up
  Spells.push_back([]() {
    SpellData d;
    d.charName = "MissFortune";
    d.dangerlevel = 2;
    d.name = "Double Up";
    d.projectileSpeed = 1500;
    d.radius = 50;
    d.range = 650;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "MissFortuneRicochetShot";
    d.spellType = SpellType::Cone;
    return d;
  }());

  // E - Make It Rain
  Spells.push_back([]() {
    SpellData d;
    d.charName = "MissFortune";
    d.dangerlevel = 1;
    d.name = "Make It Rain";
    d.radius = 225;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "MissFortuneScattershot";
    d.spellType = SpellType::Circular;
    d.defaultOff = true;
    return d;
  }());

  // R - Bullet Time
  Spells.push_back([]() {
    SpellData d;
    d.charName = "MissFortune";
    d.dangerlevel = 4;
    d.name = "Bullet Time";
    d.radius = 250;
    d.range = 1400;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "MissFortuneBulletTime";
    d.spellType = SpellType::Cone;
    d.fixedRange = true;
    return d;
  }());

  // ==== Morgana ====
  // Q - Dark Binding
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Morgana";
    d.dangerlevel = 3;
    d.missileName = "DarkBindingMissile";
    d.name = "Dark Binding";
    d.projectileSpeed = 1200;
    d.radius = 70;
    d.range = 1175;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "DarkBindingMissile";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Nami ====
  // Q - Aqua Prison
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nami";
    d.dangerlevel = 3;
    d.name = "Aqua Prison";
    d.projectileSpeed =
        2500; // Actually fixed travel time 875ms, approx 250+ time
    d.radius = 200;
    d.range = 875;
    d.spellDelay = 875.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "NamiQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // R - Tidal Wave
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nami";
    d.dangerlevel = 4;
    d.missileName = "NamiRMissile";
    d.name = "Tidal Wave";
    d.projectileSpeed = 850;
    d.radius = 250;
    d.range = 2750;
    d.spellDelay = 500.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "NamiR";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Nautilus ====
  // Q - Dredge Line
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nautilus";
    d.dangerlevel = 3;
    d.missileName = "NautilusAnchorDragMissile";
    d.name = "Dredge Line";
    d.projectileSpeed = 2000;
    d.radius = 90; // Anchor is thick
    d.range = 1100;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "NautilusAnchorDrag";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // E - Riptide
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nautilus";
    d.dangerlevel = 1;
    d.name = "Riptide";
    d.radius = 600;
    d.range = 600;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "NautilusSplashZone";
    d.spellType = SpellType::Circular;
    d.defaultOff = true;
    return d;
  }());

  // ==== Nidalee ====
  // Q - Javelin Toss (Human)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nidalee";
    d.dangerlevel = 2;
    d.missileName = "JavelinToss";
    d.name = "Javelin Toss";
    d.projectileSpeed = 1300;
    d.radius = 40;
    d.range = 1500;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "JavelinToss";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // W - Bushwhack (Trap)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nidalee";
    d.dangerlevel = 1;
    d.name = "Bushwhack";
    d.radius = 100; // Trap radius
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "Bushwhack";
    d.spellType = SpellType::Circular;
    d.hasTrap = true;
    d.defaultOff = true;
    return d;
  }());

  // ==== Nocturne ====
  // Q - Duskbringer
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nocturne";
    d.dangerlevel = 2;
    d.missileName = "NocturneDuskbringer";
    d.name = "Duskbringer";
    d.projectileSpeed = 1400;
    d.radius = 60;
    d.range = 1200;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "NocturneDuskbringer";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Olaf ====
  // Q - Undertow
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Olaf";
    d.dangerlevel = 2;
    d.missileName = "OlafAxeThrowCast";
    d.name = "Undertow";
    d.projectileSpeed = 1600;
    d.radius = 90;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "OlafAxeThrowCast";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Orianna ====
  // Q - Command: Attack
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Orianna";
    d.dangerlevel = 2;
    d.missileName = "OrianaIzuna";
    d.name = "Command: Attack";
    d.projectileSpeed = 1400; // Speed from ball to target
    d.radius = 80;
    d.range = 825;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "OrianaIzunaCommand";
    d.spellType =
        SpellType::Circular; // technically line of movement and circular dest
    d.isSpecial = true;
    return d;
  }());

  // W - Command: Dissonance
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Orianna";
    d.dangerlevel = 2;
    d.name = "Command: Dissonance";
    d.radius = 250;
    d.range = 250; // self-cast from ball
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "OrianaDissonanceCommand";
    d.spellType = SpellType::Circular;
    d.isSpecial = true;
    return d;
  }());

  // R - Command: Shockwave
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Orianna";
    d.dangerlevel = 4;
    d.name = "Command: Shockwave";
    d.radius = 325;
    d.range = 325;         // self-cast from ball
    d.spellDelay = 500.0f; // Delay for shockwave
    d.spellKey = SpellSlot::R;
    d.spellName = "OrianaDetonateCommand";
    d.spellType = SpellType::Circular;
    d.isSpecial = true;
    return d;
  }());

  // ==== Poppy ====
  // Q - Hammer Shock
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Poppy";
    d.dangerlevel = 2;
    d.name = "Hammer Shock";
    d.radius = 100;
    d.range = 430;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "PoppyQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // R - Keeper's Verdict (Missile)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Poppy";
    d.dangerlevel = 4;
    d.missileName = "PoppyRMissile";
    d.name = "Keeper's Verdict";
    d.projectileSpeed = 2000;
    d.radius = 100;
    d.range = 1200;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "PoppyRSpell";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ==== Quinn ====
  // Q - Blinding Assault
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Quinn";
    d.dangerlevel = 2;
    d.missileName = "QuinnQMissile";
    d.name = "Blinding Assault";
    d.projectileSpeed = 1550;
    d.radius = 60;
    d.range = 1050; // Max Range
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "QuinnQ";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Rek'Sai ====
  // Q - Prey Seeker (Burrowed)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "RekSai";
    d.dangerlevel = 2;
    d.missileName = "RekSaiQBurrowedMis";
    d.name = "Prey Seeker";
    d.projectileSpeed = 1950;
    d.radius = 60;
    d.range = 1500;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "RekSaiQBurrowed";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Rengar ====
  // E - Bola Strike
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Rengar";
    d.dangerlevel = 2;
    d.missileName = "RengarE";
    d.name = "Bola Strike";
    d.projectileSpeed = 1500;
    d.radius = 70;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "RengarE";
    d.extraMissileNames = {"RengarEEmp"};
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // ==== Riven ====
  // R - Wind Slash
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Riven";
    d.dangerlevel = 4;
    d.missileName = "RivenWindMullerMis"; // Wind slash missile
    d.name = "Wind Slash";
    d.projectileSpeed = 1600;
    d.radius = 100;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "RivenIzunaBlade"; // Wind slash name internally
    d.spellType = SpellType::Cone;
    return d;
  }());

  // ==== Rumble ====
  // E - Electro Harpoon
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Rumble";
    d.dangerlevel = 2;
    d.missileName = "RumbleGrenade";
    d.name = "Electro Harpoon";
    d.projectileSpeed = 2000;
    d.radius = 60;
    d.range = 850;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "RumbleGrenade";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions,
                          CollisionObjectType::EnemyMinions};
    return d;
  }());

  // R - The Equalizer
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Rumble";
    d.dangerlevel = 4;
    d.missileName = "RumbleCarpetBombM";
    d.name = "The Equalizer";
    d.projectileSpeed = 1050; // Fall speed
    d.radius = 130;
    d.range = 1000;        // Line length basically 1000 from start to end
    d.spellDelay = 250.0f; // Very complex but simplified to vector cast
    d.spellKey = SpellSlot::R;
    d.spellName = "RumbleCarpetBomb";
    d.spellType = SpellType::Line;
    d.isSpecial = true; // Vector cast!
    return d;
  }());

  // ================================================================
  //  S-Z Champions (missing from legacy database)
  // ================================================================

  // ==== Sejuani ====
  // R - Glacial Prison
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sejuani";
    d.dangerlevel = 4;
    d.missileName = "SejuaniR";
    d.name = "Glacial Prison";
    d.projectileSpeed = 1600;
    d.radius = 120;
    d.range = 1300;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "SejuaniR";
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyChampions};
    return d;
  }());

  // ==== Shaco ====
  // W - Jack In The Box
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Shaco";
    d.dangerlevel = 1;
    d.name = "Jack In The Box";
    d.radius = 300;
    d.range = 400;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "JackInTheBox";
    d.spellType = SpellType::Circular;
    d.hasTrap = true;
    d.defaultOff = true;
    return d;
  }());

  // ==== Shen ====
  // E - Shadow Dash
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Shen";
    d.dangerlevel = 3;
    d.name = "Shadow Dash";
    d.projectileSpeed = 1200; // Actually rush speed
    d.radius = 60;
    d.range = 600;
    d.spellDelay = 0.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "ShenE";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Shyvana ====
  // E - Flame Breath
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Shyvana";
    d.dangerlevel = 2;
    d.missileName = "ShyvanaFireballMissile";
    d.name = "Flame Breath";
    d.projectileSpeed = 1575;
    d.radius = 60;
    d.range = 925;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "ShyvanaFireball";
    d.spellType = SpellType::Line;
    return d;
  }());

  // R - Dragon's Descent
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Shyvana";
    d.dangerlevel = 3;
    d.name = "Dragon's Descent";
    d.projectileSpeed = 700; // Dash speed
    d.radius = 150;
    d.range = 850;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "ShyvanaTransformCast";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Singed ====
  // W - Mega Adhesive
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Singed";
    d.dangerlevel = 2;
    d.name = "Mega Adhesive";
    d.projectileSpeed = 700;
    d.radius = 175;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::W;
    d.spellName = "MegaAdhesive";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Sion ====
  // Q - Decimating Smash
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sion";
    d.dangerlevel = 3;
    d.name = "Decimating Smash";
    d.radius = 150;         // Width
    d.range = 1000;         // Length
    d.spellDelay = 2000.0f; // Cast time max
    d.spellKey = SpellSlot::Q;
    d.spellName = "SionQ";
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // E - Roar of the Slayer
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sion";
    d.dangerlevel = 2;
    d.missileName = "SionEMissile";
    d.name = "Roar of the Slayer";
    d.projectileSpeed = 1800;
    d.radius = 80;
    d.range = 800;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "SionE";
    d.spellType = SpellType::Line;
    return d;
  }());

  // R - Unstoppable Onslaught
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sion";
    d.dangerlevel = 4;
    d.name = "Unstoppable Onslaught";
    d.projectileSpeed = 950;
    d.radius = 160;
    d.range = 7500;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "SionR";
    d.spellType = SpellType::Line;
    d.fixedRange =
        true; // Technically follows path, but usually treated as line
    return d;
  }());

  // ==== Sivir ====
  // Q - Boomerang Blade
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sivir";
    d.dangerlevel = 2;
    d.missileName = "SivirQMissile";
    d.name = "Boomerang Blade";
    d.projectileSpeed = 1350;
    d.radius = 90;
    d.range = 1250;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SivirQ";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Sona ====
  // R - Crescendo
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Sona";
    d.dangerlevel = 4;
    d.missileName = "SonaR";
    d.name = "Crescendo";
    d.projectileSpeed = 2400;
    d.radius = 140;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "SonaR";
    d.spellType = SpellType::Line;
    return d;
  }());

  // ==== Soraka ====
  // Q - Starcall
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Soraka";
    d.dangerlevel = 2;
    d.name = "Starcall";
    d.projectileSpeed = 1000; // Variable drop
    d.radius = 235;
    d.range = 810;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::Q;
    d.spellName = "SorakaQ";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // E - Equinox
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Soraka";
    d.dangerlevel = 3;
    d.name = "Equinox";
    d.radius = 250;
    d.range = 925;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "SorakaE";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Teemo ====
  // R - Noxious Trap
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Teemo";
    d.dangerlevel = 2;
    d.missileName = "TeemoRCast";
    d.name = "Noxious Trap";
    d.projectileSpeed = 1000;
    d.radius = 135;
    d.range = 900;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::R;
    d.spellName = "TeemoRCast";
    d.spellType = SpellType::Circular;
    d.hasTrap = true;
    d.defaultOff = true;
    return d;
  }());

  // ==== Trundle ====
  // E - Pillar of Ice
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Trundle";
    d.dangerlevel = 3;
    d.name = "Pillar of Ice";
    d.radius = 225;
    d.range = 1000;
    d.spellDelay = 250.0f;
    d.spellKey = SpellSlot::E;
    d.spellName = "TrundleTrollSmash";
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ============================================================================
  // ── CDragon audit fill-ins (2026-04-25) ───────────────────────────────────
  // 16 champions on champion-summary.json are absent from Database.h. Most
  // are point-and-click only (Garen / Jax / Katarina / MasterYi / MonkeyKing /
  // Renekton / Udyr / Vayne / Kindred / Yuumi attach) — no skillshots to
  // predict. Below are entries for the ones that DO have skillshot abilities
  // worth tracking. Spell names extracted from
  //   `game/data/characters/<name>/<name>.bin.json` mScriptName.
  // Numeric values (radius / projectileSpeed / spellDelay / range) from
  // public LoL wiki patch 14.x baseline.
  // ============================================================================

  // ==== Akshan ====
  // Q "Avengerang" — line skillshot, boomerang returns to caster
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Akshan";
    d.dangerlevel = 2;
    d.name = "Avengerang";
    d.spellKey = SpellSlot::Q;
    d.spellName = "AkshanQ";
    d.missileName = "AkshanQMis";
    d.projectileSpeed = 1900;
    d.radius = 70;
    d.range = 1100;
    d.spellDelay = 250;
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyMinions};
    d.fixedRange = true;
    return d;
  }());

  // R "Comeuppance" — channeled piercing line (fast-firing locked-on bullets)
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Akshan";
    d.dangerlevel = 5;
    d.name = "Comeuppance";
    d.spellKey = SpellSlot::R;
    d.spellName = "AkshanR";
    d.missileName = "AkshanRMis";
    d.projectileSpeed = 5000;
    d.radius = 80;
    d.range = 3000;
    d.spellDelay = 500;
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // ==== Alistar ====
  // Q "Pulverize" — circle AOE around self, 1.0s knockup
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Alistar";
    d.dangerlevel = 4;
    d.name = "Pulverize";
    d.spellKey = SpellSlot::Q;
    d.spellName = "Pulverize";
    d.radius = 365;
    d.range = 0;
    d.spellDelay = 250;
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Kaisa ====
  // W "Void Seeker" — long line skillshot, marks target for plasma stack
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Kaisa";
    d.dangerlevel = 3;
    d.name = "Void Seeker";
    d.spellKey = SpellSlot::W;
    d.spellName = "KaisaW";
    d.missileName = "KaisaWMissile";
    d.projectileSpeed = 1750;
    d.radius = 80;
    d.range = 3000;
    d.spellDelay = 400;
    d.spellType = SpellType::Line;
    d.collisionObjects = {CollisionObjectType::EnemyMinions};
    d.fixedRange = true;
    return d;
  }());

  // ==== Nasus ====
  // E "Spirit Fire" — circle AOE on cursor, lingers 5s with armor shred
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Nasus";
    d.dangerlevel = 2;
    d.name = "Spirit Fire";
    d.spellKey = SpellSlot::E;
    d.spellName = "NasusE";
    d.radius = 400;
    d.range = 650;
    d.spellDelay = 250;
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Rammus ====
  // R "Soaring Slam" — global jump, AOE on landing with shockwaves
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Rammus";
    d.dangerlevel = 3;
    d.name = "Soaring Slam";
    d.spellKey = SpellSlot::R;
    d.spellName = "RammusR";
    d.radius = 250;
    d.range = 25000; // global cast
    d.spellDelay = 750;
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Yuumi ====
  // Q "Prowling Projectile" — line skillshot, charges up for slow + extra damage
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yuumi";
    d.dangerlevel = 2;
    d.name = "Prowling Projectile";
    d.spellKey = SpellSlot::Q;
    d.spellName = "YuumiQ";
    d.missileName = "YuumiQMissile";
    d.projectileSpeed = 1450;
    d.radius = 60;
    d.range = 1150;
    d.spellDelay = 250;
    d.spellType = SpellType::Line;
    d.fixedRange = true;
    return d;
  }());

  // R "Final Chapter" — channel 7 waves around herself, root on 3+ hits
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yuumi";
    d.dangerlevel = 4;
    d.name = "Final Chapter";
    d.spellKey = SpellSlot::R;
    d.spellName = "YuumiR";
    d.radius = 525;
    d.range = 0;
    d.spellDelay = 500;
    d.spellType = SpellType::Circular;
    return d;
  }());

  // ==== Yunara ====
  // Q "Holy Mark" — bouncing missile (CDragon: YunaraQBounceMissile).
  // NEWEST CHAMPION — values approximated, refine with wiki when stable.
  Spells.push_back([]() {
    SpellData d;
    d.charName = "Yunara";
    d.dangerlevel = 2;
    d.name = "Holy Mark";
    d.spellKey = SpellSlot::Q;
    d.spellName = "YunaraQ";
    d.extraMissileNames = {"YunaraQBounceMissile", "YunaraQBounceCritMissile"};
    d.projectileSpeed = 1500;
    d.radius = 80;
    d.range = 700;
    d.spellDelay = 250;
    d.spellType = SpellType::Line;
    return d;
  }());

  return Spells;
}

// ============================================================================
// Lookup Helpers
// ============================================================================

namespace detail {
inline bool EqualsIgnoreCase(const std::string &left,
                             const std::string &right) {
  if (left.size() != right.size())
    return false;
  for (size_t i = 0; i < left.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(left[i])) !=
        std::tolower(static_cast<unsigned char>(right[i])))
      return false;
  }
  return true;
}
} // namespace detail

// Get spell by internal name
inline const SpellData *GetSpellByName(const std::string &spellName) {
  for (auto &s : GetSpellDatabase()) {
    if (detail::EqualsIgnoreCase(s.spellName, spellName))
      return &s;
    for (auto &extra : s.extraSpellNames) {
      if (detail::EqualsIgnoreCase(extra, spellName))
        return &s;
    }
  }
  return nullptr;
}

// Get spell by missile name
inline const SpellData *GetSpellByMissile(const std::string &missileName) {
  for (auto &s : GetSpellDatabase()) {
    if (!s.missileName.empty() &&
        detail::EqualsIgnoreCase(s.missileName, missileName))
      return &s;
    if (!s.missileSpellName.empty() &&
        detail::EqualsIgnoreCase(s.missileSpellName, missileName))
      return &s;
    if (detail::EqualsIgnoreCase(s.spellName, missileName))
      return &s;
    for (auto &m : s.extraMissileNames) {
      if (detail::EqualsIgnoreCase(m, missileName))
        return &s;
    }
  }
  return nullptr;
}

// Get all spells for a champion
inline std::vector<const SpellData *>
GetSpellsForChampion(const std::string &champName) {
  std::vector<const SpellData *> result;
  for (auto &s : GetSpellDatabase()) {
    if (detail::EqualsIgnoreCase(s.charName, champName))
      result.push_back(&s);
  }
  return result;
}

// Check if a spell name exists in the database
inline bool IsKnownSpell(const std::string &spellName) {
  return GetSpellByName(spellName) != nullptr;
}

} // namespace Data
} // namespace SDK
