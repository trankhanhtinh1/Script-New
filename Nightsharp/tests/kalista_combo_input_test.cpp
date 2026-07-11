#include <cassert>

#include "../plugins/Champion/7UPAIO/KalistaComboInput.h"

int main() {
    using Plugins::AIO7UP::Kalista::ComboInput::IsActive;

    assert(IsActive(true, false, false));
    assert(IsActive(false, true, true));
    assert(!IsActive(false, false, true));
    assert(!IsActive(false, true, false));
    return 0;
}
