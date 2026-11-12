/*
 * SPDX-FileCopyrightText: 2017-2017 CSSlayer <wengxt@gmail.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "cloudpinyinmatch.h"
#include <algorithm>
#include <fcitx-utils/stringutils.h>
#include <fcitx-utils/utf8.h>
#include <iterator>
#include <libime/pinyin/pinyindecoder.h>
#include <libime/pinyin/pinyinencoder.h>
#include <libime/pinyin/pinyinime.h>
#include <stdexcept>
#include <utility>

namespace fcitx {
std::optional<CloudPinyinMatchResult>
matchCloudPinyin(const libime::PinyinContext &context,
                 const std::string &selected, const std::string &word,
                 std::optional<std::string_view> requestedFullPinyin) {
    // This ensure us to convert pinyin to the right one.
    auto sentence = context.preedit(libime::PinyinPreeditMode::RawText);
    std::string_view preedit = sentence;
    // preedit is "selected sentence" + Pinyin string.
    // Validate selected is still the same.
    if (!stringutils::consumePrefix(preedit, selected)) {
        return std::nullopt;
    }
    auto words = context.selectedWordsWithPinyin();
    auto originalSize = words.size();
    auto pinyins = stringutils::split(preedit, " '");
    std::string_view wordView = word;
    // Check word length matches pinyin length.
    // If this doesn't match, then our segmentation doesn't match the cloud
    // pinyin, so doesn't make sense to match further.
    if (pinyins.empty() || pinyins.size() != utf8::length(word)) {
        return std::nullopt;
    }
    if (requestedFullPinyin) {
        // Grouped codes have several exact readings. Keep the pronunciation
        // that was sent to the cloud, including explicit syllable
        // confirmations.
        auto requested = stringutils::split(*requestedFullPinyin, "'");
        if (requested.size() != pinyins.size() ||
            stringutils::join(requested, "'") != *requestedFullPinyin) {
            return std::nullopt;
        }
        try {
            libime::PinyinEncoder::encodeFullPinyinWithFlags(
                *requestedFullPinyin, libime::PinyinFuzzyFlag::VE_UE);
        } catch (const std::invalid_argument &) {
            return std::nullopt;
        }
        pinyins = std::move(requested);
    } else if (context.useShuangpin()) {
        for (auto &sppinyin : pinyins) {
            // Likely something goes wrong.
            if (sppinyin.size() > 2) {
                return std::nullopt;
            }
            sppinyin = libime::PinyinEncoder::shuangpinToPinyin(
                sppinyin, *context.shuangpinProfile());
            if (sppinyin.empty()) {
                return std::nullopt;
            }
        }
    }
    // If our engine based sentence can match the selected sentence, we can
    // reuse the pinyin result from engine to try to match as much as possible.
    // Ideally we can also traverse the lattice, but that's too much work.
    const auto &candidates = context.candidates();
    if (candidates.empty()) {
        return CloudPinyinMatchResult{.context = std::move(words),
                                      .word = wordView,
                                      .fullPinyin =
                                          stringutils::join(pinyins, "\'")};
    }
    auto pinyinsIter = pinyins.begin();
    auto pinyinsEnd = pinyins.end();
    const auto &bestSentence = candidates[0].sentence();
    for (const auto *segment : bestSentence) {
        auto consumed = wordView;
        if (!stringutils::consumePrefix(consumed, segment->word())) {
            break;
        }
        if (!segment->word().empty()) {
            const auto *pinyinNode =
                static_cast<const libime::PinyinLatticeNode *>(segment);
            auto pinyinSize = pinyinNode->encodedPinyin().size() / 2;
            if (pinyinSize && static_cast<size_t>(std::distance(
                                  pinyinsIter, pinyinsEnd)) >= pinyinSize) {
                pinyinsIter += pinyinSize;
                words.push_back(libime::HistoryBigram::WordWithCode{
                    segment->word(), pinyinNode->encodedPinyin()});
            } else {
                break;
            }
        }
        wordView = consumed;
    }
    // Special case for all single character, if that happens, try to learn as
    // original word.
    if (utf8::length(wordView) == 1 &&
        std::ranges::all_of(words,
                            [](const libime::HistoryBigram::WordWithCode &w) {
                                return utf8::length(w.first) == 1;
                            })) {
        words.resize(originalSize);
        wordView = word;
        pinyinsIter = pinyins.begin();
    }
    return CloudPinyinMatchResult{
        .context = std::move(words),
        .word = wordView,
        .fullPinyin = stringutils::join(pinyinsIter, pinyinsEnd, "\'")};
}

} // namespace fcitx
