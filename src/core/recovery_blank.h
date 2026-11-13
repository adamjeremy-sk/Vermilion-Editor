// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Whether a document is the blank one the program starts with, still untouched.

#pragma once

#include <cstdint>

#include "core/document_list.h"

namespace vermilion {

// The blank document the window opens with is closed when the first copy is recovered (nobody
// wants an empty "Untitled 1" beside it), but only while it is untouched: untitled, never
// modified, not read-only and empty (chars is the length of its text in characters). Any
// document that has a path, that was ever changed or that holds text is kept.
bool is_untouched_blank(const DocumentRecord& record, std::uint64_t chars);

} // namespace vermilion
