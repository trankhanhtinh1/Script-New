#pragma once

#include "../IPlugin.h"
#include "../../SDK/SDK.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "ActivatorEngine.h"
#include "SdkObservationBridge.h"
#include "PatchOverrides.h"
#include "AwarenessActivatorMenu.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace NightSharp::Companion {

class AwarenessActivatorPlugin final : public Plugins::IPlugin {
public:
    AwarenessActivatorPlugin() = default;

    const char* GetName() const override { return "Awareness + Activator"; }
    const char* GetInternalId() const override { return "core.awareness_activator"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    Plugins::PluginCategory GetCategory() const override { return Plugins::PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        if (!CanLoad()) {
            loaded_ = false;
            return;
        }
        awareness_.Reset();
        layerGuard_.Reset();
        LoadPatchOverrides();
        bridge_.Attach(awareness_);
        loaded_ = bridge_.IsAttached();
        if (loaded_) {
            menu_.Attach(
                settings_, awareness_.Registry().PatchVersion(),
                &AwarenessActivatorPlugin::ExportDecisionLogCallback, this,
                &AwarenessActivatorPlugin::ApplyChampionPresetCallback, this);
        }
        NightSharpDebug::Logf("[AwarenessActivator] load attached=%d", loaded_ ? 1 : 0);
    }

    void OnUnload() override {
        menu_.Detach();
        bridge_.Detach();
        hasCandidate_ = false;
        loaded_ = false;
        NightSharpDebug::Logf("[AwarenessActivator] unloaded");
    }

    void OnUpdate() override {
        if (!loaded_ || !bridge_.IsAttached()) return;
        menu_.SyncSettingsFromMenu();

        bridge_.Update();
        HandleAudioAlerts();
        hasCandidate_ = false;
        if (!settings_.enabled) return;

        const ActionRequest* request = activator_.Evaluate(awareness_, settings_);
        if (!request) return;
        candidate_ = *request;
        hasCandidate_ = true;
        TryExecute(candidate_);
    }

    void OnRender() override {
        if (!loaded_ || !SDK::Drawing::IsEnabled()) return;
        if (settings_.hudEditor) DrawHudEditor();
        if (!settings_.drawOverlay || settings_.audioOnly) return;
        RenderLayer(
            LayerWorld, settings_.drawWorldLayer,
            &AwarenessActivatorPlugin::DrawWorldLayer);
        RenderLayer(
            LayerMinimap, settings_.drawMinimapLayer,
            &AwarenessActivatorPlugin::DrawMinimapLayer);
        RenderLayer(
            LayerEnemyHud, settings_.drawEnemyHud,
            &AwarenessActivatorPlugin::DrawEnemyHud);
        RenderLayer(
            LayerAlertCenter, settings_.drawAlertCenter,
            &AwarenessActivatorPlugin::DrawPanel);
    }

    void OnMenu() override {
        if (!ImGui::CollapsingHeader("Awareness + Activator")) return;
        ImGui::Text("Patch registry: %s", awareness_.Registry().PatchVersion());

        ImGui::Checkbox("Enabled", &settings_.enabled);
        ImGui::Checkbox("Draw awareness overlay", &settings_.drawOverlay);
        ImGui::Checkbox("Draw isolated world layer",
                        &settings_.drawWorldLayer);
        ImGui::Checkbox("Draw isolated minimap layer",
                        &settings_.drawMinimapLayer);
        ImGui::Checkbox("Draw prioritized alert center",
                        &settings_.drawAlertCenter);
        ImGui::Checkbox("Draw enemy side HUD", &settings_.drawEnemyHud);
        ImGui::Checkbox("Draw enemy reachable areas", &settings_.drawReachableAreas);
        ImGui::Checkbox("Draw threat geometry", &settings_.drawThreats);
        ImGui::Checkbox("Draw wards", &settings_.drawWards);
        ImGui::Checkbox("Draw structures", &settings_.drawStructures);
        ImGui::Checkbox("Draw objectives", &settings_.drawObjectives);
        ImGui::Checkbox("Draw jungle confidence", &settings_.drawJungle);
        ImGui::Checkbox("Draw combat state", &settings_.drawCombatState);
        ImGui::Checkbox(
            settings_.vietnamese ? "Hien thong tin nang cao" : "Draw advanced insights",
            &settings_.drawInsights);
        ImGui::Checkbox(
            settings_.vietnamese ? "Hien trang thai linh" : "Draw wave state",
            &settings_.drawWave);
        ImGui::Checkbox("Replay activity heatmap",
                        &settings_.drawActivityHeatmap);
        ImGui::Checkbox("Vision coverage heatmap",
                        &settings_.drawVisionHeatmap);
        ImGui::Checkbox("Audio-only accessibility", &settings_.audioOnly);
        ImGui::Checkbox("Streamer privacy mode", &settings_.streamerMode);
        ImGui::Checkbox("Tieng Viet", &settings_.vietnamese);
        ImGui::Checkbox("Draggable HUD editor", &settings_.hudEditor);
        ImGui::Checkbox("Summoner activator", &settings_.summonersEnabled);
        ImGui::Checkbox("Defensive items", &settings_.defensiveItemsEnabled);
        ImGui::Checkbox("Support items", &settings_.supportItemsEnabled);
        ImGui::Checkbox("Movement items", &settings_.movementItemsEnabled);
        ImGui::Checkbox("Offensive items", &settings_.offensiveItemsEnabled);
        ImGui::Checkbox("Vision items", &settings_.visionItemsEnabled);
        ImGui::Checkbox("Potion", &settings_.potionEnabled);
        ImGui::Checkbox("Allow practice automation", &settings_.allowPracticeAutomation);
        ImGui::Checkbox("Do not interrupt recall", &settings_.doNotInterruptRecall);
        ImGui::SliderInt("Confirmation virtual key", &settings_.confirmationVirtualKey, 1, 255);
        ImGui::SliderFloat("Defensive horizon", &settings_.defensiveHorizon, 0.15f, 3.0f, "%.2f s");
        ImGui::SliderFloat("Protection threshold", &settings_.protectionThreshold, 0.15f, 1.25f, "%.2f");
        ImGui::SliderFloat("Ally save threshold", &settings_.allySaveThreshold, 0.10f, 0.90f, "%.2f");
        ImGui::SliderFloat("Ignite safety margin", &settings_.offensiveSafetyMargin, 0.0f, 100.0f, "%.1f");
        ImGui::Checkbox("Reserve one Smite charge from Scuttle", &settings_.reserveSmiteCharge);
        ImGui::SliderFloat("Cleanse reaction delay", &settings_.cleanseReactionDelay,
                           0.0f, 0.20f, "%.2f s");

        ImGui::SeparatorText("Capability action modes");
        DrawCapabilityMode("Barrier", Capability::Barrier);
        DrawCapabilityMode("Cleanse", Capability::Cleanse);
        DrawCapabilityMode("Exhaust", Capability::Exhaust);
        DrawCapabilityMode("Flash", Capability::Flash);
        DrawCapabilityMode("Ghost", Capability::Ghost);
        DrawCapabilityMode("Heal", Capability::Heal);
        DrawCapabilityMode("Ignite", Capability::Ignite);
        DrawCapabilityMode("Smite", Capability::Smite);
        DrawCapabilityMode("Teleport", Capability::Teleport);
        DrawCapabilityMode("QSS", Capability::Qss);
        DrawCapabilityMode("Mercurial", Capability::Mercurial);
        DrawCapabilityMode("Mikael", Capability::Mikael);
        DrawCapabilityMode("Zhonya", Capability::Zhonya);
        DrawCapabilityMode("Seeker", Capability::Seeker);
        DrawCapabilityMode("Seraph", Capability::Seraph);
        DrawCapabilityMode("Locket", Capability::Locket);
        DrawCapabilityMode("Redemption", Capability::Redemption);
        DrawCapabilityMode("Shurelya", Capability::Shurelya);
        DrawCapabilityMode("Youmuu", Capability::Youmuu);
        DrawCapabilityMode("Rocketbelt", Capability::Rocketbelt);
        DrawCapabilityMode("Stridebreaker", Capability::Stridebreaker);
        DrawCapabilityMode("Gunblade", Capability::Gunblade);
        DrawCapabilityMode("Tiamat", Capability::Tiamat);
        DrawCapabilityMode("Ravenous Hydra", Capability::RavenousHydra);
        DrawCapabilityMode("Titanic Hydra", Capability::TitanicHydra);
        DrawCapabilityMode("Profane Hydra", Capability::ProfaneHydra);
        DrawCapabilityMode("Randuin", Capability::Randuin);
        DrawCapabilityMode("Actualizer", Capability::Actualizer);
        DrawCapabilityMode("Oracle Lens", Capability::Oracle);
        DrawCapabilityMode("Farsight", Capability::Farsight);
        DrawCapabilityMode("Ward", Capability::Ward);
        DrawCapabilityMode("Herald", Capability::Herald);
        DrawCapabilityMode("Potion", Capability::Potion);
        DrawCapabilityMode("Knight's Vow", Capability::KnightsVow);
        if (ImGui::Button("Export decisions + telemetry")) {
            ExportDecisionLog();
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply champion preset")) {
            ApplyChampionPreset();
        }
        ImGui::Text("events: %zu  alerts: %zu",
                    awareness_.Store().TeamfightTimeline().Size(),
                    awareness_.Store().Alerts().Size());
        menu_.SyncMenuFromSettings();
    }

    bool LoadSucceeded() const override { return loaded_; }

private:
    enum PresentationLayer : std::uint32_t {
        LayerWorld = 1u << 0,
        LayerMinimap = 1u << 1,
        LayerEnemyHud = 1u << 2,
        LayerAlertCenter = 1u << 3,
    };

    using LayerDraw = void (AwarenessActivatorPlugin::*)() const;

    void RenderLayer(std::uint32_t layer,
                     bool enabled,
                     LayerDraw draw) noexcept {
        const bool wasFaulted = layerGuard_.IsFaulted(layer);
        layerGuard_.Run(layer, enabled, [&]() {
            (this->*draw)();
        });
        if (!wasFaulted && layerGuard_.IsFaulted(layer)) {
            NightSharpDebug::Logf(
                "[AwarenessActivator] presentation layer faulted: %u",
                layer);
        }
    }
    static const char* ActionModeName(ActionMode mode) noexcept {
        switch (mode) {
        case ActionMode::Off: return "Off";
        case ActionMode::Suggest: return "Suggest";
        case ActionMode::Confirm: return "Confirm";
        case ActionMode::Auto: return "Auto";
        default: return "Unknown";
        }
    }

    static const char* RuntimeModeName(RuntimeMode mode) noexcept {
        switch (mode) {
        case RuntimeMode::Practice: return "Practice";
        case RuntimeMode::Research: return "Research";
        case RuntimeMode::Spectator: return "Spectator";
        case RuntimeMode::Replay: return "Replay";
        default: return "Companion";
        }
    }

    static SDK::Vector3 ToSdkPoint(const Point3& point) noexcept {
        return SDK::Vector3(point.x, point.y, point.z);
    }

    static bool IsValidPoint(const Point3& point) noexcept {
        return !point.IsZero() && std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    }

    static std::uint32_t Fingerprint(const ActionRequest& request) noexcept {
        std::uint32_t value = 2166136261u;
        const auto mix = [&value](std::uint32_t part) {
            value ^= part;
            value *= 16777619u;
        };
        mix(static_cast<std::uint32_t>(request.capability));
        mix(static_cast<std::uint32_t>(request.itemId));
        mix(static_cast<std::uint32_t>(request.spellSlot + 2));
        mix(static_cast<std::uint32_t>(request.itemSlot + 2));
        mix(request.targetId);
        mix(static_cast<std::uint32_t>(std::max(0.0f, request.position.x) * 0.25f));
        mix(static_cast<std::uint32_t>(std::max(0.0f, request.position.z) * 0.25f));
        return value;
    }

    void DrawCapabilityMode(const char* label, Capability capability) {
        int mode = static_cast<int>(settings_.ModeFor(capability));
        char id[96] = {};
        std::snprintf(id, sizeof(id), "%s##capability_%u", label, static_cast<unsigned>(capability));
        static const char* modes[] = { "Off", "Suggest", "Confirm", "Auto" };
        if (ImGui::Combo(id, &mode, modes, 4)) {
            settings_.SetMode(capability, static_cast<ActionMode>(std::clamp(mode, 0, 3)));
        }
    }

    bool InputSafe() const noexcept {
        if (!SDK::Game::IsFocused() || !SDK::Game::ShouldProcessInput()) return false;
        if (settings_.doNotInterruptRecall) {
            bool recalling = false;
            awareness_.Store().ForEachChampion([&](const ChampionState& state) {
                if (state.local) recalling = state.recalling;
            });
            if (recalling) return false;
        }
        return true;
    }

    bool ConfirmationReady(const ActionRequest& request) const noexcept {
        if (request.mode == ActionMode::Suggest || request.mode == ActionMode::Off) return false;
        if (request.mode == ActionMode::Auto &&
            awareness_.Mode() == RuntimeMode::Practice && settings_.allowPracticeAutomation) {
            return true;
        }
        if (request.mode == ActionMode::Auto) return false;
        return (GetAsyncKeyState(settings_.confirmationVirtualKey) & 0x8000) != 0;
    }

    bool LiveActionValid(
        const SDK::AIHeroClient& player,
        const SDK::AIBaseClient& target,
        const ActionRequest& request) const noexcept {
        float range = 0.0f;
        if (request.itemId != SDK::ItemId::Unknown) {
            const ItemDefinition* definition =
                awareness_.Registry().FindItem(
                    request.itemId);
            if (definition) range = definition->range;
        } else {
            const SummonerDefinition* definition =
                awareness_.Registry().FindAvailableSummoner(
                    request.capability);
            if (definition) range = definition->range;
        }

        if (target.IsValid()) {
            if (request.capability != Capability::Teleport &&
                (target.IsDead() || !target.IsTargetable())) {
                return false;
            }
            switch (request.capability) {
            case Capability::Ignite:
            case Capability::Exhaust:
            case Capability::Gunblade:
                if (!target.IsEnemy()) return false;
                break;
            case Capability::Mikael:
            case Capability::KnightsVow:
            case Capability::Teleport:
                if (!target.IsAlly()) return false;
                break;
            default:
                break;
            }
            if (range > 0.0f &&
                player.Position().Distance2D(
                    target.Position()) >
                    range + target.BoundingRadius() + 35.0f) {
                return false;
            }
        }

        if (IsValidPoint(request.position) &&
            range > 0.0f &&
            request.capability != Capability::Rocketbelt &&
            player.Position().Distance2D(
                ToSdkPoint(request.position)) >
                range + 35.0f) {
            return false;
        }
        return true;
    }

    bool CastAction(const ActionRequest& request) {
        SDK::AIHeroClient player = SDK::GameObjects::Player();
        if (!player.IsValid()) return false;

        SDK::AIBaseClient target;
        if (request.targetId != 0) {
            target = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(request.targetId);
            if (!target.IsValid()) return false;
        }
        if (!LiveActionValid(
                player, target, request)) {
            return false;
        }

        if (request.itemId != SDK::ItemId::Unknown) {
            if (request.itemSlot < 0 ||
                request.itemSlot > ::CoreItem::kTrinketSlot) {
                return false;
            }
            const ::CoreItem::ItemSlot liveSlot =
                ::CoreItem::ReadSlot(
                    player.Address(), request.itemSlot);
            if (!liveSlot.hasItem ||
                ::CoreItem::NormalizeItemId(liveSlot.id) !=
                    SDK::ItemIdValue(request.itemId) ||
                !SDK::Items::CanUseItem(
                    player, request.itemId)) {
                return false;
            }
            if (target.IsValid()) return SDK::Items::UseItem(player, request.itemId, target);
            if (IsValidPoint(request.position)) return SDK::Items::UseItem(player, request.itemId, ToSdkPoint(request.position));
            return SDK::Items::UseItem(player, request.itemId);
        }

        if (request.spellSlot < 0) return false;
        if (!ActivatorEngine::IsSummonerSpellSlot(
                request.spellSlot)) return false;
        const SDK::SpellSlot slot = static_cast<SDK::SpellSlot>(request.spellSlot);
        const auto spellbook = player.Spellbook();
        if (!spellbook.IsValid() || spellbook.CanUseSpell(slot) != CoreSpellBook::State_Ready) return false;
        if (target.IsValid()) return spellbook.CastSpell(slot, target);
        if (IsValidPoint(request.position)) return spellbook.CastSpell(slot, ToSdkPoint(request.position));
        return spellbook.CastSpell(slot);
    }

    void TryExecute(const ActionRequest& request) {
        if (!ConfirmationReady(request) || !InputSafe()) return;
        const float now = awareness_.Now();
        const std::uint32_t fingerprint = Fingerprint(request);
        if (fingerprint == lastAttemptFingerprint_ && now - lastAttemptAt_ < 0.35f) return;
        lastAttemptFingerprint_ = fingerprint;
        lastAttemptAt_ = now;

        const bool success = CastAction(request);
        awareness_.Log().Add(request.createdAt, CapabilityName(request.capability),
                             success ? "executed" : "cast rejected",
                             success ? "SDK cast accepted" : "target, range, or spell state rejected cast",
                             static_cast<int>(request.priority), request.confidence);
    }

    void DrawWorldLayer() const {
        DrawHeatmaps();
        DrawChampions();
        DrawWards();
        DrawObjectives();
        DrawJungle();
        DrawStructures();
        DrawThreats();
        DrawCombatState();
        DrawInsights();
    }

    void DrawMinimapLayer() const {
        awareness_.Store().ForEachChampion(
            [&](const ChampionState& state) {
                if (!state.enemy || state.local || state.dead ||
                    state.clone) {
                    return;
                }
                const Point3& point = state.visible
                    ? state.position : state.lastSeenPosition;
                if (!IsValidPoint(point)) return;
                Evidence evidence = state.health.evidence;
                if (!state.visible) {
                    evidence.provenance = Provenance::LastSeen;
                    evidence.observedAt = state.lastSeenAt;
                }
                if (!VisibilityGuard::CanExposePosition(
                        evidence, awareness_.Mode(),
                        state.visible)) {
                    return;
                }
                SDK::Vector2 minimap{};
                if (!SDK::Drawing::WorldToMinimap(
                        ToSdkPoint(point), minimap)) {
                    return;
                }
                const EvidenceVisualStyle style =
                    AwarenessPresentationPolicy::StyleFor(
                        evidence);
                SDK::Drawing::DrawCircle(
                    minimap, 6.0f, style.thickness,
                    style.color, 18);
                char label[96] = {};
                std::snprintf(
                    label, sizeof(label), "%c %s%s",
                    style.marker,
                    settings_.streamerMode
                        ? "enemy"
                        : (state.name[0] ? state.name : "enemy"),
                    state.visible ? "" : " last seen");
                SDK::Drawing::DrawText(
                    minimap.x + 8.0f, minimap.y - 7.0f,
                    style.color, label);
            });

        const auto& wards = awareness_.Store().Wards();
        for (std::size_t i = 0; i < wards.Size(); ++i) {
            const WardState& ward = wards.At(i);
            if (ward.destroyed ||
                !IsValidPoint(ward.position) ||
                !ward.evidence.IsKnown()) {
                continue;
            }
            SDK::Vector2 minimap{};
            if (!SDK::Drawing::WorldToMinimap(
                    ToSdkPoint(ward.position), minimap)) {
                continue;
            }
            const EvidenceVisualStyle style =
                AwarenessPresentationPolicy::StyleFor(
                    ward.evidence);
            SDK::Drawing::DrawCircle(
                minimap, 3.5f, style.thickness,
                style.color, 12);
        }

        const auto& objectives =
            awareness_.Store().Objectives();
        for (std::size_t i = 0;
             i < objectives.Size(); ++i) {
            const ObjectiveState& objective =
                objectives.At(i);
            if (!IsValidPoint(objective.position) ||
                !objective.evidence.IsKnown()) {
                continue;
            }
            const ExposureDecision exposure =
                VisibilityGuard::CanExpose(
                    objective.evidence, awareness_.Mode(),
                    objective.visible, true);
            if (!exposure.allowed) continue;
            SDK::Vector2 minimap{};
            if (!SDK::Drawing::WorldToMinimap(
                    ToSdkPoint(objective.position), minimap)) {
                continue;
            }
            const EvidenceVisualStyle style =
                AwarenessPresentationPolicy::StyleFor(
                    objective.evidence);
            SDK::Drawing::DrawCircle(
                minimap, 5.0f, style.thickness,
                style.color, 16);
            SDK::Drawing::DrawText(
                minimap.x + 6.0f, minimap.y - 6.0f,
                style.color, ObjectiveName(objective.kind));
        }
    }

    void DrawChampions() const {
        const auto& store = awareness_.Store();
        store.ForEachChampion([&](const ChampionState& state) {
            if (state.local || !state.enemy || state.dead) return;
            Evidence evidence = state.health.evidence;
            if (!state.visible) {
                evidence.provenance = Provenance::LastSeen;
                evidence.observedAt = state.lastSeenAt;
            }
            EvidenceVisualStyle style =
                AwarenessPresentationPolicy::StyleFor(evidence);
            if (state.clone) {
                style = { 0xFFFFAA44u, 2.0f, 'C' };
            }
            if (state.visible && IsValidPoint(state.position)) {
                SDK::Drawing::DrawCircle(
                    ToSdkPoint(state.position), 125.0f,
                    style.color, style.thickness);
                char label[96] = {};
                std::snprintf(
                    label, sizeof(label), "%c %s%s",
                    style.marker,
                    settings_.streamerMode
                        ? "enemy"
                        : (state.name[0] ? state.name : "enemy"),
                    state.clone ? " [observed clone]" : "");
                SDK::Drawing::DrawText(
                    ToSdkPoint(state.position), label,
                    style.color, true);
            } else if (settings_.drawReachableAreas &&
                       evidence.provenance ==
                           Provenance::LastSeen &&
                       IsValidPoint(state.lastSeenPosition) &&
                       VisibilityGuard::CanExposePosition(
                           evidence, awareness_.Mode(), false)) {
                SDK::Drawing::DrawCircle(
                    ToSdkPoint(state.lastSeenPosition),
                    std::clamp(
                        state.reachableRadius,
                        120.0f, 6000.0f),
                    style.color, style.thickness);
                char estimate[96] = {};
                std::snprintf(
                    estimate, sizeof(estimate),
                    "%c last seen %.0fs [%s estimate]",
                    style.marker, state.visibilityAge,
                    ConfidenceName(evidence.confidence));
                SDK::Drawing::DrawText(
                    ToSdkPoint(state.lastSeenPosition),
                    estimate, style.color, true);
            }
        });
    }
    void DrawEnemyHud() const {
        if (!settings_.drawEnemyHud) return;
        float y = settings_.enemyHudY;
        int shown = 0;
        awareness_.Store().ForEachChampion([&](const ChampionState& state) {
            if (!state.enemy || state.clone || shown >= 5) return;
            char line[384] = {};
            const ObservedSpell* slots[6] = {};
            for (std::size_t i = 0; i < state.spellCount; ++i) {
                const ObservedSpell& spell = state.spells[i];
                if (spell.slot >= 0 && spell.slot < 6) slots[spell.slot] = &spell;
            }
            char q[24] = {}, w[24] = {}, e[24] = {}, r[24] = {};
            char d[24] = {}, f[24] = {};
            FormatCooldown(slots[0], state.visible, q, sizeof(q));
            FormatCooldown(slots[1], state.visible, w, sizeof(w));
            FormatCooldown(slots[2], state.visible, e, sizeof(e));
            FormatCooldown(slots[3], state.visible, r, sizeof(r));
            FormatCooldown(slots[4], state.visible, d, sizeof(d));
            FormatCooldown(slots[5], state.visible, f, sizeof(f));

            const bool statsKnown = state.health.evidence.IsKnown() &&
                                    state.maxHealth.value > 0.0f;
            const float hp = statsKnown
                ? 100.0f * state.health.value / state.maxHealth.value : 0.0f;
            const bool manaKnown = state.mana.evidence.IsKnown() &&
                                   state.maxMana.value > 0.0f;
            const float mana = manaKnown
                ? 100.0f * state.mana.value / state.maxMana.value : 0.0f;
            const char* evidenceName = statsKnown
                ? ConfidenceName(state.health.evidence.confidence) : "Unknown";
            const char* role = state.roleQuest.evidence.IsKnown() &&
                               state.roleQuest.role[0]
                ? state.roleQuest.role : "role?";
            const char* dName = slots[4]
                ? CapabilityName(ActivatorEngine::CapabilityFromSpell(
                      *slots[4], awareness_.Registry()))
                : "?";
            const char* fName = slots[5]
                ? CapabilityName(ActivatorEngine::CapabilityFromSpell(
                      *slots[5], awareness_.Registry()))
                : "?";
            char gold[48] = "?";
            if (state.goldEstimateEvidence.IsKnown()) {
                std::snprintf(
                    gold, sizeof(gold), "%.0f-%.0f[%c]",
                    state.estimatedGoldMin, state.estimatedGoldMax,
                    ConfidenceName(
                        state.goldEstimateEvidence.confidence)[0]);
            }
            const char* displayName = settings_.streamerMode
                ? "enemy" : (state.name[0] ? state.name : "enemy");
            std::snprintf(
                line, sizeof(line),
                "%s [%s/%s%s%.0fs] L%s%d HP%s%.0f MP%s%.0f "
                "G:%s D:%s %s F:%s %s R:%s Q:%s W:%s E:%s",
                displayName, role, evidenceName,
                state.visible ? " now " : " seen ",
                state.visible ? 0.0f : state.visibilityAge,
                state.level.evidence.IsKnown() ? "" : "?",
                state.level.value,
                statsKnown ? "" : "?", hp, manaKnown ? "" : "?", mana,
                gold, dName, d, fName, f, r, q, w, e);
            SDK::Drawing::DrawText(
                settings_.enemyHudX, y,
                state.visible ? 0xFFBBDDFFu : 0xAA8899CCu, line);
            y += 16.0f;
            ++shown;
        });
    }

    void DrawWards() const {
        if (!settings_.drawWards) return;
        const auto& wards = awareness_.Store().Wards();
        for (std::size_t i = 0; i < wards.Size(); ++i) {
            const WardState& ward = wards.At(i);
            if (ward.destroyed || !IsValidPoint(ward.position)) continue;
            if (!ward.visible && ward.evidence.provenance != Provenance::ObservedEvent && ward.evidence.provenance != Provenance::LastSeen) continue;
            const std::uint32_t color = ward.enemy
                ? 0xFFFF7799u : (ward.visible ? 0xFF66FF99u : 0x9988AA99u);
            SDK::Drawing::DrawCircle(ToSdkPoint(ward.position),
                                     std::max(35.0f, ward.radius), color, 1.5f);
            char label[128] = {};
            const float remaining = ward.expiresAt > 0.0f
                ? std::max(0.0f, ward.expiresAt - awareness_.Now()) : 0.0f;
            std::snprintf(label, sizeof(label), "%s%s %.0fs [%s]",
                          ward.enemy ? "enemy " : "",
                          ward.visible ? "ward" : "ward (last seen)",
                          remaining, ConfidenceName(ward.evidence.confidence));
            SDK::Drawing::DrawText(ToSdkPoint(ward.position), label, color, true);
        }
    }

    void DrawObjectives() const {
        if (!settings_.drawObjectives) return;
        float y = 220.0f;
        const auto& objectives = awareness_.Store().Objectives();
        for (std::size_t i = 0; i < objectives.Size(); ++i) {
            const ObjectiveState& objective = objectives.At(i);
            const ExposureDecision exposure = VisibilityGuard::CanExpose(
                objective.evidence, awareness_.Mode(), false, true);
            if (!exposure.allowed) continue;

            const float now = awareness_.Now();
            char stateText[96] = {};
            if ((objective.status == ObjectiveStatus::NotSpawned ||
                 objective.status == ObjectiveStatus::SpawningSoon) &&
                objective.spawnAt > now) {
                std::snprintf(stateText, sizeof(stateText), "spawns %.0fs",
                              objective.spawnAt - now);
            } else if ((objective.status == ObjectiveStatus::Dead ||
                        objective.status == ObjectiveStatus::Respawning) &&
                       objective.respawnAt > now) {
                std::snprintf(stateText, sizeof(stateText), "respawns %.0fs",
                              objective.respawnAt - now);
            } else if (objective.status == ObjectiveStatus::AliveVisible) {
                const float healthPercent = objective.maxHealth > 0.0f
                    ? 100.0f * objective.health / objective.maxHealth : 0.0f;
                std::snprintf(stateText, sizeof(stateText), "alive %.0f%%",
                              healthPercent);
            } else if (objective.status == ObjectiveStatus::InCombatVisible) {
                std::snprintf(stateText, sizeof(stateText), "in combat");
            } else if (objective.status == ObjectiveStatus::Disabled) {
                std::snprintf(stateText, sizeof(stateText), "disabled");
            } else {
                std::snprintf(stateText, sizeof(stateText), "alive?");
            }

            char label[160] = {};
            std::snprintf(label, sizeof(label), "%s: %s [%s]",
                          ObjectiveName(objective.kind), stateText,
                          ConfidenceName(objective.evidence.confidence));
            const std::uint32_t color =
                objective.status == ObjectiveStatus::Dead ||
                objective.status == ObjectiveStatus::Respawning
                    ? 0xFF888888u : 0xFFFFCC55u;
            SDK::Drawing::DrawText(18.0f, y, color, label);
            y += 16.0f;
            if (IsValidPoint(objective.position)) {
                SDK::Drawing::DrawCircle(
                    ToSdkPoint(objective.position), 220.0f, color, 2.0f);
                SDK::Drawing::DrawText(
                    ToSdkPoint(objective.position), label, color, true);
            }
        }
    }

    void DrawJungle() const {
        if (!settings_.drawJungle) return;
        const auto& camps = awareness_.Store().Jungles();
        for (std::size_t i = 0; i < camps.Size(); ++i) {
            const JungleCampState& camp = camps.At(i);
            const ExposureDecision exposure = VisibilityGuard::CanExpose(
                camp.evidence, awareness_.Mode(), false, true);
            if (!exposure.allowed || !IsValidPoint(camp.position)) continue;

            const bool estimated =
                camp.evidence.provenance == Provenance::Estimated;
            const std::uint32_t color = camp.visible
                ? 0xFF55DD88u
                : (estimated ? 0xCCFFBB55u : 0xAA88AA88u);
            char stateText[96] = {};
            if (camp.alive) {
                std::snprintf(stateText, sizeof(stateText), "alive");
            } else if (camp.respawnAt > awareness_.Now()) {
                std::snprintf(
                    stateText, sizeof(stateText), "respawn %.0fs",
                    camp.respawnAt - awareness_.Now());
            } else if (estimated) {
                std::snprintf(stateText, sizeof(stateText), "possibly down");
            } else {
                std::snprintf(stateText, sizeof(stateText), "down");
            }
            char label[160] = {};
            std::snprintf(label, sizeof(label), "%s: %s [%s/%s]",
                          camp.campId[0] ? camp.campId : "jungle",
                          stateText,
                          ConfidenceName(camp.evidence.confidence),
                          ProvenanceName(camp.evidence.provenance));
            SDK::Drawing::DrawCircle(
                ToSdkPoint(camp.position), 135.0f, color, 1.5f);
            SDK::Drawing::DrawText(
                ToSdkPoint(camp.position), label, color, true);
        }
    }

    void DrawStructures() const {
        if (!settings_.drawStructures) return;
        const auto& structures = awareness_.Store().Structures();
        for (std::size_t i = 0; i < structures.Size(); ++i) {
            const StructureState& structure = structures.At(i);
            if (!structure.alive || !IsValidPoint(structure.position)) continue;
            const std::uint32_t color = structure.backdoorProtected
                ? 0xFFAA88FFu : 0xFFFF8844u;
            SDK::Drawing::DrawCircle(ToSdkPoint(structure.position),
                                     180.0f, color, 1.5f);
            char label[160] = {};
            std::snprintf(label, sizeof(label), "%s plates:%d crystal:%.0f%% true:%.0f%s",
                          StructureName(structure.kind), structure.plates,
                          structure.overgrowthCharge, structure.nextTrueDamage,
                          structure.backdoorProtected ? " [backdoor]" : "");
            SDK::Drawing::DrawText(ToSdkPoint(structure.position), label, color, true);
        }
    }

    void DrawThreats() const {
        if (!settings_.drawThreats) return;
        const auto& threats = awareness_.Store().Threats();
        for (std::size_t i = 0; i < threats.Size(); ++i) {
            const ThreatState& threat = threats.At(i);
            const ExposureDecision exposure = VisibilityGuard::CanExpose(
                threat.evidence, awareness_.Mode(), true, true);
            if (!threat.visible || !exposure.allowed) continue;
            const Point3 center =
                IsValidPoint(threat.end) ? threat.end : threat.start;
            if (!IsValidPoint(center)) continue;
            const std::uint32_t color =
                threat.control != CrowdControl::None
                    ? 0xFFFF5577u : 0xFFFFAA44u;
            switch (threat.geometry) {
            case ThreatGeometry::Line:
            case ThreatGeometry::Cone:
                if (IsValidPoint(threat.start) &&
                    IsValidPoint(threat.end)) {
                    SDK::Drawing::DrawLine(
                        ToSdkPoint(threat.start), ToSdkPoint(threat.end),
                        color,
                        std::max(1.5f, threat.radius * 0.03f));
                }
                if (threat.geometry == ThreatGeometry::Cone) {
                    SDK::Drawing::DrawCircle(
                        ToSdkPoint(center),
                        std::max(35.0f, threat.radius), color, 1.0f);
                }
                break;
            case ThreatGeometry::Circle:
            case ThreatGeometry::Ring:
            case ThreatGeometry::Point:
                SDK::Drawing::DrawCircle(
                    ToSdkPoint(center),
                    std::max(35.0f, threat.radius), color,
                    threat.geometry == ThreatGeometry::Ring ? 2.0f : 1.0f);
                break;
            }
            char label[160] = {};
            std::snprintf(
                label, sizeof(label), "%.1fs %.0f dmg %s [%s]",
                std::max(0.0f, threat.impactAt - awareness_.Now()),
                threat.damage, CrowdControlName(threat.control),
                ConfidenceName(threat.evidence.confidence));
            SDK::Drawing::DrawText(
                ToSdkPoint(center), label, color, true);
        }
    }

    void DrawCombatState() const {
        if (!settings_.drawCombatState) return;
        const ChampionState* player = nullptr;
        awareness_.Store().ForEachChampion(
            [&](const ChampionState& state) {
                if (state.local) player = &state;
            });
        if (!player || player->dead || !IsValidPoint(player->position)) return;

        const ThreatForecast forecast = CombatPredictionService::Forecast(
            *player, awareness_.Store(), awareness_.Now(),
            settings_.defensiveHorizon);
        int controlCount = 0;
        float longestControl = 0.0f;
        for (std::size_t i = 0; i < player->buffCount; ++i) {
            const ObservedBuff& buff = player->buffs[i];
            const BuffDefinition* definition =
                awareness_.Registry().ResolveBuff(
                    buff.idHash, buff.name);
            if (!definition ||
                definition->control == CrowdControl::None) {
                continue;
            }
            ++controlCount;
            longestControl = std::max(
                longestControl,
                std::max(0.0f, buff.endTime - awareness_.Now()));
        }
        char label[192] = {};
        std::snprintf(
            label, sizeof(label),
            "combat: %.0f incoming / %d threats%s | CC %d %.1fs%s",
            forecast.incomingDamage, forecast.threatCount,
            forecast.hardCc ? " hard-CC" : "",
            controlCount, longestControl,
            player->channeling ? " | channeling" : "");
        SDK::Drawing::DrawText(
            ToSdkPoint(player->position), label,
            forecast.incomingDamage >= player->health.value
                ? 0xFFFF4466u : 0xFF66DDFFu, true);

        const auto sdkPlayer = SDK::GameObjects::Player();
        if (sdkPlayer.IsValid()) {
            const float attackRange = std::max(
                0.0f, sdkPlayer.AttackRange() +
                      sdkPlayer.BoundingRadius());
            if (attackRange > 0.0f) {
                SDK::Drawing::DrawCircle(
                    ToSdkPoint(player->position), attackRange,
                    0x4455DDFFu, 1.0f);
            }
        }
        if (!player->lastDirection.IsZero()) {
            const Point3 endpoint =
                player->position + player->lastDirection * 300.0f;
            SDK::Drawing::DrawLine(
                ToSdkPoint(player->position), ToSdkPoint(endpoint),
                0x6655FFCCu, 1.5f);
        }
        const auto& structures = awareness_.Store().Structures();
        for (std::size_t i = 0; i < structures.Size(); ++i) {
            const StructureState& structure = structures.At(i);
            if (!structure.visible || !structure.alive ||
                structure.kind != StructureKind::Turret ||
                structure.team == player->team ||
                !IsValidPoint(structure.position)) {
                continue;
            }
            SDK::Drawing::DrawCircle(
                ToSdkPoint(structure.position), 775.0f,
                0x55FF5533u, 1.0f);
        }
    }

    void DrawHeatmaps() const {
        if (settings_.drawActivityHeatmap &&
            (awareness_.Mode() == RuntimeMode::Replay ||
             awareness_.Mode() == RuntimeMode::Spectator)) {
            const auto& samples = awareness_.Insights().activity;
            for (std::size_t i = 0; i < samples.Size(); ++i) {
                const ActivitySample& sample = samples.At(i);
                const float age =
                    std::max(0.0f, awareness_.Now() - sample.at);
                if (age > 600.0f || !IsValidPoint(sample.position)) continue;
                const std::uint32_t color =
                    sample.kind == ActivityKind::Death
                        ? 0x99FF3355u
                        : (sample.kind == ActivityKind::Damage
                               ? 0x66FFAA33u
                               : (sample.team == 100
                                      ? 0x3344AAFFu : 0x33FF5566u));
                const float radius =
                    sample.kind == ActivityKind::Death ? 220.0f : 110.0f;
                SDK::Drawing::DrawCircle(
                    ToSdkPoint(sample.position), radius, color, 1.0f);
            }
        }
        if (settings_.drawVisionHeatmap) {
            const auto& wards = awareness_.Store().Wards();
            for (std::size_t i = 0; i < wards.Size(); ++i) {
                const WardState& ward = wards.At(i);
                if (ward.destroyed || !ward.ally ||
                    !IsValidPoint(ward.position)) {
                    continue;
                }
                SDK::Drawing::DrawCircle(
                    ToSdkPoint(ward.position),
                    std::max(100.0f, ward.radius),
                    ward.faelight ? 0x4455FFAAu : 0x3344CC88u,
                    ward.faelight ? 2.0f : 1.0f);
            }
        }
    }

    void DrawInsights() const {
        if (!settings_.drawInsights) return;
        const AwarenessLocale locale = settings_.vietnamese
            ? AwarenessLocale::Vietnamese
            : AwarenessLocale::English;
        float y = settings_.alertPanelY + 150.0f;
        const float x = settings_.alertPanelX;
        char line[320] = {};
        const WaveState& wave = awareness_.Store().Wave();
        if (settings_.drawWave && wave.evidence.IsKnown()) {
            std::snprintf(
                line, sizeof(line), "%s: %s %d-%d [%.0f/%s]",
                AwarenessLocalization::Text("wave", locale),
                wave.classification, wave.allyMinions,
                wave.enemyMinions, wave.laneBias,
                ConfidenceName(wave.evidence.confidence));
            SDK::Drawing::DrawText(x, y, 0xFF99DDAAu, line);
            y += 17.0f;
            if (IsValidPoint(wave.center)) {
                SDK::Drawing::DrawText(
                    ToSdkPoint(wave.center), line,
                    0xCC99DDAAu, true);
            }
        }

        const RecallAdvice& recall = awareness_.Insights().recall;
        if (recall.evidence.IsKnown()) {
            std::snprintf(
                line, sizeof(line), "%s: %d/100 %s - %s",
                AwarenessLocalization::Text("recall", locale),
                recall.score,
                recall.recommended ? "recommended" : "hold",
                recall.reason);
            SDK::Drawing::DrawText(
                x, y,
                recall.recommended ? 0xFF77EEAAu : 0xFFBBBB88u,
                line);
            y += 17.0f;
        }

        const WardEfficiencySummary& wards =
            awareness_.Insights().wardEfficiency;
        if (wards.evidence.IsKnown()) {
            std::snprintf(
                line, sizeof(line), "%s: %d/100 active %d objective %d",
                AwarenessLocalization::Text("wards", locale),
                wards.score, wards.active, wards.objectiveCoverage);
            SDK::Drawing::DrawText(x, y, 0xFF77CCAAu, line);
            y += 17.0f;
        }

        const auto& setups = awareness_.Insights().objectiveSetups;
        for (std::size_t i = 0; i < setups.Size() && i < 3; ++i) {
            const ObjectiveSetupAssessment& setup = setups.At(i);
            std::snprintf(
                line, sizeof(line), "%s %s: %d/100 %s A%d E%d V%d [%s]",
                AwarenessLocalization::Text("objective", locale),
                ObjectiveName(setup.kind), setup.score, setup.rating,
                setup.nearbyAllies, setup.nearbyEnemies,
                setup.alliedVision,
                ConfidenceName(setup.evidence.confidence));
            SDK::Drawing::DrawText(x, y, 0xFFDDCC77u, line);
            y += 17.0f;
        }

        awareness_.Store().ForEachChampion(
            [&](const ChampionState& ally) {
                if ((!ally.ally && !ally.local) || ally.dead) return;
                const ObservedSpell* ultimate = nullptr;
                for (std::size_t i = 0; i < ally.spellCount; ++i) {
                    if (ally.spells[i].slot == 3) {
                        ultimate = &ally.spells[i];
                        break;
                    }
                }
                if (!ultimate || !ultimate->evidence.IsKnown()) return;
                char cooldown[32] = {};
                FormatCooldown(
                    ultimate, ally.visible || ally.ally,
                    cooldown, sizeof(cooldown));
                const char* name = settings_.streamerMode
                    ? "ally" : (ally.name[0] ? ally.name : "ally");
                std::snprintf(
                    line, sizeof(line), "%s %s: R %s [%s]",
                    AwarenessLocalization::Text("ultimate", locale),
                    name, cooldown,
                    ConfidenceName(ultimate->evidence.confidence));
                SDK::Drawing::DrawText(x, y, 0xFF88BBFFu, line);
                y += 17.0f;
            });

        const DeathRecapSummary& recap =
            awareness_.Insights().deathRecap;
        if (recap.evidence.IsKnown()) {
            std::snprintf(
                line, sizeof(line), "%s: %.0f damage / %d sources / %.1fs",
                AwarenessLocalization::Text("death", locale),
                recap.totalDamage, recap.sourceCount,
                std::max(0.0f, recap.deathAt - recap.firstDamageAt));
            SDK::Drawing::DrawText(x, y, 0xFFFF8899u, line);
            y += 17.0f;
        }
        std::snprintf(
            line, sizeof(line), "timeline: %zu events | activity: %zu samples",
            awareness_.Store().TeamfightTimeline().Size(),
            awareness_.Insights().activity.Size());
        SDK::Drawing::DrawText(x, y, 0xFFAAAAAAu, line);
    }

    void DrawHudEditor() {
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings;
        ImGui::SetNextWindowPos(
            ImVec2(settings_.alertPanelX, settings_.alertPanelY),
            ImGuiCond_Appearing);
        if (ImGui::Begin("Awareness alert anchor", nullptr, flags)) {
            ImGui::TextUnformatted("Drag: alerts and insights");
            const ImVec2 position = ImGui::GetWindowPos();
            settings_.alertPanelX = std::max(0.0f, position.x);
            settings_.alertPanelY = std::max(0.0f, position.y);
        }
        ImGui::End();

        ImGui::SetNextWindowPos(
            ImVec2(settings_.enemyHudX, settings_.enemyHudY),
            ImGuiCond_Appearing);
        if (ImGui::Begin("Awareness enemy HUD anchor", nullptr, flags)) {
            ImGui::TextUnformatted("Drag: enemy side HUD");
            const ImVec2 position = ImGui::GetWindowPos();
            settings_.enemyHudX = std::max(0.0f, position.x);
            settings_.enemyHudY = std::max(0.0f, position.y);
        }
        ImGui::End();
        menu_.SyncMenuFromSettings();
    }

    void HandleAudioAlerts() {
        if (!settings_.audioOnly) return;
        const auto& alerts = awareness_.Store().Alerts();
        const AlertState* newest = nullptr;
        for (std::size_t i = 0; i < alerts.Size(); ++i) {
            const AlertState& alert = alerts.At(i);
            if (alert.priority < 45 || alert.expiresAt <= awareness_.Now()) {
                continue;
            }
            if (!newest || alert.at > newest->at) newest = &alert;
        }
        if (!newest ||
            (newest->at < lastAudioAlertAt_) ||
            (newest->at == lastAudioAlertAt_ &&
             newest->id == lastAudioAlertId_)) {
            return;
        }
        MessageBeep(newest->priority >= 70
                        ? MB_ICONHAND : MB_ICONEXCLAMATION);
        lastAudioAlertAt_ = newest->at;
        lastAudioAlertId_ = newest->id;
    }

    void DrawPanel() const {
        float y = settings_.alertPanelY;
        char line[256] = {};
        std::snprintf(line, sizeof(line), "Awareness | %s | t=%.1f", RuntimeModeName(awareness_.Mode()), awareness_.Now());
        SDK::Drawing::DrawText(
            settings_.alertPanelX, y, 0xFFFFFFFFu, line);
        y += 18.0f;
        if (hasCandidate_) {
            std::snprintf(line, sizeof(line), "candidate: %s [%s / %s] %.0f%% - %s",
                          CapabilityName(candidate_.capability), ActionModeName(candidate_.mode),
                          ConfidenceName(candidate_.confidence), static_cast<float>(static_cast<int>(candidate_.confidence)), candidate_.reason);
            SDK::Drawing::DrawText(
                settings_.alertPanelX, y, 0xFF66EEFFu, line);
            y += 18.0f;
        }
        const auto& alerts = awareness_.Store().Alerts();
        std::array<const AlertState*, 64> prioritized{};
        const std::size_t alertCount =
            AwarenessPresentationPolicy::PrioritizeAlerts(
                alerts, awareness_.Now(), prioritized);
        const std::size_t shown =
            std::min<std::size_t>(5, alertCount);
        for (std::size_t i = 0; i < shown; ++i) {
            const AlertState& alert = *prioritized[i];
            if (settings_.streamerMode) {
                std::snprintf(
                    line, sizeof(line), "[%s] %s",
                    ConfidenceName(alert.confidence),
                    alert.title);
            } else {
                std::snprintf(
                    line, sizeof(line), "[%s] %s: %s",
                    ConfidenceName(alert.confidence),
                    alert.title, alert.detail);
            }
            const Evidence alertEvidence{
                Provenance::ObservedEvent,
                alert.confidence, alert.at,
                alert.expiresAt, alert.id
            };
            const EvidenceVisualStyle style =
                AwarenessPresentationPolicy::StyleFor(
                    alertEvidence);
            SDK::Drawing::DrawText(
                settings_.alertPanelX, y,
                alert.priority >= 70
                    ? 0xFFFF4455u : style.color,
                line);
            y += 17.0f;
        }
    }

    static void FormatCooldown(const ObservedSpell* spell, bool visible,
                               char* out, std::size_t outSize) noexcept {
        if (!out || outSize == 0) return;
        if (!spell || !spell->evidence.IsKnown()) {
            std::snprintf(out, outSize, "?");
            return;
        }
        if (!visible && spell->cooldownKind == CooldownKind::Unknown) {
            std::snprintf(out, outSize, "?[%c]",
                          ConfidenceName(spell->evidence.confidence)[0]);
            return;
        }
        if (spell->ready && visible) {
            std::snprintf(out, outSize, "up");
            return;
        }
        if (spell->cooldownKind == CooldownKind::RangeEstimated &&
            spell->cooldownMax - spell->cooldownMin > 0.5f) {
            std::snprintf(out, outSize, "~%.0f-%.0f[%c]",
                          spell->cooldownMin, spell->cooldownMax,
                          ConfidenceName(spell->evidence.confidence)[0]);
            return;
        }
        if (spell->cooldownRemaining > 0.01f) {
            std::snprintf(out, outSize, "%s%.0f[%c]",
                          visible ? "" : "~", spell->cooldownRemaining,
                          ConfidenceName(spell->evidence.confidence)[0]);
            return;
        }
        std::snprintf(out, outSize, visible ? "up" : "?[%c]",
                      ConfidenceName(spell->evidence.confidence)[0]);
    }

    static const char* CrowdControlName(CrowdControl control) noexcept {
        switch (control) {
        case CrowdControl::Stun: return "stun";
        case CrowdControl::Root: return "root";
        case CrowdControl::Charm: return "charm";
        case CrowdControl::Fear: return "fear";
        case CrowdControl::Taunt: return "taunt";
        case CrowdControl::Blind: return "blind";
        case CrowdControl::Silence: return "silence";
        case CrowdControl::Polymorph: return "polymorph";
        case CrowdControl::Slow: return "slow";
        case CrowdControl::Sleep: return "sleep";
        case CrowdControl::Suppression: return "suppression";
        case CrowdControl::Airborne: return "airborne";
        case CrowdControl::Stasis: return "stasis";
        case CrowdControl::Grounded: return "grounded";
        default: return "no-CC";
        }
    }

    static const char* ObjectiveName(ObjectiveKind kind) noexcept {
        switch (kind) {
        case ObjectiveKind::ElementalDragon: return "dragon";
        case ObjectiveKind::DragonSoul: return "dragon soul";
        case ObjectiveKind::ElderDragon: return "elder";
        case ObjectiveKind::RiftHerald: return "herald";
        case ObjectiveKind::VoidGrubs: return "grubs";
        case ObjectiveKind::Baron: return "baron";
        case ObjectiveKind::Scuttle: return "scuttle";
        default: return "objective";
        }
    }

    static const char* StructureName(StructureKind kind) noexcept {
        switch (kind) {
        case StructureKind::Turret: return "turret";
        case StructureKind::Inhibitor: return "inhibitor";
        case StructureKind::Nexus: return "nexus";
        default: return "structure";
        }
    }

    static void ExportDecisionLogCallback(void* context) {
        auto* plugin = static_cast<AwarenessActivatorPlugin*>(context);
        if (plugin) plugin->ExportDecisionLog();
    }

    static void ApplyChampionPresetCallback(void* context) {
        auto* plugin = static_cast<AwarenessActivatorPlugin*>(context);
        if (plugin) plugin->ApplyChampionPreset();
    }

    void ApplyChampionPreset() {
        const ChampionState* player = nullptr;
        awareness_.Store().ForEachChampion(
            [&](const ChampionState& champion) {
                if (champion.local) player = &champion;
            });
        if (!player) return;
        const AwarenessPresetProfile profile =
            AwarenessPresetService::ForChampion(player->championId);
        settings_.drawEnemyHud = profile.enemyHud;
        settings_.drawCombatState = profile.combat;
        settings_.drawWards = profile.wards;
        settings_.drawJungle = profile.jungle;
        settings_.drawObjectives = profile.objectives;
        settings_.drawActivityHeatmap = profile.heatmaps;
        settings_.defensiveHorizon = profile.defensiveHorizon;
        menu_.SyncMenuFromSettings();
    }

    void LoadPatchOverrides() {
        char path[MAX_PATH] = {};
        const HMODULE module = SDK::Variables::Detail::CurrentModule();
        const DWORD length = module
            ? GetModuleFileNameA(module, path, static_cast<DWORD>(sizeof(path)))
            : 0;
        if (length == 0 || length >= sizeof(path)) return;
        char* slash = std::max(strrchr(path, '\\'), strrchr(path, '/'));
        if (!slash) return;
        strcpy_s(slash + 1,
                 sizeof(path) - static_cast<std::size_t>(slash + 1 - path),
                 "NightSharp.AwarenessActivator.csv");
        const std::size_t applied =
            PatchOverrideLoader::LoadCsv(awareness_.Registry(), path);
        if (applied > 0) {
            NightSharpDebug::Logf(
                "[AwarenessActivator] loaded %zu patch override rows from %s",
                applied, path);
        }
    }

    void ExportDecisionLog() const {
        char directory[MAX_PATH] = {};
        if (!GetTempPathA(
                static_cast<DWORD>(sizeof(directory)), directory)) {
            return;
        }
        char decisions[MAX_PATH] = {};
        char telemetry[MAX_PATH] = {};
        strcpy_s(decisions, directory);
        strcpy_s(telemetry, directory);
        strcat_s(
            decisions, sizeof(decisions),
            "NightSharp_awareness_decisions.jsonl");
        strcat_s(
            telemetry, sizeof(telemetry),
            "NightSharp_awareness_telemetry.json");
        if (awareness_.Log().ExportJsonl(decisions)) {
            NightSharpDebug::Logf(
                "[AwarenessActivator] decision log exported: %s",
                decisions);
        }
        if (LocalTelemetryExporter::Export(
                telemetry, awareness_.Store(), awareness_.Insights(),
                awareness_.Log(), settings_.streamerMode)) {
            NightSharpDebug::Logf(
                "[AwarenessActivator] local telemetry exported: %s",
                telemetry);
        }
    }

    AwarenessEngine awareness_{};
    SdkObservationBridge bridge_{};
    ActivatorEngine activator_{};
    ActivatorSettings settings_{};
    AwarenessActivatorMenu menu_{};
    PresentationLayerGuard layerGuard_{};
    ActionRequest candidate_{};
    bool loaded_ = false;
    bool hasCandidate_ = false;
    std::uint32_t lastAttemptFingerprint_ = 0;
    float lastAttemptAt_ = -100.0f;
    std::uint32_t lastAudioAlertId_ = 0;
    float lastAudioAlertAt_ = -100.0f;
};

} // namespace NightSharp::Companion
