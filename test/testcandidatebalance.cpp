/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../im/pinyin/candidatebalance.h"
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
    testCommonShortEnglishWord();
    testFrequentEnglishDictionary();
    return 0;
}
