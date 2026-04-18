#pragma once

#include "../../Enumerations/OrbwalkerMode.h"
#include "../../Enumerations/OrbwalkingType.h"
#include "../../Core/Game.h"
#include "../../Core/Objects.h"
#include "../../Utils/AutoAttack.h"

#include <algorithm>
#include <cstdint>

namespace Plugins { class OrbwalkerPlugin; }

namespace SDK {

// --- Event args matching EnsoulSharp OrbwalkingActionArgs ---
struct OrbwalkingActionArgs {
    AIBaseClient Sender = {};
    AIBaseClient Target = {};
    Vector3 Position = {};
    bool Process = true;
    OrbwalkingType Type = OrbwalkingType::None;
};

// --- OrbwalkerBase – matches EnsoulSharp OrbwalkerBase<TK, T> ---
class OrbwalkerBase {
    friend class ::Plugins::OrbwalkerPlugin;
public:
    using ActionHandler = void(*)(OrbwalkingActionArgs&);

    OrbwalkerMode ActiveMode    = OrbwalkerMode::None;
    int           LastAutoAttackTick = 0;
    float         LastAutoAttackTime = 0.0f;
    bool          MissileLaunched    = false;
    AIBaseClient  LastTarget         = {};

    struct TickDiag {
        bool canAttack   = false;
        bool canMove     = false;
        bool attackIssued = false;
        bool moveIssued  = false;
        int  targetNetId = 0;
    } lastTickDiag = {};

    void ResetSwingTimer() { LastAutoAttackTick = 0; LastAutoAttackTime = 0.0f; }

    bool AddOnAction(ActionHandler handler) {
        if (!handler) return false;
        for (int i = 0; i < kMaxHandlers; ++i) {
            if (s_actionHandlers[i] == nullptr) {
                s_actionHandlers[i] = handler;
                return true;
            }
        }
        return false;
    }

    bool OnBeforeAttack(ActionHandler h) { return AddOnAction(h); }
    bool OnAfterAttack(ActionHandler h)  { return AddOnAction(h); }

    static void ResetHandlers() {
        for (int i = 0; i < kMaxHandlers; ++i) s_actionHandlers[i] = nullptr;
    }

protected:
    void InvokeAction(OrbwalkingActionArgs& e) const {
        for (int i = 0; i < kMaxHandlers && s_actionHandlers[i]; ++i) {
            s_actionHandlers[i](e);
        }
    }

private:
    static constexpr int kMaxHandlers = 32;
    static inline ActionHandler s_actionHandlers[kMaxHandlers] = {};
};

} // namespace SDK
