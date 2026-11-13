// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Finds the recovery copies on disk, orders them for the start-up question and walks them.

#include "core/recovery_offer.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "core/log.h"

namespace vermilion {

std::vector<RecoveryItem> scan_recovery(const RecoveryStore& store) {
    Result<std::vector<std::string>> listed = store.list();
    if (!listed.is_ok()) {
        logging::warning("recovery: cannot list the copies: " + listed.error().message);
        return {};
    }
    std::vector<RecoveryItem> valid;
    std::vector<RecoveryItem> damaged;
    for (const std::string& id : listed.value()) {
        RecoveryItem item;
        item.id = id;
        Result<RecoveryParse> parsed = store.read_header(id);
        if (parsed.is_ok()) {
            item.header = parsed.value().header;
            valid.push_back(std::move(item));
        } else {
            item.damaged = true;
            item.problem = parsed.error().message;
            logging::warning("recovery: damaged copy " + id + ": " + item.problem);
            damaged.push_back(std::move(item));
        }
    }
    std::stable_sort(valid.begin(), valid.end(), [](const RecoveryItem& a, const RecoveryItem& b) {
        return a.header.copied_us < b.header.copied_us;
    });
    valid.insert(valid.end(), std::make_move_iterator(damaged.begin()),
                 std::make_move_iterator(damaged.end()));
    return valid;
}

RecoveryOffer::RecoveryOffer(std::vector<RecoveryItem> items) : items_(std::move(items)) {}

const RecoveryItem* RecoveryOffer::current() const {
    if (finished()) {
        return nullptr;
    }
    return &items_[next_];
}

std::size_t RecoveryOffer::number() const {
    if (finished()) {
        return 0;
    }
    return next_ + 1;
}

bool RecoveryOffer::can_recover_all() const {
    if (finished() || items_[next_].damaged) {
        return false;
    }
    std::size_t count = 0;
    for (std::size_t i = next_; i < items_.size(); ++i) {
        if (!items_[i].damaged) {
            ++count;
        }
    }
    return count >= 2;
}

void RecoveryOffer::answered() {
    if (!finished()) {
        ++next_;
    }
}

void RecoveryOffer::decide_later() {
    if (!finished()) {
        ++kept_;
        ++next_;
    }
}

std::vector<RecoveryItem> RecoveryOffer::take_valid() {
    std::vector<RecoveryItem> taken;
    for (std::size_t i = next_; i < items_.size(); ++i) {
        if (items_[i].damaged) {
            ++kept_;
        } else {
            taken.push_back(items_[i]);
        }
    }
    next_ = items_.size();
    return taken;
}

} // namespace vermilion
