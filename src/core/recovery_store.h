// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The folder of recovery copies: where it is, and writing, listing, reading and deleting its files.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/recovery_file.h"
#include "core/result.h"

namespace vermilion {

// The two editors keep their copies apart, so each one looks only at its own.
enum class RecoveryProgram { Light, Ide };

// The folder name of a program: "light" or "ide".
const char* recovery_program_name(RecoveryProgram program);

// A copy is stored as "<id>.vrec". An id is 1 to kRecoveryMaxIdBytes characters of the letters a
// to z, the digits and "-": nothing that can name another folder or hide a file. The editor makes
// one id per document with new_recovery_id() and keeps it, so a new copy replaces the old one.
inline constexpr std::size_t kRecoveryMaxIdBytes = 64;
inline constexpr std::string_view kRecoverySuffix = ".vrec";

// A file larger than this is not a copy this program wrote (the largest document that gets
// copies is 100,000,000 characters, at most 4 bytes each, plus a header) and is never read.
inline constexpr std::uint64_t kRecoveryMaxFileBytes = std::uint64_t{1} << 30;

// A copy is written to a temporary file in the same folder first: this prefix and six random
// characters. A crash during a write can leave one behind.
inline constexpr std::string_view kRecoveryTempPrefix = ".vrec-tmp-";

// "<state_dir>/vermilion/recovery": the folder that holds one subfolder per program. The state
// folder is a parameter so that tests use a temporary directory.
std::string recovery_root(const std::string& state_dir);

// recovery_root() of the user's XDG state folder ($XDG_STATE_HOME, else ~/.local/state).
std::string default_recovery_root();

// A new random id: a version 4 UUID in lower case, 36 characters (it passes valid_recovery_id).
std::string new_recovery_id();

bool valid_recovery_id(std::string_view id);

// A copy read back: the header as written and the document text, exactly as it was written.
struct RecoveryCopy {
    RecoveryHeader header;
    std::string text;
};

// The recovery folder of one program, with synchronous file operations in it. The object holds
// only the folder's path: no state is shared between calls or objects, so any call may run on a
// worker thread, and different documents may be written at the same time. Every function that
// takes an id refuses an invalid one with an error before it touches any file. Error messages are
// short English texts for the log; the editor does not show them to the user. The implementation
// uses GLib and the system's file calls, not GIO.
class RecoveryStore {
public:
    // The folder is root/<program name>. A root that is empty or not an absolute path gives a
    // store with no folder: folder() is empty and every operation returns an error.
    RecoveryStore(std::string root, RecoveryProgram program);

    const std::string& folder() const { return folder_; }

    // folder()/<id>.vrec; empty when there is no folder or the id is not valid.
    std::string path_of(std::string_view id) const;

    // Writes the copy: the header, then the text, as the file <id>.vrec, which is replaced if it
    // exists. header.length is set to text.size() (the caller's value is ignored). The folder is
    // created first if it is missing, with parents, with mode 0700. The bytes go to a temporary
    // file in the same folder (mode 0600), which is synced to disk and then renamed over the copy,
    // so a failure at any point leaves an earlier copy as it was and removes the temporary file. A
    // link or a file of another mode at the destination is replaced by the new file, never
    // followed or kept. An error when the id is not valid, the header cannot be written
    // (encode_header), or the system refuses.
    Status write(std::string_view id, RecoveryHeader header, std::string_view text) const;

    // The ids of the regular files "<id>.vrec" in the folder (a valid id; a link, a directory or
    // any other name is ignored), in byte order of the id. No folder on disk gives an empty list;
    // a folder that cannot be read is an error. Damaged copies are listed: reading them tells.
    Result<std::vector<std::string>> list() const;

    // The header of one copy, checked against the size of the file, without reading the body
    // (parse_header). An error for a file that is missing, is not a regular file (a link or a
    // pipe counts as not one; opening never follows a link and never waits), is larger than
    // kRecoveryMaxFileBytes, or is not a valid copy.
    Result<RecoveryParse> read_header(std::string_view id) const;

    // The whole copy, checked the same way, with the text after the header.
    Result<RecoveryCopy> read(std::string_view id) const;

    // Deletes the copy's file (never anything the file links to). Success when it is already gone.
    // An error for an invalid id or when the file cannot be deleted.
    Status remove(std::string_view id) const;

    // For the start of the program: deletes the files in the folder whose names begin with
    // kRecoveryTempPrefix (the leftovers of a write that was cut short; directories are left
    // alone, a link is deleted and never followed) and returns how many it deleted. Nothing else
    // is touched. Best effort: a file that cannot be deleted is skipped; no folder on disk gives 0.
    std::size_t remove_leftovers() const;

private:
    std::string folder_;
};

} // namespace vermilion
