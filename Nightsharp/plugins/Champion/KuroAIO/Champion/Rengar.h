#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::Rengar {

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* QSettingsMenu = nullptr;
inline Menu* WSettingsMenu = nullptr;
inline Menu* ESettingsMenu = nullptr;
inline Menu* ItemsMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* DrawMenu = nullptr;

// Id đối chiếu với CommunityDragon `latest`
// (rcp-be-lol-game-data/global/default/v1/items.json).
inline constexpr int kRavenousHydra = 3074;
inline constexpr int kTitanicHydra  = 3748;
inline constexpr int kStridebreaker = 6631;
inline constexpr int kProfaneHydra  = 6698;

inline Spell Q{ SpellSlot::Q, 150.0f };
inline Spell W{ SpellSlot::W, 450.0f };
inline Spell E{ SpellSlot::E, 1000.0f };
inline Spell R{ SpellSlot::R, 725.0f };

inline bool Loaded = false;
inline AIHeroClient CurrentTarget = AIHeroClient();

static bool IsEmp() {
    const auto instance = Q.Instance();
    if (instance.IsValid()) {
        std::string name = instance.Name();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (name.find("emp") != std::string::npos) {
            return true;
        }
    }
    const auto player = Player();
    return player.IsValid() && player.Mana() >= 4.0f;
}

static bool IsLeaping() {
    const auto player = Player();
    return player.IsValid() && (SDK::Extensions::IsDashing(player) || player.HasBuff("RengarR") || player.HasBuff("rengarpassivebuff"));
}

static bool QCanAttack(const AIBaseClient& target, float bonusRange = 75.0f) {
    if (!ValidTarget(target)) return false;
    const auto player = Player();
    const float range = player.AttackRange() + player.BoundingRadius() + target.BoundingRadius() + bonusRange;
    return player.Position().Distance2D(target.Position()) <= range;
}

static bool IsImmovableOrCC(const AIHeroClient& target) {
    if (!target.IsValid()) return false;
    return SDK::HasBuffOfType(target, SDK::BuffType::Stun) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Snare) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Taunt) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Suppression) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Charm) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Fear) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Silence) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Asleep);
}

static void CastComboQ(const AIHeroClient& target) {
    if (!Q.IsReady() || !ValidHeroTarget(target)) return;

    const bool use3Q = Key(QSettingsMenu, "Use3Q", false);
    const auto player = Player();
    const bool dashing = SDK::Extensions::IsDashing(player);

    // Fast 3Q Mid-Air Leap logic: If leaping/dashing with 4 Ferocity (EmpQ), cast Q mid-air immediately!
    if (dashing && use3Q && IsEmp()) {
        if (player.Position().Distance2D(target.Position()) <= 750.0f) {
            Q.Cast();
            return;
        }
    }

    if (!dashing || use3Q) {
        if (QCanAttack(target, 50.0f)) {
            Q.Cast();
        }
    }
}

static void CastComboW(const AIHeroClient& target) {
    if (!W.IsReady() || !ValidHeroTarget(target)) return;

    const auto player = Player();
    const bool use3Q = Key(QSettingsMenu, "Use3Q", false);
    const bool dashing = SDK::Extensions::IsDashing(player);

    if ((!dashing || use3Q) && Bool(WSettingsMenu, "UseW", true) && !IsEmp()) {
        if (player.Position().Distance2D(target.Position()) <= W.Range) {
            W.Cast(player.Position());
        }
    }
}

static void CastComboE(const AIHeroClient& target) {
    if (!E.IsReady() || !ValidHeroTarget(target)) return;

    const auto player = Player();
    const bool dashing = SDK::Extensions::IsDashing(player);
    const bool use3Q = Key(QSettingsMenu, "Use3Q", false);

    // Allow E in mid-air during dash/leap when not holding Emp, or during Fast 3Q
    if (QCanAttack(target, 100.0f) && IsEmp() && !dashing && !use3Q) {
        return; // Save empowered for EmpQ or EmpW when grounded
    }

    if (!player.HasBuff("RengarR") || dashing || use3Q) {
        if (Bool(ESettingsMenu, "UseE", true)) {
            auto pred = E.GetPrediction(target);
            if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::High)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }
}

// Hydra dùng ngay ở after-attack, độc lập với chuỗi Q/W/E: active của chúng tức
// thời, không tiêu Ferocity và không đụng vào nhịp đánh nên chen được cùng lúc.
//
// Bán kính AoE KHÔNG có trong ItemData.h lẫn CommunityDragon (RangeMax = 0 cho
// cả 4 món), nên để thành slider thay vì chôn một con số không kiểm chứng được.
static void TryUseHydraItems(const AIHeroClient& player, const AIHeroClient& target) {
    const float radius = static_cast<float>(Slider(ItemsMenu, "HydraRadius", 450));
    const float distance = player.Position().Distance2D(target.Position());

    // Nhóm AoE quanh bản thân — chỉ nổ khi mục tiêu nằm trong tầm ảnh hưởng.
    // Dùng else-if: mỗi nhịp đánh chỉ đốt MỘT active, tránh xả hết cùng lúc khi
    // ôm nhiều món Hydra; món còn lại để dành cho nhịp kế tiếp.
    if (distance <= radius) {
        if (Bool(ItemsMenu, "UseProfane", true) &&
            SDK::Items::CanUseItem(player, kProfaneHydra)) {
            SDK::Items::UseItem(player, kProfaneHydra);
            return;
        }
        if (Bool(ItemsMenu, "UseRavenous", true) &&
            SDK::Items::CanUseItem(player, kRavenousHydra)) {
            SDK::Items::UseItem(player, kRavenousHydra);
            return;
        }
        if (Bool(ItemsMenu, "UseStridebreaker", true) &&
            SDK::Items::CanUseItem(player, kStridebreaker)) {
            SDK::Items::UseItem(player, kStridebreaker);
            return;
        }
    }

    // Titanic Hydra nạp cho ĐÒN ĐÁNH KẾ TIẾP — trùng vai trò với Q. Dùng khi Q
    // không còn xài được nữa để hai thứ không đè lên nhau, và chỉ khi mục tiêu
    // đã nằm trong tầm đánh (nếu không thì buff nạp xong sẽ phí).
    if (Bool(ItemsMenu, "UseTitanic", true) &&
        !Q.IsReady() &&
        QCanAttack(target, 0.0f) &&
        SDK::Items::CanUseItem(player, kTitanicHydra)) {
        SDK::Items::UseItem(player, kTitanicHydra);
    }
}

// Chen kỹ năng vào giữa hai nhịp đánh (weaving). Mỗi lần after-attack chỉ tung
// ĐÚNG MỘT kỹ năng rồi return, chờ nhịp đánh kế tiếp — thứ tự ưu tiên Q > W > E.
//
// Luật riêng cho Ferocity: khi đang có Emp và địch nằm trong tầm Q thì Q luôn
// được ưu tiên. Nếu Q chưa hồi thì cũng KHÔNG tiêu Emp vào W/E — giữ lại chờ
// nhịp sau để đánh Emp Q, vì đó là chỗ Emp có giá trị nhất.
static void OnAfterAttack(OrbwalkingActionArgs& args) {
    (void)args;
    if (!Loaded) return;
    if (!IsComboMode() && !IsHarassMode()) return;

    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    // Tìm mục tiêu trong tầm E — tầm xa nhất trong ba kỹ năng.
    const AIHeroClient target = GetPhysicalTarget(E.Range);
    if (!ValidHeroTarget(target, E.Range)) return;

    // Chạy trước chuỗi kỹ năng và KHÔNG return: item với kỹ năng đi được cùng
    // một nhịp đánh.
    TryUseHydraItems(player, target);

    const bool useQ = Bool(ComboMenu, "UseQ", true);
    const bool emp = IsEmp();
    const bool inQRange = QCanAttack(target, 50.0f);

    // 1) Q — vừa reset đòn đánh vừa là chỗ tiêu Emp tốt nhất.
    if (useQ && inQRange && Q.IsReady()) {
        Q.Cast();
        return;
    }

    // Có Emp + địch trong tầm Q nhưng Q chưa hồi: giữ Ferocity, không tiêu vào
    // W/E. (Bỏ qua nếu người dùng tắt Q trong menu — lúc đó giữ Emp là vô nghĩa.)
    if (useQ && emp && inQRange) return;

    // 2) W — AoE quanh bản thân.
    if (Bool(ComboMenu, "UseW", true) && Bool(WSettingsMenu, "UseW", true) &&
        W.IsReady() &&
        player.Position().Distance2D(target.Position()) <= W.Range) {
        W.Cast(player.Position());
        return;
    }

    // 3) E — skillshot, chỉ tung khi prediction đủ tin cậy.
    if (Bool(ComboMenu, "UseE", true) && Bool(ESettingsMenu, "UseE", true) &&
        E.IsReady()) {
        const auto pred = E.GetPrediction(target);
        if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::High)) {
            E.Cast(pred.GetCastPosition());
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    CurrentTarget = GetPhysicalTarget(E.Range);

    // Auto EmpW Cleanse / CC removal
    if (W.IsReady() && Bool(WSettingsMenu, "AutoCleanse", true) && IsEmp()) {
        if (IsImmovableOrCC(player)) {
            W.Cast(player.Position());
            return;
        }
    }

    // Auto W Heal when low HP
    if (W.IsReady() && Bool(WSettingsMenu, "AutoHeal", true) && IsEmp()) {
        if (player.HealthPercent() <= static_cast<float>(Slider(WSettingsMenu, "HealHp", 30))) {
            W.Cast(player.Position());
            return;
        }
    }

    // Combo Mode
    if (IsComboMode() && ValidHeroTarget(CurrentTarget)) {
        // Q là reset đòn đánh. Nếu địch đã trong tầm đánh và CanAttack(250) thì
        // đằng nào cũng sắp có một nhịp đánh thường — để OnAfterAttack tung Q
        // NGAY SAU cú đánh đó mới ăn được reset. Cast từ vòng update ở tình
        // huống này là phí reset.
        // Ngưỡng tầm dùng đúng QCanAttack(+50) như OnAfterAttack để hai bên bàn
        // giao khít nhau, không chừa khoảng trống mà cả hai đều bỏ qua Q.
        const bool afterAttackWillHandleQ =
            QCanAttack(CurrentTarget, 50.0f) && CanAttack(250);
        if (Bool(ComboMenu, "UseQ", true) && CanAttack(250) &&
            !afterAttackWillHandleQ) {
            CastComboQ(CurrentTarget);
        }

        if (Bool(ComboMenu, "UseW", true)) {
            CastComboW(CurrentTarget);
        }

        if (Bool(ComboMenu, "UseE", true)) {
            CastComboE(CurrentTarget);
        }
    }

    // Harass Mode
    if (IsHarassMode() && ValidHeroTarget(CurrentTarget)) {
        if (Bool(ComboMenu, "UseQ", true) && CanAttack(250)) {
            CastComboQ(CurrentTarget);
        }
        if (Bool(ComboMenu, "UseW", true)) {
            CastComboW(CurrentTarget);
        }
        if (Bool(ComboMenu, "UseE", true)) {
            CastComboE(CurrentTarget);
        }
    }

    // Lane / Jungle Clear Mode
    if (IsClearMode()) {
        const float clearRange = player.AttackRange() + player.BoundingRadius() + 150.0f;

        // Jungle clear
        for (const auto& minion : GameObjects::Jungle()) {
            if (ValidTarget(minion, clearRange)) {
                if (Q.IsReady() && Bool(ClearMenu, "JungleQ", true)) {
                    Q.Cast();
                    return;
                }
                if (W.IsReady() && Bool(ClearMenu, "JungleW", true)) {
                    W.Cast(player.Position());
                    return;
                }
                if (E.IsReady() && Bool(ClearMenu, "JungleE", true) && !IsEmp()) {
                    E.Cast(minion.Position());
                    return;
                }
            }
        }

        // Minion clear
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (ValidTarget(minion, clearRange) && !IsEmp()) {
                if (Q.IsReady() && Bool(ClearMenu, "LaneQ", true)) {
                    Q.Cast();
                    return;
                }
            }
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    const Vector3 pos = player.Position();

    if (Bool(DrawMenu, "DrawW", true) && W.IsReady()) {
        Drawing::DrawCircle(pos, W.Range, 0xFFFFAA00u, 1.5f, 64);
    }

    if (Bool(DrawMenu, "DrawE", true) && E.IsReady()) {
        Drawing::DrawCircle(pos, E.Range, 0xFFD90429u, 1.5f, 64);
    }

    if (Bool(DrawMenu, "DrawLeap", true)) {
        const float leapRange = IsLeaping() ? 725.0f : 0.0f;
        if (leapRange > 0.0f) {
            Drawing::DrawCircle(pos, leapRange, 0xFFFF6B00u, 2.0f, 64);
        }
    }

    if (Bool(DrawMenu, "DrawTarget", true) && ValidHeroTarget(CurrentTarget)) {
        Vec2 playerScreen{}, targetScreen{};
        if (Drawing::WorldToScreen(pos, playerScreen) && Drawing::WorldToScreen(CurrentTarget.Position(), targetScreen)) {
            Drawing::DrawLine(playerScreen, targetScreen, 0xFFD90429u, 2.0f);
            Drawing::DrawText(targetScreen.x - 40.0f, targetScreen.y + 20.0f, 0xFFD90429u, "Combo Target");
        }
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("KuroAIO.Rengar", "KuroAIO - Rengar", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("UseQ", "Use Q Savagery", true));
    ComboMenu->Add(new MenuBool("UseW", "Use W Battle Roar", true));
    ComboMenu->Add(new MenuBool("UseE", "Use E Bola Strike", true));

    QSettingsMenu = MenuRoot->AddSubMenu(new Menu("QSettings", "Q Savagery Settings"));
    QSettingsMenu->Add(new MenuKeyBind("Use3Q", "Fast 3Q Combo", SDK::Keys::A, KeyBindType::Toggle))->Permashow();

    WSettingsMenu = MenuRoot->AddSubMenu(new Menu("WSettings", "W Battle Roar Settings"));
    WSettingsMenu->Add(new MenuBool("UseW", "Use W in Combo", true));
    WSettingsMenu->Add(new MenuBool("AutoCleanse", "Auto EmpW Cleanse CC", true));
    WSettingsMenu->Add(new MenuBool("AutoHeal", "Auto EmpW Heal Low HP", true));
    WSettingsMenu->Add(new MenuSlider("HealHp", "Heal at HP %", 30, 10, 80));

    ESettingsMenu = MenuRoot->AddSubMenu(new Menu("ESettings", "E Bola Strike Settings"));
    ESettingsMenu->Add(new MenuBool("UseE", "Use E in Combo", true));

    ItemsMenu = MenuRoot->AddSubMenu(new Menu("Items", "Item Settings"));
    ItemsMenu->Add(new MenuBool("UseProfane", "Use Profane Hydra", true));
    ItemsMenu->Add(new MenuBool("UseRavenous", "Use Ravenous Hydra", true));
    ItemsMenu->Add(new MenuBool("UseStridebreaker", "Use Stridebreaker", true));
    ItemsMenu->Add(new MenuBool("UseTitanic", "Use Titanic Hydra (when Q down)", true));
    ItemsMenu->Add(new MenuSlider("HydraRadius", "Hydra Active Radius", 450, 300, 600));

    ClearMenu = MenuRoot->AddSubMenu(new Menu("Clear", "Clear Settings"));
    ClearMenu->Add(new MenuBool("JungleQ", "Jungle Q", true));
    ClearMenu->Add(new MenuBool("JungleW", "Jungle W", true));
    ClearMenu->Add(new MenuBool("JungleE", "Jungle E", true));
    ClearMenu->Add(new MenuBool("LaneQ", "Lane Q", true));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Drawings"));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W Range", true));
    DrawMenu->Add(new MenuBool("DrawE", "Draw E Range", true));
    DrawMenu->Add(new MenuBool("DrawLeap", "Draw Leap Range", true));
    DrawMenu->Add(new MenuBool("DrawTarget", "Draw Current Target Line", true));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) return;
    if (auto* item = QSettingsMenu ? QSettingsMenu->Get<MenuKeyBind>("Use3Q") : nullptr) {
        item->RemovePermashow();
    }
    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    QSettingsMenu = nullptr;
    WSettingsMenu = nullptr;
    ESettingsMenu = nullptr;
    ItemsMenu = nullptr;
    ClearMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) return;

    Q = Spell(SpellSlot::Q, 150.0f);
    W = Spell(SpellSlot::W, 450.0f);
    E = Spell(SpellSlot::E, 1000.0f);
    E.SetSkillshot(0.25f, 70.0f, 1500.0f, true, SkillshotType::SkillshotLine);

    R = Spell(SpellSlot::R, 725.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Drawing::OnDraw += &OnDraw;
    Orbwalker::OnAfterAttack += &OnAfterAttack;

    Loaded = true;
    Game::Print("<font color='#d90429' size='20'>Kuro - Rengar loaded</font>");
}

static void OnUnload() {
    if (!Loaded) return;

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Drawing::OnDraw -= &OnDraw;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;

    RemoveMenu();
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Rengar
