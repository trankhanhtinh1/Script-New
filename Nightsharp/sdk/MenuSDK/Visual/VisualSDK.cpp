#include "VisualSDK.h"
#include "../../../sdk/UI/Drawing.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace NightSharp::Menu {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;

ImU32 ToImColor(std::uint32_t color) {
    return IM_COL32(
        static_cast<int>((color >> 16) & 0xFFu),
        static_cast<int>((color >> 8) & 0xFFu),
        static_cast<int>(color & 0xFFu),
        static_cast<int>((color >> 24) & 0xFFu));
}

bool IsValidScreenPoint(const Vec2& point) {
    return point.IsValid() &&
           std::isfinite(point.x) &&
           std::isfinite(point.y);
}

bool IsValidWorldPoint(const Vec3& point) {
    return point.IsValid() &&
           std::isfinite(point.x) &&
           std::isfinite(point.y) &&
           std::isfinite(point.z);
}

float ClampThickness(float thickness) {
    return std::clamp(thickness, 0.5f, 16.0f);
}

int ResolveSegments(int requested, float projectedRadius) {
    if (requested > 0) {
        return std::clamp(requested, 16, 256);
    }
    const float circumference = std::max(projectedRadius, 1.0f) * kTwoPi;
    return std::clamp(static_cast<int>(std::ceil(circumference / 5.0f)), 32, 192);
}

bool ProjectWorld(const Vec3& world, Vec2& screen) {
    return IsValidWorldPoint(world) &&
           SDK::Drawing::WorldToScreen(world, screen) &&
           IsValidScreenPoint(screen);
}

void DrawPolyline(
    ImDrawList* draw,
    const std::vector<ImVec2>& points,
    std::uint32_t color,
    float thickness,
    bool closed) {
    if (!draw || points.size() < 2) {
        return;
    }
    draw->AddPolyline(
        points.data(),
        static_cast<int>(points.size()),
        ToImColor(color),
        closed,
        ClampThickness(thickness));
}

void DrawWorldPolyline(
    ImDrawList* draw,
    const std::vector<Vec3>& points,
    std::uint32_t color,
    float thickness,
    bool closed) {
    if (!draw || points.size() < 2) {
        return;
    }

    std::vector<ImVec2> projected;
    projected.reserve(points.size());
    for (const Vec3& point : points) {
        Vec2 screen = {};
        if (!ProjectWorld(point, screen)) {
            if (projected.size() >= 2) {
                DrawPolyline(draw, projected, color, thickness, false);
            }
            projected.clear();
            continue;
        }
        projected.emplace_back(screen.x, screen.y);
    }
    if (projected.size() >= 2) {
        DrawPolyline(
            draw,
            projected,
            color,
            thickness,
            closed && projected.size() == points.size());
    }
}

std::vector<ImVec2> ProjectWorldCircle(
    const Vec3& center,
    float radius,
    int segments) {
    std::vector<ImVec2> points;
    points.reserve(static_cast<std::size_t>(segments));
    for (int index = 0; index < segments; ++index) {
        const float angle = kTwoPi *
            static_cast<float>(index) / static_cast<float>(segments);
        const Vec3 world{
            center.x + std::cos(angle) * radius,
            center.y,
            center.z + std::sin(angle) * radius};
        Vec2 screen = {};
        if (ProjectWorld(world, screen)) {
            points.emplace_back(screen.x, screen.y);
        }
    }
    return points;
}

float ProjectedWorldRadius(const Vec3& center, float radius) {
    Vec2 centerScreen = {};
    Vec2 edgeScreen = {};
    if (!ProjectWorld(center, centerScreen) ||
        !ProjectWorld(Vec3{ center.x + radius, center.y, center.z }, edgeScreen)) {
        return 64.0f;
    }
    return centerScreen.Distance(edgeScreen);
}

void DrawScreenCircle(
    ImDrawList* draw,
    const Vec2& center,
    float radius,
    const VisualStyle& style) {
    if (!draw || !IsValidScreenPoint(center) ||
        !std::isfinite(radius) || radius <= 0.0f) {
        return;
    }
    const int segments = ResolveSegments(style.segments, radius);
    const ImVec2 imCenter(center.x, center.y);
    if (style.filled) {
        draw->AddCircleFilled(imCenter, radius, ToImColor(
            style.fillColor == 0 ? style.color : style.fillColor), segments);
    }
    draw->AddCircle(
        imCenter,
        radius,
        ToImColor(style.color),
        segments,
        ClampThickness(style.thickness));
}

void DrawScreenRing(
    ImDrawList* draw,
    const Vec2& center,
    float innerRadius,
    float outerRadius,
    const VisualStyle& style) {
    if (!draw || !IsValidScreenPoint(center) ||
        !std::isfinite(innerRadius) || !std::isfinite(outerRadius) ||
        innerRadius <= 0.0f || outerRadius <= innerRadius) {
        return;
    }
    const int segments = ResolveSegments(style.segments, outerRadius);
    const ImVec2 imCenter(center.x, center.y);
    draw->AddCircle(
        imCenter,
        innerRadius,
        ToImColor(style.color),
        segments,
        ClampThickness(style.thickness));
    draw->AddCircle(
        imCenter,
        outerRadius,
        ToImColor(style.color),
        segments,
        ClampThickness(style.thickness));
}

void DrawScreenArc(
    ImDrawList* draw,
    const Vec2& center,
    float radius,
    float startAngle,
    float endAngle,
    const VisualStyle& style) {
    if (!draw || !IsValidScreenPoint(center) ||
        !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(startAngle) || !std::isfinite(endAngle)) {
        return;
    }
    while (endAngle < startAngle) {
        endAngle += kTwoPi;
    }
    const float span = std::min(endAngle - startAngle, kTwoPi);
    const int segments = std::max(
        2,
        static_cast<int>(std::ceil(
            static_cast<float>(ResolveSegments(style.segments, radius)) *
            span / kTwoPi)));
    draw->PathArcTo(
        ImVec2(center.x, center.y),
        radius,
        startAngle,
        startAngle + span,
        segments);
    draw->PathStroke(
        ToImColor(style.color),
        ImDrawFlags_None,
        ClampThickness(style.thickness));
}

void DrawScreenRectangle(
    ImDrawList* draw,
    const Vec2& minimum,
    const Vec2& maximum,
    const VisualStyle& style) {
    if (!draw || !IsValidScreenPoint(minimum) || !IsValidScreenPoint(maximum) ||
        maximum.x <= minimum.x || maximum.y <= minimum.y) {
        return;
    }
    const ImVec2 minPoint(minimum.x, minimum.y);
    const ImVec2 maxPoint(maximum.x, maximum.y);
    if (style.filled) {
        draw->AddRectFilled(
            minPoint,
            maxPoint,
            ToImColor(style.fillColor == 0 ? style.color : style.fillColor));
    }
    draw->AddRect(
        minPoint,
        maxPoint,
        ToImColor(style.color),
        0.0f,
        ImDrawFlags_None,
        ClampThickness(style.thickness));
}

void DrawScreenPolygon(
    ImDrawList* draw,
    const std::vector<Vec2>& points,
    const VisualStyle& style) {
    if (!draw || points.size() < 3) {
        return;
    }
    std::vector<ImVec2> imPoints;
    imPoints.reserve(points.size());
    for (const Vec2& point : points) {
        if (!IsValidScreenPoint(point)) {
            return;
        }
        imPoints.emplace_back(point.x, point.y);
    }
    if (style.filled) {
        draw->AddConvexPolyFilled(
            imPoints.data(),
            static_cast<int>(imPoints.size()),
            ToImColor(style.fillColor == 0 ? style.color : style.fillColor));
    }
    DrawPolyline(draw, imPoints, style.color, style.thickness, true);
}

void DrawWorldPolygon(
    ImDrawList* draw,
    const std::vector<Vec3>& points,
    const VisualStyle& style) {
    if (!draw || points.size() < 3) {
        return;
    }
    std::vector<Vec2> projected;
    projected.reserve(points.size());
    for (const Vec3& point : points) {
        Vec2 screen = {};
        if (!ProjectWorld(point, screen)) {
            return;
        }
        projected.push_back(screen);
    }
    DrawScreenPolygon(draw, projected, style);
}

void DrawScreenArrow(
    ImDrawList* draw,
    const Vec2& start,
    const Vec2& end,
    const VisualStyle& style) {
    if (!draw || !IsValidScreenPoint(start) || !IsValidScreenPoint(end)) {
        return;
    }
    const Vec2 delta = end - start;
    const float length = delta.Length();
    if (!std::isfinite(length) || length < 2.0f) {
        return;
    }
    const Vec2 direction = delta / length;
    const Vec2 normal{ -direction.y, direction.x };
    const float headLength = std::clamp(length * 0.18f, 8.0f, 24.0f);
    const float headWidth = headLength * 0.55f;
    const Vec2 left = end - direction * headLength + normal * headWidth;
    const Vec2 right = end - direction * headLength - normal * headWidth;
    const ImU32 color = ToImColor(style.color);
    const float thickness = ClampThickness(style.thickness);
    draw->AddLine(ImVec2(start.x, start.y), ImVec2(end.x, end.y), color, thickness);
    draw->AddLine(ImVec2(end.x, end.y), ImVec2(left.x, left.y), color, thickness);
    draw->AddLine(ImVec2(end.x, end.y), ImVec2(right.x, right.y), color, thickness);
}

std::string Ellipsize(const std::string& text, float maxWidth, float scale) {
    if (maxWidth <= 0.0f || !ImGui::GetCurrentContext()) {
        return text;
    }
    if (ImGui::CalcTextSize(text.c_str()).x * scale <= maxWidth) {
        return text;
    }
    std::string result = text;
    while (!result.empty()) {
        result.pop_back();
        const std::string candidate = result + "...";
        if (ImGui::CalcTextSize(candidate.c_str()).x * scale <= maxWidth) {
            return candidate;
        }
    }
    return "...";
}

void DrawStyledText(
    ImDrawList* draw,
    const Vec2& position,
    const std::string& text,
    const VisualLabelStyle& style) {
    if (!draw || text.empty() || !ImGui::GetCurrentContext()) {
        return;
    }
    const float scale = std::clamp(style.scale, 0.5f, 3.0f);
    const float fontSize = ImGui::GetFontSize() * scale;
    const std::string displayText = Ellipsize(text, style.maxWidth, scale);
    const ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
    Vec2 topLeft = position;
    if (style.centered) {
        topLeft.x -= textSize.x * scale * 0.5f;
        topLeft.y -= textSize.y * scale * 0.5f;
    }

    const ImVec2 base(topLeft.x, topLeft.y);
    if (style.outline) {
        constexpr float offsets[][2] = {
            { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f },
            { -1.0f, 0.0f },                    { 1.0f, 0.0f },
            { -1.0f, 1.0f },  { 0.0f, 1.0f },  { 1.0f, 1.0f },
        };
        for (const auto& offset : offsets) {
            draw->AddText(
                ImGui::GetFont(),
                fontSize,
                ImVec2(base.x + offset[0], base.y + offset[1]),
                ToImColor(style.outlineColor),
                displayText.c_str());
        }
    } else if (style.shadow) {
        draw->AddText(
            ImGui::GetFont(),
            fontSize,
            ImVec2(
                base.x + style.shadowOffset.x,
                base.y + style.shadowOffset.y),
            ToImColor(style.shadowColor),
            displayText.c_str());
    }
    draw->AddText(
        ImGui::GetFont(),
        fontSize,
        base,
        ToImColor(style.color),
        displayText.c_str());
}

bool ResolveTargetScreen(
    const VisualTarget& target,
    VisualAnchor anchor,
    Vec2& screen) {
    if (!IsValidWorldPoint(target.position)) {
        return false;
    }
    if (anchor == VisualAnchor::HealthBar) {
        screen = SDK::Drawing::HpBarScreenPos(
            target.position,
            target.hpBarHeight);
        return IsValidScreenPoint(screen);
    }

    Vec3 position = target.position;
    if (anchor == VisualAnchor::Head ||
        anchor == VisualAnchor::AboveHead) {
        position.y += target.hpBarHeight > 0.0f
            ? target.hpBarHeight
            : 100.0f;
    }
    if (!ProjectWorld(position, screen)) {
        return false;
    }
    if (anchor == VisualAnchor::AboveHead) {
        screen.y -= 10.0f;
    }
    return true;
}

VisualLayer CommandLayer(const VisualCommand& command) {
    switch (command.kind) {
    case VisualCommandKind::TextScreen:
    case VisualCommandKind::LabelUnit:
        return command.labelStyle.layer;
    case VisualCommandKind::HealthBar:
        return command.healthBarStyle.layer;
    default:
        return command.style.layer;
    }
}

int CommandOrder(const VisualCommand& command) {
    switch (command.kind) {
    case VisualCommandKind::TextScreen:
    case VisualCommandKind::LabelUnit:
        return command.labelStyle.order;
    case VisualCommandKind::HealthBar:
        return command.healthBarStyle.order;
    default:
        return command.style.order;
    }
}

}

VisualHandle::VisualHandle(VisualSDK* sdk, std::uint64_t id)
    : sdk_(sdk), id_(id) {}

VisualHandle::VisualHandle(VisualHandle&& other) noexcept
    : sdk_(other.sdk_), id_(other.id_) {
    other.sdk_ = nullptr;
    other.id_ = 0;
}

VisualHandle& VisualHandle::operator=(VisualHandle&& other) noexcept {
    if (this != &other) {
        Reset();
        sdk_ = other.sdk_;
        id_ = other.id_;
        other.sdk_ = nullptr;
        other.id_ = 0;
    }
    return *this;
}

VisualHandle::~VisualHandle() {
    Reset();
}

bool VisualHandle::IsValid() const {
    if (!sdk_ || id_ == 0) {
        return false;
    }
    if (sdk_->persistentCommands_.find(id_) != sdk_->persistentCommands_.end()) {
        return true;
    }
    return std::any_of(
        sdk_->frameCommands_.begin(),
        sdk_->frameCommands_.end(),
        [this](const VisualCommand& command) {
            return command.id == id_;
        });
}

void VisualHandle::Reset() {
    if (sdk_ && id_ != 0) {
        sdk_->Remove(id_);
    }
    sdk_ = nullptr;
    id_ = 0;
}

VisualContext::VisualContext(VisualSDK* sdk, std::string ownerId)
    : sdk_(sdk), ownerId_(std::move(ownerId)) {}

bool VisualContext::IsValid() const {
    return sdk_ && !ownerId_.empty() && sdk_->CanSubmit(ownerId_);
}

void VisualContext::SetEnabled(bool enabled) {
    if (sdk_ && !ownerId_.empty()) {
        sdk_->SetOwnerEnabled(ownerId_, enabled);
    }
}

bool VisualContext::IsEnabled() const {
    return sdk_ && !ownerId_.empty() && sdk_->IsOwnerEnabled(ownerId_);
}

void VisualContext::Release() {
    if (sdk_ && !ownerId_.empty()) {
        sdk_->ReleaseOwner(ownerId_);
    }
    sdk_ = nullptr;
    ownerId_.clear();
}

VisualHandle VisualContext::Submit(VisualCommand command) const {
    if (!sdk_ || ownerId_.empty()) {
        return {};
    }
    command.owner = ownerId_;
    return sdk_->Submit(std::move(command));
}

VisualHandle VisualContext::LineScreen(
    const Vec2& start,
    const Vec2& end,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::LineScreen;
    command.style = style;
    command.screenStart = start;
    command.screenEnd = end;
    return Submit(std::move(command));
}

VisualHandle VisualContext::LineWorld(
    const Vec3& start,
    const Vec3& end,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::LineWorld;
    command.style = style;
    command.worldStart = start;
    command.worldEnd = end;
    return Submit(std::move(command));
}

VisualHandle VisualContext::PolylineScreen(
    std::vector<Vec2> points,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::PolylineScreen;
    command.style = style;
    command.screenPoints = std::move(points);
    return Submit(std::move(command));
}

VisualHandle VisualContext::PolylineWorld(
    std::vector<Vec3> points,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::PolylineWorld;
    command.style = style;
    command.worldPoints = std::move(points);
    return Submit(std::move(command));
}

VisualHandle VisualContext::CircleScreen(
    const Vec2& center,
    float radius,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::CircleScreen;
    command.style = style;
    command.screenCenter = center;
    command.radius = radius;
    return Submit(std::move(command));
}

VisualHandle VisualContext::CircleWorld(
    const Vec3& center,
    float radius,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::CircleWorld;
    command.style = style;
    command.worldCenter = center;
    command.radius = radius;
    return Submit(std::move(command));
}

VisualHandle VisualContext::RingScreen(
    const Vec2& center,
    float innerRadius,
    float outerRadius,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::RingScreen;
    command.style = style;
    command.screenCenter = center;
    command.radius = innerRadius;
    command.secondaryRadius = outerRadius;
    return Submit(std::move(command));
}

VisualHandle VisualContext::RingWorld(
    const Vec3& center,
    float innerRadius,
    float outerRadius,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::RingWorld;
    command.style = style;
    command.worldCenter = center;
    command.radius = innerRadius;
    command.secondaryRadius = outerRadius;
    return Submit(std::move(command));
}

VisualHandle VisualContext::ArcScreen(
    const Vec2& center,
    float radius,
    float startAngle,
    float endAngle,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::ArcScreen;
    command.style = style;
    command.screenCenter = center;
    command.radius = radius;
    command.startAngle = startAngle;
    command.endAngle = endAngle;
    return Submit(std::move(command));
}

VisualHandle VisualContext::ArcWorld(
    const Vec3& center,
    float radius,
    float startAngle,
    float endAngle,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::ArcWorld;
    command.style = style;
    command.worldCenter = center;
    command.radius = radius;
    command.startAngle = startAngle;
    command.endAngle = endAngle;
    return Submit(std::move(command));
}

VisualHandle VisualContext::RectangleScreen(
    const Vec2& minimum,
    const Vec2& maximum,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::RectangleScreen;
    command.style = style;
    command.screenStart = minimum;
    command.screenEnd = maximum;
    return Submit(std::move(command));
}

VisualHandle VisualContext::RectangleWorld(
    const Vec3& start,
    const Vec3& end,
    float halfWidth,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::RectangleWorld;
    command.style = style;
    command.worldStart = start;
    command.worldEnd = end;
    command.radius = halfWidth;
    return Submit(std::move(command));
}

VisualHandle VisualContext::PolygonScreen(
    std::vector<Vec2> points,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::PolygonScreen;
    command.style = style;
    command.screenPoints = std::move(points);
    return Submit(std::move(command));
}

VisualHandle VisualContext::PolygonWorld(
    std::vector<Vec3> points,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::PolygonWorld;
    command.style = style;
    command.worldPoints = std::move(points);
    return Submit(std::move(command));
}

VisualHandle VisualContext::ArrowScreen(
    const Vec2& start,
    const Vec2& end,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::ArrowScreen;
    command.style = style;
    command.screenStart = start;
    command.screenEnd = end;
    return Submit(std::move(command));
}

VisualHandle VisualContext::ArrowWorld(
    const Vec3& start,
    const Vec3& end,
    VisualStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::ArrowWorld;
    command.style = style;
    command.worldStart = start;
    command.worldEnd = end;
    return Submit(std::move(command));
}

VisualHandle VisualContext::TextScreen(
    const Vec2& position,
    std::string text,
    VisualLabelStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::TextScreen;
    command.labelStyle = style;
    command.style.layer = style.layer;
    command.style.lifetime = style.lifetime;
    command.style.order = style.order;
    command.screenCenter = position;
    command.text = std::move(text);
    return Submit(std::move(command));
}

VisualHandle VisualContext::TextWorld(
    const Vec3& position,
    std::string text,
    VisualLabelStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::LabelUnit;
    command.labelStyle = style;
    command.style.layer = style.layer;
    command.style.lifetime = style.lifetime;
    command.style.order = style.order;
    command.worldCenter = position;
    command.text = std::move(text);
    return Submit(std::move(command));
}

VisualHandle VisualContext::Label(
    const VisualTarget& target,
    std::string text,
    VisualLabelStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::LabelUnit;
    command.labelStyle = style;
    command.style.layer = style.layer;
    command.style.lifetime = style.lifetime;
    command.style.order = style.order;
    command.target = target;
    command.hasTarget = true;
    command.text = std::move(text);
    return Submit(std::move(command));
}

VisualHandle VisualContext::HealthBar(
    const VisualTarget& target,
    VisualHealthBarStyle style) const {
    VisualCommand command;
    command.kind = VisualCommandKind::HealthBar;
    command.healthBarStyle = style;
    command.style.layer = style.layer;
    command.style.lifetime = style.lifetime;
    command.style.order = style.order;
    command.target = target;
    command.hasTarget = true;
    return Submit(std::move(command));
}

VisualSDK& VisualSDK::Instance() {
    static VisualSDK instance;
    return instance;
}

VisualContext VisualSDK::Owner(std::string ownerId, int ownerOrder) {
    if (ownerId.empty()) {
        return {};
    }
    RegisterOwner(ownerId, ownerOrder);
    return VisualContext(this, std::move(ownerId));
}

void VisualSDK::RegisterOwner(const std::string& ownerId, int ownerOrder) {
    if (ownerId.empty()) {
        return;
    }
    auto& state = owners_[ownerId];
    state.order = ownerOrder;
}

void VisualSDK::ReleaseOwner(const std::string& ownerId) {
    if (ownerId.empty()) {
        return;
    }
    owners_.erase(ownerId);
    for (auto it = frameCommands_.begin(); it != frameCommands_.end();) {
        if (it->owner == ownerId) {
            it = frameCommands_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = persistentCommands_.begin(); it != persistentCommands_.end();) {
        if (it->second.owner == ownerId) {
            it = persistentCommands_.erase(it);
        } else {
            ++it;
        }
    }
}

void VisualSDK::SetOwnerEnabled(const std::string& ownerId, bool enabled) {
    const auto found = owners_.find(ownerId);
    if (found != owners_.end()) {
        found->second.enabled = enabled;
    }
}

bool VisualSDK::IsOwnerEnabled(const std::string& ownerId) const {
    const auto found = owners_.find(ownerId);
    return found != owners_.end() && found->second.enabled;
}

void VisualSDK::BeginFrame() {
    frameCommands_.clear();
}

void VisualSDK::Flush() {
    if (!SDK::Drawing::IsEnabled()) {
        return;
    }

    std::vector<const VisualCommand*> commands;
    commands.reserve(frameCommands_.size() + persistentCommands_.size());
    for (const VisualCommand& command : frameCommands_) {
        commands.push_back(&command);
    }
    for (const auto& entry : persistentCommands_) {
        commands.push_back(&entry.second);
    }
    std::stable_sort(commands.begin(), commands.end(), CommandLess);
    for (const VisualCommand* command : commands) {
        if (command) {
            RenderCommand(*command);
        }
    }
}

void VisualSDK::Reset() {
    owners_.clear();
    frameCommands_.clear();
    persistentCommands_.clear();
    nextCommandId_ = 1;
    nextSequence_ = 1;
}

VisualHandle VisualSDK::Submit(VisualCommand command) {
    if (!CanSubmit(command.owner)) {
        return {};
    }
    const OwnerState* owner = FindOwner(command.owner);
    command.id = nextCommandId_++;
    command.sequence = nextSequence_++;
    command.ownerOrder = owner ? owner->order : 0;
    const std::uint64_t commandId = command.id;
    const bool persistent =
        command.style.lifetime == VisualLifetime::Persistent;
    if (persistent) {
        persistentCommands_[commandId] = std::move(command);
        return VisualHandle(this, commandId);
    }
    frameCommands_.push_back(std::move(command));
    return {};
}

void VisualSDK::Remove(std::uint64_t id) {
    if (id == 0) {
        return;
    }
    persistentCommands_.erase(id);
    for (auto it = frameCommands_.begin(); it != frameCommands_.end();) {
        if (it->id == id) {
            it = frameCommands_.erase(it);
        } else {
            ++it;
        }
    }
}

bool VisualSDK::CanSubmit(const std::string& ownerId) const {
    const OwnerState* owner = FindOwner(ownerId);
    return owner && owner->enabled;
}

const VisualSDK::OwnerState* VisualSDK::FindOwner(
    const std::string& ownerId) const {
    const auto found = owners_.find(ownerId);
    return found == owners_.end() ? nullptr : &found->second;
}

bool VisualSDK::CommandLess(
    const VisualCommand* left,
    const VisualCommand* right) {
    const auto leftLayer = static_cast<int>(CommandLayer(*left));
    const auto rightLayer = static_cast<int>(CommandLayer(*right));
    if (leftLayer != rightLayer) {
        return leftLayer < rightLayer;
    }
    if (left->ownerOrder != right->ownerOrder) {
        return left->ownerOrder < right->ownerOrder;
    }
    const int leftOrder = CommandOrder(*left);
    const int rightOrder = CommandOrder(*right);
    if (leftOrder != rightOrder) {
        return leftOrder < rightOrder;
    }
    return left->sequence < right->sequence;
}

void VisualSDK::RenderCommand(const VisualCommand& command) {
    const OwnerState* owner = FindOwner(command.owner);
    if (!owner || !owner->enabled) {
        return;
    }

    ImDrawList* draw = SDK::Drawing::GetDrawList(
        command.kind == VisualCommandKind::TextScreen
            ? command.labelStyle.foreground
            : command.kind == VisualCommandKind::LabelUnit
                ? command.labelStyle.foreground
                : command.kind == VisualCommandKind::HealthBar
                    ? command.healthBarStyle.foreground
                    : command.style.foreground);
    if (!draw) {
        return;
    }

    switch (command.kind) {
    case VisualCommandKind::LineScreen:
        SDK::Drawing::DrawLine(
            command.screenStart,
            command.screenEnd,
            command.style.color,
            command.style.thickness,
            command.style.foreground);
        break;
    case VisualCommandKind::LineWorld:
        SDK::Drawing::DrawLine(
            command.worldStart,
            command.worldEnd,
            command.style.color,
            command.style.thickness,
            command.style.foreground);
        break;
    case VisualCommandKind::PolylineScreen:
        DrawPolyline(
            draw,
            [&]() {
                std::vector<ImVec2> points;
                points.reserve(command.screenPoints.size());
                for (const Vec2& point : command.screenPoints) {
                    if (IsValidScreenPoint(point)) {
                        points.emplace_back(point.x, point.y);
                    }
                }
                return points;
            }(),
            command.style.color,
            command.style.thickness,
            command.style.closed);
        break;
    case VisualCommandKind::PolylineWorld:
        DrawWorldPolyline(
            draw,
            command.worldPoints,
            command.style.color,
            command.style.thickness,
            command.style.closed);
        break;
    case VisualCommandKind::CircleScreen:
        DrawScreenCircle(
            draw,
            command.screenCenter,
            command.radius,
            command.style);
        break;
    case VisualCommandKind::CircleWorld: {
        const float projectedRadius = ProjectedWorldRadius(
            command.worldCenter,
            command.radius);
        const int segments = ResolveSegments(
            command.style.segments,
            projectedRadius);
        const auto points = ProjectWorldCircle(
            command.worldCenter,
            command.radius,
            segments);
        if (command.style.filled && points.size() >= 3) {
            draw->AddConvexPolyFilled(
                points.data(),
                static_cast<int>(points.size()),
                ToImColor(
                    command.style.fillColor == 0
                        ? command.style.color
                        : command.style.fillColor));
        }
        DrawPolyline(
            draw,
            points,
            command.style.color,
            command.style.thickness,
            points.size() == static_cast<std::size_t>(segments));
        break;
    }
    case VisualCommandKind::RingScreen:
        DrawScreenRing(
            draw,
            command.screenCenter,
            command.radius,
            command.secondaryRadius,
            command.style);
        break;
    case VisualCommandKind::RingWorld: {
        const float projectedRadius = ProjectedWorldRadius(
            command.worldCenter,
            command.secondaryRadius);
        const int segments = ResolveSegments(
            command.style.segments,
            projectedRadius);
        const auto inner = ProjectWorldCircle(
            command.worldCenter,
            command.radius,
            segments);
        const auto outer = ProjectWorldCircle(
            command.worldCenter,
            command.secondaryRadius,
            segments);
        DrawPolyline(
            draw,
            inner,
            command.style.color,
            command.style.thickness,
            inner.size() == static_cast<std::size_t>(segments));
        DrawPolyline(
            draw,
            outer,
            command.style.color,
            command.style.thickness,
            outer.size() == static_cast<std::size_t>(segments));
        break;
    }
    case VisualCommandKind::ArcScreen:
        DrawScreenArc(
            draw,
            command.screenCenter,
            command.radius,
            command.startAngle,
            command.endAngle,
            command.style);
        break;
    case VisualCommandKind::ArcWorld: {
        float endAngle = command.endAngle;
        while (endAngle < command.startAngle) {
            endAngle += kTwoPi;
        }
        const float span = std::min(endAngle - command.startAngle, kTwoPi);
        const float projectedRadius = ProjectedWorldRadius(
            command.worldCenter,
            command.radius);
        const int segments = std::max(
            2,
            static_cast<int>(std::ceil(
                static_cast<float>(ResolveSegments(
                    command.style.segments,
                    projectedRadius)) * span / kTwoPi)));
        std::vector<Vec3> points;
        points.reserve(static_cast<std::size_t>(segments + 1));
        for (int index = 0; index <= segments; ++index) {
            const float angle = command.startAngle +
                span * static_cast<float>(index) / static_cast<float>(segments);
            points.push_back(Vec3{
                command.worldCenter.x + std::cos(angle) * command.radius,
                command.worldCenter.y,
                command.worldCenter.z + std::sin(angle) * command.radius});
        }
        DrawWorldPolyline(
            draw,
            points,
            command.style.color,
            command.style.thickness,
            false);
        break;
    }
    case VisualCommandKind::RectangleScreen:
        DrawScreenRectangle(
            draw,
            command.screenStart,
            command.screenEnd,
            command.style);
        break;
    case VisualCommandKind::RectangleWorld: {
        const Vec3 delta = command.worldEnd - command.worldStart;
        const float length = delta.Length2D();
        if (length > 0.001f && command.radius > 0.0f) {
            const Vec3 normal{
                -delta.z / length * command.radius,
                0.0f,
                delta.x / length * command.radius};
            DrawWorldPolygon(
                draw,
                {
                    command.worldStart + normal,
                    command.worldEnd + normal,
                    command.worldEnd - normal,
                    command.worldStart - normal,
                },
                command.style);
        }
        break;
    }
    case VisualCommandKind::PolygonScreen:
        DrawScreenPolygon(draw, command.screenPoints, command.style);
        break;
    case VisualCommandKind::PolygonWorld:
        DrawWorldPolygon(draw, command.worldPoints, command.style);
        break;
    case VisualCommandKind::ArrowScreen:
        DrawScreenArrow(
            draw,
            command.screenStart,
            command.screenEnd,
            command.style);
        break;
    case VisualCommandKind::ArrowWorld: {
        Vec2 start = {};
        Vec2 end = {};
        if (ProjectWorld(command.worldStart, start) &&
            ProjectWorld(command.worldEnd, end)) {
            DrawScreenArrow(draw, start, end, command.style);
        }
        break;
    }
    case VisualCommandKind::TextScreen:
        DrawStyledText(
            draw,
            command.screenCenter,
            command.text,
            command.labelStyle);
        break;
    case VisualCommandKind::LabelUnit: {
        Vec2 screen = {};
        if (!command.text.empty()) {
            if (!command.hasTarget) {
                if (ProjectWorld(command.worldCenter, screen)) {
                    screen = screen + command.labelStyle.offset;
                    DrawStyledText(draw, screen, command.text, command.labelStyle);
                }
            } else if (ResolveTargetScreen(
                           command.target,
                           command.labelStyle.anchor,
                           screen)) {
                screen = screen + command.labelStyle.offset;
                DrawStyledText(draw, screen, command.text, command.labelStyle);
            }
        }
        break;
    }
    case VisualCommandKind::HealthBar: {
        Vec2 position = {};
        if (!ResolveTargetScreen(
                command.target,
                VisualAnchor::HealthBar,
                position)) {
            break;
        }
        const VisualHealthBarStyle& style = command.healthBarStyle;
        const float width = std::max(style.width, 4.0f);
        const float height = std::max(style.height, 2.0f);
        const float ratio = command.target.maxHealth > 0.0f
            ? std::clamp(
                command.target.health / command.target.maxHealth,
                0.0f,
                1.0f)
            : 0.0f;
        const Vec2 minimum{
            position.x - width * 0.5f + style.offset.x,
            position.y + style.offset.y};
        const Vec2 maximum{
            minimum.x + width,
            minimum.y + height};
        draw->AddRectFilled(
            ImVec2(minimum.x, minimum.y),
            ImVec2(maximum.x, maximum.y),
            ToImColor(style.backgroundColor));
        draw->AddRectFilled(
            ImVec2(minimum.x, minimum.y),
            ImVec2(minimum.x + width * ratio, maximum.y),
            ToImColor(style.fillColor));
        draw->AddRect(
            ImVec2(minimum.x, minimum.y),
            ImVec2(maximum.x, maximum.y),
            ToImColor(style.borderColor),
            0.0f,
            ImDrawFlags_None,
            ClampThickness(style.borderThickness));
        if (style.drawText) {
            char text[64] = {};
            std::snprintf(
                text,
                sizeof(text),
                "%.0f / %.0f",
                command.target.health,
                command.target.maxHealth);
            VisualLabelStyle textStyle;
            textStyle.color = style.textColor;
            textStyle.shadow = true;
            textStyle.centered = true;
            DrawStyledText(
                draw,
                Vec2{ position.x, position.y + height + 2.0f },
                text,
                textStyle);
        }
        break;
    }
    }
}

}
