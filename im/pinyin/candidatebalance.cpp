/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "candidatebalance.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fcitx-utils/charutils.h>
#include <initializer_list>
#include <iterator>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fcitx {

namespace {

// Everyday English words of two to four letters, sorted for binary search.
// They are the most frequent short entries of the spell dictionary, without
// names, abbreviations and tokenizer fragments such as "ca" or "wo".
constexpr std::string_view commonShortEnglishWords[] = {
    "able", "act",  "acts", "add",  "adds", "age",  "ages", "ago",  "aid",
    "air",  "all",  "also", "am",   "an",   "and",  "any",  "app",  "are",
    "area", "arm",  "army", "art",  "as",   "ask",  "asks", "at",   "aunt",
    "away", "baby", "back", "bad",  "bag",  "bags", "ball", "band", "bank",
    "bar",  "bars", "base", "be",   "bear", "beat", "bed",  "been", "beer",
    "bell", "belt", "bent", "best", "bet",  "big",  "bike", "bird", "bit",
    "blow", "blue", "boat", "body", "bone", "book", "born", "boss", "both",
    "bowl", "box",  "boy",  "boys", "bug",  "burn", "bus",  "busy", "but",
    "buy",  "by",   "bye",  "cake", "call", "calm", "came", "camp", "can",
    "car",  "card", "care", "cars", "case", "cash", "cast", "cat",  "cell",
    "city", "clay", "clip", "club", "coat", "code", "cold", "come", "cook",
    "cool", "copy", "core", "corn", "cost", "crew", "cry",  "cup",  "cups",
    "cut",  "cuts", "dad",  "dark", "data", "date", "dawn", "day",  "days",
    "dead", "deal", "dear", "debt", "deck", "deep", "deer", "desk", "did",
    "die",  "died", "diet", "dirt", "dish", "do",   "does", "dog",  "dogs",
    "done", "door", "down", "draw", "drew", "drop", "dry",  "due",  "dust",
    "duty", "each", "ear",  "earn", "ears", "ease", "east", "easy", "eat",
    "edge", "egg",  "eggs", "else", "end",  "ends", "era",  "even", "ever",
    "evil", "eye",  "eyes", "face", "fact", "fail", "fair", "fall", "fame",
    "fan",  "fans", "far",  "farm", "fast", "fat",  "fate", "fear", "feed",
    "feel", "fees", "feet", "fell", "felt", "few",  "file", "fill", "film",
    "find", "fine", "fire", "firm", "fish", "fit",  "five", "fix",  "flag",
    "flat", "flow", "fly",  "food", "foot", "for",  "form", "four", "fox",
    "free", "from", "fuel", "full", "fun",  "fund", "gain", "game", "gang",
    "gap",  "gas",  "gate", "gave", "gaze", "gear", "get",  "gets", "gift",
    "girl", "give", "glad", "go",   "goal", "goes", "gold", "golf", "gone",
    "good", "got",  "gray", "grew", "grow", "gun",  "guy",  "guys", "had",
    "hair", "half", "hall", "hand", "hang", "hard", "harm", "has",  "hat",
    "hate", "have", "he",   "head", "hear", "heat", "held", "help", "her",
    "here", "hero", "hey",  "hi",   "hide", "high", "hill", "him",  "his",
    "hit",  "hits", "hold", "hole", "holy", "home", "hope", "host", "hot",
    "hour", "how",  "huge", "hung", "hunt", "hurt", "ice",  "idea", "if",
    "ill",  "in",   "into", "iron", "is",   "it",   "item", "its",  "jazz",
    "job",  "jobs", "join", "joke", "joy",  "jump", "just", "keep", "kept",
    "key",  "kick", "kid",  "kids", "kind", "king", "kiss", "knee", "knew",
    "know", "lab",  "lack", "lady", "lake", "land", "last", "late", "law",
    "laws", "lay",  "lead", "left", "leg",  "legs", "less", "let",  "lie",
    "lies", "life", "lift", "like", "line", "link", "lips", "list", "live",
    "load", "loan", "long", "look", "lose", "loss", "lost", "lot",  "lots",
    "loud", "love", "low",  "luck", "mad",  "made", "mail", "main", "make",
    "male", "man",  "many", "map",  "mark", "mass", "math", "may",  "me",
    "meal", "mean", "meat", "meet", "men",  "mere", "mess", "mile", "milk",
    "mind", "mine", "miss", "mix",  "mom",  "mood", "moon", "more", "most",
    "move", "much", "must", "my",   "name", "navy", "near", "neck", "need",
    "net",  "new",  "news", "next", "nice", "nine", "no",   "none", "nose",
    "not",  "note", "now",  "odd",  "of",   "off",  "oh",   "oil",  "ok",
    "okay", "old",  "on",   "once", "one",  "ones", "only", "onto", "open",
    "or",   "our",  "out",  "oven", "over", "own",  "pace", "pack", "page",
    "paid", "pain", "pair", "pale", "palm", "pan",  "park", "part", "pass",
    "past", "path", "pay",  "peak", "pick", "pink", "plan", "play", "plus",
    "poll", "pool", "poor", "pop",  "port", "post", "pull", "pure", "push",
    "put",  "puts", "quit", "race", "rain", "rare", "rate", "raw",  "read",
    "real", "rear", "red",  "rely", "rest", "rice", "rich", "ride", "ring",
    "rise", "risk", "road", "rock", "role", "roll", "roof", "room", "root",
    "rose", "row",  "rule", "run",  "runs", "rush", "sad",  "safe", "said",
    "sale", "salt", "same", "sand", "save", "saw",  "say",  "says", "sea",
    "seat", "see",  "seek", "seem", "seen", "sees", "self", "sell", "send",
    "sent", "set",  "sets", "she",  "ship", "shop", "shot", "show", "shut",
    "sick", "side", "sign", "sing", "sir",  "sit",  "site", "sits", "six",
    "size", "skin", "sky",  "slow", "snow", "so",   "soft", "soil", "sold",
    "some", "son",  "song", "sons", "soon", "sort", "soul", "soup", "spot",
    "star", "stay", "step", "stir", "stop", "such", "suit", "sun",  "sure",
    "take", "talk", "tall", "tank", "tape", "task", "tax",  "tea",  "team",
    "tech", "tell", "ten",  "tend", "term", "test", "text", "than", "that",
    "the",  "them", "then", "they", "thin", "this", "thus", "tie",  "tied",
    "ties", "till", "time", "tiny", "tip",  "tips", "to",   "told", "tone",
    "too",  "took", "tool", "top",  "tour", "town", "tree", "trip", "true",
    "try",  "turn", "two",  "type", "unit", "up",   "upon", "us",   "use",
    "used", "uses", "van",  "vast", "very", "view", "vote", "wait", "wake",
    "walk", "wall", "want", "war",  "warm", "wars", "was",  "wash", "wave",
    "way",  "ways", "we",   "weak", "wear", "web",  "week", "well", "went",
    "were", "west", "wet",  "what", "when", "who",  "whom", "why",  "wide",
    "wife", "wild", "will", "win",  "wind", "wine", "wing", "wire", "wish",
    "with", "wolf", "wood", "word", "wore", "work", "wow",  "yard", "yeah",
    "year", "yes",  "yet",  "you",  "your", "zero", "zone"};

static_assert(std::ranges::is_sorted(commonShortEnglishWords));

bool isLoose(CandidateKind kind) {
    return kind == CandidateKind::LooseSingle ||
           kind == CandidateKind::LoosePhrase;
}

// Whether the candidate spells exactly the input, or was put in its place
// regardless of the input.
bool spellsInput(CandidateKind kind) {
    return !isLoose(kind) && kind != CandidateKind::RareEnglish;
}

// Whether the kind uses up the front page allowance for words.
bool isWord(CandidateKind kind) {
    switch (kind) {
    case CandidateKind::Phrase:
    case CandidateKind::Sentence:
    case CandidateKind::LoosePhrase:
    case CandidateKind::CommonEnglish:
    case CandidateKind::PinnedPhrase:
        return true;
    case CandidateKind::Single:
    case CandidateKind::LooseSingle:
    case CandidateKind::RareEnglish:
    case CandidateKind::PinnedSingle:
        break;
    }
    return false;
}

// The candidates in engine order, except that a guessed single character
// gives the lead to the first character or dictionary word the input spells.
std::vector<size_t> leadingOrder(std::span<const CandidateInfo> candidates) {
    std::vector<size_t> order(candidates.size());
    std::iota(order.begin(), order.end(), size_t{0});
    if (candidates.empty() ||
        candidates.front().kind != CandidateKind::LooseSingle) {
        return order;
    }
    const auto lead =
        std::ranges::find_if(candidates, [](const CandidateInfo &candidate) {
            return candidate.kind == CandidateKind::Single ||
                   candidate.kind == CandidateKind::Phrase;
        });
    if (lead != candidates.end()) {
        const auto index = std::distance(candidates.begin(), lead);
        std::rotate(order.begin(), order.begin() + index,
                    order.begin() + index + 1);
    }
    return order;
}

// Counts the slots of the front page while candidates are placed.
class FrontPage {
public:
    explicit FrontPage(size_t size) : size_(size) {}

    bool full() const { return used_ >= size_; }

    // Only the first run of symbols may use a front-page slot. Other runs
    // wait until after the front page, without being folded into that run.
    CandidatePlacement place(size_t index, const CandidateInfo &candidate) {
        used_ += 1 + candidate.attachedCount;
        const bool symbolOnFront = candidate.hasSymbol && !full();
        const bool foldSymbols = symbolOnFront && !hasSymbol_;
        const bool deferSymbols = symbolOnFront && hasSymbol_;
        if (foldSymbols) {
            used_++;
            hasSymbol_ = true;
        }
        return {index, foldSymbols, deferSymbols};
    }

private:
    size_t size_;
    size_t used_ = 0;
    bool hasSymbol_ = false;
};

// Groups of the candidates behind the front page of a two key input, in
// their order.
enum RestGroup : size_t {
    RestPinned,
    RestSingle,
    RestPhrase,
    RestRareEnglish,
    RestLoose,
    RestGroupCount,
};

RestGroup restGroup(CandidateKind kind) {
    switch (kind) {
    case CandidateKind::PinnedSingle:
    case CandidateKind::PinnedPhrase:
        return RestPinned;
    case CandidateKind::Single:
        return RestSingle;
    case CandidateKind::Phrase:
    case CandidateKind::Sentence:
    case CandidateKind::CommonEnglish:
        return RestPhrase;
    case CandidateKind::RareEnglish:
        return RestRareEnglish;
    case CandidateKind::LooseSingle:
    case CandidateKind::LoosePhrase:
        break;
    }
    return RestLoose;
}

} // namespace

std::vector<CandidatePlacement>
balanceTwoKeyCandidates(std::span<const CandidateInfo> candidates,
                        const TwoKeyBalanceLimits &limits) {
    std::vector<CandidatePlacement> result;
    result.reserve(candidates.size());
    std::array<std::vector<size_t>, RestGroupCount> rest;

    FrontPage front(limits.frontSize);
    size_t phrases = 0;
    const auto placeOnFront = [&](size_t index) {
        if (isWord(candidates[index].kind)) {
            phrases++;
        }
        result.push_back(front.place(index, candidates[index]));
    };

    // Whether a word was held back. Only the characters of the syllable may
    // overtake it, so the candidates the engine ranks below it wait as well.
    bool held = false;
    for (const auto index : leadingOrder(candidates)) {
        const auto kind = candidates[index].kind;
        bool onFront = !front.full() && kind != CandidateKind::RareEnglish;
        // A pinned word stays even when the allowance is used up.
        if (onFront && isWord(kind) && kind != CandidateKind::PinnedPhrase &&
            phrases >= limits.frontPhrases) {
            onFront = false;
            held = true;
        }
        if (held && isLoose(kind)) {
            onFront = false;
        }
        if (onFront) {
            placeOnFront(index);
        } else {
            rest[restGroup(kind)].push_back(index);
        }
    }

    // Too few characters to fill the front page: the keys abbreviate a word
    // then, so the words that were held back take the room that is left.
    for (const auto group : {RestPhrase, RestLoose}) {
        auto &indices = rest[group];
        auto next = indices.begin();
        for (; next != indices.end() && !front.full(); ++next) {
            placeOnFront(*next);
        }
        indices.erase(indices.begin(), next);
    }

    for (const auto &group : rest) {
        for (const auto index : group) {
            result.push_back({index, false});
        }
    }
    return result;
}

std::vector<CandidatePlacement>
placeCandidatesInOrder(std::span<const CandidateInfo> candidates,
                       size_t frontSize) {
    FrontPage front(frontSize);
    std::vector<CandidatePlacement> result;
    result.reserve(candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i) {
        result.push_back(front.place(i, candidates[i]));
    }
    return result;
}

std::vector<CandidatePlacement>
arrangeCandidates(std::span<const CandidateInfo> candidates, size_t frontSize) {
    std::vector<CandidatePlacement> result;
    result.reserve(candidates.size());

    FrontPage front(frontSize);
    // Rare English words that are still waiting for their place.
    std::vector<size_t> waiting;
    const auto release = [&]() {
        for (const auto index : waiting) {
            result.push_back(front.place(index, candidates[index]));
        }
        waiting.clear();
    };
    auto pending =
        std::ranges::count_if(candidates, spellsInput, &CandidateInfo::kind);
    for (const auto index : leadingOrder(candidates)) {
        const auto &candidate = candidates[index];
        if (candidate.kind == CandidateKind::RareEnglish) {
            waiting.push_back(index);
            continue;
        }
        result.push_back(front.place(index, candidate));
        if (spellsInput(candidate.kind)) {
            pending--;
        }
        if (pending == 0 && front.full()) {
            release();
        }
    }
    release();
    return result;
}

FirstPageCandidateOrder
abbreviationFirstPageOrder(std::span<const FirstPageCandidateInfo> candidates,
                           size_t capacity) {
    const auto count = candidates.size();
    std::vector<size_t> order(count);
    std::iota(order.begin(), order.end(), size_t{0});
    const auto best = std::ranges::find_if(candidates, &FirstPageCandidateInfo::bestAbbreviation);
    capacity = std::min(capacity, count);
    if (!capacity || best == candidates.end()) return {std::move(order), capacity};
    const auto bestIndex = static_cast<size_t>(best - candidates.begin());
    std::vector<bool> used(count, false);
    std::vector<bool> reserved(count, false);
    for (size_t index = 0; index < count; ++index) {
        if (candidates[index].fixed) used[index] = reserved[index] = true;
    }
    const auto fallback = std::ranges::find_if(candidates, &FirstPageCandidateInfo::fallback);
    const size_t fallbackSlot = capacity > 1 ? 1 : 0;
    if (!reserved[fallbackSlot] && fallback != candidates.end()) {
        const auto index = static_cast<size_t>(fallback - candidates.begin());
        if (!candidates[index].fixed && index != bestIndex) {
            order[fallbackSlot] = index;
            used[index] = reserved[fallbackSlot] = true;
        }
    }
    std::vector<size_t> frontSlots;
    for (size_t index = 0; index < capacity; ++index)
        if (!reserved[index]) frontSlots.push_back(index);
    const auto delayed = [&](size_t index) {
        return (candidates[index].abbreviation && index != bestIndex) ||
               candidates[index].deferredFallback || candidates[index].rareEnglish ||
               (candidates[index].rareCompletion && index != bestIndex);
    };
    std::vector<size_t> front;
    for (size_t index = 0; index < count && front.size() < frontSlots.size(); ++index)
        if (!used[index] && !delayed(index)) front.push_back(index);
    if (!used[bestIndex] && !frontSlots.empty() &&
        std::ranges::find(front, bestIndex) == front.end()) {
        if (front.size() < frontSlots.size()) front.push_back(bestIndex);
        else front.back() = bestIndex;
    }
    for (size_t index = 0; index < front.size(); ++index) {
        order[frontSlots[index]] = front[index];
        used[front[index]] = reserved[frontSlots[index]] = true;
    }
    std::vector<size_t> rest;
    for (const bool delay : {false, true})
        for (size_t index = 0; index < count; ++index)
            if (!used[index] && delayed(index) == delay) rest.push_back(index);
    size_t next = 0;
    for (size_t index = 0; index < count; ++index)
        if (!reserved[index]) order[index] = rest[next++];
    size_t fixedBoundary = 0;
    for (size_t ordinal = 0; ordinal < capacity; ++ordinal)
        if (candidates[order[ordinal]].fixed) fixedBoundary = ordinal + 1;
    for (size_t ordinal = 0; ordinal < capacity; ++ordinal) {
        const auto index = order[ordinal];
        if (!candidates[index].fixed && !candidates[index].bestAbbreviation && delayed(index)) {
            // A fixed user slot outranks hiding an extra abbreviation needed
            // to occupy the preceding page position. No placeholders are added.
            capacity = std::max(ordinal, fixedBoundary);
            break;
        }
    }
    return {std::move(order), capacity};
}

bool isCommonShortEnglishWord(std::string_view word) {
    constexpr size_t minLength = 2;
    constexpr size_t maxLength = 4;
    if (word.size() < minLength || word.size() > maxLength) {
        return false;
    }
    std::array<char, maxLength> buffer;
    std::ranges::transform(word, buffer.begin(), charutils::tolower);
    return std::ranges::binary_search(
        commonShortEnglishWords, std::string_view(buffer.data(), word.size()));
}

void FrequentEnglishWords::load(std::string_view data) {
    // About the 12000 most frequent words of the dictionary reach this
    // weight. Rarer ones are mostly names, abbreviations and obscure words.
    constexpr unsigned minWeight = 50000;
    constexpr size_t minLength = 5;
    constexpr std::string_view magic = "FSCD0000";

    words_.clear();
    // The magic is followed by the number of words, which is not needed.
    if (!data.starts_with(magic) ||
        data.size() < magic.size() + sizeof(uint32_t)) {
        return;
    }
    data.remove_prefix(magic.size() + sizeof(uint32_t));
    // Each entry is a little endian 16 bit weight and a NUL terminated word.
    while (data.size() > sizeof(uint16_t)) {
        const unsigned weight =
            static_cast<uint8_t>(data[0]) | static_cast<uint8_t>(data[1]) << 8;
        data.remove_prefix(sizeof(uint16_t));
        const auto length = data.find('\0');
        if (length == std::string_view::npos) {
            break;
        }
        if (weight >= minWeight && length >= minLength) {
            auto &word = words_.emplace_back(data.substr(0, length));
            std::ranges::transform(word, word.begin(), charutils::tolower);
        }
        data.remove_prefix(length + 1);
    }
    std::ranges::sort(words_);
}

bool FrequentEnglishWords::contains(std::string_view word) const {
    std::string lower(word);
    std::ranges::transform(lower, lower.begin(), charutils::tolower);
    return std::ranges::binary_search(words_, lower);
}

} // namespace fcitx
