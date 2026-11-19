/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_PINYIN_SHUANGPINCANDIDATE_H_
#define _FCITX_PINYIN_SHUANGPINCANDIDATE_H_

#include <libime/core/historybigram.h>
#include <libime/core/lattice.h>
#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyincontext.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fcitx {

struct ShuangpinCorrection {
    std::string word;
    std::string encodedPinyin;
    std::string code;
};

enum class ShuangpinCorrectionPlacement {
    Default,
    Adaptive,
    Deferred,
    AfterLegal
};
using ShuangpinCorrectionPlacements =
    std::unordered_map<std::string, ShuangpinCorrectionPlacement>;

ShuangpinCorrectionPlacement
shuangpinCorrectionPlacement(const libime::HistoryBigram &history,
                             const ShuangpinCorrectionPlacements &placements,
                             std::string_view scope);
// A first reset off the homepage moves behind all legal candidates. On-page
// learning resets to Default; resetting Default moves it off the homepage.
ShuangpinCorrectionPlacement resetShuangpinCorrection(
    libime::HistoryBigram &history, ShuangpinCorrectionPlacements &placements,
    std::string_view scope, std::string_view word, bool isOnFirstPage = true);

// Stable across restarts and correction-option changes; isolates scheme and
// input.
std::string shuangpinSelectionScope(const libime::ShuangpinProfile &profile,
                                    std::string_view keys);
bool hasShuangpinSelections(const libime::HistoryBigram &history,
                            std::string_view scope);
void learnShuangpinSelection(libime::HistoryBigram &history,
                             std::string_view scope, std::string_view word);
// Stable ties preserve the supplied engine order, including the initial
// fallback slot.
std::vector<size_t>
rankShuangpinCandidates(std::span<const std::string_view> words,
                        const libime::HistoryBigram &history,
                        std::string_view scope);

// At most three distinct neighboring-key corrections. The first is the scoped
// personal/contextual choice; other entries preserve useful static-frequency
// and contextual alternatives for later pages. This does not change input,
// shared fuzzy options, context, or dictionary history.
std::vector<ShuangpinCorrection>
shuangpinCorrections(const libime::PinyinContext &context,
                     const libime::HistoryBigram *history = nullptr);

// The primary correction, retained for callers that only need eligibility.
std::optional<ShuangpinCorrection>
shuangpinCorrection(const libime::PinyinContext &context,
                    const libime::HistoryBigram *history = nullptr);

// Fully spelled words and words the user has learned are never suppressed.
bool isRareShuangpinAbbreviation(const libime::SentenceResult &candidate,
                                 const libime::UserLanguageModel &model);

} // namespace fcitx

#endif
