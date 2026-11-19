// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The files named on the command line: what each one is, the questions and refusals, the list.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vermilion {

// What a path is on disk. A symbolic link is judged by its target; a link whose target is missing
// is Missing, and so is a name below a file ("a.txt/x"). A failure other than "no such file" (a
// folder that cannot be entered, a link loop) counts as File, so that opening the file reports
// the real error.
enum class PathKind { Missing, File, Directory, Other };

PathKind classify_path(const std::string& path);

// A typed answer to the question about creating a missing file.
enum class Answer { Yes, No, Ask };

// Leading and trailing blanks (space, tab, CR, LF) are ignored, letters are not case-sensitive.
// Empty, "y", "yes": Yes. "n", "no": No. Anything else: Ask (the question is asked again).
// The caller treats the end of input as No; it never calls this function for it.
Answer parse_answer(std::string_view text);

// The texts, with `name` as the user typed it. None ends in a line feed or a full stop, except
// not_local_message.
// missing_prompt:             'NAME' does not exist. Create it? (Y/n)
// directory_message:          NAME is a directory, not a file
// not_regular_message:        NAME is not a regular file
// missing_no_terminal_message: NAME does not exist
// not_local_message:          Cannot open NAME: only files on this computer can be opened.
std::string missing_prompt(const std::string& name);
std::string directory_message(const std::string& name);
std::string not_regular_message(const std::string& name);
std::string missing_no_terminal_message(const std::string& name);
std::string not_local_message(const std::string& name);

// One argument of the command line, already resolved by the caller against the launch's working
// folder.
struct ArgumentFile {
    std::string name;                // as typed, for the messages
    std::optional<std::string> path; // the absolute native path; empty when it is not a local file
};

// A name that does not exist: the caller asks about it and, on yes, creates the file.
struct MissingFile {
    std::string name;
    std::string path;
    std::size_t position = 0; // how many entries of OpenRequest::paths come before it
};

struct OpenRequest {
    std::vector<std::string> paths;    // existing regular files, argument order, each path once
    std::vector<MissingFile> missing;  // names that do not exist, argument order, each path once
    std::vector<std::string> messages; // one refusal per rejected argument, argument order
};

// Sorts the arguments with classify_path(). File: into `paths`. Missing: into `missing`.
// Directory: directory_message(name) into `messages`. Other: not_regular_message(name).
// No path (not a local file): not_local_message(name). A path that occurs twice is dealt with
// once, at its first occurrence; arguments without a path are never merged.
OpenRequest make_open_request(const std::vector<ArgumentFile>& arguments);

// The paths to open after the questions: `request.paths`, with the path of every missing file
// whose entry in `created` is true put back at its `position` (argument order). A `created` that
// is shorter than `request.missing` counts as false for the entries it lacks.
std::vector<std::string> paths_to_open(const OpenRequest& request,
                                       const std::vector<bool>& created);

} // namespace vermilion
