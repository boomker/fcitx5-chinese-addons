/*
 * SPDX-FileCopyrightText: 2020~2020 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#include "testdir.h"
#include "testfrontend_public.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/charutils.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/eventloopinterface.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx-utils/standardpaths.h>
#include <fcitx-utils/testing.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidateaction.h>
#include <fcitx/globalconfig.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace fcitx;

namespace {

std::unique_ptr<EventSourceTime> endTestEvent;
void testPunctuationPart2(Instance *instance);
void confirmGroupedSyllable(InputContext *ic, std::string_view syllable);

int findCandidate(InputContext *ic, std::string_view word) {
    auto candList = ic->inputPanel().candidateList();
    for (int i = 0; i < candList->toBulk()->totalSize(); i++) {
        const auto &candidate = candList->toBulk()->candidateFromAll(i);
        if (candidate.text().toString() == word) {
            return i;
        }
    }
    return -1;
}

int findCandidateOrDie(InputContext *ic, std::string_view word) {
    auto index = findCandidate(ic, word);
    FCITX_ASSERT(index >= 0) << "Failed to find candidate " << word;
    return index;
}

void findAndSelectCandidate(InputContext *ic, std::string_view word) {
    auto candList = ic->inputPanel().candidateList();
    candList->toBulk()
        ->candidateFromAll(findCandidateOrDie(ic, word))
        .select(ic);
}

void sendControlSpace(AddonInstance *testfrontend, InputContext *ic) {
    for (int i = 0; i < 2; i++) {
        testfrontend->call<ITestFrontend::keyEvent>(ic->uuid(),
                                                    Key("Control_L"), false);
        testfrontend->call<ITestFrontend::keyEvent>(
            ic->uuid(), Key("Control+space"), false);
        testfrontend->call<ITestFrontend::keyEvent>(ic->uuid(),
                                                    Key("Control+space"), true);
        testfrontend->call<ITestFrontend::keyEvent>(
            ic->uuid(), Key("Control+Control_L"), true);
        ic->reset();
    }
}

void setup(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        RawConfig hotkeys;
        hotkeys.setValueByPath("Hotkey/TriggerKeys/0", "Control+space");
        instance->globalConfig().load(hotkeys, true);
        auto *pinyin = instance->addonManager().addon("pinyin", true);
        FCITX_ASSERT(pinyin);
        RawConfig pinyinDefaults;
        pinyinDefaults.setValueByPath("PartialSpLength", "2");
        pinyinDefaults.setValueByPath("Fuzzy/NG_GN", "True");
        pinyinDefaults.setValueByPath("CompactTypoDefaultApplied", "True");
        pinyin->setConfig(pinyinDefaults);
        auto defaultGroup = instance->inputMethodManager().currentGroup();
        defaultGroup.inputMethodList().clear();
        defaultGroup.inputMethodList().push_back(
            InputMethodGroupItem("keyboard-us"));
        defaultGroup.inputMethodList().push_back(
            InputMethodGroupItem("pinyin"));
        defaultGroup.inputMethodList().push_back(
            InputMethodGroupItem("shuangpin"));
        defaultGroup.setDefaultInputMethod("");
        instance->inputMethodManager().setGroup(std::move(defaultGroup));
    });
}

void testBasic(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *pinyin = instance->addonManager().addon("pinyin");
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        testfrontend->call<ITestFrontend::pushCommitExpectation>("呵");
        testfrontend->call<ITestFrontend::pushCommitExpectation>("ni");
        testfrontend->call<ITestFrontend::pushCommitExpectation>("ni");
        testfrontend->call<ITestFrontend::pushCommitExpectation>("你hao");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);

        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("a"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("`"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_BackSpace), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("s"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("z"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("s"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("1"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("KP_Enter"),
                                                    false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("a"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("o"), false);
        // Make a partial selection, we do search because the data might change.
        findAndSelectCandidate(ic, "你");
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);

        // Test switch input method.
        testfrontend->call<ITestFrontend::pushCommitExpectation>("nihao");
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("a"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("o"), false);
        sendControlSpace(testfrontend, ic);

        testfrontend->call<ITestFrontend::pushCommitExpectation>("你hao");
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("a"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("o"), false);
        // Make a partial selection, we do search because the data might change.
        findAndSelectCandidate(ic, "你");
        sendControlSpace(testfrontend, ic);

        RawConfig config;
        config.setValueByPath("SwitchInputMethodBehavior",
                              "Commit default selection");
        pinyin->setConfig(config);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("a"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("o"), false);
        auto sentence =
            ic->inputPanel().candidateList()->candidate(0).text().toString();
        testfrontend->call<ITestFrontend::pushCommitExpectation>(sentence);
        sendControlSpace(testfrontend, ic);

        config.setValueByPath("SwitchInputMethodBehavior", "Clear");
        pinyin->setConfig(config);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("a"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("o"), false);
        sendControlSpace(testfrontend, ic);
    });
}

void testSelectByChar(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);

        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("a"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("o"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("g"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("o"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("g"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("z"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("u"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("b"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("g"), false);

        testfrontend->call<ITestFrontend::pushCommitExpectation>("你好主病");
        findAndSelectCandidate(ic, "你好");
        auto candidateIdx = findCandidateOrDie(ic, "公主");
        ic->inputPanel().candidateList()->toBulkCursor()->setGlobalCursorIndex(
            candidateIdx);
        // With default config, this should select "主".
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("]"), false);
        findAndSelectCandidate(ic, "病");
    });
}

void testUppercase(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);

        testfrontend->call<ITestFrontend::pushCommitExpectation>("Apple");

        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("A"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("p"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("p"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("l"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("e"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("space"), false);

        testfrontend->call<ITestFrontend::pushCommitExpectation>("iPhone");

        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("P"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("o"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("e"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("space"), false);
    });
}

void sendGroupedInput(AddonInstance *frontend, InputContext *ic,
                      std::string_view input) {
    constexpr std::array<std::string_view, 18> groups = {
        "q",  "we", "rt", "y",  "u", "io", "p", "a",  "sd",
        "fg", "h",  "jk", "l;", "z", "xc", "v", "bn", "m"};
    for (char chr : input) {
        if (chr == '\'') {
            frontend->call<ITestFrontend::keyEvent>(
                ic->uuid(), Key(FcitxKey_apostrophe, KeyState::Virtual), false);
            continue;
        }
        const auto group = std::find_if(
            groups.begin(), groups.end(), [chr](std::string_view letters) {
                return letters.find(chr) != letters.npos;
            });
        FCITX_ASSERT(group != groups.end());
        const auto index = std::distance(groups.begin(), group);
        const Key key(static_cast<KeySym>(0x0200e000 + index),
                      KeyState::Virtual);
        FCITX_ASSERT(Key::keySymToUnicode(key.sym()) == 0);
        FCITX_ASSERT(frontend->call<ITestFrontend::sendKeyEvent>(ic->uuid(),
                                                                 key, false));
        frontend->call<ITestFrontend::keyEvent>(ic->uuid(), key, true);
    }
}

void configureGroupedShuangpin(Instance *instance, InputContext *ic,
                               std::string_view profile) {
    instance->setCurrentInputMethod(ic, "shuangpin", true);
    RawConfig config;
    config.setValueByPath("ShuangpinProfile", std::string(profile));
    config.setValueByPath("Prediction", "False");
    config.setValueByPath("PinyinInPreedit", "False");
    config.setValueByPath("PreeditMode", "Composing pinyin");
    // Keep the historical threshold explicit; dedicated tests cover 0/2.
    config.setValueByPath("PartialSpLength", "4");
    config.setValueByPath("Fuzzy/NG_GN", "True");
    config.setValueByPath("CompactTypoDefaultApplied", "True");
    // Keep Android's saved QWERTY preference enabled. Compact layouts must
    // suppress it at runtime without changing the 26-key preference.
    config.setValueByPath("Fuzzy/Correction", "QWERTY");
    instance->inputMethodEngine(ic)->setConfigForInputMethod(
        *instance->inputMethodEntry(ic), config);
    ic->reset();
}

void testGroupedShuangpinSchemes(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        for (const auto &[profile, nihao] :
             {std::pair{"Xiaohe", "nihc"}, std::pair{"MS", "nihk"},
              std::pair{"Ziranma", "nihk"}}) {
            const auto uuid = frontend->call<ITestFrontend::createInputContext>(
                "grouped-scheme");
            auto *ic = instance->inputContextManager().findByUUID(uuid);
            configureGroupedShuangpin(instance, ic, profile);

            sendGroupedInput(frontend, ic, nihao);
            frontend->call<ITestFrontend::pushCommitExpectation>("你好");
            findAndSelectCandidate(ic, "你好");

            sendGroupedInput(frontend, ic, "vsgo");
            frontend->call<ITestFrontend::pushCommitExpectation>("中国");
            findAndSelectCandidate(ic, "中国");

            if (std::string_view(profile) == "MS") {
                sendGroupedInput(frontend, ic, "y;");
                frontend->call<ITestFrontend::pushCommitExpectation>("应");
                findAndSelectCandidate(ic, "应");
            }
        }
    });
}

void testGroupedShuangpinEditing(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "grouped-editing");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        ic->setCapabilityFlags(CapabilityFlag::Preedit);
        configureGroupedShuangpin(instance, ic, "Xiaohe");

        sendGroupedInput(frontend, ic, "nihv");
        frontend->call<ITestFrontend::keyEvent>(
            uuid, Key(FcitxKey_BackSpace, KeyState::Virtual), false);
        sendGroupedInput(frontend, ic, "c");
        frontend->call<ITestFrontend::pushCommitExpectation>("你好");
        findAndSelectCandidate(ic, "你好");

        sendGroupedInput(frontend, ic, "ni'hc");
        frontend->call<ITestFrontend::pushCommitExpectation>("你好");
        findAndSelectCandidate(ic, "你");
        // A selected word does not consume the user's explicit boundary.
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "你'hc");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().cursor() ==
                     std::string("你'hc").size());
        findAndSelectCandidate(ic, "好");
    });
}

void testGroupedShuangpinRawCommit(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid =
            frontend->call<ITestFrontend::createInputContext>("grouped-raw");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        ic->setCapabilityFlags(CapabilityFlag::Preedit);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        sendGroupedInput(frontend, ic, "ni");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "ni");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().cursor() == 2);
        frontend->call<ITestFrontend::pushCommitExpectation>("ni");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);

        ic->setCapabilityFlags(CapabilityFlags{});
        sendGroupedInput(frontend, ic, "gp");
        // With compact correction disabled, both decoded initials have exact
        // source spellings. Return commits those decoded ordinary keys.
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == "f p")
            << ic->inputPanel().preedit().toString();
        FCITX_ASSERT(ic->inputPanel().preedit().cursor() == 3)
            << ic->inputPanel().preedit().cursor();
        frontend->call<ITestFrontend::pushCommitExpectation>("fp");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);

        // The automatic ch/c partner has no exact c source spelling on the IO
        // key. Confirm it so fallback rendering is independent of word ranking.
        sendGroupedInput(frontend, ic, "ip");
        confirmGroupedSyllable(ic, "c");
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == "i p")
            << ic->inputPanel().preedit().toString();
        FCITX_ASSERT(ic->inputPanel().preedit().cursor() == 3)
            << ic->inputPanel().preedit().cursor();
        frontend->call<ITestFrontend::pushCommitExpectation>("ip");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);
    });
}

void testShuangpinEncodingCommit(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        for (const bool compact : {false, true}) {
            for (const bool fullPinyin : {false, true}) {
                const auto uuid =
                    frontend->call<ITestFrontend::createInputContext>(
                        "shuangpin-encoding-commit");
                auto *ic = instance->inputContextManager().findByUUID(uuid);
                ic->setCapabilityFlags(CapabilityFlag::Preedit);
                configureGroupedShuangpin(instance, ic, "Xiaohe");
                RawConfig config;
                config.setValueByPath("PinyinInPreedit",
                                      fullPinyin ? "True" : "False");
                instance->inputMethodEngine(ic)->setConfigForInputMethod(
                    *instance->inputMethodEntry(ic), config);
                const auto type = [&](std::string_view input) {
                    if (compact) {
                        sendGroupedInput(frontend, ic, input);
                    } else {
                        for (const char chr : input) {
                            frontend->call<ITestFrontend::keyEvent>(
                                uuid, Key(static_cast<KeySym>(chr)), false);
                        }
                    }
                };
                const auto commit = [&](std::string_view key,
                                        const std::string &expected) {
                    frontend->call<ITestFrontend::pushCommitExpectation>(
                        expected);
                    frontend->call<ITestFrontend::keyEvent>(
                        uuid, Key(std::string(key)), false);
                    FCITX_ASSERT(ic->inputPanel().clientPreedit().empty());
                };
                for (const auto key :
                     {"Return", "KP_Enter", "Shift+Return", "Shift+KP_Enter"}) {
                    const bool separated =
                        std::string_view(key).starts_with("Shift");
                    type("nihc");
                    commit(key, fullPinyin ? (separated ? "ni hao" : "nihao")
                                           : (separated ? "ni hc" : "nihc"));
                    type("ni'hc");
                    commit(key, fullPinyin ? (separated ? "ni'hao" : "nihao")
                                           : (separated ? "ni'hc" : "nihc"));
                }
                type("ni'hc");
                findAndSelectCandidate(ic, "你");
                commit("Return", fullPinyin ? "你hao" : "你hc");
                type("ni'hc");
                findAndSelectCandidate(ic, "你");
                commit("Shift+Return", fullPinyin ? "你'hao" : "你'hc");

                if (compact) {
                    // The same group sequence also encodes bi/hua. Choosing a
                    // non-first candidate must change the committed encoding.
                    for (const auto key : {"Return", "Shift+Return"}) {
                        type("nihc");
                        const auto index = findCandidateOrDie(ic, "壁画");
                        FCITX_ASSERT(index > 0);
                        const auto list = ic->inputPanel().candidateList();
                        list->toPageable()->setPage(index / list->size());
                        list->toBulkCursor()->setGlobalCursorIndex(index);
                        FCITX_ASSERT(list->cursorIndex() >= 0);
                        commit(key,
                               fullPinyin
                                   ? (std::string_view(key).starts_with("Shift")
                                          ? "bi hua"
                                          : "bihua")
                                   : (std::string_view(key).starts_with("Shift")
                                          ? "bi hx"
                                          : "bihx"));
                    }
                }
            }
        }
    });
}

void testPinyinHighlightedEncodingCommit(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "pinyin-highlighted-encoding-commit");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        instance->setCurrentInputMethod(ic, "pinyin", true);
        RawConfig config;
        config.setValueByPath("Prediction", "False");
        for (const bool fullPinyin : {false, true}) {
            config.setValueByPath("PinyinInPreedit",
                                  fullPinyin ? "True" : "False");
            instance->inputMethodEngine(ic)->setConfigForInputMethod(
                *instance->inputMethodEntry(ic), config);
            for (const auto word : {"西安", "西"}) {
                for (const auto key : {"Return", "Shift+Return"}) {
                    for (const char chr : std::string_view("xian")) {
                        frontend->call<ITestFrontend::keyEvent>(
                            uuid, Key(static_cast<KeySym>(chr)), false);
                    }
                    const auto index = findCandidateOrDie(ic, word);
                    FCITX_ASSERT(index > 0);
                    const auto list = ic->inputPanel().candidateList();
                    list->toPageable()->setPage(index / list->size());
                    list->toBulkCursor()->setGlobalCursorIndex(index);
                    FCITX_ASSERT(list->cursorIndex() >= 0);
                    frontend->call<ITestFrontend::pushCommitExpectation>(
                        std::string_view(key).starts_with("Shift") ? "xi an"
                                                                   : "xian");
                    frontend->call<ITestFrontend::keyEvent>(uuid, Key(key),
                                                            false);
                }
            }
        }
    });
}

void testGroupedShuangpinPrivateKeyGuard(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "grouped-private-guard");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        constexpr std::array<KeySym, 3> syms = {
            static_cast<KeySym>(0x0200e000), static_cast<KeySym>(0x0200e200),
            static_cast<KeySym>(0x0200e300)};
        for (const auto sym : syms) {
            FCITX_ASSERT(frontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(sym), false));
            FCITX_ASSERT(frontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(sym, KeyStates(KeyState::Virtual) | KeyState::Ctrl),
                false));
        }
        FCITX_ASSERT(!ic->inputPanel().candidateList());

        instance->setCurrentInputMethod(ic, "pinyin", true);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        const auto preedit = ic->inputPanel().preedit().toString();
        for (const auto sym : syms) {
            FCITX_ASSERT(frontend->call<ITestFrontend::sendKeyEvent>(
                uuid, Key(sym, KeyState::Virtual), false));
        }
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == preedit);
        frontend->call<ITestFrontend::pushCommitExpectation>("ni");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);
    });
}

void confirmGroupedSyllable(InputContext *ic, std::string_view syllable) {
    // Each confirmation replaces the list and its action storage.
    auto candidates = ic->inputPanel().candidateList();
    auto *tabbed = candidates->toTabbed();
    FCITX_ASSERT(tabbed);
    const auto actions = tabbed->tabActions();
    const auto action =
        std::ranges::find_if(actions, [syllable](const auto &item) {
            return item.text() == syllable;
        });
    FCITX_ASSERT(action != actions.end()) << syllable;
    const auto id = action->id();
    tabbed->triggerTabAction(id);
}

void testGroupedShuangpinSyllableActions(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "grouped-syllable-actions");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        ic->setCapabilityFlags(CapabilityFlag::Preedit);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        sendGroupedInput(frontend, ic, "nihc");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "ni hc");
        confirmGroupedSyllable(ic, "bi");
        FCITX_ASSERT(
            ic->inputPanel().clientPreedit().toString().starts_with("bi"));
        confirmGroupedSyllable(ic, "hua");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "bi hx");
        FCITX_ASSERT(findCandidate(ic, "你好") < 0);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("BackSpace"), false);
        FCITX_ASSERT(
            ic->inputPanel().clientPreedit().toString().starts_with("bi"));
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("BackSpace"), false);
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "ni hc");
        confirmGroupedSyllable(ic, "ni");
        confirmGroupedSyllable(ic, "hao");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "ni hc");
        frontend->call<ITestFrontend::pushCommitExpectation>("你好");
        findAndSelectCandidate(ic, "你");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "你hc")
            << ic->inputPanel().clientPreedit().toString();
        findAndSelectCandidate(ic, "好");
    });
}

void testGroupedShuangpinPreeditModes(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        for (const auto &[profile, code] :
             {std::pair{"Xiaohe", "nihc"}, std::pair{"MS", "nihk"},
              std::pair{"Ziranma", "nihk"}}) {
            const auto uuid = frontend->call<ITestFrontend::createInputContext>(
                "grouped-preedit-mode");
            auto *ic = instance->inputContextManager().findByUUID(uuid);
            ic->setCapabilityFlags(CapabilityFlag::Preedit);
            configureGroupedShuangpin(instance, ic, profile);
            sendGroupedInput(frontend, ic, "n");
            FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "n");
            sendGroupedInput(frontend, ic, "i");
            FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "ni");
            sendGroupedInput(frontend, ic, "h");
            FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "ni h");
            sendGroupedInput(frontend, ic, std::string_view(code).substr(3));
            const auto expectedCode =
                std::string("ni ") +
                std::string(std::string_view(code).substr(2));
            FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() ==
                         expectedCode);
            FCITX_ASSERT(ic->inputPanel().clientPreedit().cursor() == 5);
            frontend->call<ITestFrontend::keyEvent>(uuid, Key("Left"), false);
            FCITX_ASSERT(ic->inputPanel().clientPreedit().cursor() == 4);
            frontend->call<ITestFrontend::keyEvent>(uuid, Key("Left"), false);
            FCITX_ASSERT(ic->inputPanel().clientPreedit().cursor() == 2);
            frontend->call<ITestFrontend::keyEvent>(uuid, Key("Home"), false);
            FCITX_ASSERT(ic->inputPanel().clientPreedit().cursor() == 0);
            frontend->call<ITestFrontend::keyEvent>(uuid, Key("End"), false);
            ic->reset();

            RawConfig config;
            config.setValueByPath("PinyinInPreedit", "True");
            instance->inputMethodEngine(ic)->setConfigForInputMethod(
                *instance->inputMethodEntry(ic), config);
            sendGroupedInput(frontend, ic, code);
            FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() ==
                         "ni hao");
            FCITX_ASSERT(ic->inputPanel().clientPreedit().cursor() == 6);
            confirmGroupedSyllable(ic, "bi");
            FCITX_ASSERT(
                ic->inputPanel().clientPreedit().toString().starts_with("bi"));
            frontend->call<ITestFrontend::keyEvent>(uuid, Key("BackSpace"),
                                                    false);
            FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() ==
                         "ni hao");
            confirmGroupedSyllable(ic, "ni");
            confirmGroupedSyllable(ic, "hao");
            FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() ==
                         "ni hao");
            frontend->call<ITestFrontend::keyEvent>(uuid, Key("Left"), false);
            FCITX_ASSERT(ic->inputPanel().clientPreedit().cursor() == 4);
            ic->reset();

            if (std::string_view(profile) == "MS") {
                sendGroupedInput(frontend, ic, "y;");
                FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() ==
                             "ying");
                ic->reset();
                config.setValueByPath("PinyinInPreedit", "False");
                instance->inputMethodEngine(ic)->setConfigForInputMethod(
                    *instance->inputMethodEntry(ic), config);
                sendGroupedInput(frontend, ic, "y;");
                FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() ==
                             "y;");
                ic->reset();
            }
            if (std::string_view(profile) == "Xiaohe") {
                sendGroupedInput(frontend, ic, "aa");
                FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() ==
                             "a");
                ic->reset();
                config.setValueByPath("PinyinInPreedit", "False");
                instance->inputMethodEngine(ic)->setConfigForInputMethod(
                    *instance->inputMethodEntry(ic), config);
                sendGroupedInput(frontend, ic, "aa");
                FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() ==
                             "aa");
                ic->reset();
            }
        }
    });
}

void testGroupedShuangpinQuickPhraseBoundary(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "grouped-quickphrase-boundary");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        sendGroupedInput(frontend, ic, "ni");
        const auto sentence =
            ic->inputPanel().candidateList()->candidate(0).text().toString();
        frontend->call<ITestFrontend::pushCommitExpectation>(sentence);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("@"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        frontend->call<ITestFrontend::pushCommitExpectation>("ni");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);
    });
}

void testGroupedShuangpinQuickPhraseTempMode(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "grouped-quickphrase-tempmode");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        RawConfig config;
        config.setValueByPath("QuickPhraseKey", "Control+semicolon");
        instance->inputMethodEngine(ic)->setConfigForInputMethod(
            *instance->inputMethodEntry(ic), config);
        FCITX_ASSERT(frontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("Control+semicolon"), false));
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("x"), false);
        sendGroupedInput(frontend, ic, "n");
        frontend->call<ITestFrontend::pushCommitExpectation>("x");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);
    });
}

void testGroupedShuangpinCustomPhraseBoundary(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "grouped-customphrase-boundary");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        sendGroupedInput(frontend, ic, "ni");
        const auto *candidates = ic->inputPanel().candidateList().get();
        const auto &candidate = candidates->toBulk()->candidateFromAll(
            findCandidateOrDie(ic, "你"));
        auto *actionable = candidates->toActionable();
        FCITX_ASSERT(actionable);
        const auto actions = actionable->candidateActions(candidate);
        FCITX_ASSERT(std::ranges::none_of(actions, [](const auto &action) {
            return action.id() == 1 || action.id() == 2;
        }));
        actionable->triggerAction(candidate, 1);
        ic->reset();
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("b"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        // QWERTY correction may legitimately suggest "你" for "bi". A
        // polluted custom phrase would instead pin it as the first candidate.
        FCITX_ASSERT(findCandidate(ic, "你") != 0);
        ic->reset();
    });
}

void testGroupedShuangpinModeControl(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "grouped-mode-control");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        const Key grouped(static_cast<KeySym>(0x0200e101), KeyState::Virtual);
        const Key normal(static_cast<KeySym>(0x0200e100), KeyState::Virtual);
        FCITX_ASSERT(
            frontend->call<ITestFrontend::sendKeyEvent>(uuid, grouped, false));
        sendGroupedInput(frontend, ic, "nihc");
        const auto preedit = ic->inputPanel().preedit().toString();
        FCITX_ASSERT(
            frontend->call<ITestFrontend::sendKeyEvent>(uuid, grouped, false));
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == preedit);
        const auto sentence =
            ic->inputPanel().candidateList()->candidate(0).text().toString();
        frontend->call<ITestFrontend::pushCommitExpectation>(sentence);
        FCITX_ASSERT(
            frontend->call<ITestFrontend::sendKeyEvent>(uuid, normal, false));
        FCITX_ASSERT(!frontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("space"), false));
        FCITX_ASSERT(!frontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("BackSpace"), false));
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        frontend->call<ITestFrontend::pushCommitExpectation>("ni");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);
    });
}

void testGroupedShuangpinHardwareSwitch(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "grouped-hardware");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        sendGroupedInput(frontend, ic, "nihc");
        const auto sentence =
            ic->inputPanel().candidateList()->candidate(0).text().toString();
        frontend->call<ITestFrontend::pushCommitExpectation>(sentence);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        frontend->call<ITestFrontend::pushCommitExpectation>("ni");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);

        sendGroupedInput(frontend, ic, "nihc");
        frontend->call<ITestFrontend::pushCommitExpectation>("你好");
        findAndSelectCandidate(ic, "你好");
    });
}

void testGroupedShuangpinContextIsolation(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto firstUuid =
            frontend->call<ITestFrontend::createInputContext>(
                "grouped-context-1");
        const auto secondUuid =
            frontend->call<ITestFrontend::createInputContext>(
                "grouped-context-2");
        auto *first = instance->inputContextManager().findByUUID(firstUuid);
        auto *second = instance->inputContextManager().findByUUID(secondUuid);
        configureGroupedShuangpin(instance, first, "Xiaohe");
        instance->setCurrentInputMethod(second, "shuangpin", true);
        first->focusIn();
        second->focusIn();

        sendGroupedInput(frontend, first, "nihc");
        frontend->call<ITestFrontend::keyEvent>(secondUuid, Key("n"), false);
        frontend->call<ITestFrontend::keyEvent>(secondUuid, Key("i"), false);
        frontend->call<ITestFrontend::pushCommitExpectation>("你好");
        findAndSelectCandidate(first, "你好");
        frontend->call<ITestFrontend::pushCommitExpectation>("ni");
        frontend->call<ITestFrontend::keyEvent>(secondUuid, Key("Return"),
                                                false);
    });
}

void sendGroupedProtocol(AddonInstance *frontend, InputContext *ic,
                         uint32_t sym) {
    const Key key(static_cast<KeySym>(sym), KeyState::Virtual);
    FCITX_ASSERT(Key::keySymToUnicode(key.sym()) == 0);
    FCITX_ASSERT(
        frontend->call<ITestFrontend::sendKeyEvent>(ic->uuid(), key, false));
    frontend->call<ITestFrontend::keyEvent>(ic->uuid(), key, true);
}

void testGroupedShuangpinCorrectionScope(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto firstUuid =
            frontend->call<ITestFrontend::createInputContext>(
                "grouped-correction-scope");
        auto *first = instance->inputContextManager().findByUUID(firstUuid);
        configureGroupedShuangpin(instance, first, "Xiaohe");

        const auto typeLetters = [frontend](InputContext *ic,
                                            std::string_view input,
                                            bool virtualKeys) {
            for (const char letter : input) {
                const Key key(static_cast<KeySym>(letter),
                              virtualKeys ? KeyStates{KeyState::Virtual}
                                          : KeyStates{});
                frontend->call<ITestFrontend::keyEvent>(ic->uuid(), key, false);
                frontend->call<ITestFrontend::keyEvent>(ic->uuid(), key, true);
            }
        };
        const auto expectMi = [](InputContext *ic, bool correction) {
            findCandidateOrDie(ic, "米");
            FCITX_ASSERT((findCandidate(ic, "你") >= 0) == correction);
        };

        // m and n are QWERTY neighbours but belong to different compact
        // groups. Thus "你" for mi can only come from neighbour correction.
        typeLetters(first, "mi", true);
        expectMi(first, true);
        first->reset();

        sendGroupedProtocol(frontend, first, 0x0200e101);
        sendGroupedInput(frontend, first, "mi");
        expectMi(first, false);
        first->reset();

        // A raw virtual key may leave the grouped parser without changing
        // the selected 18-key layout. Correction must remain suppressed.
        typeLetters(first, "mi", true);
        expectMi(first, false);
        first->reset();

        // Reapplying the saved preference must not restore compact correction
        // through a new shared profile or cached syllable matches.
        RawConfig config;
        config.setValueByPath("Fuzzy/Correction", "QWERTY");
        instance->inputMethodEngine(first)->setConfigForInputMethod(
            *instance->inputMethodEntry(first), config);
        sendGroupedInput(frontend, first, "mi");
        expectMi(first, false);
        first->reset();

        // Exact compact aliases continue to work even though correction is off.
        sendGroupedInput(frontend, first, "nihc");
        findCandidateOrDie(first, "你好");
        first->reset();

        sendGroupedProtocol(frontend, first, 0x0200e100);
        typeLetters(first, "mi", true);
        expectMi(first, true);
        first->reset();

        // Physical 26-key input temporarily takes over, then compact input
        // suppresses correction again without altering the saved preference.
        sendGroupedProtocol(frontend, first, 0x0200e101);
        typeLetters(first, "mi", false);
        expectMi(first, true);
        first->reset();
        sendGroupedInput(frontend, first, "mi");
        expectMi(first, false);
        first->reset();

        const auto secondUuid =
            frontend->call<ITestFrontend::createInputContext>(
                "ordinary-correction-scope");
        auto *second = instance->inputContextManager().findByUUID(secondUuid);
        instance->setCurrentInputMethod(second, "shuangpin", true);
        first->focusIn();
        second->focusIn();
        sendGroupedInput(frontend, first, "mi");
        expectMi(first, false);
        typeLetters(second, "mi", true);
        expectMi(second, true);
        expectMi(first, false);
        first->reset();
        second->reset();
    });
}

void stageCustomGroups(AddonInstance *frontend, InputContext *ic,
                       const std::vector<std::string> &groups) {
    sendGroupedProtocol(frontend, ic, 0x0200e200);
    for (const auto &group : groups) {
        for (const char letter : group) {
            sendGroupedProtocol(
                frontend, ic, 0x0200e210 + (letter == ';' ? 26 : letter - 'a'));
        }
        sendGroupedProtocol(frontend, ic, 0x0200e201);
    }
}

void configureCustomGroups(AddonInstance *frontend, InputContext *ic,
                           const std::vector<std::string> &groups) {
    stageCustomGroups(frontend, ic, groups);
    sendGroupedProtocol(frontend, ic, 0x0200e202);
    sendGroupedProtocol(frontend, ic, 0x0200e101);
}

void sendCustomGroupedInput(AddonInstance *frontend, InputContext *ic,
                            const std::vector<std::string> &groups,
                            std::string_view input) {
    for (const char letter : input) {
        if (letter == '\'') {
            frontend->call<ITestFrontend::keyEvent>(
                ic->uuid(), Key(FcitxKey_apostrophe, KeyState::Virtual), false);
            continue;
        }
        const auto group =
            std::ranges::find_if(groups, [letter](const auto &group) {
                return group.find(letter) != std::string::npos;
            });
        FCITX_ASSERT(group != groups.end());
        const char representative = group->front();
        sendGroupedProtocol(
            frontend, ic,
            0x0200e300 + (representative == ';' ? 26 : representative - 'a'));
    }
}

const std::vector<std::string> CustomRegroupedKeys = {
    "e",  "wq", "rt", "y",  "u", "io", "p", "a",  "sd",
    "fg", "h",  "jk", "l;", "z", "xc", "v", "bn", "m"};

void testCustomGroupedShuangpinLayouts(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const std::vector<std::vector<std::string>> layouts = {
            CustomRegroupedKeys,
            {"q", "w", "e", "rt", "y", "u", "io", "p", "a", "sd", "fg", "h",
             "jk", "l;", "z", "xc", "v", "bn", "m"},
            {"q", "we", "rt", "yu", "io", "p", "a", "sd", "fg", "h", "jk", "l;",
             "z", "xc", "v", "bn", "m"},
        };
        for (const auto &[profile, code] :
             {std::pair{"Xiaohe", "nihc"}, std::pair{"MS", "nihk"},
              std::pair{"Ziranma", "nihk"}}) {
            for (const auto &groups : layouts) {
                const auto uuid =
                    frontend->call<ITestFrontend::createInputContext>(
                        "custom-grouped-layout");
                auto *ic = instance->inputContextManager().findByUUID(uuid);
                ic->setCapabilityFlags(CapabilityFlag::Preedit);
                configureGroupedShuangpin(instance, ic, profile);
                configureCustomGroups(frontend, ic, groups);
                sendCustomGroupedInput(frontend, ic, groups, code);
                const auto expected =
                    std::string("ni ") + std::string(code + 2);
                FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() ==
                             expected);
                frontend->call<ITestFrontend::pushCommitExpectation>("你好");
                findAndSelectCandidate(ic, "你好");

                sendCustomGroupedInput(frontend, ic, groups, "q");
                frontend->call<ITestFrontend::pushCommitExpectation>(
                    ic->inputPanel().clientPreedit().toString());
                frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"),
                                                        false);
            }
        }
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "custom-grouped-without-semicolon");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        const std::vector<std::string> oneGroup = {
            "abcdefghijklmnopqrstuvwxyz"};
        configureCustomGroups(frontend, ic, oneGroup);
        sendCustomGroupedInput(frontend, ic, oneGroup, "e");
        frontend->call<ITestFrontend::pushCommitExpectation>(
            ic->inputPanel().preedit().toString());
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);
    });
}

void testCustomGroupedShuangpinAtomicConfig(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "custom-grouped-atomic-config");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        sendGroupedInput(frontend, ic, "nihc");
        const auto oldPreedit = ic->inputPanel().preedit().toString();
        stageCustomGroups(frontend, ic, CustomRegroupedKeys);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == oldPreedit);
        const auto sentence =
            ic->inputPanel().candidateList()->candidate(0).text().toString();
        sendGroupedProtocol(frontend, ic, 0x0200e202);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == oldPreedit);
        frontend->call<ITestFrontend::pushCommitExpectation>(sentence);
        sendGroupedProtocol(frontend, ic, 0x0200e101);
        FCITX_ASSERT(ic->inputPanel().preedit().empty());

        sendCustomGroupedInput(frontend, ic, CustomRegroupedKeys, "nihc");
        const auto customPreedit = ic->inputPanel().preedit().toString();
        auto reordered = CustomRegroupedKeys;
        std::ranges::reverse(reordered);
        configureCustomGroups(frontend, ic, reordered);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == customPreedit);

        auto invalid = CustomRegroupedKeys;
        invalid[1] += 'q';
        configureCustomGroups(frontend, ic, invalid);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == customPreedit);
        invalid = CustomRegroupedKeys;
        invalid[0] = "w";
        configureCustomGroups(frontend, ic, invalid);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == customPreedit);
        invalid = CustomRegroupedKeys;
        invalid.erase(invalid.begin());
        configureCustomGroups(frontend, ic, invalid);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == customPreedit);
        configureCustomGroups(frontend, ic, {});
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == customPreedit);

        sendGroupedProtocol(frontend, ic, 0x0200e200);
        sendGroupedProtocol(frontend, ic, 0x0200e210);
        sendGroupedProtocol(frontend, ic,
                            0x0200e202); // Missing group delimiter.
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == customPreedit);
        sendGroupedProtocol(frontend, ic, 0x0200e200);
        sendGroupedProtocol(frontend, ic,
                            0x0200e2ff); // Invalid config payload.
        sendGroupedProtocol(frontend, ic, 0x0200e202);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == customPreedit);

        // Q is a member of WQ, but cannot independently identify that key.
        sendGroupedProtocol(frontend, ic, 0x0200e300 + ('q' - 'a'));
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == customPreedit);
        frontend->call<ITestFrontend::pushCommitExpectation>("你好");
        findAndSelectCandidate(ic, "你好");

        stageCustomGroups(frontend, ic, CustomRegroupedKeys);
        ic->reset();
        sendGroupedProtocol(frontend, ic, 0x0200e202);
        sendCustomGroupedInput(frontend, ic, CustomRegroupedKeys, "q");
        frontend->call<ITestFrontend::pushCommitExpectation>("w");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);

        // Legacy indexed key presses still select the original partition.
        sendGroupedInput(frontend, ic, "q");
        frontend->call<ITestFrontend::pushCommitExpectation>("q");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);

        configureCustomGroups(frontend, ic, CustomRegroupedKeys);
        sendGroupedInput(frontend, ic, "nihc");
        const auto legacyPreedit = ic->inputPanel().preedit().toString();
        sendGroupedProtocol(frontend, ic, 0x0200e101);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == legacyPreedit);
        frontend->call<ITestFrontend::pushCommitExpectation>("你好");
        findAndSelectCandidate(ic, "你好");
    });
}

void testCustomGroupedShuangpinSyllableActions(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "custom-grouped-syllable-actions");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        ic->setCapabilityFlags(CapabilityFlag::Preedit);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        configureCustomGroups(frontend, ic, CustomRegroupedKeys);
        sendCustomGroupedInput(frontend, ic, CustomRegroupedKeys, "nihc");
        confirmGroupedSyllable(ic, "bi");
        confirmGroupedSyllable(ic, "hua");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "bi hx");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("BackSpace"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("BackSpace"), false);
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "ni hc");
        confirmGroupedSyllable(ic, "ni");
        confirmGroupedSyllable(ic, "hao");
        frontend->call<ITestFrontend::pushCommitExpectation>("你好");
        findAndSelectCandidate(ic, "你好");
    });
}

void testCustomGroupedShuangpinSemicolonAndIsolation(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto firstUuid =
            frontend->call<ITestFrontend::createInputContext>(
                "custom-grouped-context-1");
        const auto secondUuid =
            frontend->call<ITestFrontend::createInputContext>(
                "custom-grouped-context-2");
        auto *first = instance->inputContextManager().findByUUID(firstUuid);
        auto *second = instance->inputContextManager().findByUUID(secondUuid);
        configureGroupedShuangpin(instance, first, "MS");
        configureGroupedShuangpin(instance, second, "MS");
        std::vector<std::string> singles;
        for (char letter = 'a'; letter <= 'z'; ++letter) {
            singles.emplace_back(1, letter);
        }
        configureCustomGroups(frontend, first, CustomRegroupedKeys);
        sendCustomGroupedInput(frontend, first, CustomRegroupedKeys, "q");
        const auto preedit = first->inputPanel().preedit().toString();
        configureCustomGroups(frontend, first, singles); // MS requires ';'.
        FCITX_ASSERT(first->inputPanel().preedit().toString() == preedit);
        frontend->call<ITestFrontend::pushCommitExpectation>("w");
        frontend->call<ITestFrontend::keyEvent>(firstUuid, Key("Return"),
                                                false);
        singles.emplace_back(";");
        configureCustomGroups(frontend, first, singles);
        configureCustomGroups(frontend, second, CustomRegroupedKeys);
        first->focusIn();
        second->focusIn();
        sendCustomGroupedInput(frontend, first, singles, "y;");
        frontend->call<ITestFrontend::pushCommitExpectation>("应");
        findAndSelectCandidate(first, "应");
        sendCustomGroupedInput(frontend, first, singles, "q");
        frontend->call<ITestFrontend::pushCommitExpectation>("q");
        frontend->call<ITestFrontend::keyEvent>(firstUuid, Key("Return"),
                                                false);
        sendCustomGroupedInput(frontend, second, CustomRegroupedKeys, "q");
        frontend->call<ITestFrontend::pushCommitExpectation>("w");
        frontend->call<ITestFrontend::keyEvent>(secondUuid, Key("Return"),
                                                false);

        const auto thirdUuid =
            frontend->call<ITestFrontend::createInputContext>(
                "custom-grouped-profile-change");
        auto *third = instance->inputContextManager().findByUUID(thirdUuid);
        configureGroupedShuangpin(instance, third, "Xiaohe");
        singles.pop_back(); // A valid Xiaohe partition may omit ';'.
        configureCustomGroups(frontend, third, singles);
        sendCustomGroupedInput(frontend, third, singles, "e");
        frontend->call<ITestFrontend::pushCommitExpectation>("e");
        frontend->call<ITestFrontend::keyEvent>(thirdUuid, Key("Return"),
                                                false);
        RawConfig changedProfile;
        changedProfile.setValueByPath("ShuangpinProfile", "MS");
        instance->inputMethodEngine(third)->setConfigForInputMethod(
            *instance->inputMethodEntry(third), changedProfile);
        sendGroupedProtocol(frontend, third, 0x0200e300 + ('n' - 'a'));
        FCITX_ASSERT(third->inputPanel().preedit().empty());
        FCITX_ASSERT(!third->inputPanel().candidateList());
        sendGroupedProtocol(frontend, third, 0x0200e300 + ('w' - 'a'));
        frontend->call<ITestFrontend::pushCommitExpectation>("w");
        frontend->call<ITestFrontend::keyEvent>(thirdUuid, Key("Return"),
                                                false);
        changedProfile.setValueByPath("ShuangpinProfile", "Xiaohe");
        instance->inputMethodEngine(third)->setConfigForInputMethod(
            *instance->inputMethodEntry(third), changedProfile);
        sendGroupedProtocol(frontend, third, 0x0200e300 + ('w' - 'a'));
        frontend->call<ITestFrontend::pushCommitExpectation>("w");
        frontend->call<ITestFrontend::keyEvent>(thirdUuid, Key("Return"),
                                                false);
    });
}

enum class CandidateShape { Character, Word, Symbol, Other };

// Tell candidates apart by their text alone, as a user would see them.
CandidateShape candidateShape(const std::string &text) {
    size_t length = 0;
    bool readable = true;
    bool symbol = true;
    for (const uint32_t c : utf8::MakeUTF8CharRange(text)) {
        const bool han = (c >= 0x3400 && c <= 0x9fff) ||
                         (c >= 0xf900 && c <= 0xfaff) ||
                         (c >= 0x20000 && c <= 0x3ffff);
        const bool letter =
            c < 0x80 && (charutils::islower(static_cast<char>(c)) ||
                         charutils::isupper(static_cast<char>(c)));
        readable = readable && (han || letter);
        symbol = symbol && c >= 0x80 && !han;
        length++;
    }
    if (length == 0) {
        return CandidateShape::Other;
    }
    if (readable) {
        return length == 1 ? CandidateShape::Character : CandidateShape::Word;
    }
    return symbol ? CandidateShape::Symbol : CandidateShape::Other;
}

std::vector<std::string> allCandidateTexts(InputContext *ic) {
    std::vector<std::string> texts;
    auto candidateList = ic->inputPanel().candidateList();
    FCITX_ASSERT(candidateList);
    auto *bulk = candidateList->toBulk();
    for (int i = 0; i < bulk->totalSize(); i++) {
        texts.push_back(bulk->candidateFromAll(i).text().toString());
    }
    return texts;
}

bool hasAdjacentSymbols(const std::vector<std::string> &texts) {
    for (size_t i = 1; i < texts.size(); i++) {
        if (candidateShape(texts[i - 1]) == CandidateShape::Symbol &&
            candidateShape(texts[i]) == CandidateShape::Symbol) {
            return true;
        }
    }
    return false;
}

void testTwoKeyShuangpinBalance(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid = testfrontend->call<ITestFrontend::createInputContext>(
            "two-key-balance");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "shuangpin", true);
        // Abbreviations from two keys on are what floods a syllable with
        // words. The defaults are restored at the end for the other tests.
        const auto configure = [instance, ic](bool twoKeyAbbreviation) {
            RawConfig config;
            config.setValueByPath("ShuangpinProfile", "Xiaohe");
            config.setValueByPath("Prediction", "False");
            config.setValueByPath("PageSize", twoKeyAbbreviation ? "5" : "7");
            config.setValueByPath("PartialSpLength",
                                  twoKeyAbbreviation ? "2" : "0");
            config.setValueByPath("Fuzzy/NG_GN", "True");
            config.setValueByPath("CompactTypoDefaultApplied", "True");
            config.setValueByPath("Fuzzy/Correction", "QWERTY");
            instance->inputMethodEngine(ic)->setConfigForInputMethod(
                *instance->inputMethodEntry(ic), config);
            ic->reset();
        };
        const auto type = [testfrontend, uuid](std::string_view keys) {
            for (const char key : keys) {
                testfrontend->call<ITestFrontend::keyEvent>(
                    uuid, Key(static_cast<KeySym>(key)), false);
            }
        };
        configure(true);

        // "ks" is kong, and also abbreviates words like kai'shi.
        constexpr size_t frontSize = 5;
        type("ks");
        const auto texts = allCandidateTexts(ic);
        FCITX_ASSERT(texts.size() > frontSize);
        FCITX_ASSERT(candidateShape(texts[0]) != CandidateShape::Symbol);
        size_t words = 0;
        size_t symbols = 0;
        for (size_t i = 0; i < frontSize; i++) {
            switch (candidateShape(texts[i])) {
            case CandidateShape::Word:
                words++;
                // Only short English words may sit on the front page.
                FCITX_ASSERT(texts[i].size() <= 4 ||
                             !charutils::islower(texts[i][0]))
                    << texts[i];
                break;
            case CandidateShape::Symbol:
                symbols++;
                break;
            default:
                break;
            }
        }
        FCITX_ASSERT(words <= 2) << texts;
        FCITX_ASSERT(symbols <= 1) << texts;
        FCITX_ASSERT(findCandidateOrDie(ic, "空") < static_cast<int>(frontSize))
            << texts;
        const std::vector<std::string> expectedFront = {"开始", "▶️", "空",
                                                        "可是", "孔"};
        FCITX_ASSERT(std::vector<std::string>(texts.begin(),
                                              texts.begin() + frontSize) ==
                     expectedFront)
            << texts;
        // Independent emoji is deferred, never merged into a different run.
        FCITX_ASSERT(texts[frontSize] == "🈳") << texts;
        // Runs on later pages keep their individual symbols.
        FCITX_ASSERT(hasAdjacentSymbols(
            std::vector<std::string>(texts.begin() + frontSize, texts.end())))
            << texts;

        // Behind the front page the syllable's characters come before the
        // remaining words, and one-key matches come after them.
        size_t firstRestWord = frontSize;
        while (firstRestWord < texts.size() &&
               candidateShape(texts[firstRestWord]) != CandidateShape::Word) {
            firstRestWord++;
        }
        FCITX_ASSERT(firstRestWord < texts.size()) << texts;
        FCITX_ASSERT(static_cast<size_t>(findCandidateOrDie(ic, "控")) <
                     firstRestWord)
            << texts;
        FCITX_ASSERT(static_cast<size_t>(findCandidateOrDie(ic, "可")) >
                     firstRestWord)
            << texts;

        // Only the head emoji carries the folded choices and the ellipsis.
        // Its actions contain no pin, forget, reset or Chinese decomposition.
        auto candidateList = ic->inputPanel().candidateList();
        auto *actionable = candidateList->toActionable();
        FCITX_ASSERT(actionable);
        const auto &head = candidateList->toBulk()->candidateFromAll(1);
        FCITX_ASSERT(head.comment().toString() == "…");
        FCITX_ASSERT(actionable->hasAction(head));
        const auto actions = actionable->candidateActions(head);
        FCITX_ASSERT(actions.size() == 1);
        FCITX_ASSERT(actions.front().text() == "🔛");
        FCITX_ASSERT(actions.front().icon() == "folded-symbol");
        FCITX_ASSERT(!actions.front().isSeparator());
        const auto &deferred =
            candidateList->toBulk()->candidateFromAll(frontSize);
        FCITX_ASSERT(deferred.comment().toString() == "(空)");
        for (const auto &action : actionable->candidateActions(deferred)) {
            FCITX_ASSERT(action.icon() != "folded-symbol");
        }
        for (const auto word : {"开始", "空"}) {
            const auto &candidate = candidateList->toBulk()->candidateFromAll(
                findCandidateOrDie(ic, word));
            for (const auto &action : actionable->candidateActions(candidate)) {
                FCITX_ASSERT(action.icon() != "folded-symbol") << word;
            }
        }
        testfrontend->call<ITestFrontend::pushCommitExpectation>("🔛");
        actionable->triggerAction(head, actions.front().id());
        FCITX_ASSERT(!ic->inputPanel().candidateList());

        // "bq" is no syllable, so it can only abbreviate words: they keep
        // the whole front page instead of yielding to one-key matches.
        type("bq");
        const auto abbreviated = allCandidateTexts(ic);
        FCITX_ASSERT(abbreviated.size() > frontSize);
        symbols = 0;
        for (size_t i = 0; i < frontSize; i++) {
            const auto shape = candidateShape(abbreviated[i]);
            FCITX_ASSERT(shape == CandidateShape::Word ||
                         shape == CandidateShape::Symbol)
                << abbreviated;
            symbols += shape == CandidateShape::Symbol;
        }
        FCITX_ASSERT(symbols <= 1) << abbreviated;
        ic->reset();

        // A valid syllable with only one character still uses abbreviation
        // words instead of pulling one-key guesses into the front page.
        type("gw");
        const auto fewCharacters = allCandidateTexts(ic);
        FCITX_ASSERT(
            std::vector<std::string>(fewCharacters.begin(),
                                     fewCharacters.begin() + frontSize) ==
            std::vector<std::string>({"给", "岗位", "国外", "购物", "更为"}))
            << fewCharacters;
        ic->reset();

        // Corrections cannot lead when an exact character or a dictionary
        // word is available. Engine-assembled sentences do not get promoted.
        for (const auto &[input, expected] :
             std::vector<std::pair<std::string_view, std::string_view>>{
                 {"se", "色"},
                 {"no", "挪"},
                 {"we", "巍峨"},
                 {"fe", "份额"},
                 {"os", "种"},
                 {"ra", "他"}}) {
            type(input);
            const auto actual = allCandidateTexts(ic);
            FCITX_ASSERT(actual.front() == expected) << input << ": " << actual;
            ic->reset();
        }

        // The same emoji rules apply to a longer shuangpin input.
        type("kdui");
        const auto longer = allCandidateTexts(ic);
        FCITX_ASSERT(longer[0] == "开始") << longer;
        FCITX_ASSERT(longer[1] == "▶️") << longer;
        FCITX_ASSERT(ic->inputPanel()
                         .candidateList()
                         ->toBulk()
                         ->candidateFromAll(1)
                         .comment()
                         .toString() == "…");
        FCITX_ASSERT(hasAdjacentSymbols(
            std::vector<std::string>(longer.begin() + frontSize, longer.end())))
            << longer;
        ic->reset();

        // A rare completion is still selectable, but cannot crowd the front
        // page of a longer input. A frequent longer word keeps its position.
        type("ksha");
        FCITX_ASSERT(findCandidateOrDie(ic, "osha") >=
                     static_cast<int>(frontSize))
            << allCandidateTexts(ic);
        ic->reset();
        type("hello");
        FCITX_ASSERT(findCandidateOrDie(ic, "hello") <
                     static_cast<int>(frontSize))
            << allCandidateTexts(ic);
        ic->reset();

        // Capitals ask for the input as it was typed.
        type("Ks");
        FCITX_ASSERT(findCandidateOrDie(ic, "Ks") < 2);
        ic->reset();

        configure(false);
        type("ks");
        const auto defaults = allCandidateTexts(ic);
        FCITX_ASSERT(defaults[0] == "空") << defaults;
        const auto defaultFrontSize = ic->inputPanel().candidateList()->size();
        FCITX_ASSERT(!hasAdjacentSymbols(std::vector<std::string>(
            defaults.begin(), defaults.begin() + defaultFrontSize)))
            << defaults;
        FCITX_ASSERT(hasAdjacentSymbols(std::vector<std::string>(
            defaults.begin() + defaultFrontSize, defaults.end())))
            << defaults;
        ic->reset();

        // Full pinyin preserves the relative order of characters and split
        // dictionary words. Emoji still gets only one front-page position.
        instance->setCurrentInputMethod(ic, "pinyin", true);
        configure(true);
        for (const auto input : {"xian", "yue", "bian"}) {
            type(input);
            const auto full = allCandidateTexts(ic);
            FCITX_ASSERT(full.size() > frontSize) << input;
            symbols = 0;
            for (size_t i = 0; i < frontSize; i++) {
                symbols += candidateShape(full[i]) == CandidateShape::Symbol;
            }
            FCITX_ASSERT(symbols <= 1) << input << ": " << full;
            if (std::string_view(input) == "xian") {
                FCITX_ASSERT(findCandidateOrDie(ic, "西安") <
                             findCandidateOrDie(ic, "显"))
                    << full;
            } else if (std::string_view(input) == "bian") {
                FCITX_ASSERT(findCandidateOrDie(ic, "彼岸") <
                             findCandidateOrDie(ic, "辨"))
                    << full;
            }
            ic->reset();
        }
        // Full pinyin also folds only the front-page run into its first emoji.
        type("kaishi");
        const auto fullStart = allCandidateTexts(ic);
        FCITX_ASSERT(fullStart[0] == "开始") << fullStart;
        FCITX_ASSERT(fullStart[1] == "▶️") << fullStart;
        candidateList = ic->inputPanel().candidateList();
        const auto &fullHead = candidateList->toBulk()->candidateFromAll(1);
        const auto fullActions =
            candidateList->toActionable()->candidateActions(fullHead);
        FCITX_ASSERT(fullActions.size() == 1);
        FCITX_ASSERT(fullActions.front().text() == "🔛");
        FCITX_ASSERT(fullActions.front().icon() == "folded-symbol");
        ic->reset();
        configure(false);
    });
}

void testForget(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);

        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("a"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("o"), false);
        auto *candidateList = ic->inputPanel().candidateList().get();
        const auto &cand = candidateList->candidate(0);
        auto *actionable = candidateList->toActionable();
        FCITX_ASSERT(actionable);
        FCITX_ASSERT(actionable->hasAction(cand));
        auto actions = actionable->candidateActions(cand);
        FCITX_ASSERT(!actions.empty());
        FCITX_ASSERT(actions[0].id() == 0);
        actionable->triggerAction(cand, 0);
        FCITX_ASSERT(ic->inputPanel().candidateList());
    });
}

void testActionInStrokeFilter(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);
        RawConfig config;
        config.setValueByPath("PartialSpLength", "3");
        config.setValueByPath("Fuzzy/NG_GN", "True");
        config.setValueByPath("CompactTypoDefaultApplied", "True");
        instance->inputMethodEngine(ic)->setConfigForInputMethod(
            *instance->inputMethodEntry(ic), config);
        ic->reset();

        // Target ppp for 彡
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("p"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("p"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("p"), false);
        auto *candidateList = ic->inputPanel().candidateList().get();
        findCandidateOrDie(ic, "彡");
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("`"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("p"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("p"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("p"), false);
        candidateList = ic->inputPanel().candidateList().get();
        FCITX_ASSERT(findCandidate(ic, "彡") < 0);
        int index = findCandidateOrDie(ic, "䫠");
        const auto &cand = candidateList->candidate(index);
        auto *actionable = candidateList->toActionable();
        FCITX_ASSERT(actionable);
        FCITX_ASSERT(actionable->hasAction(cand));
        auto actions = actionable->candidateActions(cand);
        FCITX_ASSERT(!actions.empty());
        FCITX_ASSERT(actions[0].id() == 0);
        actionable->triggerAction(cand, 0);
        FCITX_ASSERT(ic->inputPanel().candidateList());
    });
}

void testPinyinTabFilter(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *pinyin = instance->addonManager().addon("pinyin");
        FCITX_ASSERT(pinyin);
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("x"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("a"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        findCandidateOrDie(ic, "西安");
        auto *tabbed = ic->inputPanel().candidateList()->toTabbed();
        FCITX_ASSERT(tabbed);
        auto actionSpan = tabbed->tabActions();
        std::vector<CandidateAction> actions{actionSpan.begin(),
                                             actionSpan.end()};
        std::vector<std::string> names;
        names.reserve(actions.size());
        for (const auto &action : actions) {
            names.push_back(action.text());
        }

        auto indexOf = [&names](std::string_view name) {
            auto it = std::ranges::find(names, name);
            FCITX_ASSERT(it != names.end());
            return std::distance(names.begin(), it);
        };

        const auto xiAction = actions[indexOf("xi")];
        const auto singleAction = actions[indexOf("单字")];
        const auto strokeAction = actions[indexOf("笔画")];

        auto checkedActionsAre = [tabbed](std::initializer_list<int> ids) {
            std::unordered_set<int> checkedIds;
            for (const auto &action : tabbed->tabActions()) {
                if (action.isChecked()) {
                    checkedIds.insert(action.id());
                }
            }
            return checkedIds == std::unordered_set<int>{ids};
        };

        FCITX_ASSERT(actions[0].text() == "xian");
        tabbed->triggerTabAction(actions[0].id());
        FCITX_ASSERT(findCandidate(ic, "西安") < 0);
        FCITX_ASSERT(checkedActionsAre({actions[0].id()}));

        // Trigger same action again should uncheck it.
        tabbed->triggerTabAction(actions[0].id());
        FCITX_ASSERT(findCandidate(ic, "西安") >= 0);
        FCITX_ASSERT(checkedActionsAre({}));

        FCITX_ASSERT(xiAction.text() == "xi");
        tabbed->triggerTabAction(xiAction.id());
        FCITX_ASSERT(findCandidate(ic, "西安") >= 0);
        FCITX_ASSERT(checkedActionsAre({xiAction.id()}));

        // Pinyin and single char can be checked at the same time.
        FCITX_ASSERT(singleAction.text() == "单字");
        tabbed->triggerTabAction(singleAction.id());
        FCITX_ASSERT(findCandidate(ic, "西安") < 0);
        FCITX_ASSERT(checkedActionsAre({xiAction.id(), singleAction.id()}));

        // Trigger single char action again should only uncheck itself.
        tabbed->triggerTabAction(singleAction.id());
        FCITX_ASSERT(findCandidate(ic, "西安") >= 0);
        FCITX_ASSERT(checkedActionsAre({xiAction.id()}));

        tabbed->triggerTabAction(singleAction.id());
        FCITX_ASSERT(findCandidate(ic, "西安") < 0);
        FCITX_ASSERT(checkedActionsAre({xiAction.id(), singleAction.id()}));

        // Trigger pinyin action again should only uncheck itself.
        tabbed->triggerTabAction(xiAction.id());
        FCITX_ASSERT(findCandidate(ic, "西安") < 0);
        FCITX_ASSERT(checkedActionsAre({singleAction.id()}));

        // Stroke action should not uncheck single char action.
        tabbed->triggerTabAction(strokeAction.id());
        FCITX_ASSERT(findCandidate(ic, "西安") < 0);
        FCITX_ASSERT(checkedActionsAre({}));

        actionSpan = tabbed->tabActions();
        actions = {actionSpan.begin(), actionSpan.end()};
        FCITX_ASSERT(actions.size() >= 2);
        FCITX_ASSERT(actions[actions.size() - 2].isSeparator());
        FCITX_ASSERT(actions.back().text() == "返回");
        tabbed->triggerTabAction(actions.back().id());
        FCITX_ASSERT(findCandidate(ic, "西安") < 0);
        FCITX_ASSERT(checkedActionsAre({singleAction.id()}));

        // Trigger single char action again should uncheck it.
        tabbed->triggerTabAction(singleAction.id());
        FCITX_ASSERT(findCandidate(ic, "西安") >= 0);
        FCITX_ASSERT(checkedActionsAre({}));
    });
}

void testPinyinTabFilterWithSeparator(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *pinyin = instance->addonManager().addon("pinyin");
        FCITX_ASSERT(pinyin);
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("x"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("'"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("'"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("a"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        findAndSelectCandidate(ic, "西");
        auto *tabbed = ic->inputPanel().candidateList()->toTabbed();
        FCITX_ASSERT(tabbed);
        auto actionSpan = tabbed->tabActions();
        FCITX_ASSERT(std::ranges::none_of(actionSpan, [](const auto &a) {
            return a.text().starts_with('\'');
        }));
    });
}

void testPin(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);

        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("t"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("o"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("g"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("y"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        auto index1 = findCandidateOrDie(ic, "同音");
        auto index2 = findCandidateOrDie(ic, "痛饮");
        const auto oldIndex1 = index1;
        const auto oldIndex2 = index2;
        FCITX_INFO() << "同音:" << index1 << " "
                     << "痛饮:" << index2;
        {
            auto *candidateList = ic->inputPanel().candidateList().get();
            // Pin the one that is after.
            const auto &cand =
                candidateList->candidate(std::max(index1, index2));
            auto *actionable = candidateList->toActionable();
            FCITX_ASSERT(actionable);
            FCITX_ASSERT(actionable->hasAction(cand));
            auto actions = actionable->candidateActions(cand);
            FCITX_ASSERT(!actions.empty());
            FCITX_ASSERT(std::ranges::any_of(
                actions, [](const auto &a) { return a.id() == 1; }));
            FCITX_ASSERT(!std::ranges::any_of(
                actions, [](const auto &a) { return a.id() == 2; }));
            // This is pin action, 痛饮 should be pined to head.
            actionable->triggerAction(cand, 1);
        }
        index1 = findCandidateOrDie(ic, "同音");
        index2 = findCandidateOrDie(ic, "痛饮");
        FCITX_INFO() << "同音:" << index1 << " "
                     << "痛饮:" << index2;
        FCITX_ASSERT(index1 != oldIndex1);
        FCITX_ASSERT(index2 != oldIndex2);
        FCITX_ASSERT(std::min(index1, index2) == 0);

        {
            auto *candidateList = ic->inputPanel().candidateList().get();
            const auto &candNew = candidateList->candidate(index2);
            auto *actionable = candidateList->toActionable();
            FCITX_ASSERT(actionable);
            FCITX_ASSERT(actionable->hasAction(candNew));
            auto actions = actionable->candidateActions(candNew);
            FCITX_ASSERT(!actions.empty());
            // Check if deletable action is there.
            FCITX_ASSERT(std::ranges::any_of(
                actions, [](const auto &a) { return a.id() == 2; }));
            // This is delete custom phrase action, 痛饮 should be pined to
            // head.
            actionable->triggerAction(candNew, 2);
        }
        index1 = findCandidateOrDie(ic, "同音");
        index2 = findCandidateOrDie(ic, "痛饮");
        FCITX_INFO() << "同音:" << index1 << " "
                     << "痛饮:" << index2;
        FCITX_ASSERT(index1 == oldIndex1);
        FCITX_ASSERT(index2 == oldIndex2);
    });
}

void testQuickPhraseTrigger(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);

        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("w"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("w"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("w"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("."), false);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == "www.");

        ic->reset();
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("b"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("b"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("s"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("."), false);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == "bbs.");

        ic->reset();
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("u"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("s"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("e"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("r"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("@"), false);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == "user@");

        ic->reset();
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("t"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("t"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("p"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key(":"), false);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == "http:");

        ic->reset();
        // htt: shouldn't trigger.
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("h"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("t"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("t"), false);

        FCITX_ASSERT(ic->inputPanel().candidateList());
        FCITX_ASSERT(!ic->inputPanel().candidateList()->empty());
        const auto firstCandidate =
            ic->inputPanel().candidateList()->candidate(0).text().toString();
        testfrontend->call<ITestFrontend::pushCommitExpectation>(
            firstCandidate);
        testfrontend->call<ITestFrontend::pushCommitExpectation>("：");

        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key(":"), false);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == "");
    });
}

void testVQuickPhraseTrigger(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");

        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);

        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("v"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("."), false);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == "v.");

        instance->setCurrentInputMethod(ic, "shuangpin", true);
        ic->reset();
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("V"), false);
        testfrontend->call<ITestFrontend::keyEvent>(uuid, Key("."), false);
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == "V.");
    });
}

void testPunctuationWithCursorAtBeginning(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        ic->setCapabilityFlags(CapabilityFlag::SurroundingText);
        instance->setCurrentInputMethod(ic, "pinyin", true);

        ic->reset();
        ic->surroundingText().setText("text", 0, 0);
        FCITX_ASSERT(ic->surroundingText().isValid());
        // Cursor zero has no preceding character.
        ic->updateSurroundingText();
        testfrontend->call<ITestFrontend::pushCommitExpectation>("。");
        FCITX_ASSERT(testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("."), false));
    });
}

void testPunctuation(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);

        testfrontend->call<ITestFrontend::pushCommitExpectation>("。");
        FCITX_ASSERT(testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("."), false));
        FCITX_ASSERT(!testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("1"), false));
        FCITX_ASSERT(!testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("."), false));
        FCITX_ASSERT(!testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("1"), false));
        FCITX_ASSERT(!testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("."), false));
        // This is cancel last eng.
        testfrontend->call<ITestFrontend::pushCommitExpectation>("。");
        FCITX_ASSERT(!testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("BackSpace"), false));

        auto event = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, now(CLOCK_MONOTONIC) + 2000000, 0,
            [instance](EventSourceTime *event, uint64_t) {
                testPunctuationPart2(instance);
                delete event;
                return true;
            });
        (void)event.release();
    });
}

void testPunctuationPart2(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(ic);
        instance->setCurrentInputMethod(ic, "pinyin", true);

        FCITX_ASSERT(!testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("1"), false));
        FCITX_ASSERT(!testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("."), false));
        FCITX_ASSERT(!testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("space"), false));
        // This should not cancel last eng.
        FCITX_ASSERT(!testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("BackSpace"), false));

        endTestEvent = instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, now(CLOCK_MONOTONIC) + 2000000, 0,
            [instance](EventSourceTime *, uint64_t) {
                instance->exit();
                return true;
            });
    });
}

} // namespace

int main(int argc, char **testArgs) {
    const bool groupedOnly =
        argc > 1 && std::string_view(testArgs[1]) == "--grouped-shuangpin";
    setupTestingEnvironment(
        TESTING_BINARY_DIR, {"bin"},
        {TESTING_BINARY_DIR "/test", TESTING_BINARY_DIR "/im",
         TESTING_BINARY_DIR "/modules", TESTING_SOURCE_DIR "/modules",
         // For pinyin/symbols.
         TESTING_SOURCE_DIR "/im", StandardPaths::fcitxPath("pkgdatadir")});
    // fcitx::Log::setLogRule("default=5,table=5,libime-table=5");
    char arg0[] = "testpinyin";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testim,testfrontend,pinyin,punctuation,"
                  "pinyinhelper,spell,quickphrase";
    char *argv[] = {arg0, arg1, arg2};
    fcitx::Log::setLogRule("default=5,pinyin=5,*=5");
    Instance instance(FCITX_ARRAY_SIZE(argv), argv);
    instance.addonManager().registerDefaultLoader(nullptr);
    setup(&instance);
    if (groupedOnly) {
        testGroupedShuangpinSchemes(&instance);
        testGroupedShuangpinEditing(&instance);
        testGroupedShuangpinRawCommit(&instance);
        testShuangpinEncodingCommit(&instance);
        testPinyinHighlightedEncodingCommit(&instance);
        testGroupedShuangpinPrivateKeyGuard(&instance);
        testGroupedShuangpinSyllableActions(&instance);
        testGroupedShuangpinPreeditModes(&instance);
        testGroupedShuangpinQuickPhraseBoundary(&instance);
        testGroupedShuangpinQuickPhraseTempMode(&instance);
        testGroupedShuangpinCustomPhraseBoundary(&instance);
        testGroupedShuangpinModeControl(&instance);
        testGroupedShuangpinHardwareSwitch(&instance);
        testGroupedShuangpinContextIsolation(&instance);
        testGroupedShuangpinCorrectionScope(&instance);
        testCustomGroupedShuangpinLayouts(&instance);
        testCustomGroupedShuangpinAtomicConfig(&instance);
        testCustomGroupedShuangpinSyllableActions(&instance);
        testCustomGroupedShuangpinSemicolonAndIsolation(&instance);
        instance.eventDispatcher().schedule([&instance]() { instance.exit(); });
        instance.exec();
        return 0;
    }
    testBasic(&instance);
    testSelectByChar(&instance);
    testUppercase(&instance);
    testGroupedShuangpinSchemes(&instance);
    testGroupedShuangpinEditing(&instance);
    testGroupedShuangpinRawCommit(&instance);
    testShuangpinEncodingCommit(&instance);
    testPinyinHighlightedEncodingCommit(&instance);
    testGroupedShuangpinPrivateKeyGuard(&instance);
    testGroupedShuangpinSyllableActions(&instance);
    testGroupedShuangpinPreeditModes(&instance);
    testGroupedShuangpinQuickPhraseBoundary(&instance);
    testGroupedShuangpinQuickPhraseTempMode(&instance);
    testGroupedShuangpinCustomPhraseBoundary(&instance);
    testGroupedShuangpinModeControl(&instance);
    testGroupedShuangpinHardwareSwitch(&instance);
    testGroupedShuangpinContextIsolation(&instance);
    testGroupedShuangpinCorrectionScope(&instance);
    testCustomGroupedShuangpinLayouts(&instance);
    testCustomGroupedShuangpinAtomicConfig(&instance);
    testCustomGroupedShuangpinSyllableActions(&instance);
    testCustomGroupedShuangpinSemicolonAndIsolation(&instance);
    testTwoKeyShuangpinBalance(&instance);
    testForget(&instance);
    testActionInStrokeFilter(&instance);
    testPinyinTabFilter(&instance);
    testPinyinTabFilterWithSeparator(&instance);
    testPin(&instance);
    testQuickPhraseTrigger(&instance);
    testVQuickPhraseTrigger(&instance);
    testPunctuationWithCursorAtBeginning(&instance);
    testPunctuation(&instance);
    instance.exec();
    endTestEvent.reset();
    return 0;
}
