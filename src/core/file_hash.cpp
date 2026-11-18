// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// StreamHasher and hash_file_blocking: the only file that includes the vendored xxHash header.

#include "core/file_hash.h"

#include <fcntl.h>
#include <glib.h>
#include <unistd.h>

#include <cerrno>
#include <vector>

#define XXH_INLINE_ALL
#include "xxhash.h"

namespace vermilion {

namespace {

constexpr std::size_t kBlockSize = 64 * 1024;

} // namespace

struct StreamHasher::State {
    State() { XXH3_64bits_reset(&xxh); }

    XXH3_state_t xxh;
};

StreamHasher::StreamHasher() : state_(std::make_unique<State>()) {}

StreamHasher::~StreamHasher() = default;

void StreamHasher::update(const char* data, std::size_t size) {
    if (size == 0)
        return;
    XXH3_64bits_update(&state_->xxh, data, size);
}

std::uint64_t StreamHasher::digest() const {
    return XXH3_64bits_digest(&state_->xxh);
}

std::uint64_t hash_bytes(const char* data, std::size_t size) {
    return XXH3_64bits(data, size);
}

Result<FileHash> hash_file_blocking(const std::string& path) {
    int fd = -1;
    while (true) {
        fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd >= 0)
            break;
        if (errno != EINTR)
            return Result<FileHash>::err(
                Error{"Could not read " + path + ": " + g_strerror(errno)});
    }

    std::vector<char> block(kBlockSize);
    StreamHasher hasher;
    std::uint64_t size = 0;
    for (;;) {
        ssize_t n = -1;
        while (true) {
            n = ::read(fd, block.data(), block.size());
            if (n >= 0)
                break;
            if (errno != EINTR)
                break;
        }
        if (n < 0) {
            std::string reason = g_strerror(errno);
            ::close(fd);
            return Result<FileHash>::err(Error{"Could not read " + path + ": " + reason});
        }
        if (n == 0)
            break;
        hasher.update(block.data(), static_cast<std::size_t>(n));
        size += static_cast<std::uint64_t>(n);
    }
    ::close(fd);

    FileHash out;
    out.size = size;
    out.hash = hasher.digest();
    return Result<FileHash>::ok(out);
}

} // namespace vermilion
