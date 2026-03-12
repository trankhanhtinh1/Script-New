#pragma once

#include "Game.h"

namespace SDK {

    class FrameRefresh {
    public:
        template <typename Fn>
        void Run(Fn&& fn) {
            const int currentFrame = Game::GetScriptFrameId();
            if (m_lastProcessedFrame == currentFrame) {
                return;
            }

            m_lastProcessedFrame = currentFrame;
            fn();
        }

        void Invalidate() {
            m_lastProcessedFrame = -1;
        }

        int LastProcessedFrame() const {
            return m_lastProcessedFrame;
        }

    private:
        int m_lastProcessedFrame = -1;
    };

} // namespace SDK
