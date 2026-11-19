/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "shuangpincandidate.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fcitx-utils/charutils.h>
#include <fcitx-utils/utf8.h>
#include <libime/pinyin/pinyincorrectionprofile.h>
#include <libime/pinyin/pinyindecoder.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinencoder.h>
#include <libime/pinyin/pinyinime.h>
#include <libime/pinyin/shuangpinprofile.h>
#include <numeric>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace fcitx {
namespace {
constexpr std::string_view SelectionMarker = "<shuangpin-selection>";

// Conservative defaults, not empirically tuned keyboard probabilities. A static
// prior remains half of the mixture, and context cannot amplify it over 100x.
constexpr float ContextProbabilityWeight = 0.5F;
constexpr float MaximumContextLogBoost = 2.0F;
constexpr float MaximumAlternativeLogGap = 1.0F;
constexpr float MinimumAlternativeContextLift = 0.15F;

struct ScoredCorrection {
    ShuangpinCorrection correction;
    float unigram;
    float contextual;
    float combined;
    int32_t selections;
};

bool isContextBoundary(std::string_view word) {
    for (auto it = word.begin(); it != word.end();) {
        uint32_t code;
        it = utf8::getNextChar(it, word.end(), &code);
        if (!utf8::isValidChar(code) || code <= 0x20 ||
            (code < 0x80 && std::ispunct(static_cast<unsigned char>(code))) ||
            (code >= 0x2000 && code <= 0x206f) ||
            (code >= 0x3000 && code <= 0x303f) ||
            (code >= 0xff00 && code <= 0xff65)) {
            return true;
        }
    }
    return word.empty();
}

float contextualMixture(float unigram, float contextual) {
    contextual = std::min(contextual, unigram + MaximumContextLogBoost);
    const auto largest = std::max(unigram, contextual);
    return largest + std::log10((1.0F - ContextProbabilityWeight) *
                                    std::pow(10.0F, unigram - largest) +
                                ContextProbabilityWeight *
                                    std::pow(10.0F, contextual - largest));
}

bool betterCorrection(const ScoredCorrection &a, const ScoredCorrection &b) {
    if (a.selections != b.selections) {
        return a.selections > b.selections;
    }
    if (a.combined != b.combined) {
        return a.combined > b.combined;
    }
    if (a.unigram != b.unigram) {
        return a.unigram > b.unigram;
    }
    return std::tie(a.correction.word, a.correction.encodedPinyin,
                    a.correction.code) < std::tie(b.correction.word,
                                                  b.correction.encodedPinyin,
                                                  b.correction.code);
}
} // namespace

std::string shuangpinSelectionScope(const libime::ShuangpinProfile &profile,
                                    std::string_view keys) {
    uint64_t hash = 14695981039346656037ULL;
    const auto add = [&hash](unsigned char byte) {
        hash = (hash ^ byte) * 1099511628211ULL;
    };
    for (const auto &[code, syllables] : profile.table()) {
        for (const auto &[syllable, flags] : syllables) {
            if (flags)
                continue;
            for (const auto key : code)
                add(static_cast<unsigned char>(key));
            add(0);
            add(static_cast<unsigned char>(syllable.initial()));
            add(static_cast<unsigned char>(syllable.final()));
        }
    }
    return "v1:" + std::to_string(hash) + ":" + std::string(keys);
}

bool hasShuangpinSelections(const libime::HistoryBigram &history,
                            std::string_view scope) {
    return !scope.empty() &&
           history.rawUnigramFrequency({SelectionMarker, scope}) > 0;
}

ShuangpinCorrectionPlacement
shuangpinCorrectionPlacement(const libime::HistoryBigram &history,
                             const ShuangpinCorrectionPlacements &placements,
                             std::string_view scope) {
    if (const auto it = placements.find(std::string(scope));
        it != placements.end()) {
        return it->second;
    }
    return hasShuangpinSelections(history, scope)
               ? ShuangpinCorrectionPlacement::Adaptive
               : ShuangpinCorrectionPlacement::Default;
}

ShuangpinCorrectionPlacement resetShuangpinCorrection(
    libime::HistoryBigram &history, ShuangpinCorrectionPlacements &placements,
    std::string_view scope, std::string_view word, bool isOnFirstPage) {
    if (scope.empty() || word.empty()) {
        return shuangpinCorrectionPlacement(history, placements, scope);
    }
    const std::string key(scope);
    const auto existing = placements.find(key);
    const bool firstReset = existing == placements.end();
    const bool learned =
        firstReset && history.rawUnigramFrequency({word, scope}) > 0;
    history.forget(word, scope);
    auto placement = ShuangpinCorrectionPlacement::Deferred;
    if ((firstReset && !isOnFirstPage) ||
        (!firstReset &&
         existing->second == ShuangpinCorrectionPlacement::AfterLegal)) {
        placement = ShuangpinCorrectionPlacement::AfterLegal;
    } else if (learned) {
        placement = ShuangpinCorrectionPlacement::Default;
    }
    placements[key] = placement;
    return placement;
}

void learnShuangpinSelection(libime::HistoryBigram &history,
                             std::string_view scope, std::string_view word) {
    if (scope.empty() || word.empty())
        return;
    // A coded marker in the same observation tracks the scope even when the
    // selected word is no longer in the active dictionaries. Forgetting the
    // word removes both records through HistoryBigram's existing sentence API.
    history.addWithCode({{std::string(word), std::string(scope)},
                         {std::string(SelectionMarker), std::string(scope)}});
}

std::vector<size_t>
rankShuangpinCandidates(std::span<const std::string_view> words,
                        const libime::HistoryBigram &history,
                        std::string_view scope) {
    std::vector<size_t> order(words.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::vector<int32_t> counts;
    for (const auto word : words) {
        counts.push_back(history.rawUnigramFrequency({word, scope}));
    }
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return counts[a] > counts[b]; });
    return order;
}

std::vector<ShuangpinCorrection>
shuangpinCorrections(const libime::PinyinContext &context,
                     const libime::HistoryBigram *history) {
    const auto profile = context.shuangpinProfile();
    if (!context.useShuangpin() || !profile || profile->isGrouped() ||
        context.cursor() != context.size() ||
        context.nextSyllableOffset() != context.selectedLength()) {
        return {};
    }
    const auto keys =
        std::string_view(context.userInput()).substr(context.selectedLength());
    if (keys.size() != 2 || std::ranges::any_of(keys, [](char c) {
            return !charutils::islower(c) && c != ';';
        })) {
        return {};
    }
    const auto exact =
        libime::PinyinEncoder::shuangpinToSyllablesWithFuzzyFlags(
            keys, *profile, libime::PinyinFuzzyFlag::None);
    if (std::ranges::any_of(exact, [](const auto &initial) {
            return initial.first != libime::PinyinInitial::Invalid &&
                   std::ranges::any_of(initial.second, [](const auto &final) {
                       return final.first != libime::PinyinFinal::Invalid;
                   });
        })) {
        return {};
    }

    static const libime::PinyinCorrectionProfile correction(
        libime::BuiltinPinyinCorrectionProfile::Qwerty);
    const auto *ime = context.ime();
    const auto *model = ime->model();
    const auto scope = shuangpinSelectionScope(*profile, keys);
    auto languageState = model->libime::LanguageModel::nullState();
    auto words = context.contextWords();
    const auto selectedWords = context.selectedWords();
    words.insert(words.end(), selectedWords.begin(), selectedWords.end());
    bool hasContext = false;
    for (const auto &word : words) {
        const auto index = model->index(word);
        if (isContextBoundary(word) || index == model->unknown()) {
            languageState = model->libime::LanguageModel::nullState();
            hasContext = false;
            continue;
        }
        libime::State next{};
        // Rebuild the static state, rather than using PinyinContext::state(),
        // so global user history and its unigram setting cannot leak in.
        model->libime::LanguageModel::score(
            languageState, libime::WordNode(word, index), next);
        languageState = next;
        hasContext = true;
    }

    std::vector<ScoredCorrection> pool;
    std::unordered_map<std::string, size_t> wordIndices;
    for (const auto &[code, syllables] : profile->table()) {
        if (code.size() != 2) {
            continue;
        }
        const auto difference =
            size_t(code[0] != keys[0]) + size_t(code[1] != keys[1]);
        if (difference != 1) {
            continue;
        }
        const auto position = code[0] == keys[0] ? 1 : 0;
        const auto neighbors = correction.correctionMap().find(code[position]);
        if (neighbors == correction.correctionMap().end() ||
            std::ranges::find(neighbors->second, keys[position]) ==
                neighbors->second.end()) {
            continue;
        }
        for (const auto &[syllable, flags] : syllables) {
            if (flags || syllable.initial() == libime::PinyinInitial::Invalid ||
                syllable.final() == libime::PinyinFinal::Invalid) {
                continue;
            }
            const std::string encoded{static_cast<char>(syllable.initial()),
                                      static_cast<char>(syllable.final())};
            ime->dict()->matchWords(
                encoded.data(), encoded.size(),
                [&](std::string_view pinyin, std::string_view word, float) {
                    if (utf8::length(word) != 1) {
                        return true;
                    }
                    libime::State out{};
                    const auto unigram = model->libime::LanguageModel::score(
                        model->libime::LanguageModel::nullState(),
                        libime::WordNode(word, model->index(word)), out);
                    if (!std::isfinite(unigram)) {
                        return true;
                    }
                    const auto contextual =
                        hasContext
                            ? model->libime::LanguageModel::score(
                                  languageState,
                                  libime::WordNode(word, model->index(word)),
                                  out)
                            : unigram;
                    const auto usableContext =
                        hasContext && std::isfinite(contextual);
                    // The profile permits same-row immediate neighbors, so
                    // every allowed one-key edit has the same input cost.
                    const auto combined =
                        usableContext ? contextualMixture(unigram, contextual)
                                      : unigram;
                    const auto count =
                        history ? history->rawUnigramFrequency({word, scope})
                                : 0;
                    ScoredCorrection candidate{
                        {std::string(word), std::string(pinyin), code},
                        unigram,
                        usableContext ? contextual : unigram,
                        combined,
                        count};
                    const auto [it, inserted] = wordIndices.emplace(
                        candidate.correction.word, pool.size());
                    if (inserted) {
                        pool.push_back(std::move(candidate));
                    } else if (betterCorrection(candidate, pool[it->second])) {
                        pool[it->second] = std::move(candidate);
                    }
                    return true;
                });
        }
    }
    if (pool.empty()) {
        return {};
    }
    const auto best =
        std::min_element(pool.begin(), pool.end(), betterCorrection);
    const auto frequencyBest = std::min_element(
        pool.begin(), pool.end(), [](const auto &a, const auto &b) {
            if (a.unigram != b.unigram)
                return a.unigram > b.unigram;
            return a.correction.word < b.correction.word;
        });
    std::vector<ScoredCorrection> retained{*best};
    const auto retain = [&](const ScoredCorrection &candidate) {
        if (std::ranges::none_of(retained, [&](const auto &other) {
                return other.correction.word == candidate.correction.word;
            })) {
            retained.push_back(candidate);
        }
    };
    retain(*frequencyBest);
    if (hasContext) {
        const auto contextBest = std::min_element(
            pool.begin(), pool.end(), [](const auto &a, const auto &b) {
                if (a.contextual != b.contextual)
                    return a.contextual > b.contextual;
                return betterCorrection(a, b);
            });
        if (contextBest->contextual >=
                contextBest->unigram + MinimumAlternativeContextLift &&
            contextBest->combined >=
                best->combined - MaximumAlternativeLogGap) {
            retain(*contextBest);
        }
    }
    std::sort(retained.begin(), retained.end(), betterCorrection);
    std::vector<ShuangpinCorrection> result;
    for (auto &candidate : retained) {
        result.push_back(std::move(candidate.correction));
    }
    return result;
}

std::optional<ShuangpinCorrection>
shuangpinCorrection(const libime::PinyinContext &context,
                    const libime::HistoryBigram *history) {
    auto corrections = shuangpinCorrections(context, history);
    if (corrections.empty()) {
        return std::nullopt;
    }
    return std::move(corrections.front());
}

bool isRareShuangpinAbbreviation(const libime::SentenceResult &candidate,
                                 const libime::UserLanguageModel &model) {
    // The language model stores base-10 log probabilities: below one in a
    // million is too weak to present a word from initials alone.
    constexpr float minimumUnigramScore = -6.0F;
    for (const auto *node : candidate.sentence()) {
        const auto &encoded =
            node->as<libime::PinyinLatticeNode>().encodedPinyin();
        if (encoded.size() <= 2 ||
            model.history().rawUnigramFrequency({node->word(), encoded}) > 0) {
            continue;
        }
        const auto &path = node->path();
        if (encoded.size() != (path.size() - 1) * 2) {
            continue;
        }
        bool abbreviated = true;
        for (size_t i = 1; i < path.size(); ++i) {
            abbreviated &= path[i]->index() - path[i - 1]->index() == 1;
        }
        if (abbreviated &&
            model.singleWordScore(node->word()) < minimumUnigramScore) {
            return true;
        }
    }
    return false;
}

} // namespace fcitx
