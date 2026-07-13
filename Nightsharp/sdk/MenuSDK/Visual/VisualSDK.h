#pragma once

#include "VisualTypes.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace NightSharp::Menu {

class VisualSDK;

class VisualHandle {
public:
    VisualHandle() = default;
    VisualHandle(const VisualHandle&) = delete;
    VisualHandle& operator=(const VisualHandle&) = delete;
    VisualHandle(VisualHandle&& other) noexcept;
    VisualHandle& operator=(VisualHandle&& other) noexcept;
    ~VisualHandle();

    bool IsValid() const;
    void Reset();

private:
    friend class VisualSDK;
    VisualHandle(VisualSDK* sdk, std::uint64_t id);

    VisualSDK* sdk_ = nullptr;
    std::uint64_t id_ = 0;
};

class VisualContext {
public:
    VisualContext() = default;

    bool IsValid() const;
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void Release();

    VisualHandle LineScreen(
        const Vec2& start,
        const Vec2& end,
        VisualStyle style = {}) const;
    VisualHandle LineWorld(
        const Vec3& start,
        const Vec3& end,
        VisualStyle style = {}) const;
    VisualHandle PolylineScreen(
        std::vector<Vec2> points,
        VisualStyle style = {}) const;
    VisualHandle PolylineWorld(
        std::vector<Vec3> points,
        VisualStyle style = {}) const;
    VisualHandle CircleScreen(
        const Vec2& center,
        float radius,
        VisualStyle style = {}) const;
    VisualHandle CircleWorld(
        const Vec3& center,
        float radius,
        VisualStyle style = {}) const;
    VisualHandle RingScreen(
        const Vec2& center,
        float innerRadius,
        float outerRadius,
        VisualStyle style = {}) const;
    VisualHandle RingWorld(
        const Vec3& center,
        float innerRadius,
        float outerRadius,
        VisualStyle style = {}) const;
    VisualHandle ArcScreen(
        const Vec2& center,
        float radius,
        float startAngle,
        float endAngle,
        VisualStyle style = {}) const;
    VisualHandle ArcWorld(
        const Vec3& center,
        float radius,
        float startAngle,
        float endAngle,
        VisualStyle style = {}) const;
    VisualHandle RectangleScreen(
        const Vec2& minimum,
        const Vec2& maximum,
        VisualStyle style = {}) const;
    VisualHandle RectangleWorld(
        const Vec3& start,
        const Vec3& end,
        float halfWidth,
        VisualStyle style = {}) const;
    VisualHandle PolygonScreen(
        std::vector<Vec2> points,
        VisualStyle style = {}) const;
    VisualHandle PolygonWorld(
        std::vector<Vec3> points,
        VisualStyle style = {}) const;
    VisualHandle ArrowScreen(
        const Vec2& start,
        const Vec2& end,
        VisualStyle style = {}) const;
    VisualHandle ArrowWorld(
        const Vec3& start,
        const Vec3& end,
        VisualStyle style = {}) const;
    VisualHandle TextScreen(
        const Vec2& position,
        std::string text,
        VisualLabelStyle style = {}) const;
    VisualHandle TextWorld(
        const Vec3& position,
        std::string text,
        VisualLabelStyle style = {}) const;
    VisualHandle Label(
        const VisualTarget& target,
        std::string text,
        VisualLabelStyle style = {}) const;
    VisualHandle HealthBar(
        const VisualTarget& target,
        VisualHealthBarStyle style = {}) const;

private:
    friend class VisualSDK;
    VisualContext(VisualSDK* sdk, std::string ownerId);
    VisualHandle Submit(VisualCommand command) const;

    VisualSDK* sdk_ = nullptr;
    std::string ownerId_;
};

class VisualSDK {
public:
    static VisualSDK& Instance();

    VisualContext Owner(
        std::string ownerId,
        int ownerOrder = 0);
    void RegisterOwner(const std::string& ownerId, int ownerOrder = 0);
    void ReleaseOwner(const std::string& ownerId);
    void SetOwnerEnabled(const std::string& ownerId, bool enabled);
    bool IsOwnerEnabled(const std::string& ownerId) const;

    void BeginFrame();
    void Flush();
    void Reset();

private:
    friend class VisualHandle;
    friend class VisualContext;

    struct OwnerState {
        int order = 0;
        bool enabled = true;
    };

    VisualSDK() = default;

    VisualHandle Submit(VisualCommand command);
    void Remove(std::uint64_t id);
    bool CanSubmit(const std::string& ownerId) const;
    const OwnerState* FindOwner(const std::string& ownerId) const;
    static bool CommandLess(
        const VisualCommand* left,
        const VisualCommand* right);
    void RenderCommand(const VisualCommand& command);

    std::unordered_map<std::string, OwnerState> owners_;
    std::vector<VisualCommand> frameCommands_;
    std::unordered_map<std::uint64_t, VisualCommand> persistentCommands_;
    std::uint64_t nextCommandId_ = 1;
    std::uint64_t nextSequence_ = 1;
};

}
