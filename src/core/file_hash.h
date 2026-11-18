// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A fast 64-bit hash of a file's bytes (XXH3), fed in blocks or read from a path, to tell a real
// change from a touch.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "core/result.h"

namespace vermilion {

// The size of a file and the hash of its bytes, as read in one pass.
struct FileHash {
    std::uint64_t size = 0;
    std::uint64_t hash = 0;
};

// XXH3 (64 bits, no seed, no secret) over the bytes fed so far. The digest does not depend on where
// the blocks are split; empty blocks are allowed; digest() may be called at any time and does not
// change the state, so more bytes can follow. Not for secrets: the hash is not cryptographic.
class StreamHasher {
public:
    StreamHasher();
    ~StreamHasher();
    StreamHasher(const StreamHasher&) = delete;
    StreamHasher& operator=(const StreamHasher&) = delete;

    void update(const char* data, std::size_t size);

    std::uint64_t digest() const;

private:
    struct State; // holds the xxHash state; defined in the .cpp so no other file sees xxhash.h
    std::unique_ptr<State> state_;
};

// The hash of a whole buffer: the same value StreamHasher gives for the same bytes.
std::uint64_t hash_bytes(const char* data, std::size_t size);

// Reads the file at `path` in blocks of 64 KiB and returns its size and hash. Blocks the calling
// thread: call it from a worker thread when the file can be large. An error (the file is missing,
// not readable, a directory, or a read fails half way) is returned with the path and the system's
// reason in its message; nothing is thrown and nothing is written.
Result<FileHash> hash_file_blocking(const std::string& path);

} // namespace vermilion
