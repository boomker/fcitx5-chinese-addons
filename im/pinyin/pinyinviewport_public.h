/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef FCITX_PINYIN_VIEWPORT_PUBLIC_H
#define FCITX_PINYIN_VIEWPORT_PUBLIC_H
#include <fcitx/addoninstance.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <vector>
#include <cstdint>

FCITX_ADDON_DECLARE_FUNCTION(PinyinEngine, candidateViewportPolicyEnabled,
                             bool(fcitx::InputContext *));
FCITX_ADDON_DECLARE_FUNCTION(PinyinEngine, candidateViewportGeneration,
                             uint64_t(fcitx::InputContext *));
FCITX_ADDON_DECLARE_FUNCTION(PinyinEngine, candidatePinyinAnnotationOnTop,
                             bool(const fcitx::CandidateWord *));
FCITX_ADDON_DECLARE_FUNCTION(
    PinyinEngine, setCandidateViewport,
    bool(fcitx::InputContext *, int,
         const std::vector<const fcitx::CandidateWord *> &,
         const fcitx::CandidateWord *, bool));
#endif
