/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../im/pinyin/candidatebalance.h"
#include <algorithm>
#include <numeric>
#include <cstddef>
#include <cstdint>
#include <fcitx-utils/log.h>
#include <fcitx-utils/stringutils.h>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace fcitx;

namespace {

using Kind = CandidateKind;

struct Candidate {
    std::string label;
    Kind kind;
    bool hasSymbol = false;
    size_t attachedCount = 0;
};

std::vector<CandidateInfo>
candidateInfos(const std::vector<Candidate> &candidates) {
    std::vector<CandidateInfo> result;
    for (const auto &candidate : candidates) {
        result.push_back(
            {candidate.kind, candidate.attachedCount, candidate.hasSymbol});
    }
    return result;
}

// A star marks a folded run, a minus a run deferred past the front page, and
// a plus an unmerged run. All of them remain selectable in their own group.
std::string layout(const std::vector<Candidate> &candidates,
                   const std::vector<CandidatePlacement> &placements) {
    std::vector<std::string> labels;
    for (const auto &placement : placements) {
        const auto &candidate = candidates[placement.index];
        const auto marker = placement.foldSymbols    ? "*"
                            : placement.deferSymbols ? "-"
                                                     : "+";
        labels.push_back(candidate.label + (candidate.hasSymbol ? marker : ""));
    }
    return stringutils::join(labels, " ");
}

void expectTwoKey(const std::vector<Candidate> &candidates,
                  std::string_view expected,
                  const TwoKeyBalanceLimits &limits = {}) {
    const auto actual =
        layout(candidates,
               balanceTwoKeyCandidates(candidateInfos(candidates), limits));
    FCITX_ASSERT(actual == expected)
        << "\n  actual:   " << actual << "\n  expected: " << expected;
}

void expectGeneral(const std::vector<Candidate> &candidates,
                   std::string_view expected, size_t frontSize = 5) {
    const auto actual = layout(
        candidates, arrangeCandidates(candidateInfos(candidates), frontSize));
    FCITX_ASSERT(actual == expected)
        << "\n  actual:   " << actual << "\n  expected: " << expected;
}

void testTwoKeyOrder() {
    // A syllable's characters fill the page after its two most competitive
    // words. Corrections and matches of one key remain behind those words.
    expectTwoKey({{"开始", Kind::Phrase},
                  {"kshatriya", Kind::RareEnglish},
                  {"空", Kind::Single},
                  {"可是", Kind::Phrase},
                  {"快速", Kind::Phrase},
                  {"考试", Kind::Phrase},
                  {"孔", Kind::Single},
                  {"恐", Kind::Single},
                  {"考生", Kind::Phrase},
                  {"控", Kind::Single},
                  {"箜", Kind::Single},
                  {"可", Kind::LooseSingle}},
                 "开始 空 可是 孔 恐 控 箜 快速 考试 考生 kshatriya 可");
    // With too few characters, abbreviation words take the room left over.
    expectTwoKey({{"给", Kind::Single},
                  {"岗位", Kind::Phrase},
                  {"国外", Kind::Phrase},
                  {"购物", Kind::Phrase},
                  {"更为", Kind::Phrase},
                  {"顾问", Kind::Phrase},
                  {"个", Kind::LooseSingle},
                  {"感悟", Kind::Phrase},
                  {"各", Kind::LooseSingle}},
                 "给 岗位 国外 购物 更为 顾问 感悟 个 各");
    expectTwoKey({{"并且", Kind::Phrase},
                  {"病情", Kind::Phrase},
                  {"表情", Kind::Phrase},
                  {"标签", Kind::Phrase},
                  {"被", Kind::LooseSingle},
                  {"抱歉", Kind::Phrase},
                  {"不", Kind::LooseSingle}},
                 "并且 病情 表情 标签 抱歉 被 不");
    // Common English uses the same allowance as Chinese words.
    expectTwoKey({{"词一", Kind::Phrase},
                  {"ok", Kind::CommonEnglish},
                  {"词二", Kind::Phrase},
                  {"字一", Kind::Single},
                  {"字二", Kind::Single},
                  {"字三", Kind::Single},
                  {"字四", Kind::Single}},
                 "词一 ok 字一 字二 字三 字四 词二");
}

void testCorrectionLead() {
    expectTwoKey({{"的", Kind::LooseSingle},
                  {"色", Kind::Single},
                  {"数额", Kind::Phrase},
                  {"涩", Kind::Single},
                  {"塞", Kind::Single},
                  {"善恶", Kind::Phrase},
                  {"税额", Kind::Phrase},
                  {"瑟", Kind::Single}},
                 "色 的 数额 涩 塞 瑟 善恶 税额");
    // Invalid syllables still prefer a dictionary word over a correction.
    expectTwoKey({{"为", Kind::LooseSingle},
                  {"巍峨", Kind::Phrase},
                  {"位", Kind::LooseSingle},
                  {"万恶", Kind::Phrase}},
                 "巍峨 为 位 万恶");
    // The user chose not to promote words merely assembled by the engine.
    expectTwoKey({{"种", Kind::LooseSingle},
                  {"哦是", Kind::Sentence},
                  {"重", Kind::LooseSingle}},
                 "种 哦是 重");
    expectGeneral({{"他", Kind::LooseSingle},
                   {"人啊", Kind::Sentence},
                   {"她", Kind::LooseSingle}},
                  "他 人啊 她");
    expectGeneral(
        {{"你", Kind::LooseSingle}, {"挪", Kind::Single}, {"诺", Kind::Single}},
        "挪 你 诺");
    // A user-pinned lead remains authoritative.
    expectTwoKey({{"定", Kind::PinnedSingle},
                  {"词一", Kind::Phrase},
                  {"字一", Kind::Single}},
                 "定 词一 字一");
}

void testPinnedAndAllowance() {
    // User-configured phrases stay in the front and use the word allowance.
    expectTwoKey({{"自定一", Kind::PinnedPhrase},
                  {"词一", Kind::Phrase},
                  {"词二", Kind::Phrase},
                  {"字", Kind::Single}},
                 "自定一 词一 字 词二");
    expectTwoKey({{"自定一", Kind::PinnedPhrase},
                  {"自定二", Kind::PinnedPhrase},
                  {"自定三", Kind::PinnedPhrase},
                  {"词", Kind::Phrase},
                  {"字", Kind::Single}},
                 "自定一 自定二 自定三 字 词");
    expectTwoKey({{"词", Kind::Phrase},
                  {"一", Kind::Single},
                  {"二", Kind::Single},
                  {"三", Kind::Single}},
                 "一 二 三 词", {.frontSize = 2, .frontPhrases = 0});
}

void testSymbolRunBoundaries() {
    // Only runs that begin on the front page fold. The second run below
    // begins after the page, and its individual symbols remain selectable.
    const std::vector<Candidate> candidates = {
        {"开始", Kind::Phrase, true}, {"字一", Kind::Single},
        {"字二", Kind::Single},       {"字三", Kind::Single},
        {"快速", Kind::Phrase, true}, {"字四", Kind::Single}};
    expectGeneral(candidates, "开始* 字一 字二 字三 快速+ 字四");
    expectTwoKey(candidates, "开始* 字一 字二 字三 字四 快速+");
    // Independent front-page groups are deferred intact, never merged into
    // the first run or folded into the Chinese parent candidate.
    const std::vector<Candidate> independent = {{"开始", Kind::Phrase, true},
                                                {"空", Kind::Single, true},
                                                {"可是", Kind::Phrase},
                                                {"孔", Kind::Single},
                                                {"恐", Kind::Single}};
    expectGeneral(independent, "开始* 空- 可是 孔 恐");
    expectTwoKey(independent, "开始* 空- 可是 孔 恐");
    // A run after the fifth candidate is not folded into a front-page item.
    expectGeneral({{"一", Kind::Single},
                   {"二", Kind::Single},
                   {"三", Kind::Single},
                   {"四", Kind::Single},
                   {"五", Kind::Single, true}},
                  "一 二 三 四 五+");
    // Lua-generated candidates count before deciding where the symbols start.
    expectGeneral({{"词", Kind::Phrase, true, 3}, {"字", Kind::Single, true}},
                  "词* 字+");
    expectGeneral({{"词", Kind::Phrase, true, 4}, {"字", Kind::Single, true}},
                  "词+ 字+");
}

void testGeneralOrder() {
    // Full pinyin keeps a split word such as 西安 amongst its characters.
    expectGeneral({{"先", Kind::Single},
                   {"县", Kind::Single},
                   {"现", Kind::Single},
                   {"线", Kind::Single},
                   {"西安", Kind::Phrase},
                   {"显", Kind::Single},
                   {"鲜", Kind::Single}},
                  "先 县 现 线 西安 显 鲜");
    // Rare English waits behind every exact match, while common words retain
    // the engine's position even for an input longer than two keys.
    expectGeneral({{"开始", Kind::Phrase},
                   {"kshatriya", Kind::RareEnglish},
                   {"hello", Kind::CommonEnglish},
                   {"词二", Kind::Sentence},
                   {"纠一", Kind::LooseSingle},
                   {"字", Kind::Single},
                   {"纠二", Kind::LooseSingle},
                   {"纠三", Kind::LooseSingle}},
                  "开始 hello 词二 纠一 字 kshatriya 纠二 纠三");
    expectGeneral({{"rare", Kind::RareEnglish},
                   {"字", Kind::Single},
                   {"词", Kind::Phrase}},
                  "字 词 rare");
    expectGeneral({{"rare", Kind::RareEnglish}}, "rare");
}

void testEveryCandidateIsPlacedOnce() {
    constexpr Kind kinds[] = {
        Kind::Single,      Kind::LooseSingle,  Kind::Phrase,
        Kind::Sentence,    Kind::LoosePhrase,  Kind::CommonEnglish,
        Kind::RareEnglish, Kind::PinnedSingle, Kind::PinnedPhrase};
    uint32_t seed = 20261117;
    const auto next = [&seed]() {
        seed = seed * 1664525 + 1013904223;
        return seed >> 16;
    };
    for (size_t size = 0; size < 40; size++) {
        std::vector<CandidateInfo> infos;
        for (size_t i = 0; i < size; i++) {
            infos.push_back({kinds[next() % std::size(kinds)],
                             next() % 5 == 0 ? size_t{1} : size_t{0},
                             next() % 3 == 0});
        }
        for (const auto &placements :
             {balanceTwoKeyCandidates(infos), arrangeCandidates(infos)}) {
            FCITX_ASSERT(placements.size() == size);
            std::vector<bool> seen(size);
            for (const auto &placement : placements) {
                FCITX_ASSERT(placement.index < size);
                FCITX_ASSERT(!seen[placement.index]);
                seen[placement.index] = true;
                FCITX_ASSERT(!placement.foldSymbols ||
                             infos[placement.index].hasSymbol);
                FCITX_ASSERT(!placement.deferSymbols ||
                             infos[placement.index].hasSymbol);
                FCITX_ASSERT(!placement.foldSymbols || !placement.deferSymbols);
            }
        }
    }
}

void testAbbreviationPageOrder() {
    std::vector<FirstPageCandidateInfo> candidates(12);
    candidates[1].abbreviation = true;
    candidates[2].fixed = true;
    candidates[5].abbreviation = true;
    candidates[7].fixed = true;
    candidates[9].deferredFallback = true;
    candidates[10].abbreviation = true;
    candidates[10].bestAbbreviation = true;
    auto order = abbreviationFirstPageOrder(candidates, 5).indices;
    FCITX_ASSERT((order == std::vector<size_t>{0, 3, 2, 4, 10, 6, 8, 7, 11, 1, 5, 9}));
    // Recovery keeps the second page slot without shifting either pinned slot,
    // including the one beyond the first page.
    candidates[3].fallback = true;
    order = abbreviationFirstPageOrder(candidates, 5).indices;
    FCITX_ASSERT((order == std::vector<size_t>{0, 3, 2, 4, 10, 6, 8, 7, 11, 1, 5, 9}));
    order = abbreviationFirstPageOrder(candidates, 2).indices;
    FCITX_ASSERT(order[0] == 10 && order[1] == 3);
    FCITX_ASSERT(order[2] == 2 && order[7] == 7);
    order = abbreviationFirstPageOrder(candidates, 1).indices;
    FCITX_ASSERT(order[0] == 3); // A one-slot page prioritizes recovery.
    candidates[0].fixed = true;
    order = abbreviationFirstPageOrder(candidates, 1).indices;
    FCITX_ASSERT(order[0] == 0); // Custom phrase wins over abbreviation.
    candidates[1].fixed = true;
    order = abbreviationFirstPageOrder(candidates, 2).indices;
    FCITX_ASSERT(order[0] == 0 && order[1] == 1);
    std::vector<FirstPageCandidateInfo> shortPage(4);
    shortPage[0].abbreviation = shortPage[1].abbreviation = true;
    shortPage[2].fixed = true;
    shortPage[3].abbreviation = shortPage[3].bestAbbreviation = true;
    const auto shortOrder = abbreviationFirstPageOrder(shortPage, 4);
    FCITX_ASSERT(shortOrder.indices[2] == 2);
    FCITX_ASSERT(shortOrder.indices[0] == 3);
    FCITX_ASSERT(shortOrder.firstPageSize == 3); // Keep fixed slot 2 visible.
    shortPage[2].fixed = false;
    FCITX_ASSERT(abbreviationFirstPageOrder(shortPage, 4).firstPageSize == 2);
    // A symbol whose source abbreviation was deferred shares that layout
    // classification; it must not fill the source's vacant first-page slot.
    std::vector<FirstPageCandidateInfo> attached(7);
    attached[0].abbreviation = attached[0].bestAbbreviation = true;
    attached[1].abbreviation = true;
    attached[2].deferredFallback = true;
    attached[3].abbreviation = true;
    attached[4].deferredFallback = true;
    const auto attachedOrder = abbreviationFirstPageOrder(attached, 5);
    FCITX_ASSERT(attachedOrder.firstPageSize == 3);
    FCITX_ASSERT((std::vector<size_t>(attachedOrder.indices.begin(),
                                    attachedOrder.indices.begin() + 3) ==
                  std::vector<size_t>{0, 5, 6}));
    // If symbol insertion places its pinned source outside the page, closing
    // the plan by deferring the head must retain the best and fixed bulk slot.
    std::vector<FirstPageCandidateInfo> orphan(6);
    orphan[1].deferredFallback = orphan[2].deferredFallback = true;
    orphan[3].fixed = true;
    orphan[4].deferredFallback = true;
    orphan[5].abbreviation = orphan[5].bestAbbreviation = true;
    const auto closed = abbreviationFirstPageOrder(orphan, 3);
    FCITX_ASSERT(closed.indices[3] == 3);
    FCITX_ASSERT(closed.firstPageSize == 2);
    FCITX_ASSERT((std::vector<size_t>(closed.indices.begin(), closed.indices.begin() + 2) ==
                  std::vector<size_t>{0, 5}));
    candidates[10].bestAbbreviation = false;
    order = abbreviationFirstPageOrder(candidates, 5).indices;
    for (size_t i = 0; i < order.size(); ++i) FCITX_ASSERT(order[i] == i);
    // Rare English cannot fill the holes left by deferred abbreviations.
    // Common English remains an ordinary candidate in its original order.
    std::vector<FirstPageCandidateInfo> english(5);
    english[0].abbreviation = english[0].bestAbbreviation = true;
    english[1].abbreviation = true;
    english[2].rareEnglish = true;
    const auto englishOrder = abbreviationFirstPageOrder(english, 4);
    FCITX_ASSERT(englishOrder.firstPageSize == 3);
    FCITX_ASSERT((englishOrder.indices == std::vector<size_t>{0, 3, 4, 1, 2}));
    english[2].rareEnglish = false;
    const auto commonOrder = abbreviationFirstPageOrder(english, 4);
    FCITX_ASSERT(commonOrder.firstPageSize == 4);
    FCITX_ASSERT((commonOrder.indices == std::vector<size_t>{0, 2, 3, 4, 1}));
    // Low-frequency Chinese completions obey the same first-page boundary,
    // while common completions keep the ordinary engine order.
    std::vector<FirstPageCandidateInfo> completions(5);
    completions[0].abbreviation = completions[0].bestAbbreviation = true;
    completions[1].abbreviation = true;
    completions[2].rareCompletion = true;
    const auto rareCompletionOrder = abbreviationFirstPageOrder(completions, 4);
    FCITX_ASSERT(rareCompletionOrder.firstPageSize == 3);
    FCITX_ASSERT((rareCompletionOrder.indices ==
                  std::vector<size_t>{0, 3, 4, 1, 2}));
    completions[2].rareCompletion = false;
    const auto commonCompletionOrder = abbreviationFirstPageOrder(completions, 4);
    FCITX_ASSERT(commonCompletionOrder.firstPageSize == 4);
    FCITX_ASSERT((commonCompletionOrder.indices ==
                  std::vector<size_t>{0, 2, 3, 4, 1}));
    // Deduplication can retain a completion path for the best abbreviation.
    // Its first-page eligibility and fixed user slots take precedence.
    completions[0].rareCompletion = completions[2].rareCompletion = true;
    const auto bestCompletionOrder = abbreviationFirstPageOrder(completions, 4);
    FCITX_ASSERT(bestCompletionOrder.firstPageSize == 3);
    FCITX_ASSERT((bestCompletionOrder.indices == rareCompletionOrder.indices));
    completions[2].fixed = true;
    const auto fixedCompletionOrder = abbreviationFirstPageOrder(completions, 4);
    FCITX_ASSERT(fixedCompletionOrder.firstPageSize == 4);
    FCITX_ASSERT((fixedCompletionOrder.indices ==
                  std::vector<size_t>{0, 3, 2, 4, 1}));
    // Without a best abbreviation this policy does not alter ordinary pages.
    completions[0].bestAbbreviation = false;
    const auto ordinaryCompletionOrder = abbreviationFirstPageOrder(completions, 4);
    FCITX_ASSERT(ordinaryCompletionOrder.firstPageSize == 4);
    FCITX_ASSERT((ordinaryCompletionOrder.indices ==
                  std::vector<size_t>{0, 1, 2, 3, 4}));
}

void testAbbreviationOrderIsCompleteAndStable() {
    uint32_t seed = 9173;
    const auto next = [&] { seed = seed * 1664525 + 1013904223; return seed; };
    for (size_t iteration = 0; iteration < 500; ++iteration) {
        std::vector<FirstPageCandidateInfo> candidates(next() % 40 + 1);
        for (auto &candidate : candidates) {
            candidate.fixed = next() % 5 == 0;
            candidate.abbreviation = next() % 4 == 0;
            candidate.deferredFallback = next() % 11 == 0;
            candidate.rareEnglish = next() % 7 == 0;
            candidate.rareCompletion = next() % 7 == 0;
        }
        const auto best = next() % candidates.size();
        candidates[best].abbreviation = true;
        candidates[best].bestAbbreviation = true;
        const auto fallback = next() % candidates.size();
        if (fallback != best) candidates[fallback].fallback = true;
        const auto capacity = next() % 12;
        const auto order = abbreviationFirstPageOrder(candidates, capacity).indices;
        FCITX_ASSERT(order.size() == candidates.size());
        auto sorted = order;
        std::ranges::sort(sorted);
        for (size_t i = 0; i < sorted.size(); ++i) {
            FCITX_ASSERT(sorted[i] == i);
            if (candidates[i].fixed) FCITX_ASSERT(order[i] == i);
        }
        std::vector<size_t> ordinary;
        for (const auto index : order) {
            const auto &candidate = candidates[index];
            if (!candidate.fixed && !candidate.abbreviation &&
                !candidate.deferredFallback && !candidate.rareEnglish &&
                !candidate.rareCompletion && !candidate.fallback)
                ordinary.push_back(index);
        }
        FCITX_ASSERT(std::ranges::is_sorted(ordinary));
    }
}

void testCommonShortEnglishWord() {
    for (std::string_view word :
         {"ok", "hi", "is", "the", "you", "app", "that", "okay", "OK", "Yes"}) {
        FCITX_ASSERT(isCommonShortEnglishWord(word)) << word;
    }
    for (std::string_view word :
         {"", "a", "kshatriya", "hello", "tha", "apt", "wo", "de", "ca", "al",
          "o k", "ok!", "你好", "空"}) {
        FCITX_ASSERT(!isCommonShortEnglishWord(word)) << word;
    }
}

std::string
spellDictionary(const std::vector<std::pair<uint16_t, std::string>> &words) {
    std::string result = "FSCD0000";
    const auto size = static_cast<uint32_t>(words.size());
    for (unsigned i = 0; i < sizeof(uint32_t); i++) {
        result.push_back(static_cast<char>((size >> (i * 8)) & 0xff));
    }
    for (const auto &[weight, word] : words) {
        result.push_back(static_cast<char>(weight & 0xff));
        result.push_back(static_cast<char>(weight >> 8));
        result.append(word);
        result.push_back('\0');
    }
    return result;
}

void testFrequentEnglishDictionary() {
    FrequentEnglishWords words;
    const auto dictionary = spellDictionary({{65535, "Hello"},
                                             {50000, "world"},
                                             {49999, "obscure"},
                                             {65535, "ok"},
                                             {60000, "alpha"}});
    words.load(dictionary);
    for (std::string_view word : {"hello", "HELLO", "World", "alpha"}) {
        FCITX_ASSERT(words.contains(word)) << word;
    }
    for (std::string_view word : {"obscure", "ok", "missing", ""}) {
        FCITX_ASSERT(!words.contains(word)) << word;
    }
    // Truncated entries must not produce a partial word or discard earlier
    // complete entries. Bad headers must clear a previous dictionary load.
    words.load(dictionary + std::string("\xff\xffunfinished", 12));
    FCITX_ASSERT(words.contains("hello"));
    FCITX_ASSERT(!words.contains("unfinished"));
    words.load("not-a-spell-dictionary");
    FCITX_ASSERT(!words.contains("hello"));
    words.load("FSCD0000");
    FCITX_ASSERT(!words.contains("hello"));
}

} // namespace

int main() {
    testTwoKeyOrder();
    testCorrectionLead();
    testPinnedAndAllowance();
    testSymbolRunBoundaries();
    testGeneralOrder();
    testEveryCandidateIsPlacedOnce();
    testAbbreviationPageOrder();
    testAbbreviationOrderIsCompleteAndStable();
    testCommonShortEnglishWord();
    testFrequentEnglishDictionary();
    return 0;
}
