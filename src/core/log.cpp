// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)

#include "core/log.h"
#include "core/redact.h"

#include <glib.h>

#include <string>

namespace vermilion::logging {

namespace {

void emit(GLogLevelFlags level, std::string_view message) {
    std::string redacted = vermilion::redact_secrets(message);
    g_log(kDomain, level, "%s", redacted.c_str());
}

}  // namespace

void debug(std::string_view message) {
    emit(G_LOG_LEVEL_DEBUG, message);
}

void info(std::string_view message) {
    emit(G_LOG_LEVEL_INFO, message);
}

void warning(std::string_view message) {
    emit(G_LOG_LEVEL_WARNING, message);
}

void error(std::string_view message) {
    emit(G_LOG_LEVEL_CRITICAL, message);
}

}  // namespace vermilion::logging
