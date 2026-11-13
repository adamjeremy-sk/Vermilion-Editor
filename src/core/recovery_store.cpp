// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the recovery folder: writing copies safely, listing, reading and deleting them.

#include "core/recovery_store.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vermilion {

namespace {

Error errno_error(const char* what) {
    Error error;
    error.message = std::string(what) + ": " + g_strerror(errno);
    return error;
}

// Owns a file descriptor: closes it when it goes out of scope, unless finish() gave it up.
class Fd {
public:
    explicit Fd(int fd) : fd_(fd) {}
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    ~Fd() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    int value() const { return fd_; }

    // Closes now, gives up ownership, and says whether close worked (a delayed write error shows
    // only there).
    bool finish() {
        int fd = fd_;
        fd_ = -1;
        if (fd < 0) {
            return false;
        }
        return ::close(fd) == 0;
    }

private:
    int fd_;
};

bool is_id_char(char c) {
    if (c >= 'a' && c <= 'z') {
        return true;
    }
    if (c >= '0' && c <= '9') {
        return true;
    }
    return c == '-';
}

bool ends_with(std::string_view text, std::string_view suffix) {
    if (suffix.size() > text.size()) {
        return false;
    }
    return text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool starts_with(std::string_view text, std::string_view prefix) {
    if (prefix.size() > text.size()) {
        return false;
    }
    return text.compare(0, prefix.size(), prefix) == 0;
}

std::string join_path(const std::string& folder, const std::string& name) {
    g_autofree char* path = g_build_filename(folder.c_str(), name.c_str(), nullptr);
    return path;
}

// Puts every byte of `bytes` out on `fd`. A short write goes on with the rest, EINTR tries again,
// any other failure or a result of 0 is an error.
Status write_all(int fd, std::string_view bytes) {
    std::size_t done = 0;
    while (done < bytes.size()) {
        ssize_t n = ::write(fd, bytes.data() + done, bytes.size() - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return Status::err(errno_error("write"));
        }
        if (n == 0) {
            return Status::err(Error{"write: no bytes"});
        }
        done += static_cast<std::size_t>(n);
    }
    return ok_status();
}

// Creates the temporary file (mode 0600), writes the head and the text, syncs it, and closes it.
// `temp` is a writable buffer ending in "XXXXXX"; on success it holds the name of the file. On any
// failure after the file exists it is removed and the error is returned.
Status write_temp_file(char* temp, std::string_view head, std::string_view text) {
    int fd = g_mkstemp_full(temp, O_WRONLY | O_CLOEXEC, 0600);
    if (fd < 0) {
        return Status::err(errno_error("mkstemp"));
    }
    Fd owned(fd);
    Status status = write_all(fd, head);
    if (status.is_ok()) {
        status = write_all(fd, text);
    }
    if (status.is_ok() && ::fsync(fd) != 0) {
        status = Status::err(errno_error("fsync"));
    }
    if (status.is_ok() && !owned.finish()) {
        status = Status::err(errno_error("close"));
    }
    if (!status.is_ok()) {
        g_unlink(temp);
    }
    return status;
}

// One name in a folder, and whether it is a regular file or a directory (a link is neither).
struct Entry {
    std::string name;
    bool regular = false;
    bool directory = false;
};

// The names in `folder`, each examined with g_lstat (a link is not followed). A name that cannot
// be examined is skipped. A folder that is not there gives no entries; any other failure is an
// error.
Result<std::vector<Entry>> entries_of(const std::string& folder) {
    g_autoptr(GError) open_error = nullptr;
    auto dir = std::unique_ptr<GDir, void (*)(GDir*)>(g_dir_open(folder.c_str(), 0, &open_error),
                                                      &g_dir_close);
    if (dir == nullptr) {
        if (g_error_matches(open_error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
            return Result<std::vector<Entry>>::ok(std::vector<Entry>{});
        }
        Error failure;
        failure.message = std::string("open folder: ") + open_error->message;
        return Result<std::vector<Entry>>::err(failure);
    }
    std::vector<Entry> entries;
    for (const char* name = g_dir_read_name(dir.get()); name != nullptr;
         name = g_dir_read_name(dir.get())) {
        g_autofree char* path = g_build_filename(folder.c_str(), name, nullptr);
        struct stat info;
        if (g_lstat(path, &info) != 0) {
            continue;
        }
        Entry entry;
        entry.name = name;
        if (S_ISREG(info.st_mode)) {
            entry.regular = true;
        } else if (S_ISDIR(info.st_mode)) {
            entry.directory = true;
        }
        entries.push_back(std::move(entry));
    }
    return Result<std::vector<Entry>>::ok(std::move(entries));
}

// The beginning of a file and the size of the whole file. The file is opened without following a
// link and without blocking, examined on the open descriptor, and read only as far as max_bytes.
struct Prefix {
    std::string bytes;
    std::uint64_t size = 0;
};

Result<Prefix> read_prefix(const std::string& path, std::uint64_t max_bytes) {
    int fd = g_open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0);
    if (fd < 0) {
        return Result<Prefix>::err(errno_error("open"));
    }
    Fd owned(fd);
    struct stat info;
    if (::fstat(fd, &info) != 0) {
        return Result<Prefix>::err(errno_error("fstat"));
    }
    if (!S_ISREG(info.st_mode)) {
        return Result<Prefix>::err(Error{"not a regular file"});
    }
    std::uint64_t size = static_cast<std::uint64_t>(info.st_size);
    if (size > kRecoveryMaxFileBytes) {
        return Result<Prefix>::err(Error{"too large"});
    }
    std::uint64_t to_read = size < max_bytes ? size : max_bytes;
    std::string bytes;
    bytes.resize(to_read);
    std::uint64_t done = 0;
    while (done < to_read) {
        ssize_t n = ::read(fd, bytes.data() + done, to_read - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return Result<Prefix>::err(errno_error("read"));
        }
        if (n == 0) {
            return Result<Prefix>::err(Error{"changed while it was read"});
        }
        done += static_cast<std::uint64_t>(n);
    }
    Prefix prefix;
    prefix.bytes = std::move(bytes);
    prefix.size = size;
    return Result<Prefix>::ok(std::move(prefix));
}

} // namespace

const char* recovery_program_name(RecoveryProgram program) {
    if (program == RecoveryProgram::Light) {
        return "light";
    }
    return "ide";
}

std::string recovery_root(const std::string& state_dir) {
    g_autofree char* path = g_build_filename(state_dir.c_str(), "vermilion", "recovery", nullptr);
    return path;
}

std::string default_recovery_root() {
    return recovery_root(g_get_user_state_dir());
}

std::string new_recovery_id() {
    g_autofree char* id = g_uuid_string_random();
    return id;
}

bool valid_recovery_id(std::string_view id) {
    if (id.empty() || id.size() > kRecoveryMaxIdBytes) {
        return false;
    }
    for (char c : id) {
        if (!is_id_char(c)) {
            return false;
        }
    }
    return true;
}

RecoveryStore::RecoveryStore(std::string root, RecoveryProgram program) {
    if (!root.empty() && g_path_is_absolute(root.c_str())) {
        folder_ = join_path(root, recovery_program_name(program));
    }
}

std::string RecoveryStore::path_of(std::string_view id) const {
    if (folder_.empty() || !valid_recovery_id(id)) {
        return std::string();
    }
    return join_path(folder_, std::string(id) + std::string(kRecoverySuffix));
}

Status RecoveryStore::write(std::string_view id, RecoveryHeader header,
                            std::string_view text) const {
    std::string path = path_of(id);
    if (path.empty()) {
        return Status::err(Error{"no folder or invalid id"});
    }
    header.length = text.size();
    std::optional<std::string> head = encode_header(header);
    if (!head.has_value()) {
        return Status::err(Error{"header cannot be written"});
    }
    if (g_mkdir_with_parents(folder_.c_str(), 0700) != 0) {
        return Status::err(errno_error("make folder"));
    }
    g_autofree char* temp = g_build_filename(
        folder_.c_str(), (std::string(kRecoveryTempPrefix) + "XXXXXX").c_str(), nullptr);
    Status status = write_temp_file(temp, *head, text);
    if (status.is_ok() && g_rename(temp, path.c_str()) != 0) {
        status = Status::err(errno_error("rename"));
        g_unlink(temp);
    }
    return status;
}

Result<std::vector<std::string>> RecoveryStore::list() const {
    if (folder_.empty()) {
        return Result<std::vector<std::string>>::err(Error{"no folder"});
    }
    Result<std::vector<Entry>> listed = entries_of(folder_);
    if (!listed.is_ok()) {
        return Result<std::vector<std::string>>::err(listed.error());
    }
    std::vector<std::string> ids;
    for (const Entry& entry : listed.value()) {
        if (!entry.regular) {
            continue;
        }
        std::string_view name = entry.name;
        if (!ends_with(name, kRecoverySuffix)) {
            continue;
        }
        std::string_view id = name.substr(0, name.size() - kRecoverySuffix.size());
        if (valid_recovery_id(id)) {
            ids.push_back(std::string(id));
        }
    }
    std::sort(ids.begin(), ids.end());
    return Result<std::vector<std::string>>::ok(std::move(ids));
}

Result<RecoveryParse> RecoveryStore::read_header(std::string_view id) const {
    std::string path = path_of(id);
    if (path.empty()) {
        return Result<RecoveryParse>::err(Error{"no folder or invalid id"});
    }
    Result<Prefix> prefix = read_prefix(path, kRecoveryMaxHeaderBytes);
    if (!prefix.is_ok()) {
        return Result<RecoveryParse>::err(prefix.error());
    }
    return parse_header(prefix.value().bytes, prefix.value().size);
}

Result<RecoveryCopy> RecoveryStore::read(std::string_view id) const {
    std::string path = path_of(id);
    if (path.empty()) {
        return Result<RecoveryCopy>::err(Error{"no folder or invalid id"});
    }
    Result<Prefix> prefix = read_prefix(path, kRecoveryMaxFileBytes);
    if (!prefix.is_ok()) {
        return Result<RecoveryCopy>::err(prefix.error());
    }
    std::string bytes = std::move(prefix.value().bytes);
    std::uint64_t size = prefix.value().size;
    Result<RecoveryParse> parsed = parse_header(bytes, size);
    if (!parsed.is_ok()) {
        return Result<RecoveryCopy>::err(parsed.error());
    }
    std::size_t offset = parsed.value().body_offset;
    bytes.erase(0, offset);
    RecoveryCopy copy;
    copy.header = parsed.value().header;
    copy.text = std::move(bytes);
    return Result<RecoveryCopy>::ok(std::move(copy));
}

Status RecoveryStore::remove(std::string_view id) const {
    std::string path = path_of(id);
    if (path.empty()) {
        return Status::err(Error{"no folder or invalid id"});
    }
    if (g_unlink(path.c_str()) == 0 || errno == ENOENT) {
        return ok_status();
    }
    return Status::err(errno_error("unlink"));
}

std::size_t RecoveryStore::remove_leftovers() const {
    if (folder_.empty()) {
        return 0;
    }
    Result<std::vector<Entry>> listed = entries_of(folder_);
    if (!listed.is_ok()) {
        return 0;
    }
    std::size_t deleted = 0;
    for (const Entry& entry : listed.value()) {
        if (entry.directory || !starts_with(entry.name, kRecoveryTempPrefix)) {
            continue;
        }
        g_autofree char* path = g_build_filename(folder_.c_str(), entry.name.c_str(), nullptr);
        if (g_unlink(path) == 0) {
            ++deleted;
        }
    }
    return deleted;
}

} // namespace vermilion
