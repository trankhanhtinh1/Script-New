#pragma once
// ============================================================================
// Base.h — port 1-1 từ ImpulseAIO.Common.Base.cs
// Chứa các helper dùng chung cho Orbwalker7UP (và sau này cho các plugin khác
// port từ ImpulseAIO). Hiện tại chỉ port PlusRender.GetFullColorList vì
// Orbwalker7UP::OnDraw dùng. Khi cần thêm helper khác, port theo đúng thứ tự
// xuất hiện trong Base.cs.
// ============================================================================

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Orbwalker7UP::Common {

// ---------------------------------------------------------------------------
// PlusRender — port từ ImpulseAIO.Common.Base.PlusRender (Base.cs line 580+)
// ---------------------------------------------------------------------------
namespace PlusRender {

// Port 1-1 từ Base.cs line 607-623
// GetSingleColorList(srcColor, desColor, count) — trả list count màu nội suy
// tuyến tính từ srcColor → desColor. Mỗi màu là ARGB uint32 (0xAARRGGBB).
// C# dùng System.Drawing.Color (RGB, alpha mặc định 255).
inline std::vector<std::uint32_t> GetSingleColorList(
    std::uint32_t srcColor, std::uint32_t desColor, int count)
{
    std::vector<std::uint32_t> colorFactorList;
    if (count <= 0) return colorFactorList;

    const int srcR = static_cast<int>((srcColor >> 16) & 0xFF);
    const int srcG = static_cast<int>((srcColor >> 8) & 0xFF);
    const int srcB = static_cast<int>(srcColor & 0xFF);
    const int redSpan   = static_cast<int>((desColor >> 16) & 0xFF) - srcR;
    const int greenSpan = static_cast<int>((desColor >> 8) & 0xFF) - srcG;
    const int blueSpan  = static_cast<int>(desColor & 0xFF) - srcB;

    colorFactorList.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const int r = srcR + static_cast<int>(static_cast<double>(i) / count * redSpan);
        const int g = srcG + static_cast<int>(static_cast<double>(i) / count * greenSpan);
        const int b = srcB + static_cast<int>(static_cast<double>(i) / count * blueSpan);
        // C# Color (RGB, alpha=255) -> ARGB uint32 0xFFRRGGBB
        colorFactorList.push_back(0xFF000000u |
            (static_cast<std::uint32_t>(r & 0xFF) << 16) |
            (static_cast<std::uint32_t>(g & 0xFF) << 8) |
            static_cast<std::uint32_t>(b & 0xFF));
    }
    return colorFactorList;
}

// Port 1-1 từ Base.cs line 630-653
// GetFullColorList(totalCount, redToPurple=true) — trả list totalCount màu
// tạo thành gradient rainbow: Red→Yellow→Lime→Cyan→Blue→Magenta (hoặc ngược).
//
// Logic chia 5 đoạn (mỗi đoạn totalCount/5 + dư phân phối theo remainder):
//   - redToPurple=true:  Red→Yellow, Yellow→Lime, Lime→Cyan, Cyan→Blue, Blue→Magenta
//   - redToPurple=false: Magenta→Blue, Blue→Cyan, Cyan→Lime, Lime→Yellow, Yellow→Red
//
// C# System.Drawing.Color constants (RGB):
//   Red     = 255, 0, 0   -> 0xFF0000
//   Yellow  = 255, 255, 0 -> 0xFFFF00
//   Lime    = 0, 255, 0   -> 0x00FF00
//   Cyan    = 0, 255, 255 -> 0x00FFFF
//   Blue    = 0, 0, 255   -> 0x0000FF
//   Magenta = 255, 0, 255 -> 0xFF00FF
inline std::vector<std::uint32_t> GetFullColorList(int totalCount, bool redToPurple = true)
{
    std::vector<std::uint32_t> colorList;
    if (totalCount <= 0) return colorList;

    // C# `totalCount / 5 + (totalCount % 5 > N ? 1 : 0)` — chia 5 đoạn, dư phân phối
    const int base = totalCount / 5;
    const int rem = totalCount % 5;
    auto segCount = [base, rem](int idx) {
        return base + (rem > idx ? 1 : 0);
    };

    // System.Drawing.Color RGB constants (alpha=255 implicit)
    const std::uint32_t Red     = 0xFF0000u;
    const std::uint32_t Yellow  = 0xFFFF00u;
    const std::uint32_t Lime    = 0x00FF00u;
    const std::uint32_t Cyan    = 0x00FFFFu;
    const std::uint32_t Blue    = 0x0000FFu;
    const std::uint32_t Magenta = 0xFF00FFu;

    if (redToPurple) {
        colorList = GetSingleColorList(Red, Yellow, segCount(0));
        auto s1 = GetSingleColorList(Yellow, Lime, segCount(1));
        colorList.insert(colorList.end(), s1.begin(), s1.end());
        auto s2 = GetSingleColorList(Lime, Cyan, segCount(2));
        colorList.insert(colorList.end(), s2.begin(), s2.end());
        auto s3 = GetSingleColorList(Cyan, Blue, segCount(3));
        colorList.insert(colorList.end(), s3.begin(), s3.end());
        auto s4 = GetSingleColorList(Blue, Magenta, segCount(4));
        colorList.insert(colorList.end(), s4.begin(), s4.end());
    } else {
        colorList = GetSingleColorList(Magenta, Blue, segCount(0));
        auto s1 = GetSingleColorList(Blue, Cyan, segCount(1));
        colorList.insert(colorList.end(), s1.begin(), s1.end());
        auto s2 = GetSingleColorList(Cyan, Lime, segCount(2));
        colorList.insert(colorList.end(), s2.begin(), s2.end());
        auto s3 = GetSingleColorList(Lime, Yellow, segCount(3));
        colorList.insert(colorList.end(), s3.begin(), s3.end());
        auto s4 = GetSingleColorList(Yellow, Red, segCount(4));
        colorList.insert(colorList.end(), s4.begin(), s4.end());
    }
    return colorList;
}

} // namespace PlusRender

} // namespace Orbwalker7UP::Common
