// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// font_css: turns a Pango font description into the one CSS rule that sets the editor font.

#include "ui/font_css.h"

#include <glib.h>
#include <pango/pango.h>

#include <memory>
#include <string>

namespace vermilion::ui {

namespace {

using DescriptionPtr =
    std::unique_ptr<PangoFontDescription, decltype(&pango_font_description_free)>;

// The name in double quotes; a backslash or a double quote gets a backslash in front, and every
// character below U+0020 becomes one space.
std::string quoted(const std::string& name) {
    std::string result = "\"";
    for (char ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '\\' || c == '"') {
            result.push_back('\\');
        }
        result.push_back(c < 0x20 ? ' ' : ch);
    }
    result.push_back('"');
    return result;
}

// Every name of the comma separated family list, quoted, joined with ", "; empty when no name is
// left. The parts are not trimmed: Pango cut the blanks around the commas already. When no name is
// "monospace" (any case), the generic family monospace (unquoted) is added last, so a family that
// is not installed falls back to a monospace font and not to a proportional one.
std::string family_value(const std::string& list) {
    std::string result;
    bool has_monospace = false;
    std::string::size_type start = 0;
    while (start < list.size()) {
        const std::string::size_type comma = list.find(',', start);
        const std::string part =
            comma == std::string::npos ? list.substr(start) : list.substr(start, comma - start);
        if (!part.empty()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += quoted(part);
            if (g_ascii_strcasecmp(part.c_str(), "monospace") == 0) {
                has_monospace = true;
            }
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    if (!result.empty() && !has_monospace) {
        result += ", monospace";
    }
    return result;
}

// The number with at most two decimals, no trailing zeros and a dot: locale independent.
std::string number(double value) {
    char buffer[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_formatd(buffer, G_ASCII_DTOSTR_BUF_SIZE, "%.2f", value);
    std::string result(buffer);
    while (!result.empty() && result.back() == '0') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '.') {
        result.pop_back();
    }
    return result;
}

// The CSS word for the Pango style.
const char* style_word(PangoStyle style) {
    switch (style) {
    case PANGO_STYLE_OBLIQUE:
        return "oblique";
    case PANGO_STYLE_ITALIC:
        return "italic";
    case PANGO_STYLE_NORMAL:
        break;
    }
    return "normal";
}

} // namespace

std::string font_css(const std::string& description) {
    DescriptionPtr font(pango_font_description_from_string(description.c_str()),
                        &pango_font_description_free);
    const PangoFontMask set = pango_font_description_get_set_fields(font.get());
    std::string body;
    if (set & PANGO_FONT_MASK_FAMILY) {
        const char* family = pango_font_description_get_family(font.get());
        if (family != nullptr) {
            const std::string value = family_value(family);
            if (!value.empty()) {
                body += "    font-family: " + value + ";\n";
            }
        }
    }
    if (set & PANGO_FONT_MASK_SIZE) {
        const gint size = pango_font_description_get_size(font.get());
        if (size > 0) {
            body += "    font-size: " + number(static_cast<double>(size) / PANGO_SCALE) +
                    (pango_font_description_get_size_is_absolute(font.get()) ? "px" : "pt") + ";\n";
        }
    }
    if (set & PANGO_FONT_MASK_WEIGHT) {
        const int weight = static_cast<int>(pango_font_description_get_weight(font.get()));
        if (weight != PANGO_WEIGHT_NORMAL) {
            body += "    font-weight: " + std::to_string(weight) + ";\n";
        }
    }
    if (set & PANGO_FONT_MASK_STYLE) {
        const PangoStyle style = pango_font_description_get_style(font.get());
        if (style != PANGO_STYLE_NORMAL) {
            body += std::string("    font-style: ") + style_word(style) + ";\n";
        }
    }
    if (body.empty()) {
        return "";
    }
    return std::string(".") + kEditorFontClass + " {\n" + body + "}\n";
}

} // namespace vermilion::ui
