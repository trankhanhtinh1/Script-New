#pragma once

#include <Windows.h>
#include <cstdint>
#include <cstdlib>
#include <functional>

namespace EloBuddy {
namespace Native {

using byte = std::uint8_t;

class Humanizer {
private:
    int m_minDelay;
    int m_maxDelay;
    int m_delay;
    DWORD m_lastExec;
    byte m_hash;

public:
    Humanizer(int minDelay, int maxDelay)
        : m_minDelay(minDelay),
          m_maxDelay(maxDelay),
          m_delay(0),
          m_lastExec(0),
          m_hash(0) {
        m_delay = GetDelay();
    }

    int GetDelay() const {
        if (m_maxDelay <= m_minDelay) {
            return m_minDelay > 0 ? m_minDelay : 0;
        }

        const int span = (m_maxDelay - m_minDelay) + 1;
        return m_minDelay + (std::rand() % span);
    }

    bool Execute(const std::function<bool()>& fnc) {
        if (!CanExecute()) {
            return false;
        }

        return fnc ? fnc() : false;
    }

    bool CanExecute() {
        const DWORD tickCount = GetTickCount();
        if (tickCount - m_lastExec >= static_cast<DWORD>(m_delay)) {
            m_lastExec = tickCount;
            m_delay = GetDelay();
            return true;
        }

        return false;
    }

    bool CanExecute(byte hash) {
        if (m_hash == hash) {
            return CanExecute();
        }

        m_hash = hash;
        m_lastExec = GetTickCount();
        m_delay = GetDelay();
        return true;
    }
};

} // namespace Native
} // namespace EloBuddy
