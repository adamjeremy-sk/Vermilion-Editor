// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Reading and saving the settings of the Settings window.

#include "core/settings_io.h"

namespace vermilion {

EditorSettings reread_settings(SettingsFile& file) {
    file.load();
    return load_settings(file);
}

Status save_settings(const EditorSettings& settings, SettingsFile& file) {
    store_settings(settings, file);
    return file.save();
}

} // namespace vermilion
