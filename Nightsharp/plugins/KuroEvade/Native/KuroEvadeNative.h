#pragma once

// KuroEvade integration surface. Database content and special-spell adapters
// are local to this plugin; the runtime layout mirrors the supplied Evade
// project and only crosses into NightSharp through Core/SDK APIs.

#include "CastType.h"
#include "Drawing/Primitives/RenderCircle.h"
#include "Drawing/Primitives/RenderLine.h"
#include "Drawing/Primitives/RenderObject.h"
#include "Drawing/Primitives/RenderObjects.h"
#include "Drawing/Primitives/RenderText.h"
#include "Config/EvadeConfig.h"
#include "Database/EvadeSpellData.h"
#include "Database/EvadeSpellDatabase.h"
#include "EvadeType.h"
#include "Helpers/Utils.h"
#include "Helpers/Helpers.h"
#include "Database/SpellData.h"
#include "Database/SpellDatabase.h"
#include "Database/SpellBlocker.h"
#include "Benchmarking/Benchmark.h"
#include "SpellMenuKey.h"
#include "SpellTargets.h"

#include "SpecialSpells/SpecialSpellCommon.h"
#include "SpecialSpells/SpecialSpellProcessor.h"

#include "Engine/Geometry.h"
#include "Engine/Skillshot.h"
#include "Engine/Collision.h"
#include "Engine/SkillshotDetector.h"
#include "Engine/Evader.h"
#include "Engine/EvadeSpell.h"
#include "Engine/EvadeEngine.h"

#include "Drawing/SpellDrawer.h"
