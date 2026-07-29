#include "extended-item-stats-tooltip.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <string>

using namespace ruffneck::extended_item_stats;

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

float MeasureRows(std::string_view text, void* context) noexcept {
    const auto lineHeight = *static_cast<const float*>(context);
    if (text.empty()) return 0.0F;
    std::size_t rows = 1;
    for (const auto character : text) {
        if (character == '\n') ++rows;
    }
    return static_cast<float>(rows) * lineHeight;
}

std::string BottomToTopTooltip(std::size_t lines) {
    std::string text;
    for (auto index = lines; index > 0; --index) {
        if (!text.empty()) text.push_back('\n');
        text += "stat-" + std::to_string(index - 1);
    }
    return text;
}

std::string LongBottomToTopTooltip(
    std::size_t lines,
    std::size_t charactersPerLine) {
    std::string text;
    for (auto index = lines; index > 0; --index) {
        if (!text.empty()) text.push_back('\n');
        text.append(charactersPerLine, static_cast<char>('A' + (index % 26)));
    }
    return text;
}

} // namespace

int main() {
    Require(VanillaTooltipLineCapacity(1080, 200) == 38);
    Require(VanillaTooltipLineCapacity(1440, 200) == 39);
    Require(VanillaTooltipLineCapacity(2160, 200) == 39);
    Require(VanillaTooltipLineCapacity(2160, 20) == 20);
    Require(VanillaTooltipLineCapacity(0, 200) == 0);
    Require(CountVisibleTooltipTextUnits("abc\n123") == 7);
    Require(CountVisibleTooltipTextUnits(
        "\xEE\x81\xBE" "3Blue") == 4);

    const std::string truncatedStats =
        "stat one\nstat two without a terminator";
    const std::string expandedStats =
        "stat one\nstat two without a terminator\nstat three\nstat four\n";
    const std::string truncatedTooltip =
        "footer\n" + truncatedStats + "Can be inserted into socketed items\nitem name";
    const auto expandedTooltip = ExpandTooltipSections(
        truncatedTooltip,
        {{truncatedStats, expandedStats}});
    Require(expandedTooltip ==
        "footer\n" + expandedStats + "Can be inserted into socketed items\nitem name");
    Require(ExpandTooltipSections(truncatedTooltip, {}) == truncatedTooltip);
    Require(ExpandTooltipSections(
        truncatedTooltip,
        {{"missing", "missing\nreplacement"}}) == truncatedTooltip);

    const std::vector<std::string> knownTruncatedSections{
        "native truncated",
    };
    Require(IsKnownTruncatedTooltipPass(
        "footer\nnative truncated\nitem name", knownTruncatedSections));
    Require(!IsKnownTruncatedTooltipPass(
        "footer\nrerolled stats\nitem name", knownTruncatedSections));
    Require(ReconcileTooltipGenerationText(
        "footer\nnative truncated\nitem name",
        "footer\nnative truncated\nfull stat block\nitem name",
        true) ==
        "footer\nnative truncated\nfull stat block\nitem name");
    Require(ReconcileTooltipGenerationText(
        "new", "old tooltip that is much longer", false) == "new");
    Require(ReconcileTooltipGenerationText(
        "new affix 02", "old affix 01", false) == "new affix 02");
    Require(ReconcileTooltipGenerationText(
        "new complete stat block", "old", false) == "new complete stat block");

    const std::string longFirstStat(540, 'A');
    const std::string longRemainingStats(540, 'B');
    const std::string longExtraStats(540, 'C');
    const std::string nativeSentence = "Can be inserted into socketed items\n";
    const std::string interleavedTruncated =
        longFirstStat + "\n" + longRemainingStats;
    const std::string interleavedExpanded =
        interleavedTruncated + "\n" + longExtraStats;
    const std::string interleavedTooltip =
        "item name\n" + longFirstStat + nativeSentence + longRemainingStats;
    Require(ExpandTooltipSections(
        interleavedTooltip,
        {{interleavedTruncated, interleavedExpanded}}) ==
        "item name\n" + longFirstStat + nativeSentence
            + longRemainingStats + "\n" + longExtraStats);

    const std::string clippedTooltip =
        "color" + interleavedTruncated.substr(0, interleavedTruncated.size() - 3)
        + nativeSentence + "item name\n";
    Require(ExpandTooltipSections(
        clippedTooltip,
        {{interleavedTruncated, interleavedExpanded}}) ==
        "color" + interleavedExpanded + nativeSentence + "item name\n");

    constexpr float lineHeight = 16.0F;
    const std::string vanilla = "\xC3\xBF" "c1Damage +10\n" "\xC3\xBF" "c4Test Item";
    const auto unchanged = BuildFittedTooltipWindow(
        vanilla,
        {.firstVisibleLine = 0, .availableHeightPixels = 64.0F, .originalHeightPixels = 32.0F},
        MeasureRows,
        const_cast<float*>(&lineHeight));
    Require(!unchanged.overflow);
    Require(!unchanged.refused);
    Require(unchanged.text == vanilla);

    const auto ordinary = BottomToTopTooltip(13);
    TooltipWindowOptions extremeOnly{};
    extremeOnly.minimumScrollableLines = 30;
    const auto ordinaryUnchanged = BuildFittedTooltipWindow(
        ordinary,
        {.firstVisibleLine = 0, .availableHeightPixels = 12.0F,
            .originalHeightPixels = 13.0F},
        MeasureRows,
        const_cast<float*>(&lineHeight),
        extremeOnly);
    Require(!ordinaryUnchanged.overflow);
    Require(!ordinaryUnchanged.refused);
    Require(ordinaryUnchanged.text == ordinary);

    const std::string coloredBottomToTop =
        "\xC3\xBF" "c3Affix three\n"
        "Affix two\n"
        "Affix one\n"
        "\xC3\xBF" "c0Item title";
    const auto coloredWindow = BuildFittedTooltipWindow(
        coloredBottomToTop,
        {.firstVisibleLine = 0, .availableHeightPixels = 3.0F,
            .originalHeightPixels = 4.0F},
        [](std::string_view text, void*) noexcept {
            return static_cast<float>(std::count(text.begin(), text.end(), '\n') + 1);
        },
        nullptr);
    Require(coloredWindow.overflow);
    Require(!coloredWindow.refused);
    Require(coloredWindow.text.starts_with(
        "[Lines 1-2 of 4]\n\xC3\xBF" "c3Affix one\n"));
    Require(coloredWindow.text.ends_with("\xC3\xBF" "c0Item title"));

    const std::string d2rColoredBottomToTop =
        "\xEE\x81\xBE" "3Affix three\n"
        "Affix two\n"
        "Affix one\n"
        "\xEE\x81\xBE" "0Can be inserted into socketed items\n"
        "\xEE\x81\xBE" "3Jewel";
    const auto d2rColoredWindow = BuildFittedTooltipWindow(
        d2rColoredBottomToTop,
        {.firstVisibleLine = 0, .availableHeightPixels = 3.0F,
            .originalHeightPixels = 5.0F},
        [](std::string_view text, void*) noexcept {
            return static_cast<float>(std::count(text.begin(), text.end(), '\n') + 1);
        },
        nullptr,
        {.showPosition = false});
    Require(d2rColoredWindow.overflow);
    Require(!d2rColoredWindow.refused);
    Require(d2rColoredWindow.text.starts_with(
        "\xEE\x81\xBE" "3Affix one\n"));
    Require(d2rColoredWindow.text.ends_with(
        "\xEE\x81\xBE" "3Jewel"));

    const auto huge = BottomToTopTooltip(1019);
    const auto top = BuildFittedTooltipWindow(
        huge,
        {.firstVisibleLine = 0, .availableHeightPixels = 160.0F,
            .originalHeightPixels = 1019.0F * lineHeight},
        MeasureRows,
        const_cast<float*>(&lineHeight));
    Require(top.overflow);
    Require(!top.refused);
    Require(top.totalLineCount == 1019);
    Require(top.visibleLineCount == 9);
    Require(top.firstVisibleLine == 0);
    Require(top.text.starts_with("[Lines 1-9 of 1019]\nstat-8\n"));
    Require(top.text.ends_with("stat-0"));
    Require(MeasureRows(top.text, const_cast<float*>(&lineHeight)) == 160.0F);

    const auto next = ScrollTooltipByLines(
        top.firstVisibleLine, top.totalLineCount, top.visibleLineCount, 3);
    Require(next == 3);
    const auto scrolled = BuildFittedTooltipWindow(
        huge,
        {.firstVisibleLine = next, .availableHeightPixels = 160.0F,
            .originalHeightPixels = 1019.0F * lineHeight},
        MeasureRows,
        const_cast<float*>(&lineHeight));
    Require(scrolled.text.starts_with("[Lines 4-12 of 1019]\nstat-11\n"));
    Require(scrolled.text.ends_with("stat-3"));

    const auto lastPage = ScrollTooltipByLines(0, 1019, 9, 100000);
    Require(lastPage == 1010);
    const auto bottom = BuildFittedTooltipWindow(
        huge,
        {.firstVisibleLine = lastPage, .availableHeightPixels = 160.0F,
            .originalHeightPixels = 1019.0F * lineHeight},
        MeasureRows,
        const_cast<float*>(&lineHeight));
    Require(bottom.text.starts_with("[Lines 1011-1019 of 1019]\nstat-1018\n"));
    Require(bottom.text.ends_with("stat-1010"));

    TooltipWindowOptions layoutBounded{};
    layoutBounded.maxVisibleTextUnits = 1024;
    layoutBounded.showPosition = false;

    // The native vanilla-height viewport remains untouched for short lines.
    const auto fortyShortLines = BottomToTopTooltip(40);
    const auto shortPage = BuildFittedTooltipWindow(
        fortyShortLines,
        {.firstVisibleLine = 0, .availableHeightPixels = 39.0F * lineHeight,
            .originalHeightPixels = 40.0F * lineHeight},
        MeasureRows,
        const_cast<float*>(&lineHeight),
        layoutBounded);
    Require(shortPage.overflow);
    Require(!shortPage.refused);
    Require(shortPage.visibleLineCount == 39);

    // Long affixes dynamically lower the row count so every page stays below
    // D2R's 64-KiB native layout allocation ceiling.
    const auto fortyLongLines = LongBottomToTopTooltip(40, 120);
    const auto vanillaHeightLongPage = BuildFittedTooltipWindow(
        fortyLongLines,
        {.firstVisibleLine = 0, .availableHeightPixels = 39.0F * lineHeight,
            .originalHeightPixels = 40.0F * lineHeight},
        MeasureRows,
        const_cast<float*>(&lineHeight),
        {.maxVisibleTextUnits = 0, .showPosition = false});
    Require(vanillaHeightLongPage.overflow);
    Require(!vanillaHeightLongPage.refused);
    Require(vanillaHeightLongPage.visibleLineCount == 39);
    Require(CountVisibleTooltipTextUnits(vanillaHeightLongPage.text) > 1024);

    const auto longPage = BuildFittedTooltipWindow(
        fortyLongLines,
        {.firstVisibleLine = 0, .availableHeightPixels = 39.0F * lineHeight,
            .originalHeightPixels = 40.0F * lineHeight},
        MeasureRows,
        const_cast<float*>(&lineHeight),
        layoutBounded);
    Require(longPage.overflow);
    Require(!longPage.refused);
    Require(longPage.visibleLineCount == 8);
    Require(longPage.text.size() <= 1024);

    for (std::size_t offset = 0; offset < 40; ++offset) {
        const auto page = BuildFittedTooltipWindow(
            fortyLongLines,
            {.firstVisibleLine = offset,
                .availableHeightPixels = 39.0F * lineHeight,
                .originalHeightPixels = 40.0F * lineHeight},
            MeasureRows,
            const_cast<float*>(&lineHeight),
            layoutBounded);
        Require(page.overflow);
        Require(!page.refused);
        Require(page.visibleLineCount >= 1);
        Require(page.text.size() <= 1024);
    }

    // The workload budget also protects a tooltip that fits vertically but
    // contains unusually wide lines.
    const auto eightVeryLongLines = LongBottomToTopTooltip(8, 200);
    const auto widePage = BuildFittedTooltipWindow(
        eightVeryLongLines,
        {.firstVisibleLine = 0, .availableHeightPixels = 39.0F * lineHeight,
            .originalHeightPixels = 8.0F * lineHeight},
        MeasureRows,
        const_cast<float*>(&lineHeight),
        layoutBounded);
    Require(widePage.overflow);
    Require(!widePage.refused);
    Require(widePage.visibleLineCount == 5);
    Require(widePage.text.size() <= 1024);

    TooltipWindowOptions singleLineBounded{};
    singleLineBounded.maxVisibleTextUnits = 32;
    singleLineBounded.showPosition = false;
    const auto oversizedSingleLine = BuildFittedTooltipWindow(
        std::string(200, 'X'),
        {.firstVisibleLine = 0, .availableHeightPixels = 39.0F * lineHeight,
            .originalHeightPixels = lineHeight},
        MeasureRows,
        const_cast<float*>(&lineHeight),
        singleLineBounded);
    Require(oversizedSingleLine.overflow);
    Require(!oversizedSingleLine.refused);
    Require(oversizedSingleLine.visibleLineCount == 1);
    Require(oversizedSingleLine.text == std::string(29, 'X') + "...");

    TooltipWindowOptions bounded{};
    bounded.maxLines = 1000;
    const auto refused = BuildFittedTooltipWindow(
        huge,
        {.firstVisibleLine = 0, .availableHeightPixels = 160.0F,
            .originalHeightPixels = 1019.0F * lineHeight},
        MeasureRows,
        const_cast<float*>(&lineHeight),
        bounded);
    Require(refused.refused);
    Require(refused.text == huge);

    Require(ScrollTooltipByLines(5, 100, 10, -3) == 2);
    Require(ScrollTooltipByLines(5, 100, 10, -1000) == 0);
    Require(ScrollTooltipByLines(5, 100, 100, 3) == 0);

    TooltipRefreshCoalescer refreshes;
    Require(refreshes.Request());
    Require(!refreshes.Request());
    auto decision = refreshes.Decide(1000, 33);
    Require(decision.refreshNow);
    Require(decision.delayMilliseconds == 0);
    refreshes.MarkRefreshed(1000);
    Require(!refreshes.Pending());

    Require(refreshes.Request());
    decision = refreshes.Decide(1008, 33);
    Require(!decision.refreshNow);
    Require(decision.delayMilliseconds == 25);
    for (auto request = 0; request < 500; ++request) {
        Require(!refreshes.Request());
    }
    decision = refreshes.Decide(1032, 33);
    Require(!decision.refreshNow);
    Require(decision.delayMilliseconds == 1);
    decision = refreshes.Decide(1033, 33);
    Require(decision.refreshNow);
    refreshes.MarkRefreshed(1033);

    Require(refreshes.Request());
    refreshes.Cancel();
    Require(!refreshes.Pending());
    decision = refreshes.Decide(5000, 33);
    Require(!decision.refreshNow);
    Require(decision.delayMilliseconds == 0);

    refreshes.Reset();
    Require(refreshes.Request());
    decision = refreshes.Decide(1, 33);
    Require(decision.refreshNow);
    return 0;
}
