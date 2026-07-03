#pragma once

#include "../Core/Objects.h"
#include "../UI/Drawing.h"
#include "../UI/Icons.h"
#include "Logging.h"

#include "../../imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace SDK::Core::Utils {

class Render {
public:
    struct RenderObject {
        using VisibleConditionDelegate = std::function<bool(RenderObject*)>;

        float Layer = 0.0f;
        VisibleConditionDelegate VisibleCondition;

        virtual ~RenderObject() = default;

        bool HasValidLayer() const {
            return Layer >= -5.0f && Layer <= 5.0f;
        }

        bool Visible() {
            return VisibleCondition ? VisibleCondition(this) : visible_;
        }

        void Visible(bool value) {
            visible_ = value;
        }

        virtual void Dispose() {}
        virtual void OnDraw() {}
        virtual void OnEndScene() {}
        virtual void OnPostReset() {}
        virtual void OnPreReset() {}

    private:
        bool visible_ = true;
    };

private:
    static std::vector<RenderObject*>& Objects() {
        static std::vector<RenderObject*> objects;
        return objects;
    }

    static std::vector<RenderObject*>& VisibleObjects() {
        static std::vector<RenderObject*> objects;
        return objects;
    }

    static ImU32 ToImColor(std::uint32_t argb) {
        const int a = static_cast<int>((argb >> 24) & 0xFF);
        const int r = static_cast<int>((argb >> 16) & 0xFF);
        const int g = static_cast<int>((argb >> 8) & 0xFF);
        const int b = static_cast<int>(argb & 0xFF);
        return IM_COL32(r, g, b, a);
    }

    static void EnsureInitialized() {
        static bool initialized = false;
        if (initialized) {
            return;
        }
        initialized = true;
        SDK::Drawing::AddOnDraw(&OnDraw);
        SDK::Drawing::AddOnEndScene(&OnEndScene);
        SDK::Drawing::AddOnPreReset(&OnPreReset);
        SDK::Drawing::AddOnPostReset(&OnPostReset);
    }

    static void PrepareObjects() {
        auto& visible = VisibleObjects();
        visible.clear();
        for (RenderObject* object : Objects()) {
            if (object && object->Visible() && object->HasValidLayer()) {
                visible.push_back(object);
            }
        }
        std::sort(visible.begin(), visible.end(), [](const RenderObject* lhs, const RenderObject* rhs) {
            return lhs->Layer < rhs->Layer;
        });
    }

    static void OnDraw() {
        PrepareObjects();
        for (RenderObject* object : VisibleObjects()) {
            try {
                object->OnDraw();
            } catch (...) {
                Logging::Write()(LogLevel::Error, "SDK.Render.OnDraw: render object crashed.");
            }
        }
    }

    static void OnEndScene() {
        for (RenderObject* object : VisibleObjects()) {
            try {
                object->OnEndScene();
            } catch (...) {
                Logging::Write()(LogLevel::Error, "SDK.Render.OnEndScene: render object crashed.");
            }
        }
    }

    static void OnPreReset() {
        for (RenderObject* object : Objects()) {
            if (object) {
                object->OnPreReset();
            }
        }
    }

    static void OnPostReset() {
        for (RenderObject* object : Objects()) {
            if (object) {
                object->OnPostReset();
            }
        }
    }

public:
    class Circle : public RenderObject {
    public:
        std::uint32_t Color = 0xFFFFFFFFu;
        Vec3 Offset = {};
        Vec3 Position = {};
        float Radius = 0.0f;
        int Width = 1;
        bool ZDeep = false;
        GameObject Unit = {};

        Circle(GameObject unit, float radius, std::uint32_t color, int width = 1, bool zDeep = false)
            : Color(color), Radius(radius), Width(width), ZDeep(zDeep), Unit(unit) {}

        Circle(GameObject unit, Vec3 offset, float radius, std::uint32_t color, int width = 1, bool zDeep = false)
            : Color(color), Offset(offset), Radius(radius), Width(width), ZDeep(zDeep), Unit(unit) {}

        Circle(Vec3 position, Vec3 offset, float radius, std::uint32_t color, int width = 1, bool zDeep = false)
            : Color(color), Offset(offset), Position(position), Radius(radius), Width(width), ZDeep(zDeep) {}

        Circle(Vec3 position, float radius, std::uint32_t color, int width = 1, bool zDeep = false)
            : Color(color), Position(position), Radius(radius), Width(width), ZDeep(zDeep) {}

        void OnDraw() override {
            Vec3 world = Position;
            if (Unit.IsValid()) {
                world = Unit.Position();
            }
            world = world + Offset;

            Vec2 center{};
            if (!SDK::Drawing::WorldToScreen(world, center)) {
                return;
            }

            float screenRadius = Radius;
            Vec2 edge{};
            if (SDK::Drawing::WorldToScreen(Vec3(world.x + Radius, world.y, world.z), edge)) {
                screenRadius = std::max(1.0f, center.Distance(edge));
            }
            SDK::Drawing::DrawCircle(center, screenRadius, static_cast<float>(Width), Color);
        }
    };

    class Line : public RenderObject {
    public:
        using PositionDelegate = std::function<Vec2()>;

        std::uint32_t Color = 0xFFFFFFFFu;
        Vec2 Start = {};
        Vec2 End = {};
        PositionDelegate StartPositionUpdate;
        PositionDelegate EndPositionUpdate;
        int Width = 1;

        Line(Vec2 start, Vec2 end, int width, std::uint32_t color)
            : Color(color), Start(start), End(end), Width(width) {}

        void OnEndScene() override {
            if (StartPositionUpdate) {
                Start = StartPositionUpdate();
            }
            if (EndPositionUpdate) {
                End = EndPositionUpdate();
            }
            SDK::Drawing::DrawLine(Start, End, static_cast<float>(Width), Color);
        }
    };

    class Rectangle : public RenderObject {
    public:
        using PositionDelegate = std::function<Vec2()>;

        std::uint32_t Color = 0xFFFFFFFFu;
        int X = 0;
        int Y = 0;
        int Width = 0;
        int Height = 0;
        PositionDelegate PositionUpdate;

        Rectangle(int x, int y, int width, int height, std::uint32_t color)
            : Color(color), X(x), Y(y), Width(width), Height(height) {}

        void OnEndScene() override {
            if (PositionUpdate) {
                const Vec2 pos = PositionUpdate();
                X = static_cast<int>(pos.x);
                Y = static_cast<int>(pos.y);
            }
            if (!ImGui::GetCurrentContext()) {
                return;
            }
            ImGui::GetForegroundDrawList()->AddRectFilled(
                ImVec2(static_cast<float>(X), static_cast<float>(Y)),
                ImVec2(static_cast<float>(X + Width), static_cast<float>(Y + Height)),
                ToImColor(Color));
        }
    };

    class Sprite : public RenderObject {
    public:
        using PositionDelegate = std::function<Vec2()>;
        using OnResetting = std::function<void(Sprite*)>;

        std::uint32_t Color = 0xFFFFFFFFu;
        Vec2 Position = {};
        Vec2 Scale = { 1.0f, 1.0f };
        float Rotation = 0.0f;
        int X = 0;
        int Y = 0;
        int Width = 0;
        int Height = 0;
        PositionDelegate PositionUpdate;
        OnResetting OnReset;

        Sprite() = default;
        ~Sprite() override {
            Dispose();
        }

        explicit Sprite(Vec2 position) : Position(position), X(static_cast<int>(position.x)), Y(static_cast<int>(position.y)) {}
        Sprite(const char* fileLocation, Vec2 position) : Sprite(position) {
            SDK::UI::Icons::ImagePixels pixels{};
            if (SDK::UI::Icons::LoadPixelsFromFile(fileLocation, pixels)) {
                UpdateTexturePixels(pixels, position);
            }
        }
        Sprite(const std::vector<std::uint8_t>& bytes, Vec2 position) : Sprite(position) {
            SDK::UI::Icons::ImagePixels pixels{};
            if (SDK::UI::Icons::LoadPixelsFromMemory(bytes.data(), static_cast<int>(bytes.size()), pixels)) {
                UpdateTexturePixels(pixels, position);
            }
        }
        Sprite(ImTextureID texture, int width, int height, Vec2 position) : Sprite(position) {
            SDK::UI::Icons::LoadedTexture loaded{};
            loaded.Texture = texture;
            loaded.Width = width;
            loaded.Height = height;
            SetTexture(loaded, false);
        }

        void Dispose() override {
            if (ownsTexture_) {
                SDK::UI::Icons::ReleaseTexture(texture_);
            } else {
                texture_ = {};
            }
        }

        void SetTexture(SDK::UI::Icons::LoadedTexture texture, bool ownsTexture = true) {
            Dispose();
            texture_ = texture;
            ownsTexture_ = ownsTexture;
            Width = texture.Width;
            Height = texture.Height;
            originalPixels_ = {};
            currentPixels_ = {};
        }

        void UpdateTexturePixels(const SDK::UI::Icons::ImagePixels& pixels, Vec2 position = {}) {
            if (!pixels.IsValid()) {
                return;
            }
            if (position.IsValid()) {
                Position = position;
                X = static_cast<int>(position.x);
                Y = static_cast<int>(position.y);
            }

            if (!originalPixels_.IsValid()) {
                originalPixels_ = pixels;
            }
            currentPixels_ = pixels;
            SDK::UI::Icons::LoadedTexture texture = SDK::UI::Icons::CreateTextureFromPixels(currentPixels_);
            if (!texture.Texture) {
                return;
            }

            Dispose();
            texture_ = texture;
            ownsTexture_ = true;
            Width = texture.Width;
            Height = texture.Height;
        }

        bool UpdateTextureBytes(const std::vector<std::uint8_t>& bytes, Vec2 position = {}) {
            SDK::UI::Icons::ImagePixels pixels{};
            if (!SDK::UI::Icons::LoadPixelsFromMemory(bytes.data(), static_cast<int>(bytes.size()), pixels)) {
                return false;
            }
            UpdateTexturePixels(pixels, position);
            return texture_.Texture != nullptr;
        }

        void Crop(int x, int y, int w, int h, bool scale = false) {
            if (texture_.Width <= 0 || texture_.Height <= 0) {
                return;
            }

            if (scale) {
                x = static_cast<int>(Scale.x * x);
                y = static_cast<int>(Scale.y * y);
                w = static_cast<int>(Scale.x * w);
                h = static_cast<int>(Scale.y * h);
            }

            uv0_ = ImVec2(
                static_cast<float>(x) / static_cast<float>(texture_.Width),
                static_cast<float>(y) / static_cast<float>(texture_.Height));
            uv1_ = ImVec2(
                static_cast<float>(x + w) / static_cast<float>(texture_.Width),
                static_cast<float>(y + h) / static_cast<float>(texture_.Height));
            Width = w;
            Height = h;
        }

        void Complement() {
            SetSaturation(-1.0f);
        }

        void Fade() {
            SetSaturation(0.5f);
        }

        void GrayScale() {
            SetSaturation(0.0f);
        }

        void Hide() {
            Visible(false);
        }

        void Reset() {
            saturation_ = 1.0f;
            if (originalPixels_.IsValid()) {
                currentPixels_ = originalPixels_;
                SDK::UI::Icons::LoadedTexture texture = SDK::UI::Icons::CreateTextureFromPixels(currentPixels_);
                if (texture.Texture) {
                    Dispose();
                    texture_ = texture;
                    ownsTexture_ = true;
                    Width = texture.Width;
                    Height = texture.Height;
                }
            }
            if (OnReset) {
                OnReset(this);
            }
        }

        void SetSaturation(float saturation) {
            saturation_ = saturation;
            if (!originalPixels_.IsValid()) {
                return;
            }

            currentPixels_ = SaturatePixels(originalPixels_, saturation_);
            SDK::UI::Icons::LoadedTexture texture = SDK::UI::Icons::CreateTextureFromPixels(currentPixels_);
            if (!texture.Texture) {
                return;
            }

            const int oldWidth = Width;
            const int oldHeight = Height;
            Dispose();
            texture_ = texture;
            ownsTexture_ = true;
            Width = oldWidth > 0 ? oldWidth : texture.Width;
            Height = oldHeight > 0 ? oldHeight : texture.Height;
        }

        void Show() {
            Visible(true);
        }

        void OnEndScene() override {
            if (PositionUpdate) {
                Position = PositionUpdate();
                X = static_cast<int>(Position.x);
                Y = static_cast<int>(Position.y);
            }
            if (!texture_.Texture || !ImGui::GetCurrentContext()) {
                return;
            }

            const float width = static_cast<float>(Width > 0 ? Width : texture_.Width) * Scale.x;
            const float height = static_cast<float>(Height > 0 ? Height : texture_.Height) * Scale.y;
            const ImVec2 min(static_cast<float>(X), static_cast<float>(Y));
            const ImVec2 max(static_cast<float>(X) + width, static_cast<float>(Y) + height);
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            const ImU32 tint = ToImColor(SaturatedColor());

            if (std::fabs(Rotation) <= 0.0001f) {
                drawList->AddImage(texture_.Texture, min, max, uv0_, uv1_, tint);
                return;
            }

            const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
            const float c = std::cos(Rotation);
            const float s = std::sin(Rotation);
            auto rotate = [&](float x, float y) {
                const float rx = x * c - y * s;
                const float ry = x * s + y * c;
                return ImVec2(center.x + rx, center.y + ry);
            };
            const float hw = width * 0.5f;
            const float hh = height * 0.5f;
            drawList->AddImageQuad(
                texture_.Texture,
                rotate(-hw, -hh),
                rotate(hw, -hh),
                rotate(hw, hh),
                rotate(-hw, hh),
                uv0_,
                ImVec2(uv1_.x, uv0_.y),
                uv1_,
                ImVec2(uv0_.x, uv1_.y),
                tint);
        }

    private:
        SDK::UI::Icons::LoadedTexture texture_{};
        SDK::UI::Icons::ImagePixels originalPixels_{};
        SDK::UI::Icons::ImagePixels currentPixels_{};
        bool ownsTexture_ = true;
        ImVec2 uv0_ = ImVec2(0.0f, 0.0f);
        ImVec2 uv1_ = ImVec2(1.0f, 1.0f);
        float saturation_ = 1.0f;

        static SDK::UI::Icons::ImagePixels SaturatePixels(const SDK::UI::Icons::ImagePixels& source,
                                                          float saturation) {
            SDK::UI::Icons::ImagePixels result = source;
            if (!result.IsValid()) {
                return result;
            }

            constexpr float kRWeight = 0.3086f;
            constexpr float kGWeight = 0.6094f;
            constexpr float kBWeight = 0.0820f;
            const float a = ((1.0f - saturation) * kRWeight) + saturation;
            const float b = (1.0f - saturation) * kRWeight;
            const float c = (1.0f - saturation) * kRWeight;
            const float d = (1.0f - saturation) * kGWeight;
            const float e = ((1.0f - saturation) * kGWeight) + saturation;
            const float f = (1.0f - saturation) * kGWeight;
            const float g = (1.0f - saturation) * kBWeight;
            const float h = (1.0f - saturation) * kBWeight;
            const float i = ((1.0f - saturation) * kBWeight) + saturation;
            const auto clampByte = [](float value) {
                return static_cast<std::uint8_t>(std::clamp(std::round(value), 0.0f, 255.0f));
            };

            for (std::size_t offset = 0; offset + 3 < result.Rgba.size(); offset += 4) {
                const float r = static_cast<float>(source.Rgba[offset + 0]);
                const float gr = static_cast<float>(source.Rgba[offset + 1]);
                const float bl = static_cast<float>(source.Rgba[offset + 2]);
                result.Rgba[offset + 0] = clampByte((a * r) + (d * gr) + (g * bl));
                result.Rgba[offset + 1] = clampByte((b * r) + (e * gr) + (h * bl));
                result.Rgba[offset + 2] = clampByte((c * r) + (f * gr) + (i * bl));
            }
            return result;
        }

        std::uint32_t SaturatedColor() const {
            if (originalPixels_.IsValid()) {
                return Color;
            }
            const float a = static_cast<float>((Color >> 24) & 0xFF);
            float r = static_cast<float>((Color >> 16) & 0xFF);
            float g = static_cast<float>((Color >> 8) & 0xFF);
            float b = static_cast<float>(Color & 0xFF);
            const float gray = (0.299f * r) + (0.587f * g) + (0.114f * b);
            r = gray + ((r - gray) * saturation_);
            g = gray + ((g - gray) * saturation_);
            b = gray + ((b - gray) * saturation_);
            const auto clampByte = [](float value) {
                return static_cast<std::uint32_t>(std::clamp(value, 0.0f, 255.0f));
            };
            return (clampByte(a) << 24) |
                   (clampByte(r) << 16) |
                   (clampByte(g) << 8) |
                   clampByte(b);
        }
    };

    class Text : public RenderObject {
    public:
        using PositionDelegate = std::function<Vec2()>;
        using TextDelegate = std::function<std::string()>;

        bool Centered = false;
        Vec2 Offset = {};
        bool OutLined = false;
        PositionDelegate PositionUpdate;
        TextDelegate TextUpdate;
        AIBaseClient Unit = {};
        std::uint32_t Color = 0xFFFFFFFFu;
        int Height = 0;
        int Width = 0;

        Text(std::string text, int x, int y, int size, std::uint32_t color, const char* fontName = "Calibri")
            : text_(std::move(text)), x_(x), y_(y), size_(size), fontName_(fontName ? fontName : ""), Color(color) {
            Measure();
        }

        Text(std::string text, Vec2 position, int size, std::uint32_t color, const char* fontName = "Calibri")
            : Text(std::move(text), static_cast<int>(position.x), static_cast<int>(position.y), size, color, fontName) {}

        Text(std::string text, AIBaseClient unit, Vec2 offset, int size, std::uint32_t color, const char* fontName = "Calibri")
            : Text(std::move(text), 0, 0, size, color, fontName) {
            Unit = unit;
            Offset = offset;
        }

        Text(int x, int y, std::string text, int size, std::uint32_t color, const char* fontName = "Calibri")
            : Text(std::move(text), x, y, size, color, fontName) {}

        Text(Vec2 position, std::string text, int size, std::uint32_t color, const char* fontName = "Calibri")
            : Text(std::move(text), position, size, color, fontName) {}

        const std::string& TextString() const {
            return text_;
        }

        void TextString(std::string value) {
            text_ = std::move(value);
            Measure();
        }

        int X() const {
            return PositionUpdate ? xCalculated_ : x_ + XOffset();
        }

        void X(int value) {
            x_ = value;
        }

        int Y() const {
            return PositionUpdate ? yCalculated_ : y_ + YOffset();
        }

        void Y(int value) {
            y_ = value;
        }

        void OnEndScene() override {
            if (TextUpdate) {
                TextString(TextUpdate());
            }
            if (text_.empty() || !ImGui::GetCurrentContext()) {
                return;
            }

            if (Unit.IsValid()) {
                Vec2 screen{};
                if (SDK::Drawing::WorldToScreen(Unit.Position(), screen)) {
                    x_ = static_cast<int>(screen.x + Offset.x);
                    y_ = static_cast<int>(screen.y + Offset.y);
                }
            }

            if (PositionUpdate) {
                const Vec2 pos = PositionUpdate();
                xCalculated_ = static_cast<int>(pos.x) + XOffset();
                yCalculated_ = static_cast<int>(pos.y) + YOffset();
            }

            const int x = X();
            const int y = Y();
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            if (OutLined) {
                const ImU32 outline = IM_COL32(0, 0, 0, 255);
                drawList->AddText(ImVec2(static_cast<float>(x - 1), static_cast<float>(y - 1)), outline, text_.c_str());
                drawList->AddText(ImVec2(static_cast<float>(x + 1), static_cast<float>(y + 1)), outline, text_.c_str());
                drawList->AddText(ImVec2(static_cast<float>(x - 1), static_cast<float>(y)), outline, text_.c_str());
                drawList->AddText(ImVec2(static_cast<float>(x + 1), static_cast<float>(y)), outline, text_.c_str());
            }
            drawList->AddText(ImVec2(static_cast<float>(x), static_cast<float>(y)), ToImColor(Color), text_.c_str());
        }

    private:
        std::string text_;
        int x_ = 0;
        int y_ = 0;
        int xCalculated_ = 0;
        int yCalculated_ = 0;
        int size_ = 12;
        std::string fontName_;

        int XOffset() const {
            return Centered ? -Width / 2 : 0;
        }

        int YOffset() const {
            return Centered ? -Height / 2 : 0;
        }

        void Measure() {
            if (ImGui::GetCurrentContext()) {
                const ImVec2 size = ImGui::CalcTextSize(text_.c_str());
                Width = static_cast<int>(size.x);
                Height = static_cast<int>(size.y);
            } else {
                Width = static_cast<int>(text_.size() * (size_ / 2));
                Height = size_;
            }
        }
    };

    static RenderObject* Add(RenderObject* renderObject, float layer = 3.402823466e+38F) {
        EnsureInitialized();
        if (!renderObject) {
            return nullptr;
        }
        if (layer != 3.402823466e+38F) {
            renderObject->Layer = layer;
        }
        auto& objects = Objects();
        if (std::find(objects.begin(), objects.end(), renderObject) == objects.end()) {
            objects.push_back(renderObject);
        }
        return renderObject;
    }

    static void Remove(RenderObject* renderObject) {
        auto& objects = Objects();
        objects.erase(std::remove(objects.begin(), objects.end(), renderObject), objects.end());
        auto& visible = VisibleObjects();
        visible.erase(std::remove(visible.begin(), visible.end(), renderObject), visible.end());
    }

    static bool OnScreen(Vec2 point) {
        return SDK::Drawing::OnScreen(point);
    }

    template <typename TObject>
    static TObject* Add(TObject& renderObject, float layer = 3.402823466e+38F) {
        return static_cast<TObject*>(Add(&renderObject, layer));
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using Render = ::SDK::Core::Utils::Render;
} // namespace SDK::Utils
