// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Masks API keys and other secrets in log lines and error messages.

#pragma once

#include <string>
#include <string_view>

namespace vermilion {

// Returns a copy of text with secrets replaced by [REDACTED].
// Known limitation: a bare key= query parameter is not masked (too many false positives).
std::string redact_secrets(std::string_view text);

} // namespace vermilion
