#pragma once

#include "../ITargetSelectorMode.h"
#include "WeightItemWrapper.h"
#include "Weights/AbilityPower.h"
#include "Weights/Aggro.h"
#include "Weights/AttackDamage.h"
#include "Weights/CrowdControl.h"
#include "Weights/Distance.h"
#include "Weights/Health.h"
#include "Weights/Killable.h"
#include "Weights/LessAttack.h"
#include "Weights/LessCast.h"
#include "Weights/MaxHealth.h"
#include "Weights/NearMouse.h"
#include "Weights/PriorityWeight.h"

#include <algorithm>
#include <cfloat>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SDK::Modes {

class Weight : public ITargetSelectorMode {
public:
    static Weight*& Instance() {
        static Weight* inst = nullptr;
        return inst;
    }

    static float GetLastScore(uint32_t netId) {
        auto* inst = Instance();
        if (inst) {
            auto it = inst->lastScores_.find(netId);
            if (it != inst->lastScores_.end()) return it->second;
        }
        return 0.0f;
    }

    Weight() {
        Instance() = this;
        items_.push_back(std::make_unique<Weights::AbilityPower>());
        items_.push_back(std::make_unique<Weights::Aggro>());
        items_.push_back(std::make_unique<Weights::AttackDamage>());
        items_.push_back(std::make_unique<Weights::CrowdControl>());
        items_.push_back(std::make_unique<Weights::Distance>());
        items_.push_back(std::make_unique<Weights::Health>());
        items_.push_back(std::make_unique<Weights::Killable>());
        items_.push_back(std::make_unique<Weights::LessAttack>());
        items_.push_back(std::make_unique<Weights::LessCast>());
        items_.push_back(std::make_unique<Weights::MaxHealth>());
        items_.push_back(std::make_unique<Weights::NearMouse>());
        items_.push_back(std::make_unique<Weights::PriorityWeight>());

        for (auto& item : items_) {
            wrappers_.emplace_back(item.get());
        }
    }

    const char* DisplayName() const override { return "Weight"; }
    const char* Name() const override { return "weight"; }

    void AddToMenu(Menu* menu) override {
        sub_ = new Menu("weight_sub", "Weight Settings");
        for (auto& w : wrappers_) {
            sub_->Add(new MenuSlider(("w_" + std::string(w.Item->Name())).c_str(), w.Item->DisplayName(), w.Item->DefaultWeight(), 0, 100));
        }
        menu->Add(sub_);
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) override {
        if (heroes.empty()) return heroes;

        // Reset ranges
        for (auto& w : wrappers_) {
            w.MinValue = FLT_MAX;
            w.MaxValue = -FLT_MAX;
            if (sub_) {
                auto* s = sub_->Get<MenuSlider>(("w_" + std::string(w.Item->Name())).c_str());
                if (s) w.Weight = static_cast<float>(s->Value);
            } else {
                w.Weight = static_cast<float>(w.Item->DefaultWeight());
            }
        }

        // TEMP PROBE: per-item GetValue timing (remove after profiling).
        LARGE_INTEGER _ocStart{}, _ocFreq{};
        QueryPerformanceCounter(&_ocStart);
        QueryPerformanceFrequency(&_ocFreq);
        double _itemMs[32] = {};

        // Pass 1: find min/max values
        std::map<uint32_t, std::vector<float>> heroRawValues;
        for (const auto& hero : heroes) {
            uint32_t nid = hero.NetworkId();
            for (size_t i = 0; i < wrappers_.size(); ++i) {
                LARGE_INTEGER _a{}, _b{};
                QueryPerformanceCounter(&_a);
                float val = wrappers_[i].Item->GetValue(hero);
                QueryPerformanceCounter(&_b);
                if (i < 32) _itemMs[i] += static_cast<double>(_b.QuadPart - _a.QuadPart) * 1000.0 / static_cast<double>(_ocFreq.QuadPart);
                heroRawValues[nid].push_back(val);
                if (val < wrappers_[i].MinValue) wrappers_[i].MinValue = val;
                if (val > wrappers_[i].MaxValue) wrappers_[i].MaxValue = val;
            }
        }

        // Pass 2: compute score
        std::map<uint32_t, float> currentScores;
        for (const auto& hero : heroes) {
            uint32_t nid = hero.NetworkId();
            float totalScore = 0.0f;
            const auto& vals = heroRawValues[nid];
            for (size_t i = 0; i < wrappers_.size(); ++i) {
                if (wrappers_[i].Weight <= 0.0f) continue;
                float minV = wrappers_[i].MinValue;
                float maxV = wrappers_[i].MaxValue;
                float norm = 0.0f;
                if (maxV > minV) {
                    norm = (vals[i] - minV) / (maxV - minV);
                } else {
                    norm = 1.0f;
                }
                if (wrappers_[i].Item->Inverted()) {
                    norm = 1.0f - norm;
                }
                totalScore += norm * wrappers_[i].Weight;
            }
            currentScores[nid] = totalScore;
            lastScores_[nid] = totalScore;
        }

        auto result = heroes;
        std::sort(result.begin(), result.end(), [&currentScores](const AIHeroClient& a, const AIHeroClient& b) {
            return currentScores[a.NetworkId()] > currentScores[b.NetworkId()];
        });

        // TEMP PROBE: if this OrderChampions pass was slow, log per-item ms + hero
        // count so we can see which weight item dominates. Throttled to 500ms.
        {
            LARGE_INTEGER _ocEnd{};
            QueryPerformanceCounter(&_ocEnd);
            const double _totalMs = static_cast<double>(_ocEnd.QuadPart - _ocStart.QuadPart) * 1000.0 / static_cast<double>(_ocFreq.QuadPart);
            static DWORD _lastLog = 0;
            const DWORD _now = GetTickCount();
            if (_totalMs > 1.0 && _now - _lastLog >= 500) {
                _lastLog = _now;
                char b[512];
                int n = std::snprintf(b, sizeof(b),
                    "[TSWeight] total=%.2fms heroes=%zu items[AP,Aggro,AD,CC,Dist,HP,Kill,LessAtk,LessCast,MaxHP,NearMs,Prio]=",
                    _totalMs, heroes.size());
                for (size_t i = 0; i < wrappers_.size() && i < 12 && n < (int)sizeof(b) - 16; ++i) {
                    n += std::snprintf(b + n, sizeof(b) - n, "%.2f ", _itemMs[i]);
                }
                n += std::snprintf(b + n, sizeof(b) - n, "\r\n");
                HANDLE h = CreateFileA("C:\\Users\\Public\\nightsharp_fps_drop_debug.txt",
                                       FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (h != INVALID_HANDLE_VALUE) { DWORD w = 0; WriteFile(h, b, (DWORD)n, &w, nullptr); CloseHandle(h); }
            }
        }
        return result;
    }

private:
    Menu* sub_ = nullptr;
    std::vector<std::unique_ptr<IWeightItem>> items_;
    std::vector<WeightItemWrapper> wrappers_;
    std::map<uint32_t, float> lastScores_;
};

} // namespace SDK::Modes
