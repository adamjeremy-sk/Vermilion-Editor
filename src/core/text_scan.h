// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Reads a file's bytes once, block by block, and decides its format before the editor loads it.

#pragma once

#include <cstddef>
#include <cstdint>

#include "core/text_format.h"

namespace vermilion {

// Above this many characters on one line, opening a file asks first (measured by Adam at M2.2:
// 500,000 still felt fine; a setting at M2.5).
inline constexpr std::uint64_t kDefaultLongLineLimit = 350000;

struct ScanResult {
    TextFormat format; // encoding, byte-order mark and the majority line ending
    LineEndingCounts endings;
    bool mixed_line_endings = false;
    // Characters on the longest line, without its line ending and without the byte-order mark:
    // code points for UTF-8, bytes for Windows-1252 and Latin-1, 16-bit units for UTF-16.
    std::uint64_t longest_line = 0;
    // A NUL character, or a UTF-16 file with an odd number of bytes: the file looks binary.
    bool binary = false;
    std::uint64_t size = 0; // bytes fed, byte-order mark included
};

// Feed the whole file in blocks of any size (empty blocks are allowed), then call finish() once.
// The result does not depend on where the blocks are split. Line endings are LF, CR LF and a
// lone CR; a CR at the end of one block and an LF at the start of the next are one CR LF.
//
// Encoding: a byte-order mark decides (EF BB BF: UTF-8; FF FE: UTF-16 LE; FE FF: UTF-16 BE).
// Without one, the whole text is checked as UTF-8 (strict: no overlong forms, no surrogates,
// nothing above U+10FFFF, no sequence cut off at the end); if it is not valid UTF-8 it is
// Windows-1252, unless one of the five bytes Windows-1252 leaves undefined (0x81, 0x8D, 0x8F,
// 0x90, 0x9D) occurs, in which case it is Latin-1, which accepts every byte.
class TextScanner {
public:
    void feed(const char* data, std::size_t size);

    ScanResult finish();

private:
    enum class Mode { Detecting, Bytes, Utf16 };

    void feed_byte(unsigned char byte);
    void feed_unit(std::uint32_t unit);
    void check_utf8(unsigned char byte);
    void start_content();

    Mode mode_ = Mode::Detecting;
    unsigned char head_[3] = {0, 0, 0}; // first bytes, kept until the byte-order mark is decided
    std::size_t head_size_ = 0;
    bool big_endian_ = false;
    bool have_low_byte_ = false; // UTF-16: one byte of a unit waits for the next block
    unsigned char low_byte_ = 0;

    TextFormat format_;
    LineEndingCounts endings_;
    bool pending_cr_ = false;
    std::uint64_t line_units_ = 0;  // bytes or UTF-16 units on the current line
    std::uint64_t line_points_ = 0; // UTF-8 code points on the current line
    std::uint64_t longest_units_ = 0;
    std::uint64_t longest_points_ = 0;
    bool binary_ = false;
    std::uint64_t size_ = 0;

    bool utf8_valid_ = true;
    int utf8_needed_ = 0;             // continuation bytes still expected
    unsigned char utf8_lower_ = 0x80; // allowed range of the next continuation byte
    unsigned char utf8_upper_ = 0xBF;
    bool cp1252_undefined_ = false;
};

// Why opening a file asks first.
enum class OpenWarning { None, Binary, LongLine };

// Binary wins over a long line; a line longer than long_line_limit characters is a long line.
OpenWarning open_warning(const ScanResult& result, std::uint64_t long_line_limit);

} // namespace vermilion
