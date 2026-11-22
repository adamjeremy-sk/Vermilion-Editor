// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The contents tree of the manual: topics in the order of the index page, each with its headings.

#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "core/manual_text.h"

namespace vermilion {

// A heading of a topic as the tree shows it.
struct OutlineHeading {
    std::string title;     // the heading's words as plain text
    int level = 1;         // 1 to 6
    std::size_t index = 0; // its place in ManualText::headings of the topic, so the view can jump
                           // to ManualText::headings[index].begin
};

// A topic as the tree shows it: a row with the headings of its page as its children.
struct OutlineTopic {
    std::string file;                     // the topic file, for example "open-and-save.md"
    std::string title;                    // the row's text, see make_outline_topic()
    std::vector<OutlineHeading> headings; // the children, in page order
};

// The topic files that `page` links to, in the order of their links (ManualText::links), each
// file once (its first link counts).
std::vector<std::string> linked_topics(const ManualText& page);

// The row of one topic. The title is the title of the first heading of level 1; that heading is
// not one of the children. A page without a level-1 heading gets `file` as its title, and every
// heading is a child. All other headings, of any level, are children in page order.
OutlineTopic make_outline_topic(const std::string& file, const ManualText& page);

// The whole tree: first the topic `home_file` (the index page), then the topics it links to in
// the order of linked_topics(), each once. A linked file that is not a key of `pages`, and a link
// from the home page to itself, are skipped. Empty when `home_file` is not a key of `pages`. A
// topic that is not linked from the home page is not in the tree.
std::vector<OutlineTopic> build_outline(const std::string& home_file,
                                        const std::map<std::string, ManualText>& pages);

} // namespace vermilion
