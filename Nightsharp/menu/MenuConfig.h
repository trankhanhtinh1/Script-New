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
        inline bool clickThrough = true;
    }

    namespace PermaShow {
        // PermaShow is enabled by default; the panel itself stays empty
        // until plugins call MenuItem::AddPermashow().
        inline bool enabled = true;

        // Geometry — width/height defaults match the EnsoulSharp box.
        inline int width = 250;
        inline int indicatorWidth = 45;

        // Default panel position. Dragging the panel can still change this at runtime.
        inline int  x = 1350;
        inline int  y = 800;
        inline bool positionInitialized = true;
    }

} // namespace Config
