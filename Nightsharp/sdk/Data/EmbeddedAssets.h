#pragma once

#include <cstddef>
#include <cstdint>

namespace SDK::Data::EmbeddedAssets {

enum class AssetGroup : std::uint8_t {
    DragonSoul,
    Images,
    Cursor,
};

struct AssetEntry {
    AssetGroup Group;
    const char* RelativePath;
    const char* Key;
    const std::uint8_t* Bytes;
    std::size_t Size;
};

const AssetEntry* AllAssets(std::size_t& count);
const AssetEntry* DragonSoulAssets(std::size_t& count);
const AssetEntry* ImageAssets(std::size_t& count);
const AssetEntry* CursorAssets(std::size_t& count);

} // namespace SDK::Data::EmbeddedAssets