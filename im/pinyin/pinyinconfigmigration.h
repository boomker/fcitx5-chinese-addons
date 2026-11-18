/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef FCITX_PINYIN_CONFIG_MIGRATION_H
#define FCITX_PINYIN_CONFIG_MIGRATION_H

#include <fcitx-config/rawconfig.h>

namespace fcitx {

inline void migrateAbbreviationConfig(RawConfig &config, bool fullPinyin = false) {
    const auto migrated = config.get("AbbreviationConfigMigrated");
    if (migrated && migrated->value() == "True") {
        return;
    }
    const auto legacy = config.get("Fuzzy/PartialSp");
    const auto existing = config.get("PartialSpLength");
    if (fullPinyin) {
        // The old numeric value only affected shuangpin, even when present
        // in a full-pinyin file. Preserve full pinyin's enabled default.
        config.setValueByPath("PartialSpLength", "2");
    } else if (!legacy && !existing) {
        // No config file (or an unrelated config) needs no migration.
        return;
    } else if (!legacy && existing) {
        // A legacy file may retain the old numeric threshold while the old
        // switch was omitted because its default was False.
        config.setValueByPath("PartialSpLength", "0");
    } else if (legacy && legacy->value() != "True" && legacy->value() != "true") {
        // INI saving comments out default False, so absence also means off.
        config.setValueByPath("PartialSpLength", "0");
    } else if (!existing) {
        config.setValueByPath("PartialSpLength", "4");
    }
    if (auto fuzzy = config.get("Fuzzy"); fuzzy && fuzzy->get("PartialSp")) {
        fuzzy->remove("PartialSp");
    }
    config.setValueByPath("AbbreviationConfigMigrated", "True");
}

} // namespace fcitx

#endif
