#pragma once

// ============================================================================
// Prediction.h - Umbrella header for the prediction subsystem
// ----------------------------------------------------------------------------
// Includes Movement.h which contains:
//   - PredictionInput / PredictionOutput structs
//   - Movement namespace with all prediction methods
//   - Vec2Ext / Vec3Ext helper functions
// Also exposes:
//   - GamePath::PathTracker for path tracking
// ============================================================================

#include "Prediction/Movement.h"
#include "Prediction/Cluster.h"
#include "Prediction/GamePath.h"
#include "ConvexHull.h"
#include "Geometry.h"
