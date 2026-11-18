// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The names of the settings in settings.ini: groups, keys and defaults (a name never changes).

#pragma once

namespace vermilion::settings {

// [files]
inline constexpr const char* kFilesGroup = "files";

// Ask before File > Reload from Disk replaces a clean tab over 10 MB. Boolean.
inline constexpr const char* kConfirmLargeReload = "confirm-large-reload";
inline constexpr bool kConfirmLargeReloadDefault = true;

} // namespace vermilion::settings
