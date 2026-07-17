#pragma once

namespace Config {

    namespace ZoomHack {
        inline bool enabled = false;
        inline float maxZoom = 4000.0f;
    }

    namespace SkinChanger {
        inline bool enabled = false;
        inline int skinId = 0;
    }

    namespace StreamProtection {
        inline bool bypassObs = false;
    }

    namespace OverlayInput {
        // The overlay remains transparent outside the visible menu bounds.
        // A fully click-through menu cannot be interacted with, so menu input
        // capture is the safe/default mode.
        inline bool clickThrough = false;
    }

    namespace Language {
        // 0 = EN, 1 = CN, 2 = VN. Persisted to core.ini via ConfigStore.
        inline int index = 0;
    }

    namespace PermaShow {
        // PermaShow is enabled by default; the panel itself stays empty
        // until plugins call MenuItem::AddPermashow().
        inline bool enabled = true;

        // EnsoulSharp stores X as the horizontal center of the panel and scales
        // Width/IndicatorWidth by screenWidth / 1366 at draw time.
        inline int width = 300;
        inline int indicatorWidth = 45;

        // The source defaults depend on the current game resolution, so the
        // renderer initializes these once the swap-chain dimensions are known.
        inline int  x = 0;
        inline int  y = 0;
        inline bool positionInitialized = false;
    }

    namespace MenuStyle {
        // 0 = EnsoulSharp, 1 = BGX
        inline int index = 0;
        inline int maxItemsPerColumn = 15;
    }

} // namespace Config
