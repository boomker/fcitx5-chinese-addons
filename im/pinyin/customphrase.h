/*
 * SPDX-FileCopyrightText: 2023-2023 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _PINYIN_CUSTOMPHRASE_H_
#define _PINYIN_CUSTOMPHRASE_H_

#include <cstddef>
#include <cstdint>
#include <fcitx-utils/macros.h>
#include <functional>
#include <istream>
#include <libime/core/datrie.h>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fcitx {

class CustomPhrase {
public:
    explicit CustomPhrase(int order, std::string value)
        : order_(order), value_(std::move(value)) {}
    FCITX_INLINE_DEFINE_DEFAULT_DTOR_COPY_AND_MOVE_WITH_SPEC(CustomPhrase,
                                                             noexcept)

    int order() const { return order_; }
    const std::string &value() const { return value_; }
    void setOrder(int order) { order_ = order; }
    std::string &mutableValue() { return value_; }

    bool isDynamic() const;

    std::string evaluate(const std::function<std::string(std::string_view key)>
                             &evaluator) const;

    static std::string builtinEvaluator(std::string_view key);

private:
    int order_ = -1;
    std::string value_;
};

class CustomPhraseDict {
public:
    using TrieType = libime::DATrie<uint32_t>;
    CustomPhraseDict();

    void load(std::istream &in, bool loadDisabled = false);
    void save(std::ostream &out) const;
    void clear();

    const std::vector<CustomPhrase> *lookup(std::string_view key) const;

    void addPhrase(std::string_view key, std::string_view value, int order);
    void pinPhrase(std::string_view key, std::string_view value);
    void removePhrase(std::string_view key, std::string_view value);

    template <typename T>
    void foreach(const T &callback) {
        std::string buf;
        index_.foreach([this, &buf, &callback](uint32_t index, size_t len,
                                               TrieType::position_type pos) {
            index_.suffix(buf, len, pos);
            callback(buf, data_[index]);
            return true;
        });
    }

    template <typename T>
    bool foreachMatchingPrefix(const std::vector<std::string> &letters,
                               const T &callback) const {
        if (letters.empty()) {
            return true;
        }
        using Prefix = std::pair<TrieType::position_type, std::string>;
        std::vector<Prefix> prefixes{{0, {}}};
        for (const auto &group : letters) {
            std::vector<Prefix> next;
            for (const auto &[position, prefix] : prefixes) {
                for (const auto letter : group) {
                    const auto append = [&](char candidate) {
                        auto nextPosition = position;
                        const auto result = index_.traverseRaw(
                            std::string_view(&candidate, 1), nextPosition);
                        if (!TrieType::isNoPathRaw(result)) {
                            next.emplace_back(nextPosition,
                                              prefix + candidate);
                        }
                    };
                    append(letter);
                    if (letter >= 'a' && letter <= 'z') {
                        append(letter - 'a' + 'A');
                    } else if (letter >= 'A' && letter <= 'Z') {
                        append(letter - 'A' + 'a');
                    }
                }
            }
            prefixes = std::move(next);
            if (prefixes.empty()) {
                return true;
            }
        }

        std::string suffix;
        for (const auto &[position, prefix] : prefixes) {
            if (!index_.foreach(
                    [this, &suffix, &prefix, &callback](
                        uint32_t index, size_t length,
                        TrieType::position_type suffixPosition) {
                        index_.suffix(suffix, length, suffixPosition);
                        return callback(prefix + suffix, data_[index]);
                    },
                    position)) {
                return false;
            }
        }
        return true;
    }

private:
    std::vector<CustomPhrase> *getOrCreateEntry(std::string_view key);
    TrieType index_;
    std::vector<std::vector<CustomPhrase>> data_;
};

} // namespace fcitx

#endif // _PINYIN_SYMBOLDICTIONARY_H_
