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
        // PermaShow is enabled by default; the panel itself stays empty
        // until plugins call MenuItem::AddPermashow().
        inline bool enabled = true;

        // Geometry — width/height defaults match the EnsoulSharp box.
        inline int width = 250;
        inline int indicatorWidth = 45;

        // x/y are populated on first frame from `positionInitialized` so the
        // panel snaps to the bottom-right corner of the overlay window.
        // Once the user drags it, we keep their custom position.
        inline int  x = 0;
        inline int  y = 0;
        inline bool positionInitialized = false;
    }

} // namespace Config
