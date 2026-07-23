#pragma once

// ============================================================================
// KuroAIO — champion dispatcher for Kuro-owned ports.
// Mirrors the small 7UPAIO loader pattern, but keeps non-7UP champions separate.
// ============================================================================

#include "../../IPlugin.h"
#include "../../../SDK/SDK.h"
#include "../../../SDK/Data/EmbeddedAssets.h"

#include <climits>
#include <string>

#include "Champion/Katarina.h"
#include "Champion/Kindred.h"
#include "Champion/Lucian.h"
#include "Champion/Samira.h"
#include "Champion/Senna.h"
#include "Champion/Syndra.h"
#include "Champion/TwistedFate.h"
#include "Champion/Viktor.h"
#include "Champion/Yasuo/Yasuo.h"
#include "Champion/Fiora/Fiora.h"
#include "Champion/Jinx.h"
#include "Champion/Rengar.h"
#include "AI/AIChampionCatalog.h"

namespace Plugins::KuroAIO::ChampionMenuTheme {

struct Palette {
    ImU32 From;
    ImU32 To;
    float Speed;
};

inline Palette GetPalette(const char* championName) {
    if (_stricmp(championName, "Rengar") == 0) {
        return { IM_COL32(217, 4, 41, 255), IM_COL32(255, 107, 0, 255), 1.10f };
    }
    if (_stricmp(championName, "Jinx") == 0) {
        return { IM_COL32(255, 42, 112, 255), IM_COL32(0, 229, 255, 255), 1.10f };
    }
    if (_stricmp(championName, "Katarina") == 0) {
        return { IM_COL32(255, 59, 92, 255), IM_COL32(157, 78, 221, 255), 1.05f };
    }
    if (_stricmp(championName, "Kindred") == 0) {
        return { IM_COL32(245, 241, 232, 255), IM_COL32(103, 199, 235, 255), 0.82f };
    }
    if (_stricmp(championName, "Lucian") == 0) {
        return { IM_COL32(116, 232, 255, 255), IM_COL32(244, 208, 111, 255), 0.95f };
    }
    if (_stricmp(championName, "Samira") == 0) {
        return { IM_COL32(255, 180, 94, 255), IM_COL32(214, 40, 57, 255), 1.12f };
    }
    if (_stricmp(championName, "Senna") == 0) {
        return { IM_COL32(87, 242, 192, 255), IM_COL32(138, 92, 255, 255), 0.90f };
    }
    if (_stricmp(championName, "Syndra") == 0) {
        return { IM_COL32(212, 124, 255, 255), IM_COL32(108, 77, 255, 255), 1.00f };
    }
    if (_stricmp(championName, "TwistedFate") == 0) {
        return { IM_COL32(255, 209, 102, 255), IM_COL32(239, 71, 111, 255), 0.88f };
    }
    if (_stricmp(championName, "Viktor") == 0) {
        return { IM_COL32(82, 217, 255, 255), IM_COL32(255, 138, 61, 255), 1.04f };
    }
    if (_stricmp(championName, "Yasuo") == 0) {
        return { IM_COL32(142, 235, 255, 255), IM_COL32(82, 113, 255, 255), 0.92f };
    }
    if (_stricmp(championName, "Fiora") == 0) {
        return { IM_COL32(113, 196, 255, 255), IM_COL32(229, 107, 178, 255), 0.98f };
    }
    return { IM_COL32(255, 170, 64, 255), IM_COL32(156, 64, 255, 255), 1.0f };
}

inline SDK::UI::Menu* GetRoot(const char* championName) {
    if (!championName || !championName[0]) {
        return nullptr;
    }
    if (_stricmp(championName, "Rengar") == 0) {
        return ::Plugins::KuroAIO::Rengar::MenuRoot;
    }
    if (_stricmp(championName, "Jinx") == 0) {
        return ::Plugins::KuroAIO::Jinx::MenuRoot;
    }
    if (_stricmp(championName, "Katarina") == 0) {
        return ::Plugins::KuroAIO::Katarina::MenuRoot;
    }
    if (_stricmp(championName, "Kindred") == 0) {
        return ::Plugins::KuroAIO::Kindred::MenuRoot;
    }
    if (_stricmp(championName, "Lucian") == 0) {
        return ::Plugins::KuroAIO::Lucian::MenuRoot;
    }
    if (_stricmp(championName, "Samira") == 0) {
        return ::Plugins::KuroAIO::Samira::MenuRoot;
    }
    if (_stricmp(championName, "Senna") == 0) {
        return ::Plugins::KuroAIO::Senna::MenuRoot;
    }
    if (_stricmp(championName, "Syndra") == 0) {
        return ::Plugins::KuroAIO::Syndra::MenuRoot;
    }
    if (_stricmp(championName, "TwistedFate") == 0) {
        return ::Plugins::KuroAIO::TwistedFate::MenuRoot;
    }
    if (_stricmp(championName, "Viktor") == 0) {
        return ::Plugins::KuroAIO::Viktor::MenuRoot;
    }
    if (_stricmp(championName, "Yasuo") == 0) {
        return ::Plugins::KuroAIO::Yasuo::MenuRoot;
    }
    if (_stricmp(championName, "Fiora") == 0) {
        return ::Plugins::KuroAIO::Fiora::MenuRoot;
    }
    if (KuroAIO::AI::Catalog::Supports(championName)) {
        return ::Plugins::KuroAIO::AI::Engine::MenuRoot;
    }

    return nullptr;
}

inline void ApplyGradient(SDK::UI::Menu* menu, const Palette& palette) {
    if (!menu) {
        return;
    }

    menu->SetAnimatedGradientText(palette.From, palette.To, palette.Speed);
    for (int i = 0; i < menu->Components.size(); ++i) {
        SDK::UI::AMenuComponent* component = menu->Components[i];
        if (component && component->IsMenu()) {
            ApplyGradient(static_cast<SDK::UI::Menu*>(component), palette);
        }
    }
}

inline ImTextureID LoadChampionIcon(const char* championName) {
    ImTextureID placeholder = SDK::UI::Icons::GetPlaceholder();
    ImTextureID icon = SDK::UI::Icons::GetChampionSquare(championName);
    if (icon && icon != placeholder) {
        return icon;
    }

    std::size_t assetCount = 0;
    const auto* assets = SDK::Data::EmbeddedAssets::ImageAssets(assetCount);
    constexpr char championDirectory[] = "Images\\Champions\\";
    const std::size_t championDirectoryLength = sizeof(championDirectory) - 1;

    for (std::size_t i = 0; assets && i < assetCount; ++i) {
        const auto& asset = assets[i];
        if (!asset.Key || !asset.RelativePath || !asset.Bytes || asset.Size == 0 ||
            asset.Size > static_cast<std::size_t>(INT_MAX) ||
            _stricmp(asset.Key, championName) != 0 ||
            _strnicmp(asset.RelativePath, championDirectory, championDirectoryLength) != 0) {
            continue;
        }

        if (SDK::UI::Icons::LoadIconFromBytes(
                asset.Key,
                asset.Bytes,
                static_cast<int>(asset.Size))) {
            icon = SDK::UI::Icons::GetChampionSquare(championName);
            return icon != placeholder ? icon : nullptr;
        }
        break;
    }

    return nullptr;
}

inline bool Apply(const char* championName) {
    SDK::UI::Menu* root = GetRoot(championName);
    if (!root) {
        return false;
    }

    ApplyGradient(root, GetPalette(championName));

    ImTextureID icon = LoadChampionIcon(championName);
    if (!icon) {
        root->ClearLogo();
        return false;
    }

    root->SetLogo(icon, 24.0f, 24.0f);
    return true;
}

} // namespace Plugins::KuroAIO::ChampionMenuTheme

namespace Plugins {

class KuroAIOPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "KuroAIO"; }
    const char* GetInternalId() const override {
        const std::string champ = CurrentChampionName();
        if (_stricmp(champ.c_str(), "Rengar") == 0) return "champion.kuroaio.rengar";
        if (_stricmp(champ.c_str(), "Jinx") == 0) return "champion.kuroaio.jinx";
        if (_stricmp(champ.c_str(), "Katarina") == 0) return "champion.kuroaio.katarina";
        if (_stricmp(champ.c_str(), "Kindred") == 0) return "champion.kuroaio.kindred";
        if (_stricmp(champ.c_str(), "Lucian") == 0) return "champion.kuroaio.lucian";
        if (_stricmp(champ.c_str(), "Samira") == 0) return "champion.kuroaio.samira";
        if (_stricmp(champ.c_str(), "Senna") == 0) return "champion.kuroaio.senna";
        if (_stricmp(champ.c_str(), "Syndra") == 0) return "champion.kuroaio.syndra";
        if (_stricmp(champ.c_str(), "Yasuo") == 0) return "champion.kuroaio.yasuo";
        if (_stricmp(champ.c_str(), "Fiora") == 0) return "champion.kuroaio.fiora";
        if (_stricmp(champ.c_str(), "TwistedFate") == 0) return "champion.kuroaio.twistedfate";
        if (_stricmp(champ.c_str(), "Viktor") == 0) return "champion.kuroaio.viktor";

        if (KuroAIO::AI::Catalog::Supports(champ.c_str())) {
            static std::string cachedId;
            cachedId = "champion.kuroaio." + champ;
            std::transform(cachedId.begin(), cachedId.end(), cachedId.begin(),
                [](unsigned char c){ return std::tolower(c); });
            return cachedId.c_str();
        }

        return "champion.kuroaio";
    }
    const char* GetAuthor() const override { return "Kuro"; }
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    const char* GetChampionName() const override { return CurrentSupportedChampionName(); }
    bool AutoLoadByDefault() const override { return true; }
    bool CanLoad() const override { return IsCurrentChampionSupported(); }

    void OnLoad() override {
        const std::string champ = CurrentChampionName();
        if (!IsSupportedChampionName(champ.c_str())) {
            return;
        }

        if (_stricmp(champ.c_str(), "Rengar") == 0) {
            KuroAIO::Rengar::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Jinx") == 0) {
            KuroAIO::Jinx::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Katarina") == 0) {
            KuroAIO::Katarina::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Kindred") == 0) {
            KuroAIO::Kindred::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Lucian") == 0) {
            KuroAIO::Lucian::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Samira") == 0) {
            KuroAIO::Samira::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Senna") == 0) {
            KuroAIO::Senna::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Syndra") == 0) {
            KuroAIO::Syndra::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Yasuo") == 0) {
            KuroAIO::Yasuo::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Fiora") == 0) {
            KuroAIO::Fiora::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "TwistedFate") == 0) {
            KuroAIO::TwistedFate::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Viktor") == 0) {
            KuroAIO::Viktor::OnGameLoad();
        } else if (KuroAIO::AI::Catalog::Supports(champ.c_str())) {
            KuroAIO::AI::Catalog::Load(champ.c_str());
        }

        m_menuThemeApplied = KuroAIO::ChampionMenuTheme::Apply(champ.c_str());
        m_nextMenuThemeRetry = ::GetTickCount() + 1000;
    }

    void OnUpdate() override {
        if (m_menuThemeApplied) {
            return;
        }

        const DWORD now = ::GetTickCount();
        if (m_nextMenuThemeRetry != 0 &&
            static_cast<LONG>(now - m_nextMenuThemeRetry) < 0) {
            return;
        }

        const std::string champ = CurrentChampionName();
        if (IsSupportedChampionName(champ.c_str())) {
            m_menuThemeApplied = KuroAIO::ChampionMenuTheme::Apply(champ.c_str());
        }
        m_nextMenuThemeRetry = now + 1000;
    }

    void OnUnload() override {
        const std::string champ = CurrentChampionName();
        if (KuroAIO::AI::Catalog::Supports(champ.c_str())) {
            KuroAIO::AI::Catalog::Unload();
        } else {
            KuroAIO::Rengar::OnUnload();
            KuroAIO::Jinx::OnUnload();
            KuroAIO::Katarina::OnUnload();
            KuroAIO::Kindred::OnUnload();
            KuroAIO::Lucian::OnUnload();
            KuroAIO::Samira::OnUnload();
            KuroAIO::Senna::OnUnload();
            KuroAIO::Syndra::OnUnload();
            KuroAIO::Yasuo::OnUnload();
            KuroAIO::Fiora::OnUnload();
            KuroAIO::TwistedFate::OnUnload();
            KuroAIO::Viktor::OnUnload();
        }

        m_menuThemeApplied = false;
        m_nextMenuThemeRetry = 0;
    }

private:
    bool m_menuThemeApplied = false;
    DWORD m_nextMenuThemeRetry = 0;

    static bool IsSupportedChampionName(const char* championName) {
        if (!championName || !championName[0]) return false;
        if (_stricmp(championName, "Rengar") == 0 ||
            _stricmp(championName, "Jinx") == 0 ||
            _stricmp(championName, "Katarina") == 0 ||
            _stricmp(championName, "Kindred") == 0 ||
            _stricmp(championName, "Lucian") == 0 ||
            _stricmp(championName, "Samira") == 0 ||
            _stricmp(championName, "Senna") == 0 ||
            _stricmp(championName, "Syndra") == 0 ||
            _stricmp(championName, "Yasuo") == 0 ||
            _stricmp(championName, "Fiora") == 0 ||
            _stricmp(championName, "TwistedFate") == 0 ||
            _stricmp(championName, "Viktor") == 0) {
            return true;
        }
        return KuroAIO::AI::Catalog::Supports(championName);
    }

    static std::string CurrentChampionName() {
        const std::string& cached = SDK::GameObject::GetCachedChampionName();
        if (!cached.empty()) {
            return cached;
        }

        const auto player = GameObjects::Player();
        return player.IsValid() ? player.CharacterName() : std::string();
    }

    static bool IsCurrentChampionSupported() {
        const std::string championName = CurrentChampionName();
        return IsSupportedChampionName(championName.c_str());
    }

    static const char* CurrentSupportedChampionName() {
        const std::string championName = CurrentChampionName();
        if (_stricmp(championName.c_str(), "Rengar") == 0) return "Rengar";
        if (_stricmp(championName.c_str(), "Jinx") == 0) return "Jinx";
        if (_stricmp(championName.c_str(), "Katarina") == 0) return "Katarina";
        if (_stricmp(championName.c_str(), "Kindred") == 0) return "Kindred";
        if (_stricmp(championName.c_str(), "Lucian") == 0) return "Lucian";
        if (_stricmp(championName.c_str(), "Samira") == 0) return "Samira";
        if (_stricmp(championName.c_str(), "Senna") == 0) return "Senna";
        if (_stricmp(championName.c_str(), "Syndra") == 0) return "Syndra";
        if (_stricmp(championName.c_str(), "Yasuo") == 0) return "Yasuo";
        if (_stricmp(championName.c_str(), "Fiora") == 0) return "Fiora";
        if (_stricmp(championName.c_str(), "TwistedFate") == 0) return "TwistedFate";
        if (_stricmp(championName.c_str(), "Viktor") == 0) return "Viktor";

        if (KuroAIO::AI::Catalog::Supports(championName.c_str())) {
            const auto* entry = KuroAIO::AI::Catalog::FindEntry(championName.c_str());
            if (entry && entry->Profile) {
                return entry->Profile->ChampionName;
            }
        }
        return nullptr;
    }
};

} // namespace Plugins

