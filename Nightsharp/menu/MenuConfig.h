#pragma once

namespace Config {

    namespace ZoomHack {
        inline bool enabled = false;
    }

    namespace SkinChanger {
        inline bool enabled = false;
        inline int skinId = 0;
    }

    namespace StreamProtection {
        inline bool bypassObs = false;
    }

    namespace OverlayInput {
        inline bool clickThrough = true;
    }

    namespace PermaShow {
        inline bool enabled = true;
        inline int width = 250;
        inline int indicatorWidth = 45;
        inline int  x = 0;
        inline int  y = 0;
        inline bool positionInitialized = false;
    }

    namespace Rendering {
        inline bool useInternal = true;
    }

} // namespace Config
