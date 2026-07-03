#pragma once

#include <string>
#include <unordered_set>

namespace SDK::Modes {

class PriorityCategory {
public:
    int Value = 1;
    std::unordered_set<std::string> Champions;

    PriorityCategory() = default;
    PriorityCategory(int value, std::initializer_list<const char*> champs)
        : Value(value) {
        for (const char* c : champs) {
            Champions.insert(c);
        }
    }
};

} // namespace SDK::Modes
