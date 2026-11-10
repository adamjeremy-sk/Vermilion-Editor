// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// GPU memory readers: AMD sysfs counters and nvidia-smi output, as in the Python original.

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace vermilion::ctxmon {

enum class GpuVendor { Amd, Nvidia, Intel, Other };

// The vendor as the Python original names it: "amd", "nvidia", "intel" or "other".
const char* vendor_key(GpuVendor vendor);

// The vendor as shown in the card (VENDOR_NAMES): "AMD", "NVIDIA", "Intel" or "GPU".
const char* vendor_label(GpuVendor vendor);

// The memory of one GPU, in bytes.
struct VramInfo {
    long long used_bytes = 0;
    long long total_bytes = 0;
    std::string name;
    GpuVendor vendor = GpuVendor::Other;
};

// The sysfs half of read_vram of the Python original. Looks at every directory card<digit>* in
// drm_root (the card0-DP-1 connector entries match too, as in the original) and reads
// device/mem_info_vram_total and device/mem_info_vram_used, two non-negative decimal numbers
// (surrounding whitespace is allowed; unlike Python's int(), a sign, an underscore or a value
// beyond long long is rejected). A card with a missing or unparsable counter is skipped. Returns
// the card with the most memory, the first in name order on a tie, with the card directory as its
// name. The vendor comes from device/vendor ("0x1002" amd, "0x10de" nvidia, "0x8086" intel, in
// any letter case, anything else other) and is amd when that file is missing. Every file is read
// only if it is an ordinary file (symlinks are followed) of at most 64 bytes; anything else (a
// pipe, a device, a longer file) counts as missing. Absent when no card qualifies; the caller
// then tries nvidia-smi (running the program is I/O, left to M2).
std::optional<VramInfo> read_vram_sysfs(const std::string& drm_root = "/sys/class/drm");

// The nvidia-smi half of read_vram: parses the output of
//   nvidia-smi --query-gpu=name,memory.used,memory.total --format=csv,noheader,nounits
// which has one "name, MiB used, MiB total" line per GPU (a name may contain commas). Returns the
// GPU with the most memory, the first on a tie, in bytes, with vendor nvidia. As in the original,
// one malformed line (fewer than three fields, a count that is not a number such as "[N/A]", a
// blank line between rows) makes the whole result absent, and so does empty output.
std::optional<VramInfo> parse_nvidia_smi(std::string_view output);

// fmt_gib of the Python original: bytes as GiB with one decimal ("16.0"); the decimal separator
// is always a point.
std::string fmt_gib(long long bytes);

} // namespace vermilion::ctxmon
