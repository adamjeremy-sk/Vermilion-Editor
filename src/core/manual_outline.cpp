// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the contents tree of the manual.

#include "core/manual_outline.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace vermilion {

std::vector<std::string> linked_topics(const ManualText& page) {
    std::vector<std::string> result;
    for (const LinkRange& link : page.links) {
        if (std::find(result.begin(), result.end(), link.target) == result.end()) {
            result.push_back(link.target);
        }
    }
    return result;
}

OutlineTopic make_outline_topic(const std::string& file, const ManualText& page) {
    OutlineTopic topic;
    topic.file = file;
    topic.title = file;
    bool have_title = false;
    for (std::size_t i = 0; i < page.headings.size(); ++i) {
        const HeadingMark& heading = page.headings[i];
        if (!have_title && heading.level == 1) {
            topic.title = heading.title;
            have_title = true;
            continue;
        }
        topic.headings.push_back({heading.title, heading.level, i});
    }
    return topic;
}

std::vector<OutlineTopic> build_outline(const std::string& home_file,
                                        const std::map<std::string, ManualText>& pages) {
    std::vector<OutlineTopic> result;
    const auto home = pages.find(home_file);
    if (home == pages.end()) {
        return result;
    }
    result.push_back(make_outline_topic(home_file, home->second));
    for (const std::string& file : linked_topics(home->second)) {
        if (file == home_file) {
            continue;
        }
        const auto page = pages.find(file);
        if (page == pages.end()) {
            continue;
        }
        result.push_back(make_outline_topic(file, page->second));
    }
    return result;
}

} // namespace vermilion
