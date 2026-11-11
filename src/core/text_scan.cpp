// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// TextScanner: byte-order mark, strict UTF-8 check, line endings and line lengths, block by block.

#include "core/text_scan.h"

#include <algorithm>

namespace vermilion {

namespace {

constexpr unsigned char kEf = 0xEF;
constexpr unsigned char kBb = 0xBB;
constexpr unsigned char kBf = 0xBF;
constexpr unsigned char kFf = 0xFF;
constexpr unsigned char kFe = 0xFE;

bool mark_prefix(const unsigned char* head, std::size_t n) {
    if (n == 1) {
        return head[0] == kEf || head[0] == kFf || head[0] == kFe;
    }
    if (n == 2) {
        return head[0] == kEf && head[1] == kBb;
    }
    return false;
}

} // namespace

void TextScanner::feed(const char* data, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);
        ++size_;
        if (mode_ == Mode::Detecting) {
            head_[head_size_] = byte;
            ++head_size_;
            if (head_size_ == 3 && head_[0] == kEf && head_[1] == kBb && head_[2] == kBf) {
                format_.encoding = TextEncoding::Utf8;
                format_.bom = true;
                mode_ = Mode::Bytes;
                head_size_ = 0;
            } else if (head_size_ == 2 && head_[0] == kFf && head_[1] == kFe) {
                format_.encoding = TextEncoding::Utf16Le;
                format_.bom = true;
                mode_ = Mode::Utf16;
                big_endian_ = false;
                head_size_ = 0;
            } else if (head_size_ == 2 && head_[0] == kFe && head_[1] == kFf) {
                format_.encoding = TextEncoding::Utf16Be;
                format_.bom = true;
                mode_ = Mode::Utf16;
                big_endian_ = true;
                head_size_ = 0;
            } else if (mark_prefix(head_, head_size_)) {
                // Still the beginning of a byte-order mark; wait for the next byte.
            } else {
                start_content();
            }
        } else if (mode_ == Mode::Bytes) {
            feed_byte(byte);
        } else {
            if (!have_low_byte_) {
                low_byte_ = byte;
                have_low_byte_ = true;
            } else {
                std::uint32_t unit = big_endian_ ? (static_cast<std::uint32_t>(low_byte_) << 8) |
                                                       static_cast<std::uint32_t>(byte)
                                                 : (static_cast<std::uint32_t>(byte) << 8) |
                                                       static_cast<std::uint32_t>(low_byte_);
                have_low_byte_ = false;
                feed_unit(unit);
            }
        }
    }
}

void TextScanner::start_content() {
    mode_ = Mode::Bytes;
    std::size_t count = head_size_;
    head_size_ = 0;
    for (std::size_t i = 0; i < count; ++i) {
        feed_byte(head_[i]);
    }
}

void TextScanner::feed_byte(unsigned char byte) {
    if (utf8_valid_) {
        check_utf8(byte);
    }
    if (byte == 0x81 || byte == 0x8D || byte == 0x8F || byte == 0x90 || byte == 0x9D) {
        cp1252_undefined_ = true;
    }
    feed_unit(byte);
}

void TextScanner::check_utf8(unsigned char byte) {
    if (utf8_needed_ == 0) {
        if (byte < 0x80) {
            return;
        }
        utf8_lower_ = 0x80;
        utf8_upper_ = 0xBF;
        if (byte >= 0xC2 && byte <= 0xDF) {
            utf8_needed_ = 1;
        } else if (byte == 0xE0) {
            utf8_needed_ = 2;
            utf8_lower_ = 0xA0;
        } else if ((byte >= 0xE1 && byte <= 0xEC) || (byte >= 0xEE && byte <= 0xEF)) {
            utf8_needed_ = 2;
        } else if (byte == 0xED) {
            utf8_needed_ = 2;
            utf8_lower_ = 0x80;
            utf8_upper_ = 0x9F;
        } else if (byte == 0xF0) {
            utf8_needed_ = 3;
            utf8_lower_ = 0x90;
        } else if (byte >= 0xF1 && byte <= 0xF3) {
            utf8_needed_ = 3;
        } else if (byte == 0xF4) {
            utf8_needed_ = 3;
            utf8_upper_ = 0x8F;
        } else {
            utf8_valid_ = false;
        }
        return;
    }
    if (byte < utf8_lower_ || byte > utf8_upper_) {
        utf8_valid_ = false;
        return;
    }
    --utf8_needed_;
    utf8_lower_ = 0x80;
    utf8_upper_ = 0xBF;
}

void TextScanner::feed_unit(std::uint32_t unit) {
    if (unit == 0x0D) {
        if (pending_cr_) {
            ++endings_.cr;
        }
        longest_units_ = std::max(longest_units_, line_units_);
        longest_points_ = std::max(longest_points_, line_points_);
        line_units_ = 0;
        line_points_ = 0;
        pending_cr_ = true;
        return;
    }
    if (unit == 0x0A) {
        if (pending_cr_) {
            ++endings_.crlf;
            pending_cr_ = false;
        } else {
            ++endings_.lf;
            longest_units_ = std::max(longest_units_, line_units_);
            longest_points_ = std::max(longest_points_, line_points_);
            line_units_ = 0;
            line_points_ = 0;
        }
        return;
    }
    if (pending_cr_) {
        ++endings_.cr;
        pending_cr_ = false;
    }
    if (unit == 0) {
        binary_ = true;
    }
    ++line_units_;
    if (!(mode_ == Mode::Bytes && (unit & 0xC0) == 0x80)) {
        ++line_points_;
    }
}

ScanResult TextScanner::finish() {
    if (mode_ == Mode::Detecting) {
        start_content();
    }
    if (utf8_needed_ > 0) {
        utf8_valid_ = false;
    }
    if (have_low_byte_) {
        binary_ = true;
    }
    if (pending_cr_) {
        ++endings_.cr;
        pending_cr_ = false;
    }
    longest_units_ = std::max(longest_units_, line_units_);
    longest_points_ = std::max(longest_points_, line_points_);
    if (mode_ == Mode::Bytes && !format_.bom) {
        if (utf8_valid_) {
            format_.encoding = TextEncoding::Utf8;
        } else if (cp1252_undefined_) {
            format_.encoding = TextEncoding::Latin1;
        } else {
            format_.encoding = TextEncoding::Windows1252;
        }
    }
    ScanResult result;
    result.format = format_;
    result.format.line_ending = majority_line_ending(endings_, LineEnding::Lf);
    result.endings = endings_;
    result.mixed_line_endings = has_mixed_line_endings(endings_);
    result.longest_line = format_.encoding == TextEncoding::Utf8 ? longest_points_ : longest_units_;
    result.binary = binary_;
    result.size = size_;
    return result;
}

OpenWarning open_warning(const ScanResult& result, std::uint64_t long_line_limit) {
    if (result.binary) {
        return OpenWarning::Binary;
    }
    if (result.longest_line > long_line_limit) {
        return OpenWarning::LongLine;
    }
    return OpenWarning::None;
}

} // namespace vermilion
