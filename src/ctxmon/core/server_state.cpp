// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// ServerState of the Python original: histories, exact versus estimated token counts.

#include "ctxmon/core/server_state.h"

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
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

// pct with no decimals, like Python's f"{pct:.0f}" (halves go to even).
std::string whole_percent(double pct) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.0f", pct);
    return buffer;
}

// Sets kind, shown, estimating and source of view: exact only when the request has finished and
// the log says how many tokens the context holds; otherwise an estimate whose source text says
// which parts are known.
void classify_count(const Snapshot& s, StateView& v) {
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

ServerState::ServerState(long long default_ctx, std::vector<int> warn_levels,
                         double history_seconds)
    : default_ctx_(default_ctx), levels_(std::move(warn_levels)), window_(history_seconds) {
    std::sort(levels_.begin(), levels_.end());
}

void ServerState::mark_down(StateView& view, double now) {
    view.ctx = last_ctx_ != 0 ? last_ctx_ : default_ctx_;
    view.kind = CountKind::Down;
    push_ctx(now, 0);
    push_speed(now, std::nullopt);
    samples_.clear();
    warned_.clear();
    first_ = true;
    was_busy_ = false;
}

// Live speed from the tokens generated so far by the running request.
std::optional<double> ServerState::track_live_speed(const Snapshot& s, double now) {
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
    return live;
}

// _warnings of the Python original: fills view.alerts and view.level.
void ServerState::add_warnings(StateView& v) {
    const double pct = v.frac * 100.0;
    const long long free_tokens = std::max(v.ctx - v.shown, 0LL);
    if (first_) {
        warned_.clear(); // do not alert about the past
        for (int lv : levels_) {
            if (pct >= lv) {
                warned_.insert(lv);
            }
        }
        first_ = false;
    } else {
        std::vector<int> newly;
        for (int lv : levels_) {
            if (pct >= lv && warned_.count(lv) == 0) {
                newly.push_back(lv);
            }
        }
        warned_.insert(newly.begin(), newly.end());
        if (!newly.empty()) {
            const int lv = newly.back();
            v.alerts.push_back({lv,
                                "Context " + whole_percent(pct) + "% full (" +
                                    with_commas(v.shown) + " / " + with_commas(v.ctx) +
                                    " tokens, " + with_commas(free_tokens) + " free)",
                                lv >= levels_.back()});
        }
    }
    for (auto it = warned_.begin(); it != warned_.end();) { // re-arm once usage drops well below
        it = pct < *it - 10 ? warned_.erase(it) : std::next(it);
    }
    for (int lv : levels_) {
        if (pct >= lv) {
            v.level = lv;
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
        mark_down(v, now);
        return v;
    }

    v.ctx = s.n_ctx && *s.n_ctx != 0 ? *s.n_ctx : default_ctx_;
    last_ctx_ = v.ctx;
    v.live = track_live_speed(s, now);
    classify_count(s, v);
    v.frac =
        v.ctx != 0 ? std::min(static_cast<double>(v.shown) / static_cast<double>(v.ctx), 1.0) : 0.0;
    push_ctx(now, v.shown);

    v.gen = s.gen;
    v.prompt = s.prompt;
    v.busy = s.busy;
    v.model = s.model;
    v.log_found = s.log_found;
    add_warnings(v);
    return v;
}

} // namespace vermilion::ctxmon
