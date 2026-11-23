/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "testdir.h"
#include "testfrontend_public.h"
#include "../im/pinyin/pinyinviewport_public.h"
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/standardpaths.h>
#include <fcitx-utils/testing.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/candidateaction.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace fcitx;

namespace {
const CandidateWord *find(InputContext *ic, const std::string &text) {
    const auto list = ic->inputPanel().candidateList();
    for (int index = 0; index < list->toBulk()->totalSize(); ++index) {
        const auto &word = list->toBulk()->candidateFromAll(index);
        if (word.text().toString() == text) return &word;
    }
    FCITX_ASSERT(false) << "Missing viewport candidate " << text;
    return nullptr;
}

std::vector<const CandidateWord *> roots(InputContext *ic) {
    std::vector<const CandidateWord *> result;
    const auto list = ic->inputPanel().candidateList();
    for (int index = 0; index < list->toBulk()->totalSize(); ++index) {
        const auto *word = &list->toBulk()->candidateFromAll(index);
        if (!word->sourceCandidate()) result.push_back(word);
    }
    return result;
}

void verifyPolicy(InputContext *ic, int capacity) {
    auto list = ic->inputPanel().candidateList();
    auto *bulk = list->toBulk();
    auto *paged = list->toPageable();
    const int originalPage = paged->currentPage();
    std::unordered_set<const CandidateWord *> first;
    paged->setPage(0);
    size_t emojis = 0;
    for (int index = 0; index < list->size(); ++index) {
        const auto *word = &list->candidate(index);
        first.insert(word);
        emojis += word->sourceCandidate() != nullptr;
    }
    FCITX_ASSERT(emojis <= 1);
    FCITX_ASSERT(list->size() <= capacity);
    std::unordered_set<const CandidateWord *> seen;
    for (int page = 0; page < paged->totalPages(); ++page) {
        paged->setPage(page);
        FCITX_ASSERT(list->size() <= capacity);
        for (int index = 0; index < list->size(); ++index) {
            const auto *word = &list->candidate(index);
            FCITX_ASSERT(seen.insert(word).second);
        }
    }
    FCITX_ASSERT(seen.size() == static_cast<size_t>(bulk->totalSize()));
    std::unordered_map<const CandidateWord *, int> last;
    for (int index = 0; index < bulk->totalSize(); ++index) {
        const auto &word = bulk->candidateFromAll(index);
        const auto *source = word.sourceCandidate();
        if (!source) continue;
        if (auto previous = last.find(source); previous != last.end())
            FCITX_ASSERT(previous->second + 1 == index);
        last[source] = index;
        if (!first.contains(&word)) {
            FCITX_ASSERT(word.comment().toString() != "…");
            for (const auto &action : list->toActionable()->candidateActions(word))
                FCITX_ASSERT(action.icon() != "folded-symbol");
        }
    }
    paged->setPage(originalPage);
}
} // namespace

int main() {
    setupTestingEnvironment(
        TESTING_BINARY_DIR, {"bin"},
        {TESTING_BINARY_DIR "/test", TESTING_BINARY_DIR "/im",
         TESTING_BINARY_DIR "/modules", TESTING_SOURCE_DIR "/modules",
         TESTING_SOURCE_DIR "/im", StandardPaths::fcitxPath("pkgdatadir")});
    char arg0[] = "testpinyinviewport";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testim,testfrontend,pinyin,punctuation,pinyinhelper,spell,quickphrase";
    char *argv[] = {arg0, arg1, arg2};
    Instance instance(FCITX_ARRAY_SIZE(argv), argv);
    instance.addonManager().registerDefaultLoader(nullptr);
    instance.eventDispatcher().schedule([&instance] {
        auto *pinyin = instance.addonManager().addon("pinyin", true);
        auto group = instance.inputMethodManager().currentGroup();
        group.inputMethodList().clear();
        group.inputMethodList().push_back(InputMethodGroupItem("keyboard-us"));
        group.inputMethodList().push_back(InputMethodGroupItem("pinyin"));
        group.inputMethodList().push_back(InputMethodGroupItem("shuangpin"));
        group.setDefaultInputMethod("");
        instance.inputMethodManager().setGroup(std::move(group));
        auto *frontend = instance.addonManager().addon("testfrontend", true);
        const auto uuid = frontend->call<ITestFrontend::createInputContext>("candidate-viewport");
        auto *ic = instance.inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        ic->setCapabilityFlags(CapabilityFlags(CapabilityFlag::Sensitive));
        instance.setCurrentInputMethod(ic, "pinyin", true);
        const auto configure = [&instance, ic](bool symbols = true) {
            RawConfig config;
            config.setValueByPath("PageSize", "3");
            config.setValueByPath("SymbolsEnabled", symbols ? "True" : "False");
            config.setValueByPath("FoldSymbolCandidates", "True");
            config.setValueByPath("Prediction", "False");
            config.setValueByPath("ShuangpinProfile", "Xiaohe");
            config.setValueByPath("PartialSpLength", "2");
            config.setValueByPath("CompactTypoDefaultApplied", "True");
            instance.inputMethodEngine(ic)->setConfigForInputMethod(
                *instance.inputMethodEntry(ic), config);
            ic->reset();
        };
        const auto type = [frontend, uuid](const std::string &text) {
            for (const char key : text)
                frontend->call<ITestFrontend::keyEvent>(uuid, Key(static_cast<KeySym>(key)), false);
        };
        configure();
        type("kaishi");
        FCITX_ASSERT(pinyin->call<IPinyinEngine::candidateViewportPolicyEnabled>(ic));
        verifyPolicy(ic, 3);
        const auto *begin = find(ic, "开始");
        const auto beforeRoots = roots(ic);
        std::vector<const CandidateWord *> prefix = {begin};
        FCITX_ASSERT(pinyin->call<IPinyinEngine::setCandidateViewport>(ic, 1, prefix, nullptr, false));
        FCITX_ASSERT(roots(ic) == beforeRoots);
        auto list = ic->inputPanel().candidateList();
        FCITX_ASSERT(list->size() == 1 && &list->candidate(0) == begin);
        const auto *play = find(ic, "▶️");
        const auto *on = find(ic, "🔛");
        FCITX_ASSERT(play != on && play->sourceCandidate() == begin && on->sourceCandidate() == begin);
        FCITX_ASSERT(play->comment().toString() != "…");
        verifyPolicy(ic, 1);
        prefix = {begin, play};
        const auto beforeGeneration = pinyin->call<IPinyinEngine::candidateViewportGeneration>(ic);
        FCITX_ASSERT(pinyin->call<IPinyinEngine::setCandidateViewport>(ic, 2, prefix, begin, false));
        const auto changedGeneration = pinyin->call<IPinyinEngine::candidateViewportGeneration>(ic);
        FCITX_ASSERT(changedGeneration > beforeGeneration);
        list = ic->inputPanel().candidateList();
        const auto *folded = &list->candidate(1);
        FCITX_ASSERT(folded->comment().toString() == "…");
        prefix = {&list->candidate(0), folded};
        FCITX_ASSERT(pinyin->call<IPinyinEngine::setCandidateViewport>(ic, 2, prefix, begin, false));
        FCITX_ASSERT(&list->candidate(1) == folded); // No rebuild on duplicate request.
        FCITX_ASSERT(pinyin->call<IPinyinEngine::candidateViewportGeneration>(ic) == changedGeneration);
        verifyPolicy(ic, 2);
        RawConfig saved;
        instance.inputMethodEngine(ic)->getConfigForInputMethod(*instance.inputMethodEntry(ic))->save(saved);
        FCITX_ASSERT(saved.valueByPath("PageSize") && *saved.valueByPath("PageSize") == "3");
        FCITX_ASSERT(pinyin->call<IPinyinEngine::setCandidateViewport>(ic, 0, std::vector<const CandidateWord *>{}, nullptr, true));

        // Late ordinary insertion must not push a folded head onto floating page two.
        list->toModifiable()->insert(0, std::make_unique<DisplayOnlyCandidateWord>(Text("甲")));
        list->toModifiable()->insert(1, std::make_unique<DisplayOnlyCandidateWord>(Text("乙")));
        FCITX_ASSERT(!pinyin->call<IPinyinEngine::candidateViewportPolicyEnabled>(ic));
        const auto dirtyGeneration = pinyin->call<IPinyinEngine::candidateViewportGeneration>(ic);
        FCITX_ASSERT(pinyin->call<IPinyinEngine::candidateViewportGeneration>(ic) == dirtyGeneration);
        InputContextFlushUIEvent flush(UserInterfaceComponent::InputPanel, ic);
        instance.postEvent(flush);
        FCITX_ASSERT(pinyin->call<IPinyinEngine::candidateViewportPolicyEnabled>(ic));
        FCITX_ASSERT(pinyin->call<IPinyinEngine::candidateViewportGeneration>(ic) > dirtyGeneration);
        list = ic->inputPanel().candidateList();
        FCITX_ASSERT(list->size() == 3);
        FCITX_ASSERT(list->candidate(0).text().toString() == "甲");
        FCITX_ASSERT(list->candidate(1).text().toString() == "乙");
        FCITX_ASSERT(list->candidate(2).text().toString() == "开始");
        verifyPolicy(ic, 3);

        // Sparse actual first page: the folded head's bulk index is outside H.
        const auto *first = find(ic, "甲");
        play = find(ic, "▶️");
        prefix = {first, begin, play};
        FCITX_ASSERT(pinyin->call<IPinyinEngine::setCandidateViewport>(ic, 3, prefix, begin, false));
        list = ic->inputPanel().candidateList();
        FCITX_ASSERT(&list->candidate(0) == first && &list->candidate(1) == begin);
        FCITX_ASSERT(list->candidate(2).comment().toString() == "…");
        int headIndex = -1;
        for (int index = 0; index < list->toBulk()->totalSize(); ++index)
            if (&list->toBulk()->candidateFromAll(index) == &list->candidate(2)) headIndex = index;
        FCITX_ASSERT(headIndex >= 3);
        verifyPolicy(ic, 3);
        list->toBulkCursor()->setGlobalCursorIndex(0);
        list->toCursorMovable()->nextCandidate();
        FCITX_ASSERT(&list->candidate(list->cursorIndex()) == begin);
        list->toCursorMovable()->nextCandidate();
        FCITX_ASSERT(list->cursorIndex() == 2);
        const auto actions = list->toActionable()->candidateActions(list->candidate(2));
        FCITX_ASSERT(actions.size() == 1 && actions[0].text() == "🔛");
        frontend->call<ITestFrontend::pushCommitExpectation>("🔛");
        list->toActionable()->triggerAction(list->candidate(2), actions[0].id());
        FCITX_ASSERT(!ic->inputPanel().candidateList());

        // Filtering removes earlier phrases, but must not promote multiple emoji runs.
        instance.setCurrentInputMethod(ic, "shuangpin", true);
        configure();
        type("ks");
        FCITX_ASSERT(pinyin->call<IPinyinEngine::setCandidateViewport>(ic, 0, std::vector<const CandidateWord *>{}, nullptr, true));
        list = ic->inputPanel().candidateList();
        const auto *tabbed = list->toTabbed();
        int single = 0;
        for (const auto &action : list->toTabbed()->tabActions())
            if (action.text() == "单字") single = action.id();
        list->toTabbed()->triggerTabAction(single);
        verifyPolicy(ic, 3);
        list = ic->inputPanel().candidateList();
        FCITX_ASSERT(list->toTabbed() == tabbed);
        bool checked = false;
        for (const auto &action : list->toTabbed()->tabActions())
            if (action.text() == "单字") checked = action.isChecked();
        FCITX_ASSERT(checked);
        prefix = {&list->candidate(0)};
        FCITX_ASSERT(pinyin->call<IPinyinEngine::setCandidateViewport>(ic, 1, prefix, nullptr, false));
        FCITX_ASSERT(ic->inputPanel().candidateList()->toTabbed() == tabbed);
        for (const auto &action : list->toTabbed()->tabActions())
            if (action.text() == "单字") FCITX_ASSERT(action.isChecked());
        verifyPolicy(ic, 1);
        auto foreign = std::make_unique<DisplayOnlyCandidateWord>(Text("foreign"));
        prefix = {foreign.get()};
        FCITX_ASSERT(!pinyin->call<IPinyinEngine::setCandidateViewport>(ic, 1, prefix, nullptr, false));

        // The cloud consumer chains remove(0) + move(duplicate - 1, 0).
        // Generated children must not change that one-slot removal or lose
        // the source object's lifetime while the pending update is normalized.
        ic->reset();
        instance.setCurrentInputMethod(ic, "pinyin", true);
        configure();
        type("kaishi");
        FCITX_ASSERT(pinyin->call<IPinyinEngine::setCandidateViewport>(
            ic, 0, std::vector<const CandidateWord *>{}, nullptr, true));
        list = ic->inputPanel().candidateList();
        begin = find(ic, "开始");
        int beginIndex = -1;
        for (int index = 0; index < list->toBulk()->totalSize(); ++index)
            if (&list->toBulk()->candidateFromAll(index) == begin) beginIndex = index;
        if (beginIndex != 0) {
            list->toModifiable()->move(beginIndex, 0);
            instance.postEvent(flush);
        }
        const CandidateWord *duplicate = nullptr;
        int duplicateIndex = -1;
        for (int index = 1; index < list->toBulk()->totalSize(); ++index) {
            const auto *word = &list->toBulk()->candidateFromAll(index);
            if (!word->sourceCandidate()) { duplicate = word; duplicateIndex = index; break; }
        }
        FCITX_ASSERT(duplicate && duplicateIndex > 0);
        const int oldTotal = list->toBulk()->totalSize();
        list->toModifiable()->remove(0);
        FCITX_ASSERT(list->toBulk()->totalSize() == oldTotal - 1);
        FCITX_ASSERT(begin->text().toString() == "开始");
        list->toModifiable()->move(duplicateIndex - 1, 0);
        instance.postEvent(flush);
        FCITX_ASSERT(&list->candidate(0) == duplicate);
        verifyPolicy(ic, 3);

        // Deferred abbreviation roots keep their symbol children off the
        // automatic first page, rather than leaving orphaned symbols there.
        ic->reset();
        instance.setCurrentInputMethod(ic, "shuangpin", true);
        configure();
        type("bq");
        list = ic->inputPanel().candidateList();
        for (int index = 0; index < list->size(); ++index) {
            const auto *source = list->candidate(index).sourceCandidate();
            FCITX_ASSERT(!source || !source->isAbbreviation() ||
                         source->isBestAbbreviation());
            if (source) {
                bool sourceVisible = false;
                for (int ordinal = 0; ordinal < list->size(); ++ordinal)
                    sourceVisible |= &list->candidate(ordinal) == source;
                FCITX_ASSERT(sourceVisible);
            }
        }
        verifyPolicy(ic, 3);

        // With symbols disabled, abbreviation paging still maps sparse native
        // indices; promoting a page hint must never mutate the bulk order.
        ic->reset();
        instance.setCurrentInputMethod(ic, "shuangpin", true);
        configure(false);
        type("wum");
        FCITX_ASSERT(pinyin->call<IPinyinEngine::candidateViewportPolicyEnabled>(ic));
        list = ic->inputPanel().candidateList();
        const CandidateWord *bestAbbreviation = nullptr;
        for (int index = 0; index < list->toBulk()->totalSize(); ++index) {
            const auto *word = &list->toBulk()->candidateFromAll(index);
            if (word->isBestAbbreviation()) bestAbbreviation = word;
        }
        FCITX_ASSERT(bestAbbreviation);
        const auto abbreviationText = bestAbbreviation->text().toString();
        for (int index = 0; index < 6; ++index)
            list->toModifiable()->insert(index,
                std::make_unique<DisplayOnlyCandidateWord>(Text("prefix" + std::to_string(index))));
        instance.postEvent(flush);
        list = ic->inputPanel().candidateList();
        const auto bulkBefore = roots(ic);
        FCITX_ASSERT(std::find(bulkBefore.begin(), bulkBefore.begin() + 3,
                              bestAbbreviation) == bulkBefore.begin() + 3);
        bool abbreviationOnPage = false;
        for (int index = 0; index < list->size(); ++index) {
            abbreviationOnPage |= &list->candidate(index) == bestAbbreviation;
            FCITX_ASSERT(!list->candidate(index).isAbbreviation() ||
                         list->candidate(index).isBestAbbreviation());
        }
        FCITX_ASSERT(abbreviationOnPage);
        verifyPolicy(ic, 3);
        prefix = {bulkBefore.front(), bestAbbreviation};
        FCITX_ASSERT(pinyin->call<IPinyinEngine::setCandidateViewport>(ic, 2, prefix, nullptr, false));
        FCITX_ASSERT(roots(ic) == bulkBefore);
        FCITX_ASSERT(&list->candidate(1) == bestAbbreviation);
        verifyPolicy(ic, 2);
        const auto bestIndex = std::find(bulkBefore.begin(), bulkBefore.end(), bestAbbreviation) - bulkBefore.begin();
        FCITX_ASSERT(bestIndex >= 6);
        FCITX_ASSERT(&list->toBulk()->candidateFromAll(bestIndex) == bestAbbreviation);
        frontend->call<ITestFrontend::pushCommitExpectation>(abbreviationText);
        list->toBulk()->candidateFromAll(bestIndex).select(ic);
        FCITX_ASSERT(!ic->inputPanel().candidateList());

        // An invalid pair keeps vowel-initial abbreviations and all of its
        // correction readings, without exposing single-key prefix candidates.
        configure(false);
        RawConfig invalidPairConfig;
        instance.inputMethodEngine(ic)->getConfigForInputMethod(
            *instance.inputMethodEntry(ic))->save(invalidPairConfig);
        invalidPairConfig.setValueByPath("Fuzzy/Correction", "QWERTY");
        invalidPairConfig.setValueByPath("PinyinAnnotation", "Always show");
        instance.inputMethodEngine(ic)->setConfigForInputMethod(
            *instance.inputMethodEntry(ic), invalidPairConfig);
        ic->reset();
        type("qo");
        list = ic->inputPanel().candidateList();
        FCITX_ASSERT(list && list->toBulk() && list->toTabbed());
        std::unordered_set<std::string> correctionReadings;
        for (const auto &action : list->toTabbed()->tabActions()) {
            if (action.text() == "qi" || action.text() == "qie" ||
                action.text() == "wo") {
                correctionReadings.insert(action.text());
            }
        }
        FCITX_ASSERT(correctionReadings.size() == 3);
        std::unordered_set<std::string> candidateReadings;
        bestAbbreviation = nullptr;
        int invalidPairAbbreviationIndex = -1;
        size_t singleCandidates = 0;
        for (int index = 0; index < list->toBulk()->totalSize(); ++index) {
            const auto &word = list->toBulk()->candidateFromAll(index);
            if (word.isBestAbbreviation()) {
                FCITX_ASSERT(!bestAbbreviation);
                bestAbbreviation = &word;
                invalidPairAbbreviationIndex = index;
            }
            if (utf8::length(word.text().toString()) != 1) continue;
            const auto reading = word.comment().toString();
            FCITX_ASSERT(reading == "(qi)" || reading == "(qie)" ||
                         reading == "(wo)") << word.text() << " " << reading;
            candidateReadings.insert(reading);
            ++singleCandidates;
        }
        FCITX_ASSERT(candidateReadings.size() == correctionReadings.size());
        FCITX_ASSERT(singleCandidates > 3);
        FCITX_ASSERT(bestAbbreviation);
        abbreviationOnPage = false;
        for (int index = 0; index < list->size(); ++index)
            abbreviationOnPage |= &list->candidate(index) == bestAbbreviation;
        FCITX_ASSERT(abbreviationOnPage);
        verifyPolicy(ic, 3);
        frontend->call<ITestFrontend::pushCommitExpectation>(
            bestAbbreviation->text().toString());
        list->toBulk()->candidateFromAll(invalidPairAbbreviationIndex).select(ic);
        FCITX_ASSERT(!ic->inputPanel().candidateList());
        instance.exit();
    });
    instance.exec();
}
