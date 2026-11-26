/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "englishprediction.h"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <tuple>
#include <utility>

namespace fcitx {

bool EnglishPredictor::load(std::istream &stream) {
    words_.clear();
    const std::string bytes{std::istreambuf_iterator<char>(stream),
                            std::istreambuf_iterator<char>()};
    std::string_view data(bytes);
    if (!data.starts_with("FSCD0000") || data.size() < 12) {
        return false;
    }
    const auto byte = [&](size_t index) {
        return static_cast<uint32_t>(static_cast<uint8_t>(data[index]));
    };
    const auto count = byte(8) | byte(9) << 8 | byte(10) << 16 | byte(11) << 24;
    data.remove_prefix(12);
    std::vector<EnglishPrediction> words;
    // The count must fit at least one weight, letter and NUL per entry.
    if (count > data.size() / 4) {
        return false;
    }
    words.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (data.size() < 3) {
            return false;
        }
        const unsigned weight =
            static_cast<uint8_t>(data[0]) | static_cast<uint8_t>(data[1]) << 8;
        data.remove_prefix(2);
        const auto length = data.find(char(0));
        if (length == std::string_view::npos || length == 0) {
            return false;
        }
        std::string word(data.substr(0, length));
        if (std::ranges::all_of(word,
                                [](unsigned char c) { return c < 0x80; })) {
            for (auto &c : word) {
                if (c >= 'A' && c <= 'Z')
                    c += 'a' - 'A';
            }
            words.push_back({std::move(word), weight});
        }
        data.remove_prefix(length + 1);
    }
    if (!data.empty()) {
        return false;
    }
    words_ = std::move(words);
    return true;
}

std::vector<EnglishPrediction>
EnglishPredictor::match(const std::vector<std::string> &letters,
                        size_t limit) const {
    if (letters.empty() || !limit ||
        std::ranges::any_of(letters,
                            [](const auto &set) { return set.empty(); })) {
        return {};
    }
    std::vector<EnglishPrediction> matches;
    for (const auto &entry : words_) {
        if (entry.word.size() < letters.size())
            continue;
        bool matchesPrefix = true;
        for (size_t i = 0; i < letters.size(); ++i) {
            if (letters[i].find(entry.word[i]) == std::string::npos) {
                matchesPrefix = false;
                break;
            }
        }
        if (matchesPrefix)
            matches.push_back(entry);
    }
    const auto rank = [&](const auto &entry) {
        return std::tuple{entry.word.size() != letters.size(),
                          -static_cast<int>(entry.frequency), entry.word.size(),
                          entry.word};
    };
    const auto less = [&](const auto &left, const auto &right) {
        return rank(left) < rank(right);
    };
    const auto end = matches.begin() + std::min(limit, matches.size());
    std::partial_sort(matches.begin(), end, matches.end(), less);
    matches.erase(end, matches.end());
    return matches;
}

} // namespace fcitx
