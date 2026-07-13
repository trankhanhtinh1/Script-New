#pragma once

#include "../Core/MenuNode.h"

#include <Windows.h>

#include <string>

namespace NightSharp::Menu {

class KeyBindController {
public:
    void BeginCapture(const MenuItemHandle& item);
    void Cancel();
    void Reset();
    bool IsCapturing(const MenuItem& item) const;
    bool HandleMessage(UINT message, WPARAM wParam);
    static std::string KeyName(int key);

private:
    std::weak_ptr<MenuItem> activeItem_;
};

}
