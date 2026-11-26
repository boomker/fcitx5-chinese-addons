/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _PINYIN_ENGLISHPREDICTION_H_
#define _PINYIN_ENGLISHPREDICTION_H_

#include <cstddef>
#include <istream>
#include <string>
#include <vector>

namespace fcitx {

struct EnglishPrediction {
    std::string word;
    unsigned frequency;
};

class EnglishPredictor {
public:
    bool load(std::istream &stream);
    std::vector<EnglishPrediction>
    match(const std::vector<std::string> &letters, size_t limit) const;

private:
    std::vector<EnglishPrediction> words_;
};

} // namespace fcitx

#endif
