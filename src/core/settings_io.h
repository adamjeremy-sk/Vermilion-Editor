// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The two things the program does with settings.ini for the Settings window: read it again when
// the window opens, and save what the person applied.

#pragma once

#include "core/editor_settings.h"
#include "core/result.h"
#include "core/settings_file.h"

namespace vermilion {

// Reads settings.ini again from disk (SettingsFile::load) and returns the settings it holds
// (load_settings, so the result is normalised). A missing, damaged or unreadable file gives the
// defaults. `file` keeps what it read, so reads of single keys through it see the same values.
EditorSettings reread_settings(SettingsFile& file);

// Remembers in `file` the settings that differ from what it holds (store_settings) and writes it
// (SettingsFile::save). Keys the program does not know stay in the file; nothing is written when
// nothing differs. Returns the Status of save(). When saving fails, `file` still holds the new
// values in memory and keeps the remembered changes, so the program goes on with them.
Status save_settings(const EditorSettings& settings, SettingsFile& file);

} // namespace vermilion
