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
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/eventloopinterface.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx-utils/standardpaths.h>
#include <fcitx-utils/testing.h>
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
    // Match Android's default: correction collisions in a grouped profile
    // must not hide an exact syllable's multi-character candidates.
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
        findAndSelectCandidate(ic, "好");
    });
}

void testGroupedShuangpinRawCommit(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid =
            frontend->call<ITestFrontend::createInputContext>("grouped-raw");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        sendGroupedInput(frontend, ic, "ni");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "bn io");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().cursor() == 5);
        frontend->call<ITestFrontend::pushCommitExpectation>("BN IO");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);

        sendGroupedInput(frontend, ic, "gp");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "fg p");
        frontend->call<ITestFrontend::pushCommitExpectation>("FG P");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);
    });
}

void testGroupedShuangpinPrivateKeyGuard(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "grouped-private-guard");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        const auto sym = static_cast<KeySym>(0x0200e000);
        FCITX_ASSERT(
            frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key(sym), false));
        FCITX_ASSERT(frontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(sym, KeyStates(KeyState::Virtual) | KeyState::Ctrl),
            false));
        FCITX_ASSERT(!ic->inputPanel().candidateList());

        instance->setCurrentInputMethod(ic, "pinyin", true);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("n"), false);
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("i"), false);
        const auto preedit = ic->inputPanel().preedit().toString();
        FCITX_ASSERT(frontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(sym, KeyState::Virtual), false));
        FCITX_ASSERT(ic->inputPanel().preedit().toString() == preedit);
        frontend->call<ITestFrontend::pushCommitExpectation>("ni");
        frontend->call<ITestFrontend::keyEvent>(uuid, Key("Return"), false);
    });
}

void testGroupedShuangpinSyllableActions(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *frontend = instance->addonManager().addon("testfrontend");
        const auto uuid = frontend->call<ITestFrontend::createInputContext>(
            "grouped-syllable-actions");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        configureGroupedShuangpin(instance, ic, "Xiaohe");
        sendGroupedInput(frontend, ic, "ni");
        auto *tabbed = ic->inputPanel().candidateList()->toTabbed();
        FCITX_ASSERT(tabbed);
        const auto actions = tabbed->tabActions();
        const auto niAction = std::ranges::find_if(
            actions, [](const auto &action) { return action.text() == "ni"; });
        FCITX_ASSERT(niAction != actions.end());
        tabbed->triggerTabAction(niAction->id());
        frontend->call<ITestFrontend::pushCommitExpectation>("你");
        findAndSelectCandidate(ic, "你");
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
         StandardPaths::fcitxPath("pkgdatadir")});
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
        testGroupedShuangpinPrivateKeyGuard(&instance);
        testGroupedShuangpinSyllableActions(&instance);
        testGroupedShuangpinQuickPhraseBoundary(&instance);
        testGroupedShuangpinQuickPhraseTempMode(&instance);
        testGroupedShuangpinCustomPhraseBoundary(&instance);
        testGroupedShuangpinModeControl(&instance);
        testGroupedShuangpinHardwareSwitch(&instance);
        testGroupedShuangpinContextIsolation(&instance);
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
    testGroupedShuangpinPrivateKeyGuard(&instance);
    testGroupedShuangpinSyllableActions(&instance);
    testGroupedShuangpinQuickPhraseBoundary(&instance);
    testGroupedShuangpinQuickPhraseTempMode(&instance);
    testGroupedShuangpinCustomPhraseBoundary(&instance);
    testGroupedShuangpinModeControl(&instance);
    testGroupedShuangpinHardwareSwitch(&instance);
    testGroupedShuangpinContextIsolation(&instance);
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
