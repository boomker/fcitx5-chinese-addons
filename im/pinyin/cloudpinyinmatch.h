/*
 * SPDX-FileCopyrightText: 2017-2017 CSSlayer <wengxt@gmail.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_PINYIN_CLOUDPINYINMATCH_H_
#define _FCITX_PINYIN_CLOUDPINYINMATCH_H_

#include <libime/core/historybigram.h>
#include <libime/pinyin/pinyincontext.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fcitx {
struct CloudPinyinMatchResult {
    std::vector<libime::HistoryBigram::WordWithCode> context;
    std::string_view word;
    std::string fullPinyin;
};

// Supply the original cloud request for ambiguous grouped Shuangpin input.
std::optional<CloudPinyinMatchResult>
matchCloudPinyin(const libime::PinyinContext &context,
                 const std::string &selected, const std::string &word,
                 std::optional<std::string_view> requestedFullPinyin);
} // namespace fcitx
#endif
