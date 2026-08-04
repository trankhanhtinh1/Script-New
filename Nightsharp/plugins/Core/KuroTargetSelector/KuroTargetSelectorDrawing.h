#pragma once

#include "KuroTargetSelectorContracts.h"
#include "KuroTargetSelectorMenu.h"

#include "../../../sdk/UI/Drawing.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace Plugins::KuroTargetSelector {

class Drawing final {
public:
    explicit Drawing(Menu* menu)
        : menu_(menu) {
        BuildMenu();
        Resume();
    }

    ~Drawing() {
        Suspend();
    }

    void Suspend() {
        if (suspended_) return;
        ::SDK::Drawing::OnDraw -= &OnDrawHandler;
        if (Ptr() == this) Ptr() = nullptr;
        suspended_ = true;
    }

    void Resume() {
        if (!suspended_) return;

        // The event list de-duplicates identical function pointers, but the
        // static trampoline can only safely point at one live drawing owner.
        // Releasing the previous owner here also prevents stale callbacks if
        // a plugin is reloaded without a process restart.
        if (Ptr() && Ptr() != this) Ptr()->Suspend();
        Ptr() = this;
        ::SDK::Drawing::OnDraw += &OnDrawHandler;
        suspended_ = false;
    }

    void Draw(const SelectionState& state,
              const std::vector<TargetDecision>& decisions) {
        state_ = state;
        decisions_ = decisions;
    }

private:
    static Drawing*& Ptr() {
        static Drawing* ptr = nullptr;
        return ptr;
    }

    void BuildMenu() {
        if (!menu_ || !menu_->NativeMenu()) return;
        auto* native = menu_->NativeMenu();
        drawingMenu_ = native->AddSubMenu(
            new ::SDK::Menu("Drawing", "Drawing"));
        drawingMenu_->Add(new ::SDK::MenuBool(
            "Selected", "Draw Selected Target", true));
        drawingMenu_->Add(new ::SDK::MenuBool(
            "LegalTargets", "Draw Legal Targets", false));
        drawingMenu_->Add(new ::SDK::MenuBool(
            "TopCandidates", "Draw Top 3 Candidates", false));
        drawingMenu_->Add(new ::SDK::MenuBool(
            "Diagnostics", "Draw Score Diagnostics", false));
    }

    static void OnDrawHandler() {
        auto* self = Ptr();
        if (self && !self->suspended_) self->Render();
    }

    bool Enabled(const char* key, bool fallback) const {
        const auto* item = drawingMenu_
            ? drawingMenu_->Get<::SDK::MenuBool>(key)
            : nullptr;
        return item ? item->Value : fallback;
    }

    void Render() {
        if (!menu_ || !::SDK::Drawing::IsEnabled()) return;

        if (Enabled("Selected", true) && state_.Selected.IsValid() &&
            state_.Selected.IsTargetable()) {
            ::SDK::Drawing::DrawCircle(
                state_.Selected.Position(),
                state_.Selected.BoundingRadius() + 28.0f,
                0xFFFF3B30u,
                3.0f,
                48);
        }

        if (Enabled("LegalTargets", false)) {
            const bool topCandidates = Enabled("TopCandidates", false);
            int drawn = 0;
            for (const auto& decision : decisions_) {
                if (!decision.Legal || !decision.Target.IsValid()) continue;
                ::SDK::Drawing::DrawCircle(
                    decision.Target.Position(),
                    decision.Target.BoundingRadius() + 12.0f,
                    0xFF39D353u,
                    1.5f,
                    32);
                if (topCandidates && ++drawn >= 3) break;
            }
        }

        if (!Enabled("Diagnostics", false)) return;
        for (const auto& decision : decisions_) {
            if (!decision.Target.IsValid()) continue;
            char text[96] = {};
            if (decision.Legal) {
                std::snprintf(text, sizeof(text), "%.1f", decision.Score);
            } else {
                std::snprintf(text, sizeof(text), "%s",
                              RejectReasonName(decision.Rejection));
            }
            ::SDK::Drawing::DrawText(
                decision.Target.Position(), text,
                decision.Legal ? 0xFFFFFFFFu : 0xFFFFA07Au,
                true);
        }
    }

    Menu* menu_ = nullptr;
    ::SDK::Menu* drawingMenu_ = nullptr;
    SelectionState state_ = {};
    std::vector<TargetDecision> decisions_;
    bool suspended_ = true;
};

} // namespace Plugins::KuroTargetSelector
