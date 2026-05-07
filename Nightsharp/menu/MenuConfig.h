#pragma once
/*
 * NightSharp v2.0 — Menu configuration
 */

namespace Config {

    namespace ZoomHack {
        inline bool  enabled  = false;
        inline float maxZoom  = 4000.0f;  // vanilla cap ~ 1750
    }

    namespace SkinChanger {
        inline bool enabled = false;
        inline int  skinId  = 0;          // 0 = base skin
    }

    namespace StreamProtection {
        inline bool bypassObs = false;
    }

} // namespace Config
