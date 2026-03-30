#pragma once

namespace SDK::UI::IMenu::Skins {

    class ADrawable {
    public:
        virtual ~ADrawable() = default;
        virtual int Width() const { return 0; }
        virtual void Draw() {}
    };

    template<typename T>
    class ADrawableT : public ADrawable {
    public:
        explicit ADrawableT(T* component = nullptr)
            : m_component(component) {}

        T* Component() const { return m_component; }

    protected:
        T* m_component = nullptr;
    };

}
