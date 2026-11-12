/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../im/pinyin/cloudpinyinmatch.h"
#include <algorithm>
#include <fcitx-utils/log.h>
#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinencoder.h>
#include <libime/pinyin/pinyinime.h>
#include <libime/pinyin/shuangpinprofile.h>
#include <memory>
#include <string>
#include <unordered_map>

using namespace fcitx;
using namespace libime;

int main() {
    // All learning stays in this shared in-memory dictionary; no cloud requests
    // or user files are involved.
    PinyinIME ime(std::make_unique<PinyinDictionary>(),
                  std::make_unique<UserLanguageModel>());
    ime.dict()->addWord(PinyinDictionary::SystemDict, "ni", "你");
    ime.dict()->addWord(PinyinDictionary::SystemDict, "hao", "好");
    ime.dict()->addWord(PinyinDictionary::SystemDict, "ni'hao", "你好");
    auto ordinary =
        std::make_shared<ShuangpinProfile>(ShuangpinBuiltinProfile::Xiaohe);
    ime.setShuangpinProfile(ordinary);
    std::unordered_map<char, char> mapping;
    for (const std::string group :
         {"q", "we", "rt", "y", "u", "io", "p", "a", "sd", "fg", "h", "jk",
          "l;", "z", "xc", "v", "bn", "m"}) {
        for (char key : group) {
            mapping.emplace(key, group.front());
        }
    }
    auto grouped = std::make_shared<ShuangpinProfile>(*ordinary, mapping);
    PinyinContext context(&ime);
    context.setUseShuangpin(true);
    context.setShuangpinProfile(grouped);
    context.type("bihx");
    FCITX_ASSERT(context.selectSyllable(
        0, 2, PinyinSyllable(PinyinInitial::N, PinyinFinal::I)));
    FCITX_ASSERT(context.selectSyllable(
        2, 4, PinyinSyllable(PinyinInitial::H, PinyinFinal::AO)));
    const auto requested = context.candidateFullPinyin(0);
    FCITX_ASSERT(requested == "ni'hao");
    FCITX_ASSERT(context.preedit(PinyinPreeditMode::RawText) == "bi hx");
    const auto guessed = PinyinEncoder::shuangpinToPinyin("bi", *grouped);
    FCITX_ASSERT(guessed != "ni");
    const std::string alternative = "拟好";
    auto result = matchCloudPinyin(context, "", alternative, requested);
    FCITX_ASSERT(result);
    FCITX_ASSERT(result->word == alternative);
    FCITX_ASSERT(result->fullPinyin == requested);
    const std::string sharedPrefixAlternative = "你号";
    auto sharedPrefixResult =
        matchCloudPinyin(context, "", sharedPrefixAlternative, requested);
    FCITX_ASSERT(sharedPrefixResult);
    FCITX_ASSERT(sharedPrefixResult->word == sharedPrefixAlternative);
    FCITX_ASSERT(sharedPrefixResult->fullPinyin == requested);

    const std::string unchanged = "你好";
    auto unchangedResult = matchCloudPinyin(context, "", unchanged, requested);
    FCITX_ASSERT(unchangedResult);
    FCITX_ASSERT(unchangedResult->word.empty());
    FCITX_ASSERT(unchangedResult->fullPinyin.empty());
    std::string unchangedWords;
    std::string unchangedCodes;
    for (const auto &[word, code] : unchangedResult->context) {
        unchangedWords += word;
        unchangedCodes += code;
    }
    FCITX_ASSERT(unchangedWords == unchanged);
    const auto requestedCode = PinyinEncoder::encodeFullPinyin(requested);
    FCITX_ASSERT(unchangedCodes ==
                 std::string(requestedCode.data(), requestedCode.size()));

    PinyinContext reuse(&ime);
    reuse.setUseShuangpin(true);
    reuse.setShuangpinProfile(grouped);
    reuse.type("bihxbihx");
    for (size_t offset : {0, 4}) {
        FCITX_ASSERT(reuse.selectSyllable(
            offset, offset + 2,
            PinyinSyllable(PinyinInitial::N, PinyinFinal::I)));
        FCITX_ASSERT(reuse.selectSyllable(
            offset + 2, offset + 4,
            PinyinSyllable(PinyinInitial::H, PinyinFinal::AO)));
    }
    const std::string reuseAlternative = "你好拟号";
    auto reuseResult = matchCloudPinyin(reuse, "", reuseAlternative,
                                        reuse.candidateFullPinyin(0));
    FCITX_ASSERT(reuseResult);
    FCITX_ASSERT(reuseResult->word == "拟号");
    FCITX_ASSERT(reuseResult->fullPinyin == "ni'hao");
    std::string reusedWords;
    for (const auto &[word, code] : reuseResult->context) {
        reusedWords += word;
    }
    FCITX_ASSERT(reusedWords == "你好");

    ime.dict()->addWord(PinyinDictionary::UserDict, result->fullPinyin,
                        result->word);
    FCITX_ASSERT(ime.dict()->lookupWord(PinyinDictionary::UserDict, "ni'hao",
                                        alternative));
    FCITX_ASSERT(!ime.dict()->lookupWord(PinyinDictionary::UserDict,
                                         guessed + "'hao", alternative));

    // The learned reading must also be correct in the ordinary context sharing
    // this dictionary, rather than leaking a grouped representative reading.
    PinyinContext regular(&ime);
    regular.setUseShuangpin(true);
    regular.type("nihc");
    FCITX_ASSERT(regular.candidateSet().contains(alternative));
    auto learned = std::ranges::find(regular.candidates(), alternative,
                                     &SentenceResult::toString);
    FCITX_ASSERT(learned != regular.candidates().end());
    FCITX_ASSERT(regular.candidateFullPinyin(std::distance(
                     regular.candidates().begin(), learned)) == "ni'hao");
    regular.clear();
    regular.type("bohc");
    FCITX_ASSERT(!regular.candidateSet().contains(alternative));

    FCITX_ASSERT(!matchCloudPinyin(context, "", alternative, "ni"));
    FCITX_ASSERT(!matchCloudPinyin(context, "", alternative, "invalid'hao"));
    FCITX_ASSERT(!matchCloudPinyin(context, "", alternative, "ni''hao"));
    FCITX_ASSERT(!matchCloudPinyin(context, "你", alternative, requested));
    const std::string wrongLength = "拟";
    FCITX_ASSERT(!matchCloudPinyin(context, "", wrongLength, requested));

    // Existing full-pinyin input is segmented by the engine, even when its
    // cloud request uses unsegmented nihao. Ordinary 26-key Shuangpin keeps its
    // established conversion path as well.
    for (bool shuangpin : {false, true}) {
        PinyinContext normal(&ime);
        normal.setUseShuangpin(shuangpin);
        normal.type(shuangpin ? "nihc" : "nihao");
        auto ordinaryResult =
            matchCloudPinyin(normal, "", alternative, std::nullopt);
        FCITX_ASSERT(ordinaryResult);
        FCITX_ASSERT(ordinaryResult->fullPinyin == "ni'hao");
    }

    // A selected Chinese prefix is carried once in history, while the exact
    // cloud request and newly learned word describe only the remaining suffix.
    auto prefix = std::ranges::find(context.candidates(), std::string("你"),
                                    &SentenceResult::toString);
    FCITX_ASSERT(prefix != context.candidates().end());
    context.select(std::distance(context.candidates().begin(), prefix));
    FCITX_ASSERT(context.selectedSentence() == "你");
    const auto suffixRequest = context.candidateFullPinyin(0);
    FCITX_ASSERT(suffixRequest == "hao");
    const std::string suffix = "号";
    auto suffixResult = matchCloudPinyin(context, "你", suffix, suffixRequest);
    FCITX_ASSERT(suffixResult);
    FCITX_ASSERT(suffixResult->word == suffix);
    FCITX_ASSERT(suffixResult->fullPinyin == "hao");
    FCITX_ASSERT(suffixResult->context.size() == 1);
    FCITX_ASSERT(suffixResult->context.front().first == "你");
    const auto ni = PinyinEncoder::encodeFullPinyin("ni");
    FCITX_ASSERT(suffixResult->context.front().second ==
                 std::string(ni.data(), ni.size()));
    ime.dict()->addWord(PinyinDictionary::UserDict, suffixResult->fullPinyin,
                        suffixResult->word);
    FCITX_ASSERT(
        ime.dict()->lookupWord(PinyinDictionary::UserDict, "hao", suffix));
    ime.dict()->addWord(PinyinDictionary::UserDict,
                        sharedPrefixResult->fullPinyin,
                        sharedPrefixResult->word);
    FCITX_ASSERT(ime.dict()->lookupWord(PinyinDictionary::UserDict, "ni'hao",
                                        sharedPrefixAlternative));
    FCITX_ASSERT(!ime.dict()->lookupWord(
        PinyinDictionary::UserDict, guessed + "'hao", sharedPrefixAlternative));
}
