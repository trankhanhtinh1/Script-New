#pragma once

// ============================================================================
// Prediction.h - Umbrella header for the prediction subsystem
// ----------------------------------------------------------------------------
// Includes Movement.h which contains:
//   - PredictionInput / PredictionOutput structs
//   - Movement namespace with all prediction methods
//   - GamePath::PathTracker for path tracking
//   - Vec2Ext / Vec3Ext helper functions
// ============================================================================

#include "Prediction/Movement.h"
#include "Prediction/Cluster.h"
#include "ConvexHull.h"
#include "Geometry.h"
