#pragma once

// Public include for consumers that need the concrete Kuro service.  The
// implementation currently lives with the plugin registration so lifecycle
// ownership stays in one translation unit/header; keeping this façade gives
// SDK consumers a stable, intention-revealing include path.
#include "KuroTargetSelectorPlugin.h"

