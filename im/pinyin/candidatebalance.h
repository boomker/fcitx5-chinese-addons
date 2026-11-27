/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_PINYIN_CANDIDATEBALANCE_H_
#define _FCITX_PINYIN_CANDIDATEBALANCE_H_

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fcitx {

// What a candidate is, relative to the input that is still pending.
enum class CandidateKind {
    // A character spelled by exactly the input.
    Single,
    // Any other single character: a correction, a match of the leading part
    // of the input only, a character unknown to the language model.
    LooseSingle,
    // A dictionary word spelled by exactly the input.
    Phrase,
    // Several dictionary words the engine chained to spell exactly the input.
    Sentence,
    // A longer candidate that only matches through a correction, or that
    // matches the leading part of the input only.
    LoosePhrase,
    // An English word that is common enough to stay where the engine put it.
    CommonEnglish,
    // Any other English word.
    RareEnglish,
    // A single character fixed at its position by the user or the engine.
    PinnedSingle,
    // A longer candidate fixed at its position by the user or the engine.
    PinnedPhrase,
};

struct CandidateInfo {
    CandidateKind kind = CandidateKind::LooseSingle;
    // Number of extra candidates always listed after this one, including Lua
    // results and symbols when folding is disabled.
    size_t attachedCount = 0;
    // Whether a foldable run of symbols such as emoji follows this one.
    bool hasSymbol = false;
};

struct CandidatePlacement {
    // Index into the candidates that were passed in.
    size_t index = 0;
    // Whether only the first symbol of the candidate is listed, with the
    // others folded into it. All symbols are listed otherwise.
    bool foldSymbols = false;
    // An independent run that would use another front-page symbol slot is
    // listed in full immediately after the front page instead.
    bool deferSymbols = false;

    bool operator==(const CandidatePlacement &other) const = default;
};

// Both functions below take the candidates in engine order and place every
// one of them exactly once. They share two rules:
//
// - A single character that is merely a guess, such as a correction, does not
//   lead the list while the input spells a character or a dictionary word.
//   The first of those leads instead.
// - The front page has at most one symbol. The first run is folded into its
//   first symbol; independent runs that would also land there wait until
//   immediately after the front page. Runs behind it are listed in full.

struct TwoKeyBalanceLimits {
    // Number of candidates that count as the front page.
    size_t frontSize = 5;
    // Words (Chinese and English together) allowed on the front page.
    size_t frontPhrases = 2;
};

// Lay out the candidates of a two key shuangpin input. The two keys spell one
// syllable, but abbreviation matching also reads them as the initials of a two
// character word, which buries the characters of the syllable.
//
// The front page is filled in engine order with at most frontPhrases words. A
// word beyond that is held back, together with the corrections and one-key
// matches ranked below it, so that only the characters of the syllable
// overtake it. If those run out before the page is full, the keys can only
// abbreviate a word, and the held back words take the room that is left.
//
// Behind the front page come the remaining characters of the syllable, then
// the remaining words, then the rare English words, and finally everything
// else. Engine order is kept inside each of these groups.
std::vector<CandidatePlacement>
balanceTwoKeyCandidates(std::span<const CandidateInfo> candidates,
                        const TwoKeyBalanceLimits &limits = {});

// Lay out the candidates of any other input. Engine order is kept, except that
// rare English words wait until the front page is full and every candidate
// that spells exactly the input has been placed.
std::vector<CandidatePlacement>
arrangeCandidates(std::span<const CandidateInfo> candidates,
                  size_t frontSize = 5);

// Personal preference already determined the order; only apply symbol folding.
std::vector<CandidatePlacement>
placeCandidatesInOrder(std::span<const CandidateInfo> candidates,
                       size_t frontSize = 5);

struct FirstPageCandidateInfo {
    bool fixed = false;
    bool abbreviation = false;
    bool bestAbbreviation = false;
    bool fallback = false;
    bool deferredFallback = false;
    bool rareEnglish = false;
    bool rareCompletion = false;
};

struct FirstPageCandidateOrder {
    std::vector<size_t> indices;
    size_t firstPageSize = 0;
};

// Page mapping only: keep every fixed slot and the ordinary relative order.
// Reserve the second slot for recovery and one available front slot for the
// best abbreviation; other abbreviations wait behind the front page. Fixed
// user slots win when the capacity cannot satisfy all requested hints.
FirstPageCandidateOrder
abbreviationFirstPageOrder(std::span<const FirstPageCandidateInfo> candidates,
                           size_t capacity);

// Whether word is an everyday English word of two to four letters.
bool isCommonShortEnglishWord(std::string_view word);

// The words of the spell checker's English dictionary that are used often.
class FrequentEnglishWords {
public:
    // Read a dictionary in the format of spell/en_dict.fscd. Malformed data
    // leaves the words that were read up to that point.
    void load(std::string_view data);

    // Whether word is one of them, ignoring case. Only words of at least five
    // letters are kept; shorter ones are for isCommonShortEnglishWord.
    bool contains(std::string_view word) const;

private:
    std::vector<std::string> words_;
};

} // namespace fcitx

#endif // _FCITX_PINYIN_CANDIDATEBALANCE_H_
