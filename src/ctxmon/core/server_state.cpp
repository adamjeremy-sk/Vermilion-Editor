// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// ServerState of the Python original: histories, exact versus estimated token counts.

#include "ctxmon/core/server_state.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace vermilion::ctxmon {

namespace {

// n with comma thousands separators, like Python's f"{n:,}".
std::string with_commas(long long n) {
    const bool negative = n < 0;
    const unsigned long long magnitude =
        negative ? 0ULL - static_cast<unsigned long long>(n) : static_cast<unsigned long long>(n);
    const std::string digits = std::to_string(magnitude);
    std::string out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            out += ',';
        }
        out += digits[i];
    }
    return negative ? "-" + out : out;
}

// Drops points from the front while the second one is older than the window: one point older
// than the window stays, so lines enter from the left edge.
template <typename History>
void prune(History& history, double now, double window) {
    while (history.size() > 1 && history[1].time < now - window) {
        history.pop_front();
    }
}

} // namespace

std::optional<double> live_speed(const std::deque<Sample>& samples, double window) {
    if (samples.size() < 2) {
        return std::nullopt;
    }
    const double t_end = samples.back().time;
    const Sample* first = nullptr;
    for (const Sample& s : samples) {
        if (t_end - s.time <= window) {
            first = &s;
            break;
        }
    }
    const Sample& last = samples.back();
    if (first == nullptr || first == &last) {
        return std::nullopt; // fewer than two samples in the window
    }
    if (last.time <= first->time || last.n_decoded <= first->n_decoded) {
        return std::nullopt;
    }
    return static_cast<double>(last.n_decoded - first->n_decoded) / (last.time - first->time);
}

const char* count_kind_name(CountKind kind) {
    switch (kind) {
    case CountKind::Down:
        return "down";
    case CountKind::None:
        return "none";
    case CountKind::Exact:
        return "exact";
    case CountKind::Processing:
        return "processing";
    case CountKind::Estimate:
        return "estimate";
    }
    return "";
}

void ServerState::push_ctx(double now, long long tokens) {
    if (ctx_hist_.empty() || tokens != ctx_hist_.back().tokens ||
        now - ctx_hist_.back().time >= 2.0) {
        ctx_hist_.push_back({now, tokens});
    }
    prune(ctx_hist_, now, window_);
}

void ServerState::push_speed(double now, std::optional<double> tps) {
    if (!tps) {
        if (!speed_hist_.empty() && speed_hist_.back().tps) {
            speed_hist_.push_back({now, std::nullopt});
        }
    } else if (speed_hist_.empty() || !speed_hist_.back().tps ||
               now - speed_hist_.back().time >= 1.0) {
        speed_hist_.push_back({now, tps});
    }
    prune(speed_hist_, now, window_);
}

void ServerState::add_requests(const std::vector<SnapshotRequest>& requests, double now) {
    for (const SnapshotRequest& r : requests) {
        requests_.push_back({r.request, r.seed ? std::nullopt : std::optional<double>(now)});
        if (requests_.size() > kMaxRequests) {
            requests_.pop_front();
        }
        if (!r.seed && r.request.gen_tps != 0.0) {
            push_speed(now, r.request.gen_tps);
        }
    }
}

StateView ServerState::update(const Snapshot& s, double now) {
    StateView v;
    v.state = s.state;
    if (s.state == LinkState::Slow) {
        return v;
    }
    add_requests(s.requests, now);

    if (s.state == LinkState::Down) {
        v.ctx = last_ctx_ != 0 ? last_ctx_ : default_ctx_;
        v.kind = CountKind::Down;
        push_ctx(now, 0);
        push_speed(now, std::nullopt);
        samples_.clear();
        was_busy_ = false;
        return v;
    }

    const long long ctx = s.n_ctx && *s.n_ctx != 0 ? *s.n_ctx : default_ctx_;
    last_ctx_ = ctx;

    // Live speed from the tokens generated so far by the running request.
    std::optional<double> live;
    if (s.busy && s.n_decoded) {
        if (!samples_.empty() && *s.n_decoded < samples_.back().n_decoded) {
            samples_.clear(); // a new request started
        }
        samples_.push_back({now, *s.n_decoded});
        if (samples_.size() > kMaxSamples) {
            samples_.pop_front();
        }
        live = live_speed(samples_);
    } else {
        samples_.clear();
    }
    if (live) {
        push_speed(now, live);
    } else if (was_busy_ && !s.busy) {
        push_speed(now, std::nullopt); // break the line between requests
    }
    was_busy_ = s.busy;

    // Exact versus estimate.
    const long long gen_now = s.busy && s.n_decoded && *s.n_decoded != 0 ? *s.n_decoded : 0;
    if (!s.used) {
        v.kind = CountKind::None;
        v.shown = gen_now;
        v.estimating = gen_now != 0;
        v.source = v.estimating ? "estimate: " + with_commas(gen_now) +
                                      " generated, prompt size not in the log yet"
                                : "no request in the log yet";
    } else if (!s.busy) {
        v.kind = CountKind::Exact;
        v.shown = *s.used;
        v.source = "exact: from the server log";
    } else if (s.used_kind == TokenKind::Final && gen_now == 0) {
        // Busy, but the newest log line is the previous request's end: its prompt is not known.
        v.kind = CountKind::Processing;
        v.shown = *s.used;
        v.estimating = true;
        v.source = "estimate: new prompt loading (last known " + with_commas(*s.used) + ")";
    } else if (gen_now == 0) {
        v.kind = CountKind::Processing;
        v.shown = *s.used;
        v.estimating = true;
        v.source = "estimate: prompt loading, " + with_commas(*s.used) + " so far";
    } else {
        v.kind = CountKind::Estimate;
        v.shown = *s.used + gen_now;
        v.estimating = true;
        v.source = "estimate: prompt " + with_commas(*s.used) + " + " + with_commas(gen_now) +
                   " generated";
    }

    v.ctx = ctx;
    v.frac =
        ctx != 0 ? std::min(static_cast<double>(v.shown) / static_cast<double>(ctx), 1.0) : 0.0;
    const long long free_tokens = std::max(ctx - v.shown, 0LL);
    if (s.busy && live && free_tokens > 0) {
        v.eta = static_cast<double>(free_tokens) / *live;
    }
    push_ctx(now, v.shown);

    v.live = live;
    v.gen = s.gen;
    v.prompt = s.prompt;
    v.busy = s.busy;
    v.model = s.model;
    v.log_found = s.log_found;
    return v;
}

} // namespace vermilion::ctxmon
