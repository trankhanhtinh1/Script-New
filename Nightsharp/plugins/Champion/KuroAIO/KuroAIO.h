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
#include "Champion/Sylas.h"
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

inline Palette GetPalette(SDK::ChampionId championId) {
    switch (championId) {
    case SDK::ChampionId::Aphelios:
        return { IM_COL32(0, 229, 255, 255), IM_COL32(157, 78, 221, 255), 1.0f };
    case SDK::ChampionId::Sylas:
        return { IM_COL32(0, 240, 255, 255), IM_COL32(230, 230, 250, 255), 1.0f };
    case SDK::ChampionId::Rengar:
        return { IM_COL32(217, 4, 41, 255), IM_COL32(255, 107, 0, 255), 1.10f };
    case SDK::ChampionId::Jinx:
        return { IM_COL32(255, 42, 112, 255), IM_COL32(0, 229, 255, 255), 1.10f };
    case SDK::ChampionId::Katarina:
        return { IM_COL32(255, 59, 92, 255), IM_COL32(157, 78, 221, 255), 1.05f };
    case SDK::ChampionId::Kindred:
        return { IM_COL32(245, 241, 232, 255), IM_COL32(103, 199, 235, 255), 0.82f };
    case SDK::ChampionId::Lucian:
        return { IM_COL32(116, 232, 255, 255), IM_COL32(244, 208, 111, 255), 0.95f };
    case SDK::ChampionId::Samira:
        return { IM_COL32(255, 180, 94, 255), IM_COL32(214, 40, 57, 255), 1.12f };
    case SDK::ChampionId::Senna:
        return { IM_COL32(87, 242, 192, 255), IM_COL32(138, 92, 255, 255), 0.90f };
    case SDK::ChampionId::Syndra:
        return { IM_COL32(212, 124, 255, 255), IM_COL32(108, 77, 255, 255), 1.00f };
    case SDK::ChampionId::TwistedFate:
        return { IM_COL32(255, 209, 102, 255), IM_COL32(239, 71, 111, 255), 0.88f };
    case SDK::ChampionId::Viktor:
        return { IM_COL32(82, 217, 255, 255), IM_COL32(255, 138, 61, 255), 1.04f };
    case SDK::ChampionId::Yasuo:
        return { IM_COL32(142, 235, 255, 255), IM_COL32(82, 113, 255, 255), 0.92f };
    case SDK::ChampionId::Fiora:
        return { IM_COL32(113, 196, 255, 255), IM_COL32(229, 107, 178, 255), 0.98f };
    default:
        return { IM_COL32(255, 170, 64, 255), IM_COL32(156, 64, 255, 255), 1.0f };
    }
}

inline SDK::UI::Menu* GetRoot(SDK::ChampionId championId) {
    if (championId == SDK::ChampionId::Unknown) {
        return nullptr;
    }
    if (::Plugins::KuroAIO::AI::Catalog::Supports(championId)) {
        return ::Plugins::KuroAIO::AI::Engine::MenuRoot;
    }
    switch (championId) {
    case SDK::ChampionId::Sylas: return ::Plugins::KuroAIO::Sylas::MenuRoot;
    case SDK::ChampionId::Rengar: return ::Plugins::KuroAIO::Rengar::MenuRoot;
    case SDK::ChampionId::Jinx: return ::Plugins::KuroAIO::Jinx::MenuRoot;
    case SDK::ChampionId::Katarina: return ::Plugins::KuroAIO::Katarina::MenuRoot;
    case SDK::ChampionId::Kindred: return ::Plugins::KuroAIO::Kindred::MenuRoot;
    case SDK::ChampionId::Lucian: return ::Plugins::KuroAIO::Lucian::MenuRoot;
    case SDK::ChampionId::Samira: return ::Plugins::KuroAIO::Samira::MenuRoot;
    case SDK::ChampionId::Senna: return ::Plugins::KuroAIO::Senna::MenuRoot;
    case SDK::ChampionId::Syndra: return ::Plugins::KuroAIO::Syndra::MenuRoot;
    case SDK::ChampionId::TwistedFate: return ::Plugins::KuroAIO::TwistedFate::MenuRoot;
    case SDK::ChampionId::Viktor: return ::Plugins::KuroAIO::Viktor::MenuRoot;
    case SDK::ChampionId::Yasuo: return ::Plugins::KuroAIO::Yasuo::MenuRoot;
    case SDK::ChampionId::Fiora: return ::Plugins::KuroAIO::Fiora::MenuRoot;
    default: return nullptr;
    }
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

inline ImTextureID LoadChampionIcon(SDK::ChampionId championId) {
    ImTextureID placeholder = SDK::UI::Icons::GetPlaceholder();
    const char* championName = SDK::ChampionName(championId);
    if (!championName || !championName[0]) {
        return nullptr;
    }
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

inline bool Apply(SDK::ChampionId championId) {
    SDK::UI::Menu* root = GetRoot(championId);
    if (!root) {
        return false;
    }

    ApplyGradient(root, GetPalette(championId));

    ImTextureID icon = LoadChampionIcon(championId);
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
        const SDK::ChampionId championId = CurrentChampionId();
        switch (championId) {
        case SDK::ChampionId::Rengar: return "champion.kuroaio.rengar";
        case SDK::ChampionId::Jinx: return "champion.kuroaio.jinx";
        case SDK::ChampionId::Katarina: return "champion.kuroaio.katarina";
        case SDK::ChampionId::Kindred: return "champion.kuroaio.kindred";
        case SDK::ChampionId::Lucian: return "champion.kuroaio.lucian";
        case SDK::ChampionId::Samira: return "champion.kuroaio.samira";
        case SDK::ChampionId::Senna: return "champion.kuroaio.senna";
        case SDK::ChampionId::Syndra: return "champion.kuroaio.syndra";
        case SDK::ChampionId::Sylas: return "champion.kuroaio.sylas";
        case SDK::ChampionId::Yasuo: return "champion.kuroaio.yasuo";
        case SDK::ChampionId::Fiora: return "champion.kuroaio.fiora";
        case SDK::ChampionId::TwistedFate: return "champion.kuroaio.twistedfate";
        case SDK::ChampionId::Viktor: return "champion.kuroaio.viktor";
        default: return "champion.kuroaio";
        }
    }
    const char* GetAuthor() const override { return "Kuro"; }
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    const char* GetChampionName() const override { return CurrentSupportedChampionName(); }
    bool AutoLoadByDefault() const override { return true; }
    bool CanLoad() const override { return IsCurrentChampionSupported(); }

    void OnLoad() override {
        const SDK::ChampionId championId = CurrentChampionId();
        if (!IsSupportedChampion(championId)) {
            return;
        }

        switch (championId) {
        case SDK::ChampionId::Rengar: KuroAIO::Rengar::OnGameLoad(); break;
        case SDK::ChampionId::Jinx: KuroAIO::Jinx::OnGameLoad(); break;
        case SDK::ChampionId::Katarina: KuroAIO::Katarina::OnGameLoad(); break;
        case SDK::ChampionId::Kindred: KuroAIO::Kindred::OnGameLoad(); break;
        case SDK::ChampionId::Lucian: KuroAIO::Lucian::OnGameLoad(); break;
        case SDK::ChampionId::Samira: KuroAIO::Samira::OnGameLoad(); break;
        case SDK::ChampionId::Senna: KuroAIO::Senna::OnGameLoad(); break;
        case SDK::ChampionId::Syndra: KuroAIO::Syndra::OnGameLoad(); break;
        case SDK::ChampionId::Sylas: KuroAIO::Sylas::OnGameLoad(); break;
        case SDK::ChampionId::Yasuo: KuroAIO::Yasuo::OnGameLoad(); break;
        case SDK::ChampionId::Fiora: KuroAIO::Fiora::OnGameLoad(); break;
        case SDK::ChampionId::TwistedFate: KuroAIO::TwistedFate::OnGameLoad(); break;
        case SDK::ChampionId::Viktor: KuroAIO::Viktor::OnGameLoad(); break;
        default: KuroAIO::AI::Catalog::Load(championId); break;
        }

        m_menuThemeApplied = KuroAIO::ChampionMenuTheme::Apply(championId);
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

        const SDK::ChampionId championId = CurrentChampionId();
        if (IsSupportedChampion(championId)) {
            m_menuThemeApplied = KuroAIO::ChampionMenuTheme::Apply(championId);
        }
        m_nextMenuThemeRetry = now + 1000;
    }


    void OnUnload() override {
        KuroAIO::Rengar::OnUnload();
        KuroAIO::Jinx::OnUnload();
        KuroAIO::Katarina::OnUnload();
        KuroAIO::Kindred::OnUnload();
        KuroAIO::Lucian::OnUnload();
        KuroAIO::Samira::OnUnload();
        KuroAIO::Senna::OnUnload();
        KuroAIO::Syndra::OnUnload();
        KuroAIO::Sylas::OnUnload();
        KuroAIO::Yasuo::OnUnload();
        KuroAIO::Fiora::OnUnload();
        KuroAIO::TwistedFate::OnUnload();
        KuroAIO::Viktor::OnUnload();
        KuroAIO::AI::Catalog::Unload();

        m_menuThemeApplied = false;
        m_nextMenuThemeRetry = 0;
    }

private:
    bool m_menuThemeApplied = false;
    DWORD m_nextMenuThemeRetry = 0;

    static bool IsSupportedChampion(SDK::ChampionId championId) {
        if (championId == SDK::ChampionId::Unknown) {
            return false;
        }
        switch (championId) {
        case SDK::ChampionId::Rengar:
        case SDK::ChampionId::Jinx:
        case SDK::ChampionId::Katarina:
        case SDK::ChampionId::Kindred:
        case SDK::ChampionId::Lucian:
        case SDK::ChampionId::Samira:
        case SDK::ChampionId::Senna:
        case SDK::ChampionId::Syndra:
        case SDK::ChampionId::Sylas:
        case SDK::ChampionId::Yasuo:
        case SDK::ChampionId::Fiora:
        case SDK::ChampionId::TwistedFate:
        case SDK::ChampionId::Viktor:
            return true;
        default:
            return KuroAIO::AI::Catalog::Supports(championId);
        }
    }

    static SDK::ChampionId CurrentChampionId() {
        const std::string& cached = SDK::GameObject::GetCachedChampionName();
        if (!cached.empty()) {
            return SDK::ChampionIdFromName(cached.c_str());
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return SDK::ChampionId::Unknown;
        }
        return SDK::ChampionIdFromName(player.CharacterName().c_str());
    }

    static bool IsCurrentChampionSupported() {
        return IsSupportedChampion(CurrentChampionId());
    }

    static const char* CurrentSupportedChampionName() {
        const SDK::ChampionId championId = CurrentChampionId();
        return IsSupportedChampion(championId) ? SDK::ChampionName(championId) : nullptr;
    }
};

} // namespace Plugins

