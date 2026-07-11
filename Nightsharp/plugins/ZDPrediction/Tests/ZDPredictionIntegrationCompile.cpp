#include "../../../FpsDropDebug.h"
#include "../ZDPrediction.h"

#include <type_traits>

static_assert(std::is_base_of_v<SDK::Prediction::IPrediction, ZDPrediction::PredictionEngine>);
static_assert(std::is_base_of_v<Plugins::IPlugin, Plugins::ZDPredictionPlugin>);

void CompileZDPredictionIntegration() {
    ZDPrediction::PredictionEngine engine;
    Plugins::ZDPredictionPlugin plugin;
    (void)engine.GetConfig();
    (void)plugin.GetInternalId();
}
