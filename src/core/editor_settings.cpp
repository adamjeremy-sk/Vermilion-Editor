// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// EditorSettings: defaults, limits, and the reading and writing of settings.ini.

#include "core/editor_settings.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace vermilion {

namespace {

std::string trim_blanks(const std::string& text) {
    const auto is_blank = [](char c) { return c == ' ' || c == '\t'; };
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && is_blank(text[begin]))
        ++begin;
    while (end > begin && is_blank(text[end - 1]))
        --end;
    return text.substr(begin, end - begin);
}

} // namespace

EditorSettings default_settings() {
    return EditorSettings{};
}

EditorSettings normalise_settings(EditorSettings settings) {
    if (settings.tab_width != 4 && settings.tab_width != 8)
        settings.tab_width = settings::kTabWidthDefault;
    const std::uint64_t lowest = static_cast<std::uint64_t>(settings::kLongLineLimitMin);
    const std::uint64_t highest = static_cast<std::uint64_t>(settings::kLongLineLimitMax);
    settings.long_line_limit = std::min(std::max(settings.long_line_limit, lowest), highest);
    settings.font = trim_blanks(settings.font);
    return settings;
}

EditorSettings load_settings(const SettingsFile& file) {
    EditorSettings result;
    result.colour_mode = parse_colour_mode(file.get_string(
        settings::kAppearanceGroup, settings::kColourMode, settings::kColourModeDefault));
    result.highlight_current_line =
        file.get_bool(settings::kAppearanceGroup, settings::kHighlightCurrentLine,
                      settings::kHighlightCurrentLineDefault);
    result.font = file.get_string(settings::kEditorGroup, settings::kFont, settings::kFontDefault);
    result.tab_width =
        file.get_int(settings::kEditorGroup, settings::kTabWidth, settings::kTabWidthDefault);
    result.confirm_large_reload = file.get_bool(
        settings::kFilesGroup, settings::kConfirmLargeReload, settings::kConfirmLargeReloadDefault);
    const int limit = file.get_int(settings::kFilesGroup, settings::kLongLineLimit,
                                   settings::kLongLineLimitDefault);
    result.long_line_limit = limit < 0 ? 0u : static_cast<std::uint64_t>(limit);
    return normalise_settings(result);
}

void store_settings(const EditorSettings& settings, SettingsFile& file) {
    const EditorSettings next = normalise_settings(settings);
    const EditorSettings now = load_settings(file);
    if (next.colour_mode != now.colour_mode)
        file.set_string(settings::kAppearanceGroup, settings::kColourMode,
                        colour_mode_id(next.colour_mode));
    if (next.highlight_current_line != now.highlight_current_line)
        file.set_bool(settings::kAppearanceGroup, settings::kHighlightCurrentLine,
                      next.highlight_current_line);
    if (next.font != now.font)
        file.set_string(settings::kEditorGroup, settings::kFont, next.font);
    if (next.tab_width != now.tab_width)
        file.set_int(settings::kEditorGroup, settings::kTabWidth, next.tab_width);
    if (next.confirm_large_reload != now.confirm_large_reload)
        file.set_bool(settings::kFilesGroup, settings::kConfirmLargeReload,
                      next.confirm_large_reload);
    if (next.long_line_limit != now.long_line_limit)
        file.set_int(settings::kFilesGroup, settings::kLongLineLimit,
                     static_cast<int>(next.long_line_limit));
}

} // namespace vermilion
