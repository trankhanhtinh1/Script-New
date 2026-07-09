#pragma once

#include "EvadeOrderCommand.h"
#include "EvadeSpellData.h"

#include "../../../Core/CoreControl.h"
#include "../../../SDK/SDK.h"

namespace Plugins::KuroEvade {

struct EvadeCommand {
    EvadeOrderCommand order = EvadeOrderCommand::MoveTo;
    Vec2 targetPosition;
    SDK::AIBaseClient target;
    float timestamp = 0.0f;
    bool isProcessed = false;
    EvadeSpellData evadeSpellData;

    static EvadeCommand MoveTo(const Vec2& movePos) {
        EvadeCommand command;
        command.order = EvadeOrderCommand::MoveTo;
        command.targetPosition = movePos;
        command.timestamp = static_cast<float>(SDK::Variables::TickCount());

        const auto player = SDK::ObjectManager::Player();
        if (player.IsValid()) {
            CoreControl::IssueMove(Vec3::From2D(movePos, player.ServerPosition().y), true);
        }
        return command;
    }
};

} // namespace Plugins::KuroEvade

