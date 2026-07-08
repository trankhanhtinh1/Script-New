#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreObjectManager.h"
#include "../../Core/CoreObjects.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace Plugins {

class YasuoWallDebugPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Yasuo Wall Debug"; }
    const char* GetInternalId() const override { return "utility.yasuo_wall_debug"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        s_instance = this;
        SDK::Events::AddOnCreateObject(&YasuoWallDebugPlugin::OnCreateObject);
        SDK::Events::AddOnDeleteObject(&YasuoWallDebugPlugin::OnDeleteObject);
        lastRefreshTick_ = SDK::Game::TickCount();
        NightSharpDebug::Logf("[YasuoWallDebug] loaded");
    }

    void OnUnload() override {
        if (doCastSubscribed_) {
            SDK::Events::RemoveOnDoCast(&YasuoWallDebugPlugin::OnDoCast);
            doCastSubscribed_ = false;
        }
        SDK::Events::RemoveOnDeleteObject(&YasuoWallDebugPlugin::OnDeleteObject);
        SDK::Events::RemoveOnCreateObject(&YasuoWallDebugPlugin::OnCreateObject);
        walls_.clear();
        syntheticWalls_.clear();
        if (s_instance == this) {
            s_instance = nullptr;
        }
        NightSharpDebug::Logf("[YasuoWallDebug] unloaded");
    }

    void OnUpdate() override {
        SyncDoCastSubscription();
        const int now = SDK::Game::TickCount();
        if (now - lastRefreshTick_ >= refreshMs_) {
            RefreshWalls();
        }
    }

    void OnRender() override {
        if (!drawEnabled_ || !ImGui::GetCurrentContext() || !SDK::Drawing::IsEnabled()) {
            return;
        }

        RefreshWalls();
        DrawPlayerProbe();

        for (const auto& wall : walls_) {
            DrawWall(wall);
        }
        for (const auto& wall : syntheticWalls_) {
            DrawWall(wall);
        }
    }

    void OnMenu() override {
        ImGui::Checkbox("Draw Yasuo walls", &drawEnabled_);
        ImGui::Checkbox("Draw player -> cursor probe", &drawPlayerProbe_);
        ImGui::Checkbox("Draw raw emitter direction", &drawRawDirection_);
        ImGui::Checkbox("Direct scan effect emitters", &directScanEnabled_);
        ImGui::Checkbox("Synthetic OnDoCast fallback", &syntheticHookEnabled_);
        SyncDoCastSubscription();
        ImGui::SliderInt("Refresh ms", &refreshMs_, 25, 500);
        ImGui::SliderFloat("Probe extra radius", &probeExtraRadius_, 0.0f, 250.0f, "%.0f");
        ImGui::Separator();
        ImGui::Text("Direct scan walls: %d", static_cast<int>(walls_.size()));
        ImGui::Text("Synthetic cast walls: %d", static_cast<int>(syntheticWalls_.size()));
        ImGui::Text("OnCreate windwalls: %d", createSeen_);
        ImGui::Text("OnDelete windwalls: %d", deleteSeen_);
        ImGui::Text("OnDoCast Yasuo W: %d", doCastSeen_);
        ImGui::Text("Last event: %s", lastEventText_.empty() ? "-" : lastEventText_.c_str());

        const auto player = SDK::ObjectManager::Player();
        const bool hasPlayer = player.IsValid();
        const bool collision = hasPlayer
            ? HasProbeCollision(player.Position(), SDK::Game::CursorPos())
            : false;
        ImGui::Text("Player -> cursor collision: %s", collision ? "true" : "false");
    }

private:
    struct WallSnapshot {
        SDK::EffectEmitter emitter = {};
        std::string name;
        int networkId = 0;
        int level = 1;
        Vec3 position = {};
        Vec3 direction3D = {};
        Vec2 spanDirection = {};
        float directionLengthSqr = 0.0f;
        float getCollisionWidth = 0.0f;
        float helperWidth = 0.0f;
        Vec3 getCollisionStart = {};
        Vec3 getCollisionEnd = {};
        Vec3 helperStart = {};
        Vec3 helperEnd = {};
        uintptr_t address = 0;
        int castTick = 0;
        bool synthetic = false;
    };

    static inline YasuoWallDebugPlugin* s_instance = nullptr;

    bool drawEnabled_ = true;
    bool drawPlayerProbe_ = true;
    bool drawRawDirection_ = true;
    bool directScanEnabled_ = false;
    bool syntheticHookEnabled_ = false;
    bool doCastSubscribed_ = false;
    int refreshMs_ = 50;
    int lastRefreshTick_ = 0;
    float probeExtraRadius_ = 50.0f;
    int createSeen_ = 0;
    int deleteSeen_ = 0;
    int doCastSeen_ = 0;
    std::string lastEventText_;
    std::vector<WallSnapshot> walls_;
    std::vector<WallSnapshot> syntheticWalls_;

    static std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    static bool IsWindWallName(const std::string& name) {
        const std::string lower = ToLower(name);
        return lower.find("yasuo") != std::string::npos &&
               lower.find("_w_windwall") != std::string::npos;
    }

    static std::string RuntimeObjectName(const SDK::GameObject& object) {
        std::string name = object.Name();
        if (!name.empty()) {
            return name;
        }

        char buffer[128] = {};
        if (::Core::Objects::ReadCharacterName(object.Address(), buffer, static_cast<int>(sizeof(buffer))) ||
            ::Core::Objects::ReadName(object.Address(), buffer, static_cast<int>(sizeof(buffer)))) {
            name = buffer;
        }
        return name;
    }

    static int WindWallLevel(const std::string& name) {
        const std::string lower = ToLower(name);
        if (lower.find("windwall5") != std::string::npos) return 5;
        if (lower.find("windwall4") != std::string::npos) return 4;
        if (lower.find("windwall3") != std::string::npos) return 3;
        if (lower.find("windwall2") != std::string::npos) return 2;
        return 1;
    }

    static Vec3 From2DAtHeight(const Vec2& point, float height) {
        return Vec3(point.x, height, point.y);
    }

    static Vec2 CollisionSpanDirection(const SDK::EffectEmitter& emitter, float* lengthSqrOut) {
        Vec2 wallDir = emitter.Direction().To2D();
        const float lengthSqr = wallDir.LengthSqr();
        if (lengthSqrOut) {
            *lengthSqrOut = lengthSqr;
        }
        if (lengthSqr < 0.001f) {
            return Vec2(1.0f, 0.0f);
        }
        return Vec2(-wallDir.y, wallDir.x).Normalized();
    }

    static WallSnapshot MakeSnapshot(const SDK::EffectEmitter& emitter) {
        WallSnapshot wall = {};
        wall.emitter = emitter;
        wall.address = emitter.Address();
        wall.name = RuntimeObjectName(emitter);
        wall.networkId = emitter.NetworkId();
        wall.level = WindWallLevel(wall.name);
        wall.position = emitter.Position();
        wall.direction3D = emitter.Direction();
        wall.spanDirection = CollisionSpanDirection(emitter, &wall.directionLengthSqr);
        wall.getCollisionWidth = 350.0f + 50.0f * static_cast<float>(wall.level);
        wall.helperWidth = 250.0f + 50.0f * static_cast<float>(wall.level) + 50.0f;

        const Vec2 center = wall.position.To2D();
        const Vec2 getCollisionStart = center + wall.spanDirection * (wall.getCollisionWidth * 0.5f);
        const Vec2 getCollisionEnd = getCollisionStart - wall.spanDirection * wall.getCollisionWidth;
        const Vec2 helperStart = center + wall.spanDirection * (wall.helperWidth * 0.5f);
        const Vec2 helperEnd = helperStart - wall.spanDirection * wall.helperWidth;

        wall.getCollisionStart = From2DAtHeight(getCollisionStart, wall.position.y);
        wall.getCollisionEnd = From2DAtHeight(getCollisionEnd, wall.position.y);
        wall.helperStart = From2DAtHeight(helperStart, wall.position.y);
        wall.helperEnd = From2DAtHeight(helperEnd, wall.position.y);
        return wall;
    }

    void SyncDoCastSubscription() {
        if (syntheticHookEnabled_ && !doCastSubscribed_) {
            doCastSubscribed_ = SDK::Events::AddOnDoCast(&YasuoWallDebugPlugin::OnDoCast);
            NightSharpDebug::Logf("[YasuoWallDebug] synthetic OnDoCast subscribed=%d",
                                  doCastSubscribed_ ? 1 : 0);
        } else if (!syntheticHookEnabled_ && doCastSubscribed_) {
            SDK::Events::RemoveOnDoCast(&YasuoWallDebugPlugin::OnDoCast);
            doCastSubscribed_ = false;
            NightSharpDebug::Logf("[YasuoWallDebug] synthetic OnDoCast unsubscribed");
        }
    }

    static bool IsYasuoWCast(const SDK::Events::ProcessSpellEventArgs& args) {
        const std::string championName = ToLower(
            args.Sender.CharacterName[0] ? args.Sender.CharacterName : args.Sender.Name);
        if (championName != "yasuo") {
            return false;
        }

        if (args.Slot == static_cast<int>(SDK::SpellSlot::W)) {
            return true;
        }

        const std::string spellName = ToLower(args.SpellName);
        const std::string slotName = ToLower(args.SpellSlotName);
        return spellName.find("yasuow") != std::string::npos ||
               slotName.find("yasuow") != std::string::npos;
    }

    static int ResolveYasuoWLevel(const SDK::Events::ProcessSpellEventArgs& args) {
        if (!args.Sender.IsValid()) {
            return 1;
        }

        SDK::AIHeroClient caster(args.Sender.Ptr);
        if (!caster.IsValid()) {
            return 1;
        }

        return std::clamp(caster.Spellbook().GetSpell(SDK::SpellSlot::W).Level(), 1, 5);
    }

    static Vec3 FirstValidPosition(std::initializer_list<Vec3> positions) {
        for (const auto& position : positions) {
            if (position.IsValid() && !position.IsZero()) {
                return position;
            }
        }
        return Vec3();
    }

    static bool MakeSyntheticSnapshot(const SDK::Events::ProcessSpellEventArgs& args,
                                      WallSnapshot& wall) {
        if (!IsYasuoWCast(args)) {
            return false;
        }

        const Vec3 start = FirstValidPosition({
            args.StartPosition,
            args.Sender.Position
        });
        Vec3 end = FirstValidPosition({
            args.CastPosition,
            args.EndPosition
        });

        Vec2 castDirection;
        if (start.IsValid() && !start.IsZero() && end.IsValid() && !end.IsZero()) {
            castDirection = (end - start).To2D();
        }
        if (castDirection.LengthSqr() < 0.001f && args.Sender.Ptr) {
            const SDK::GameObject caster(args.Sender.Ptr, args.Sender.Type);
            castDirection = caster.Direction().To2D();
        }
        if (castDirection.LengthSqr() < 0.001f) {
            return false;
        }

        castDirection = castDirection.Normalized();
        if (!end.IsValid() || end.IsZero()) {
            end = start + Vec3(castDirection.x, 0.0f, castDirection.y) * 400.0f;
        }

        Vec3 center = end;
        const float distance = start.IsValid() && !start.IsZero()
            ? start.Distance2D(center)
            : 0.0f;
        if (!std::isfinite(distance) || distance < 75.0f || distance > 475.0f) {
            center = start + Vec3(castDirection.x, 0.0f, castDirection.y) * 400.0f;
        }

        wall = {};
        wall.synthetic = true;
        wall.castTick = SDK::Game::TickCount();
        wall.name = "synthetic: YasuoW OnDoCast";
        wall.networkId = static_cast<int>(args.CasterNetworkId);
        wall.level = ResolveYasuoWLevel(args);
        wall.position = center;
        wall.direction3D = Vec3(castDirection.x, 0.0f, castDirection.y);
        wall.spanDirection = Vec2(-castDirection.y, castDirection.x).Normalized();
        wall.directionLengthSqr = castDirection.LengthSqr();
        wall.getCollisionWidth = 350.0f + 50.0f * static_cast<float>(wall.level);
        wall.helperWidth = 250.0f + 50.0f * static_cast<float>(wall.level) + 50.0f;

        const Vec2 center2D = wall.position.To2D();
        const Vec2 getCollisionStart = center2D + wall.spanDirection * (wall.getCollisionWidth * 0.5f);
        const Vec2 getCollisionEnd = getCollisionStart - wall.spanDirection * wall.getCollisionWidth;
        const Vec2 helperStart = center2D + wall.spanDirection * (wall.helperWidth * 0.5f);
        const Vec2 helperEnd = helperStart - wall.spanDirection * wall.helperWidth;

        wall.getCollisionStart = From2DAtHeight(getCollisionStart, wall.position.y);
        wall.getCollisionEnd = From2DAtHeight(getCollisionEnd, wall.position.y);
        wall.helperStart = From2DAtHeight(helperStart, wall.position.y);
        wall.helperEnd = From2DAtHeight(helperEnd, wall.position.y);
        return wall.position.IsValid() && !wall.position.IsZero();
    }

    void RefreshWalls() {
        const int now = SDK::Game::TickCount();
        syntheticWalls_.erase(
            std::remove_if(
                syntheticWalls_.begin(),
                syntheticWalls_.end(),
                [now](const WallSnapshot& wall) {
                    return !wall.synthetic || now - wall.castTick > 4000;
                }),
            syntheticWalls_.end());

        if (directScanEnabled_) {
            DirectScanWalls();
        }

        lastRefreshTick_ = now;
    }

    void DirectScanWalls() {
        walls_.clear();
        walls_.reserve(4);

        uintptr_t entries[16384] = {};
        const int count = ::Core::ObjectManager::EnumerateAll(entries, 16384);
        for (int i = 0; i < count; ++i) {
            const uintptr_t address = entries[i];
            if (!address) {
                continue;
            }

            SDK::GameObject object(address);
            if (!IsWindWallName(RuntimeObjectName(object))) {
                continue;
            }

            walls_.push_back(MakeSnapshot(SDK::EffectEmitter(address)));
            if (walls_.size() >= 8) {
                break;
            }
        }
    }

    static void OnCreateObject(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) {
            s_instance->HandleObjectEvent(args, true);
        }
    }

    static void OnDeleteObject(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) {
            s_instance->HandleObjectEvent(args, false);
        }
    }

    static void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
        if (s_instance) {
            s_instance->HandleDoCast(args);
        }
    }

    void HandleObjectEvent(const SDK::Events::ObjectEventArgs& args, bool created) {
        if (!args.Sender.IsValid()) {
            return;
        }

        const SDK::GameObject object(args.Sender.Ptr);
        std::string name = args.Sender.Name;
        if (name.empty()) {
            name = RuntimeObjectName(object);
        }

        if (created && IsWindWallName(name)) {
            ++createSeen_;
            UpsertWall(MakeSnapshot(SDK::EffectEmitter(args.Sender.Ptr)));
        } else if (!created) {
            const bool removed = RemoveWall(args.Sender.Ptr, args.Sender.NetworkId);
            if (!removed && !IsWindWallName(name)) {
                return;
            }
            ++deleteSeen_;
        } else {
            return;
        }

        char buffer[192] = {};
        std::snprintf(buffer,
                      sizeof(buffer),
                      "%s id=%d name=%s",
                      created ? "create" : "delete",
                      object.NetworkId(),
                      name.c_str());
        lastEventText_ = buffer;
    }

    void UpsertWall(const WallSnapshot& wall) {
        if (wall.address == 0 && wall.networkId == 0) {
            return;
        }

        const auto sameWall = [&](const WallSnapshot& entry) {
            return (wall.address != 0 && entry.address == wall.address) ||
                   (wall.networkId != 0 && entry.networkId == wall.networkId);
        };

        const auto it = std::find_if(walls_.begin(), walls_.end(), sameWall);
        if (it != walls_.end()) {
            *it = wall;
            return;
        }

        walls_.push_back(wall);
    }

    bool RemoveWall(uintptr_t address, std::uint32_t networkId) {
        const auto oldSize = walls_.size();
        walls_.erase(
            std::remove_if(
                walls_.begin(),
                walls_.end(),
                [&](const WallSnapshot& wall) {
                    return (address != 0 && wall.address == address) ||
                           (networkId != 0 && wall.networkId == static_cast<int>(networkId));
                }),
            walls_.end());
        return walls_.size() != oldSize;
    }

    void HandleDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
        WallSnapshot wall = {};
        if (!MakeSyntheticSnapshot(args, wall)) {
            return;
        }

        ++doCastSeen_;
        syntheticWalls_.clear();
        syntheticWalls_.push_back(wall);

        char buffer[192] = {};
        std::snprintf(buffer,
                      sizeof(buffer),
                      "docast id=%u name=%s",
                      args.CasterNetworkId,
                      args.SpellName[0] ? args.SpellName : args.SpellSlotName);
        lastEventText_ = buffer;
    }

    void DrawPlayerProbe() const {
        if (!drawPlayerProbe_) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return;
        }

        const Vec3 from = player.Position();
        const Vec3 to = SDK::Game::CursorPos();
        if (!from.IsValid() || !to.IsValid() || to.IsZero()) {
            return;
        }

        const bool collision = HasProbeCollision(from, to);
        const std::uint32_t color = collision ? kColorCollisionTrue : kColorProbe;
        SDK::Drawing::DrawLine(from, to, color, 2.0f);

        char text[96] = {};
        std::snprintf(text,
                      sizeof(text),
                      "YasuoWall player->cursor: %s",
                      collision ? "TRUE" : "false");
        DrawTextWorld(player.Position(), text, color, 0.0f, -62.0f);
    }

    bool HasProbeCollision(const Vec3& from, const Vec3& to) const {
        if (!from.IsValid() || !to.IsValid() || from.IsZero() || to.IsZero()) {
            return false;
        }

        const Vec2 from2D = from.To2D();
        const Vec2 to2D = to.To2D();
        auto intersects = [&](const WallSnapshot& wall) {
            return SDK::Prediction::Vec2Ext::Intersection(
                       wall.getCollisionStart.To2D(),
                       wall.getCollisionEnd.To2D(),
                       to2D,
                       from2D).Valid ||
                   SDK::Prediction::Vec2Ext::Intersection(
                       wall.helperStart.To2D(),
                       wall.helperEnd.To2D(),
                       to2D,
                       from2D).Valid;
        };

        for (const auto& wall : walls_) {
            if (intersects(wall)) {
                return true;
            }
        }
        for (const auto& wall : syntheticWalls_) {
            if (intersects(wall)) {
                return true;
            }
        }
        return false;
    }

    void DrawWall(const WallSnapshot& wall) const {
        SDK::Drawing::DrawCircleAlways(wall.position, 45.0f, kColorEmitter, 2.0f, 32);
        SDK::Drawing::DrawLine(wall.getCollisionStart, wall.getCollisionEnd, kColorPredictionWall, 4.0f);
        SDK::Drawing::DrawLine(wall.helperStart, wall.helperEnd, kColorHelperWall, 2.0f);

        if (drawRawDirection_) {
            const Vec3 rawEnd = wall.position + wall.direction3D.Normalized() * 250.0f;
            if (!rawEnd.IsZero() && rawEnd.IsValid()) {
                SDK::Drawing::DrawLine(wall.position, rawEnd, kColorRawDirection, 1.5f);
            }
        }

        char label[256] = {};
        std::snprintf(label,
                      sizeof(label),
                      "%sYasuoWall L%d id=%d dir2=%.3f predW=%.0f helperW=%.0f",
                      wall.synthetic ? "Synthetic " : "",
                      wall.level,
                      wall.networkId,
                      wall.directionLengthSqr,
                      wall.getCollisionWidth,
                      wall.helperWidth);
        DrawTextWorld(wall.position, label, kColorText, 0.0f, -28.0f);

        char nameText[256] = {};
        std::snprintf(nameText, sizeof(nameText), "%s", wall.name.c_str());
        DrawTextWorld(wall.position, nameText, kColorMutedText, 0.0f, -12.0f);
    }

    static void DrawTextWorld(const Vec3& world,
                              const char* text,
                              std::uint32_t color,
                              float offsetX,
                              float offsetY) {
        Vec2 screen = {};
        if (!SDK::Drawing::WorldToScreen(world, screen)) {
            return;
        }
        SDK::Drawing::DrawText(screen.x + offsetX, screen.y + offsetY, color, text);
    }

    static constexpr std::uint32_t kColorEmitter = 0xFFFFD24Au;
    static constexpr std::uint32_t kColorPredictionWall = 0xFFFF5050u;
    static constexpr std::uint32_t kColorHelperWall = 0xFF45D6FFu;
    static constexpr std::uint32_t kColorRawDirection = 0xFFB875FFu;
    static constexpr std::uint32_t kColorProbe = 0xAAFFFFFFu;
    static constexpr std::uint32_t kColorCollisionTrue = 0xFFFF3030u;
    static constexpr std::uint32_t kColorText = 0xFFFFFFFFu;
    static constexpr std::uint32_t kColorMutedText = 0xFFB8C2CCu;
};

} // namespace Plugins
