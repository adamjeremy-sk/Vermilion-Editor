// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The files named on the command line: what each one is, the questions and refusals, the list.

#include "core/open_arguments.h"

#include <cstddef>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <system_error>

namespace vermilion {

PathKind classify_path(const std::string& path) {
    std::error_code ec;
    std::filesystem::file_type type = std::filesystem::status(path, ec).type();
    switch (type) {
    case std::filesystem::file_type::not_found:
        return PathKind::Missing;
    case std::filesystem::file_type::regular:
        return PathKind::File;
    case std::filesystem::file_type::directory:
        return PathKind::Directory;
    case std::filesystem::file_type::none:
        return PathKind::File;
    default:
        return PathKind::Other;
    }
}

Answer parse_answer(std::string_view text) {
    auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && blank(text[begin])) {
        ++begin;
    }
    while (end > begin && blank(text[end - 1])) {
        --end;
    }
    std::string word;
    word.reserve(end - begin);
    for (std::size_t i = begin; i < end; ++i) {
        char c = text[i];
        if (c >= 'A' && c <= 'Z') {
            c = char(c + ('a' - 'A'));
        }
        word.push_back(c);
    }
    if (word.empty() || word == "y" || word == "yes") {
        return Answer::Yes;
    }
    if (word == "n" || word == "no") {
        return Answer::No;
    }
    return Answer::Ask;
}

std::string missing_prompt(const std::string& name) {
    return "'" + name + "' does not exist. Create it? (Y/n)";
}

std::string directory_message(const std::string& name) {
    return name + " is a directory, not a file";
}

std::string not_regular_message(const std::string& name) {
    return name + " is not a regular file";
}

std::string missing_no_terminal_message(const std::string& name) {
    return name + " does not exist";
}

std::string not_local_message(const std::string& name) {
    return "Cannot open " + name + ": only files on this computer can be opened.";
}

OpenRequest make_open_request(const std::vector<ArgumentFile>& arguments) {
    OpenRequest request;
    std::set<std::string> seen;
    for (const ArgumentFile& argument : arguments) {
        if (!argument.path.has_value()) {
            request.messages.push_back(not_local_message(argument.name));
            continue;
        }
        const std::string& path = *argument.path;
        if (!seen.insert(path).second) {
            continue;
        }
        switch (classify_path(path)) {
        case PathKind::File:
            request.paths.push_back(path);
            break;
        case PathKind::Missing:
            request.missing.push_back(MissingFile{argument.name, path, request.paths.size()});
            break;
        case PathKind::Directory:
            request.messages.push_back(directory_message(argument.name));
            break;
        case PathKind::Other:
            request.messages.push_back(not_regular_message(argument.name));
            break;
        }
    }
    return request;
}

std::vector<std::string> paths_to_open(const OpenRequest& request,
                                       const std::vector<bool>& created) {
    std::vector<std::string> result;
    std::size_t next = 0;
    auto handle = [&](std::size_t k) {
        if (k < created.size() && created[k]) {
            result.push_back(request.missing[k].path);
        }
    };
    for (std::size_t i = 0; i < request.paths.size(); ++i) {
        while (next < request.missing.size() && request.missing[next].position <= i) {
            handle(next);
            ++next;
        }
        result.push_back(request.paths[i]);
    }
    while (next < request.missing.size()) {
        handle(next);
        ++next;
    }
    return result;
}

} // namespace vermilion
