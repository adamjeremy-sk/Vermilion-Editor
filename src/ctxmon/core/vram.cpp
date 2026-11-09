// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// GPU memory readers: read_vram (sysfs and nvidia-smi parts) and fmt_gib of the Python original.

#include "ctxmon/core/vram.h"

#include <glib.h>

#include <algorithm>
#include <charconv>
#include <climits>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace vermilion::ctxmon {

namespace {

constexpr long long kMiB = 1048576;
constexpr double kGiB = 1073741824.0;

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

std::string ascii_lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

// A non-negative decimal number that fills text (after trimming), or nothing.
std::optional<long long> parse_count(std::string_view text) {
    text = trim(text);
    if (text.empty() || text.front() < '0' || text.front() > '9') {
        return std::nullopt;
    }
    long long value = 0;
    const char* last = text.data() + text.size();
    const auto result = std::from_chars(text.data(), last, value);
    if (result.ec != std::errc() || result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

// The whole content of the file at path, or nothing if it cannot be opened.
std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool is_card_name(const std::string& name) {
    return name.size() >= 5 && name.compare(0, 4, "card") == 0 && name[4] >= '0' && name[4] <= '9';
}

// The vendor in device/vendor; amd when the file is missing (mem_info_vram_* is amdgpu's).
GpuVendor vendor_of(const std::filesystem::path& device) {
    const std::optional<std::string> text = read_file(device / "vendor");
    if (!text) {
        return GpuVendor::Amd;
    }
    const std::string id = ascii_lower(trim(*text));
    if (id == "0x1002") {
        return GpuVendor::Amd;
    }
    if (id == "0x10de") {
        return GpuVendor::Nvidia;
    }
    if (id == "0x8086") {
        return GpuVendor::Intel;
    }
    return GpuVendor::Other;
}

// One card, or nothing if a counter is missing or is not a plain number.
std::optional<VramInfo> read_card(const std::filesystem::path& device, const std::string& name) {
    const std::optional<std::string> total_text = read_file(device / "mem_info_vram_total");
    const std::optional<std::string> used_text = read_file(device / "mem_info_vram_used");
    if (!total_text || !used_text) {
        return std::nullopt;
    }
    const std::optional<long long> total = parse_count(*total_text);
    const std::optional<long long> used = parse_count(*used_text);
    if (!total || !used) {
        return std::nullopt;
    }
    return VramInfo{*used, *total, name, vendor_of(device)};
}

// One "name, used, total" line (sizes in MiB), split at its last two commas; nothing if it has
// fewer than three fields, a size is not a plain number, or a size in bytes would overflow.
std::optional<VramInfo> parse_gpu_line(std::string_view line) {
    const std::size_t last = line.rfind(',');
    if (last == std::string_view::npos || last == 0) {
        return std::nullopt;
    }
    const std::size_t first = line.rfind(',', last - 1);
    if (first == std::string_view::npos) {
        return std::nullopt;
    }
    const std::optional<long long> used = parse_count(line.substr(first + 1, last - first - 1));
    const std::optional<long long> total = parse_count(line.substr(last + 1));
    if (!used || !total || *used > LLONG_MAX / kMiB || *total > LLONG_MAX / kMiB) {
        return std::nullopt;
    }
    return VramInfo{*used * kMiB, *total * kMiB, std::string(trim(line.substr(0, first))),
                    GpuVendor::Nvidia};
}

} // namespace

const char* vendor_key(GpuVendor vendor) {
    switch (vendor) {
    case GpuVendor::Amd:
        return "amd";
    case GpuVendor::Nvidia:
        return "nvidia";
    case GpuVendor::Intel:
        return "intel";
    case GpuVendor::Other:
        break;
    }
    return "other";
}

const char* vendor_label(GpuVendor vendor) {
    switch (vendor) {
    case GpuVendor::Amd:
        return "AMD";
    case GpuVendor::Nvidia:
        return "NVIDIA";
    case GpuVendor::Intel:
        return "Intel";
    case GpuVendor::Other:
        break;
    }
    return "GPU";
}

std::optional<VramInfo> read_vram_sysfs(const std::string& drm_root) {
    namespace fs = std::filesystem;
    const fs::path root(drm_root);
    std::vector<std::string> names;
    std::error_code error;
    for (fs::directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        std::string name = it->path().filename().string();
        if (is_card_name(name)) {
            names.push_back(std::move(name));
        }
    }
    std::sort(names.begin(), names.end());

    std::optional<VramInfo> best;
    for (const std::string& name : names) {
        const std::optional<VramInfo> card = read_card(root / name / "device", name);
        if (card && (!best || card->total_bytes > best->total_bytes)) {
            best = card;
        }
    }
    return best;
}

std::optional<VramInfo> parse_nvidia_smi(std::string_view output) {
    output = trim(output);
    std::optional<VramInfo> best;
    while (!output.empty()) {
        const std::size_t end = output.find('\n');
        const std::string_view line = output.substr(0, end);
        output = end == std::string_view::npos ? std::string_view() : output.substr(end + 1);
        const std::optional<VramInfo> gpu = parse_gpu_line(line);
        if (!gpu) {
            return std::nullopt;
        }
        if (!best || gpu->total_bytes > best->total_bytes) {
            best = gpu;
        }
    }
    return best;
}

std::string fmt_gib(long long bytes) {
    char buffer[64];
    g_ascii_formatd(buffer, sizeof buffer, "%.1f", static_cast<double>(bytes) / kGiB);
    return buffer;
}

} // namespace vermilion::ctxmon
