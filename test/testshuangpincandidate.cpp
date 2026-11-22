/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../im/pinyin/shuangpincandidate.h"
#include <algorithm>
#include <cstdlib>
#include <fcitx-utils/log.h>
#include <filesystem>
#include <fstream>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinime.h>
#include <libime/pinyin/shuangpinprofile.h>
#include <memory>
#include <sstream>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

using namespace libime;
using namespace fcitx;

namespace {

class TestModel : public UserLanguageModel {
public:
    float score(const State &state, const WordNode &node,
                State &out) const override {
        out = state;
        return node.word() == "你好吗" ? -7.0F : -3.0F;
    }
    bool isUnknown(WordIndex, std::string_view) const override { return false; }
};

class ContextModelFile {
public:
    ContextModelFile() {
        auto pattern = (std::filesystem::temp_directory_path() /
                        "fcitx-shuangpin-context-XXXXXX")
                           .string();
        std::vector<char> name(pattern.begin(), pattern.end());
        name.push_back('\0');
        const auto fd = mkstemp(name.data());
        FCITX_ASSERT(fd >= 0);
        close(fd);
        path_ = name.data();
        std::ofstream out(path_);
        out << "\\data\\\nngram 1=13\nngram 2=10\n\n\\1-grams:\n"
            << "-5\t<unk>\t0\n-1\t<s>\t0\n-1\t</s>\t0\n"
            << "-1\t开\t0\n-1\t点\t0\n-2\t电\t0\n-1.5\t店\t0\n"
            << "-3\t典\t0\n-2.5\t分\t0\n-1\t来\t0\n-1\t。\t0\n-8\t殿\t0\n"
            << "-2\t曾\t0\n"
            << "\n\\2-grams:\n"
            << "-3\t开 点\n-0.05\t开 电\n-1.5\t开 店\n"
            << "-0.01\t开 典\n-0.001\t开 殿\n"
            << "-0.4\t来 点\n-0.05\t来 电\n-1.5\t来 店\n"
            << "-0.01\t来 典\n-8\t来 殿\n\n\\end\\\n";
        out.close();
        FCITX_ASSERT(out);
    }

    ~ContextModelFile() { std::filesystem::remove(path_); }

    const std::string &path() const { return path_; }

private:
    std::string path_;
};

std::unique_ptr<PinyinIME> makeContextIME(const ContextModelFile &file) {
    auto ime = std::make_unique<PinyinIME>(
        std::make_unique<PinyinDictionary>(),
        std::make_unique<UserLanguageModel>(file.path().c_str()));
    ime->setShuangpinProfile(
        std::make_shared<ShuangpinProfile>(ShuangpinBuiltinProfile::Xiaohe));
    for (const auto &word : {"点", "电", "店", "典", "殿"}) {
        ime->dict()->addWord(PinyinDictionary::SystemDict, "dian", word);
    }
    return ime;
}

auto correctionIdentities(const std::vector<ShuangpinCorrection> &corrections) {
    std::vector<std::tuple<std::string, std::string, std::string>> result;
    for (const auto &correction : corrections) {
        result.emplace_back(correction.word, correction.encodedPinyin,
                            correction.code);
    }
    return result;
}

std::unique_ptr<PinyinIME> makeIME() {
    auto ime = std::make_unique<PinyinIME>(std::make_unique<PinyinDictionary>(),
                                           std::make_unique<TestModel>());
    ime->setShuangpinProfile(
        std::make_shared<ShuangpinProfile>(ShuangpinBuiltinProfile::Xiaohe));
    ime->dict()->addWord(PinyinDictionary::SystemDict, "fen", "分");
    ime->dict()->addWord(PinyinDictionary::SystemDict, "fen", "粉", -1.0F);
    ime->dict()->addWord(PinyinDictionary::SystemDict, "fan", "反");
    ime->dict()->addWord(PinyinDictionary::SystemDict, "dian", "点");
    ime->dict()->addWord(PinyinDictionary::SystemDict, "ni'hao'ma", "你好吗");
    ime->dict()->addWord(PinyinDictionary::SystemDict, "ni'hao'men", "你好们");
    return ime;
}

void testInvalidPairCorrection() {
    auto ime = makeIME();
    PinyinContext context(ime.get());
    context.setUseShuangpin(true);
    context.type("fm");
    const auto original = context.userInput();
    const auto flags = ime->fuzzyFlags();
    const auto correction = shuangpinCorrection(context);
    FCITX_ASSERT(correction);
    FCITX_ASSERT(correction->word == "点");
    FCITX_ASSERT(correction->code == "dm");
    FCITX_ASSERT(context.userInput() == original);
    FCITX_ASSERT(ime->fuzzyFlags() == flags);
    context.selectCustom(2, correction->word, correction->encodedPinyin);
    FCITX_ASSERT(context.selected());
    FCITX_ASSERT(context.selectedSentence() == "点");
    FCITX_ASSERT(context.selectedFullPinyin() == "dian");
    context.clear();
    context.type("fm");
    FCITX_ASSERT(context.selectSyllable(
        0, 1, PinyinSyllable(PinyinInitial::F, PinyinFinal::Invalid)));
    FCITX_ASSERT(!shuangpinCorrection(context));
    context.clear();
    context.type("ff");
    FCITX_ASSERT(!shuangpinCorrection(context));
    std::unordered_map<char, char> identity;
    for (char c = 'a'; c <= 'z'; ++c)
        identity[c] = c;
    context.setShuangpinProfile(
        std::make_shared<ShuangpinProfile>(*ime->shuangpinProfile(), identity));
    context.setUseShuangpin(true);
    context.type("fm");
    FCITX_ASSERT(!shuangpinCorrection(context));
    context.setShuangpinProfile(nullptr);
    context.clear();
    context.type("f");
    FCITX_ASSERT(!shuangpinCorrection(context));
    context.clear();
    context.type("fm");
    context.setCursor(1);
    FCITX_ASSERT(!shuangpinCorrection(context));
    context.clear();
    context.setUseShuangpin(false);
    context.type("fm");
    FCITX_ASSERT(!shuangpinCorrection(context));
}

void testFullInputRecoveryKeepsConsumptionAndPinyin() {
    auto ime = makeIME();
    const auto correctionProfile = std::make_shared<PinyinCorrectionProfile>(
        BuiltinPinyinCorrectionProfile::Qwerty);
    ime->setCorrectionProfile(correctionProfile);
    ime->setShuangpinProfile(std::make_shared<ShuangpinProfile>(
        ShuangpinBuiltinProfile::Xiaohe, correctionProfile.get()));
    ime->setFuzzyFlags(PinyinFuzzyFlag::Correction);
    ime->dict()->addWord(PinyinDictionary::SystemDict, "fei'yi", "非遗");
    ime->dict()->addWord(PinyinDictionary::SystemDict, "fei'fei", "菲菲");
    PinyinContext context(ime.get());
    context.setUseShuangpin(true);
    context.type("fqyi");
    const auto input = context.userInput();
    const auto offset = context.nextSyllableOffset();
    const auto corrections = shuangpinCorrections(context);
    FCITX_ASSERT(!corrections.empty());
    FCITX_ASSERT(corrections.size() <= 3);
    FCITX_ASSERT(corrections.front().word == "非遗");
    FCITX_ASSERT(corrections.front().inputLength == 4);
    FCITX_ASSERT(context.userInput() == input);
    FCITX_ASSERT(context.nextSyllableOffset() == offset);
    FCITX_ASSERT(context.cursor() == input.size());
    FCITX_ASSERT(!context.selected());
    FCITX_ASSERT(ime->model()->history().rawUnigramFrequency({"非遗", ""}) ==
                 0);
    context.selectCustom(corrections.front().inputLength,
                         corrections.front().word,
                         corrections.front().encodedPinyin);
    FCITX_ASSERT(context.selected());
    FCITX_ASSERT(context.selectedSentence() == "非遗");
    FCITX_ASSERT(context.selectedFullPinyin() == "fei'yi");

    context.clear();
    context.type("nifqyi");
    const auto ni = PinyinEncoder::encodeFullPinyin("ni");
    context.selectCustom(2, "你", std::string(ni.begin(), ni.end()));
    const auto suffix = shuangpinCorrections(context);
    FCITX_ASSERT(!suffix.empty());
    FCITX_ASSERT(suffix.front().word == "非遗");
    FCITX_ASSERT(suffix.front().inputLength == 4);
    context.selectCustom(suffix.front().inputLength, suffix.front().word,
                         suffix.front().encodedPinyin);
    FCITX_ASSERT(context.selected());
    FCITX_ASSERT(context.selectedSentence() == "你非遗");
    FCITX_ASSERT(context.selectedFullPinyin() == "ni'fei'yi");

    context.clear();
    context.type("fqfq");
    const auto doubleTypo = shuangpinCorrections(context);
    FCITX_ASSERT(!doubleTypo.empty());
    FCITX_ASSERT(doubleTypo.front().word == "菲菲");
    FCITX_ASSERT(doubleTypo.front().inputLength == 4);
    context.clear();
    context.type("fqfqfq");
    FCITX_ASSERT(shuangpinCorrections(context).empty());
    context.clear();
    context.type("fqy");
    FCITX_ASSERT(shuangpinCorrections(context).empty());
    context.clear();
    context.type("fqyi");
    context.setCursor(2);
    FCITX_ASSERT(shuangpinCorrections(context).empty());
    context.setCursor(context.size());
    FCITX_ASSERT(!shuangpinCorrections(context).empty());
    ime->setFuzzyFlags(PinyinFuzzyFlag::None);
    FCITX_ASSERT(shuangpinCorrections(context).empty());
    // The original two-key recovery is deliberately independent of that switch.
    context.clear();
    context.type("fm");
    FCITX_ASSERT(shuangpinCorrection(context));
    FCITX_ASSERT(shuangpinCorrection(context)->inputLength == 2);
}

void testRareInitialWords() {
    auto ime = makeIME();
    ime->setFuzzyFlags(PinyinFuzzyFlag::PartialSp);
    ime->setPartialSpLength(3);
    PinyinContext context(ime.get());
    context.setUseShuangpin(true);
    context.type("nhm");
    const auto find = [&](std::string_view word) -> const SentenceResult & {
        const auto &candidates = context.candidates();
        const auto it =
            std::ranges::find(candidates, word, &SentenceResult::toString);
        FCITX_ASSERT(it != candidates.end()) << word;
        return *it;
    };
    FCITX_ASSERT(isRareShuangpinAbbreviation(find("你好吗"), *ime->model()));
    FCITX_ASSERT(!isRareShuangpinAbbreviation(find("你好们"), *ime->model()));
    context.clear();
    context.type("nihcma");
    FCITX_ASSERT(!isRareShuangpinAbbreviation(find("你好吗"), *ime->model()));
    context.clear();
    context.type("nhm");
    const auto index = &find("你好吗") - context.candidates().data();
    context.select(index);
    context.learn();
    context.clear();
    context.type("nhm");
    FCITX_ASSERT(!isRareShuangpinAbbreviation(find("你好吗"), *ime->model()));
}

void testSelectionScopeAndPersistence() {
    const ShuangpinProfile xiaohe(ShuangpinBuiltinProfile::Xiaohe);
    const ShuangpinProfile microsoft(ShuangpinBuiltinProfile::MS);
    const PinyinCorrectionProfile correction(
        BuiltinPinyinCorrectionProfile::Qwerty);
    const ShuangpinProfile withCorrection(ShuangpinBuiltinProfile::Xiaohe,
                                          &correction);
    const auto scope = shuangpinSelectionScope(xiaohe, "fm");
    FCITX_ASSERT(scope == shuangpinSelectionScope(withCorrection, "fm"));
    FCITX_ASSERT(scope != shuangpinSelectionScope(xiaohe, "wm"));
    FCITX_ASSERT(scope != shuangpinSelectionScope(microsoft, "fm"));
    HistoryBigram history;
    FCITX_ASSERT(!hasShuangpinSelections(history, scope));
    learnShuangpinSelection(history, scope, "点");
    learnShuangpinSelection(history, scope, "点");
    FCITX_ASSERT(history.rawUnigramFrequency({"点", scope}) == 2);
    FCITX_ASSERT(hasShuangpinSelections(history, scope));
    FCITX_ASSERT(!hasShuangpinSelections(
        history, shuangpinSelectionScope(microsoft, "fm")));
    FCITX_ASSERT(history.rawUnigramFrequency(
                     {"点", shuangpinSelectionScope(xiaohe, "wm")}) == 0);
    std::stringstream saved;
    history.save(saved);
    HistoryBigram restored;
    restored.load(saved);
    FCITX_ASSERT(restored.rawUnigramFrequency({"点", scope}) == 2);
    FCITX_ASSERT(hasShuangpinSelections(restored, scope));
    restored.forget("点", scope);
    FCITX_ASSERT(!hasShuangpinSelections(restored, scope));
    FCITX_ASSERT(restored.rawUnigramFrequency({"点", scope}) == 0);
}

void testAdaptiveCandidateOrder() {
    const std::vector<std::string_view> words{"词一", "词二", "词三", "词四",
                                              "纠错", "词五", "词六"};
    const std::vector<size_t> initial{0, 1, 2, 3, 4, 5, 6};
    const std::string scope = "v1:test:fm";
    HistoryBigram history;
    FCITX_ASSERT(rankShuangpinCandidates(words, history, scope) == initial);
    learnShuangpinSelection(history, scope, "纠错");
    learnShuangpinSelection(history, scope, "纠错");
    FCITX_ASSERT(rankShuangpinCandidates(words, history, scope).front() == 4);
    for (const auto word : {"词一", "词二", "词三", "词四", "词五", "词六"}) {
        for (int i = 0; i < 3; ++i)
            learnShuangpinSelection(history, scope, word);
    }
    const auto ranked = rankShuangpinCandidates(words, history, scope);
    FCITX_ASSERT(ranked.back() == 4);
    FCITX_ASSERT((ranked == std::vector<size_t>{0, 1, 2, 3, 5, 6, 4}));
    FCITX_ASSERT(rankShuangpinCandidates(words, history, "v1:test:wm") ==
                 initial);
}

void testCorrectionPromotionIsGradual() {
    const std::vector<std::string_view> words{"词一", "词二", "词三", "词四",
                                              "纠错", "词五", "词六"};
    const std::vector<size_t> initial{0, 1, 2, 3, 4, 5, 6};
    const std::string scope = "v1:test:fm";
    HistoryBigram history;
    FCITX_ASSERT(rankShuangpinCandidates(words, history, scope, 4) == initial);
    for (int i = 0; i < 2; ++i) {
        learnShuangpinSelection(history, scope, "纠错");
        FCITX_ASSERT(rankShuangpinCandidates(words, history, scope, 4) ==
                     initial);
    }
    learnShuangpinSelection(history, scope, "纠错");
    FCITX_ASSERT((rankShuangpinCandidates(words, history, scope, 4) ==
                  std::vector<size_t>{0, 1, 2, 4, 3, 5, 6}));
    for (int i = 0; i < 2; ++i)
        learnShuangpinSelection(history, scope, "纠错");
    FCITX_ASSERT((rankShuangpinCandidates(words, history, scope, 4) ==
                  std::vector<size_t>{0, 1, 4, 2, 3, 5, 6}));
    for (int i = 0; i < 4; ++i)
        learnShuangpinSelection(history, scope, "纠错");
    FCITX_ASSERT((rankShuangpinCandidates(words, history, scope, 4) ==
                  std::vector<size_t>{0, 4, 1, 2, 3, 5, 6}));
    for (int i = 0; i < 512; ++i)
        learnShuangpinSelection(history, scope, "纠错");
    FCITX_ASSERT((rankShuangpinCandidates(words, history, scope, 4) ==
                  std::vector<size_t>{0, 4, 1, 2, 3, 5, 6}));
}

void testCorrectionCanFollowLearnedOrdinaryCandidates() {
    const std::vector<std::string_view> words{"词一", "词二", "词三", "词四",
                                              "纠错", "词五", "词六"};
    const std::string scope = "v1:test:fm";
    HistoryBigram history;
    for (int i = 0; i < 2; ++i)
        learnShuangpinSelection(history, scope, "纠错");
    for (const auto word : {"词一", "词二", "词三", "词四", "词五", "词六"}) {
        for (int i = 0; i < 3; ++i)
            learnShuangpinSelection(history, scope, word);
    }
    FCITX_ASSERT((rankShuangpinCandidates(words, history, scope, 4) ==
                  std::vector<size_t>{0, 1, 2, 3, 5, 6, 4}));
}

void testCorrectionRankingBoundaries() {
    const std::string scope = "v1:test:fm";
    HistoryBigram history;
    for (int i = 0; i < 512; ++i)
        learnShuangpinSelection(history, scope, "纠错");
    const std::vector<std::string_view> empty;
    FCITX_ASSERT(rankShuangpinCandidates(empty, history, scope, 0).empty());
    const std::vector<std::string_view> onlyCorrection{"纠错"};
    FCITX_ASSERT((rankShuangpinCandidates(onlyCorrection, history, scope, 0) ==
                  std::vector<size_t>{0}));
    const std::vector<std::string_view> lastCorrection{"合法", "纠错"};
    FCITX_ASSERT((rankShuangpinCandidates(lastCorrection, history, scope, 1) ==
                  std::vector<size_t>{0, 1}));
    const std::vector<std::string_view> firstCorrection{"纠错", "合法"};
    FCITX_ASSERT((rankShuangpinCandidates(firstCorrection, history, scope, 0) ==
                  std::vector<size_t>{1, 0}));
}

void testCorrectionRankingPersistsAndStaysScoped() {
    const ShuangpinProfile xiaohe(ShuangpinBuiltinProfile::Xiaohe);
    const ShuangpinProfile microsoft(ShuangpinBuiltinProfile::MS);
    const auto scope = shuangpinSelectionScope(xiaohe, "fm");
    const std::vector<std::string_view> words{"词一", "词二", "词三",
                                              "词四", "纠错", "词五"};
    const std::vector<size_t> initial{0, 1, 2, 3, 4, 5};
    HistoryBigram history;
    for (int i = 0; i < 2; ++i)
        learnShuangpinSelection(history, scope, "纠错");
    std::stringstream saved;
    history.save(saved);
    HistoryBigram restored;
    restored.load(saved);
    FCITX_ASSERT(rankShuangpinCandidates(words, restored, scope, 4) == initial);
    learnShuangpinSelection(restored, scope, "纠错");
    const auto promoted = rankShuangpinCandidates(words, restored, scope, 4);
    FCITX_ASSERT((promoted == std::vector<size_t>{0, 1, 2, 4, 3, 5}));
    FCITX_ASSERT(rankShuangpinCandidates(words, restored,
                                         shuangpinSelectionScope(xiaohe, "wm"),
                                         4) == initial);
    FCITX_ASSERT(rankShuangpinCandidates(
                     words, restored, shuangpinSelectionScope(microsoft, "fm"),
                     4) == initial);
    std::stringstream savedPromoted;
    restored.save(savedPromoted);
    HistoryBigram reloaded;
    reloaded.load(savedPromoted);
    FCITX_ASSERT(rankShuangpinCandidates(words, reloaded, scope, 4) ==
                 promoted);
    reloaded.forget("纠错", scope);
    FCITX_ASSERT(rankShuangpinCandidates(words, reloaded, scope, 4) == initial);
}

void testCorrectionUsesScopedChoicesInsteadOfGlobalHistory() {
    PinyinIME ime(std::make_unique<PinyinDictionary>(),
                  std::make_unique<UserLanguageModel>());
    ime.setShuangpinProfile(
        std::make_shared<ShuangpinProfile>(ShuangpinBuiltinProfile::Xiaohe));
    ime.dict()->addWord(PinyinDictionary::SystemDict, "dian", "点");
    ime.dict()->addWord(PinyinDictionary::SystemDict, "dian", "电");
    PinyinContext context(&ime);
    context.setUseShuangpin(true);
    context.type("fm");
    const auto initial = shuangpinCorrection(context);
    FCITX_ASSERT(initial && initial->word == "点");
    const auto pinyin = PinyinEncoder::encodeFullPinyin("dian");
    const std::string encoded(pinyin.begin(), pinyin.end());
    for (int i = 0; i < 8; ++i)
        ime.model()->history().addWithCode({{"电", encoded}});
    FCITX_ASSERT(shuangpinCorrection(context)->word == "点");
    HistoryBigram scoped;
    const auto scope = shuangpinSelectionScope(*ime.shuangpinProfile(), "fm");
    for (int i = 0; i < 2; ++i) {
        learnShuangpinSelection(scoped, scope, "电");
        FCITX_ASSERT(shuangpinCorrection(context, &scoped)->word == "点");
    }
    learnShuangpinSelection(scoped, scope, "电");
    FCITX_ASSERT(shuangpinCorrection(context, &scoped)->word == "电");
    learnShuangpinSelection(
        scoped, shuangpinSelectionScope(*ime.shuangpinProfile(), "wm"), "点");
    FCITX_ASSERT(shuangpinCorrection(context, &scoped)->word == "电");
    scoped.forget("电", scope);
    FCITX_ASSERT(shuangpinCorrection(context, &scoped)->word == "点");
}

void testTwoStageManualReset() {
    HistoryBigram history;
    ShuangpinCorrectionPlacements placements;
    const std::string scope = "v1:test:fm";
    const std::string otherScope = "v1:test:wm";
    FCITX_ASSERT(shuangpinCorrectionPlacement(history, placements, scope) ==
                 ShuangpinCorrectionPlacement::Default);
    learnShuangpinSelection(history, scope, "点");
    learnShuangpinSelection(history, scope, "父母");
    learnShuangpinSelection(history, otherScope, "钱");
    FCITX_ASSERT(resetShuangpinCorrection(history, placements, scope, "点") ==
                 ShuangpinCorrectionPlacement::Default);
    FCITX_ASSERT(hasShuangpinSelections(history, scope));
    FCITX_ASSERT(history.rawUnigramFrequency({"父母", scope}) == 1);
    FCITX_ASSERT(history.rawUnigramFrequency({"点", scope}) == 0);
    FCITX_ASSERT(shuangpinCorrectionPlacement(history, placements, scope) ==
                 ShuangpinCorrectionPlacement::Default);
    learnShuangpinSelection(history, scope, "父母");
    FCITX_ASSERT(shuangpinCorrectionPlacement(history, placements, scope) ==
                 ShuangpinCorrectionPlacement::Default);
    FCITX_ASSERT(resetShuangpinCorrection(history, placements, scope, "点") ==
                 ShuangpinCorrectionPlacement::Deferred);
    FCITX_ASSERT(resetShuangpinCorrection(history, placements, scope, "点") ==
                 ShuangpinCorrectionPlacement::Deferred);
    FCITX_ASSERT(
        shuangpinCorrectionPlacement(history, placements, otherScope) ==
        ShuangpinCorrectionPlacement::Adaptive);
    FCITX_ASSERT(history.rawUnigramFrequency({"钱", otherScope}) == 1);
    // An ordinary selection cannot cancel an explicit page restriction.
    learnShuangpinSelection(history, scope, "分明");
    FCITX_ASSERT(shuangpinCorrectionPlacement(history, placements, scope) ==
                 ShuangpinCorrectionPlacement::Deferred);
    // Manual placement is independent from the bounded frequency history.
    history.clear();
    FCITX_ASSERT(shuangpinCorrectionPlacement(history, placements, scope) ==
                 ShuangpinCorrectionPlacement::Deferred);
    placements.erase(scope);
    learnShuangpinSelection(history, scope, "点");
    FCITX_ASSERT(shuangpinCorrectionPlacement(history, placements, scope) ==
                 ShuangpinCorrectionPlacement::Adaptive);
}

void testResetWithoutCorrectionLearningDefersImmediately() {
    HistoryBigram history;
    ShuangpinCorrectionPlacements placements;
    const std::string scope = "v1:test:fm";
    FCITX_ASSERT(resetShuangpinCorrection(history, placements, scope, "点") ==
                 ShuangpinCorrectionPlacement::Deferred);
    placements.clear();
    learnShuangpinSelection(history, scope, "父母");
    FCITX_ASSERT(resetShuangpinCorrection(history, placements, scope, "点") ==
                 ShuangpinCorrectionPlacement::Deferred);
    learnShuangpinSelection(history, scope, "点");
    resetShuangpinCorrection(history, placements, "", "点");
    FCITX_ASSERT(history.rawUnigramFrequency({"点", scope}) == 1);
}

void testFirstResetOffHomepageFollowsAllLegalCandidates() {
    HistoryBigram history;
    ShuangpinCorrectionPlacements placements;
    const std::string scope = "v1:test:fm";
    const std::string otherScope = "v1:test:wm";
    learnShuangpinSelection(history, scope, "点");
    learnShuangpinSelection(history, scope, "父母");
    learnShuangpinSelection(history, otherScope, "钱");
    FCITX_ASSERT(
        resetShuangpinCorrection(history, placements, scope, "点", false) ==
        ShuangpinCorrectionPlacement::AfterLegal);
    FCITX_ASSERT(history.rawUnigramFrequency({"点", scope}) == 0);
    FCITX_ASSERT(history.rawUnigramFrequency({"父母", scope}) == 1);
    FCITX_ASSERT(history.rawUnigramFrequency({"钱", otherScope}) == 1);
    // Repeated resets, ordinary learning and aging cannot promote this
    // override.
    FCITX_ASSERT(
        resetShuangpinCorrection(history, placements, scope, "点", true) ==
        ShuangpinCorrectionPlacement::AfterLegal);
    learnShuangpinSelection(history, scope, "分明");
    FCITX_ASSERT(shuangpinCorrectionPlacement(history, placements, scope) ==
                 ShuangpinCorrectionPlacement::AfterLegal);
    history.clear();
    FCITX_ASSERT(shuangpinCorrectionPlacement(history, placements, scope) ==
                 ShuangpinCorrectionPlacement::AfterLegal);
    // A correction can already be off-page solely because legal words learned.
    placements.clear();
    learnShuangpinSelection(history, scope, "父母");
    FCITX_ASSERT(
        resetShuangpinCorrection(history, placements, scope, "点", false) ==
        ShuangpinCorrectionPlacement::AfterLegal);
    placements.erase(scope);
    learnShuangpinSelection(history, scope, "点");
    FCITX_ASSERT(shuangpinCorrectionPlacement(history, placements, scope) ==
                 ShuangpinCorrectionPlacement::Adaptive);
}

void testContextualCorrectionRetainsFrequencyFallback() {
    ContextModelFile file;
    auto ime = makeContextIME(file);
    PinyinContext context(ime.get());
    context.setUseShuangpin(true);
    context.type("fm");
    auto corrections = shuangpinCorrections(context);
    FCITX_ASSERT(corrections.size() == 1);
    FCITX_ASSERT(corrections.front().word == "点");
    context.setContextWords({"开"});
    const auto input = context.userInput();
    const auto words = context.contextWords();
    const auto flags = ime->fuzzyFlags();
    corrections = shuangpinCorrections(context);
    FCITX_ASSERT(corrections.front().word == "电");
    FCITX_ASSERT(std::ranges::any_of(corrections, [](const auto &candidate) {
        return candidate.word == "点";
    }));
    // An extreme contextual probability cannot rescue an eight-log rare word.
    FCITX_ASSERT(std::ranges::none_of(corrections, [](const auto &candidate) {
        return candidate.word == "殿";
    }));
    FCITX_ASSERT(context.userInput() == input);
    FCITX_ASSERT(context.contextWords() == words);
    FCITX_ASSERT(ime->fuzzyFlags() == flags);
    FCITX_ASSERT(shuangpinCorrection(context)->word ==
                 corrections.front().word);
}

void testContextFallbackAndSelectedPrefix() {
    ContextModelFile file;
    auto ime = makeContextIME(file);
    PinyinContext context(ime.get());
    context.setUseShuangpin(true);
    context.type("fm");
    for (const auto &words : std::vector<std::vector<std::string>>{
             {}, {"开", "unknown-word"}, {"开", "。"}, {"开", "."}}) {
        context.setContextWords(words);
        FCITX_ASSERT(shuangpinCorrection(context)->word == "点");
    }
    context.setContextWords({"unknown-word", "开"});
    FCITX_ASSERT(shuangpinCorrection(context)->word == "电");
    context.clearContextWords();
    context.clear();
    context.type("kdfm");
    const auto encoded = PinyinEncoder::encodeFullPinyin("kai");
    context.selectCustom(2, "开", std::string(encoded.begin(), encoded.end()));
    FCITX_ASSERT(shuangpinCorrection(context)->word == "电");
    FCITX_ASSERT(context.selectedSentence() == "开");
    FCITX_ASSERT(context.userInput() == "kdfm");
}

void testContextDoesNotUseGlobalPersonalHistory() {
    ContextModelFile file;
    auto ime = makeContextIME(file);
    PinyinContext context(ime.get());
    context.setUseShuangpin(true);
    context.type("fm");
    context.setContextWords({"开"});
    const auto before = correctionIdentities(shuangpinCorrections(context));
    for (int i = 0; i < 20; ++i) {
        ime->model()->history().add({"开", "点"});
    }
    ime->model()->setUseOnlyUnigram(true);
    FCITX_ASSERT(correctionIdentities(shuangpinCorrections(context)) == before);
    FCITX_ASSERT(ime->model()->history().rawUnigramFrequency({"点", ""}) == 20);
    HistoryBigram scoped;
    const auto scope = shuangpinSelectionScope(*ime->shuangpinProfile(), "fm");
    for (int i = 0; i < 2; ++i) {
        learnShuangpinSelection(scoped, scope, "点");
        FCITX_ASSERT(shuangpinCorrection(context, &scoped)->word == "电");
    }
    for (int i = 2; i < 17; ++i)
        learnShuangpinSelection(scoped, scope, "点");
    FCITX_ASSERT(shuangpinCorrection(context, &scoped)->word == "点");
    HistoryBigram otherCode;
    learnShuangpinSelection(
        otherCode, shuangpinSelectionScope(*ime->shuangpinProfile(), "wm"),
        "点");
    FCITX_ASSERT(shuangpinCorrection(context, &otherCode)->word == "电");
}

void testCorrectionHistoryBoostIsBounded() {
    ContextModelFile file;
    auto ime = makeContextIME(file);
    PinyinContext context(ime.get());
    context.setUseShuangpin(true);
    context.type("fm");
    const auto scope = shuangpinSelectionScope(*ime->shuangpinProfile(), "fm");
    HistoryBigram lowerFrequency;
    HistoryBigram rare;
    for (int i = 0; i < 512; ++i) {
        learnShuangpinSelection(lowerFrequency, scope, "典");
        learnShuangpinSelection(rare, scope, "殿");
    }
    // An uncapped logarithmic bonus would overcome the two-log prior gap.
    FCITX_ASSERT(shuangpinCorrection(context, &lowerFrequency)->word == "点");
    FCITX_ASSERT(shuangpinCorrection(context, &rare)->word == "点");
    context.setContextWords({"开"});
    const auto corrections = shuangpinCorrections(context, &rare);
    FCITX_ASSERT(!corrections.empty());
    FCITX_ASSERT(corrections.front().word == "电");
    FCITX_ASSERT(std::ranges::none_of(corrections, [](const auto &candidate) {
        return candidate.word == "殿";
    }));
}

void testCorrectionPoolIsDistinctBoundedAndKeepsPinyinBinding() {
    ContextModelFile file;
    auto ime = makeContextIME(file);
    PinyinContext context(ime.get());
    context.setUseShuangpin(true);
    context.type("fm");
    context.setContextWords({"来"});
    const auto corrections = shuangpinCorrections(context);
    FCITX_ASSERT(corrections.size() == 3);
    FCITX_ASSERT(corrections.front().word == "电");
    std::unordered_set<std::string> seen;
    for (const auto &candidate : corrections) {
        FCITX_ASSERT(seen.insert(candidate.word).second);
    }
    const auto expected = PinyinEncoder::encodeFullPinyin("dian");
    FCITX_ASSERT(corrections.front().encodedPinyin ==
                 std::string(expected.begin(), expected.end()));
    FCITX_ASSERT(corrections.front().code == "dm");
    for (int i = 0; i < 5; ++i) {
        FCITX_ASSERT(correctionIdentities(shuangpinCorrections(context)) ==
                     correctionIdentities(corrections));
    }
    context.selectCustom(2, corrections.front().word,
                         corrections.front().encodedPinyin);
    FCITX_ASSERT(context.selectedFullPinyin() == "dian");

    // xg is invalid, while both neighboring cg/zg encode readings of 曾.
    ime->dict()->addWord(PinyinDictionary::SystemDict, "ceng", "曾");
    ime->dict()->addWord(PinyinDictionary::SystemDict, "zeng", "曾");
    context.clear();
    context.clearContextWords();
    context.type("xg");
    const auto polyphonic = shuangpinCorrections(context);
    FCITX_ASSERT(polyphonic.size() == 2);
    FCITX_ASSERT(polyphonic.front().word == "曾");
    FCITX_ASSERT(polyphonic[0].encodedPinyin != polyphonic[1].encodedPinyin);
    FCITX_ASSERT(polyphonic[1].word == "曾");
    FCITX_ASSERT(polyphonic.front().code == "cg" ||
                 polyphonic.front().code == "zg");
    const std::string reading =
        polyphonic.front().code == "cg" ? "ceng" : "zeng";
    const auto encodedReading = PinyinEncoder::encodeFullPinyin(reading);
    FCITX_ASSERT(polyphonic.front().encodedPinyin ==
                 std::string(encodedReading.begin(), encodedReading.end()));
    FCITX_ASSERT(correctionIdentities(shuangpinCorrections(context)) ==
                 correctionIdentities(polyphonic));
    context.selectCustom(2, polyphonic.front().word,
                         polyphonic.front().encodedPinyin);
    FCITX_ASSERT(context.selectedFullPinyin() == reading);
}

} // namespace

int main() {
    testInvalidPairCorrection();
    testFullInputRecoveryKeepsConsumptionAndPinyin();
    testRareInitialWords();
    testSelectionScopeAndPersistence();
    testAdaptiveCandidateOrder();
    testCorrectionPromotionIsGradual();
    testCorrectionCanFollowLearnedOrdinaryCandidates();
    testCorrectionRankingBoundaries();
    testCorrectionRankingPersistsAndStaysScoped();
    testCorrectionUsesScopedChoicesInsteadOfGlobalHistory();
    testTwoStageManualReset();
    testResetWithoutCorrectionLearningDefersImmediately();
    testFirstResetOffHomepageFollowsAllLegalCandidates();
    testContextualCorrectionRetainsFrequencyFallback();
    testContextFallbackAndSelectedPrefix();
    testContextDoesNotUseGlobalPersonalHistory();
    testCorrectionHistoryBoostIsBounded();
    testCorrectionPoolIsDistinctBoundedAndKeepsPinyinBinding();
}
