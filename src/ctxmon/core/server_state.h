// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Turns server snapshots into what the monitor shows and keeps the histories.

#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ctxmon/core/log_tokens.h"
#include "ctxmon/core/timing.h"

namespace vermilion::ctxmon {

constexpr double kLiveWindow = 2.0;      // seconds of samples used for the live speed
constexpr std::size_t kMaxRequests = 50; // request history length per server
constexpr std::size_t kMaxSamples = 64;  // live-speed samples kept per server
constexpr double kEtaWarnSeconds = 60.0; // warn when the context would fill within this

// How many tokens a running request has generated, and when that was read.
struct Sample {
    double time = 0.0;
    long long n_decoded = 0;
};

// live_speed of the Python original: tokens per second from the samples within window seconds
// of the newest one. Absent with fewer than two samples in the window or without growth.
std::optional<double> live_speed(const std::deque<Sample>& samples, double window = kLiveWindow);

// Whether the server answered: Up, Down (nothing listening) or Slow (no timely answer, which a
// busy server can give, so it is not treated as down).
enum class LinkState { Up, Down, Slow };

// A finished request handed over with a snapshot. seed is true for history that was already in
// the log when the monitor started, so its time is unknown.
struct SnapshotRequest {
    FinishedRequest request;
    bool seed = false;
};

// Everything one poll found, as the Python original's poll_once returns it.
struct Snapshot {
    LinkState state = LinkState::Up;
    std::optional<long long> used; // newest token count in the log
    std::optional<TokenKind> used_kind;
    std::optional<long long> n_ctx;
    bool busy = false;
    std::optional<long long> n_decoded;
    std::optional<std::string> model;
    bool log_found = false;
    std::optional<double> gen;    // tokens per second of the latest finished request
    std::optional<double> prompt; // prompt speed of the latest finished request
    std::vector<SnapshotRequest> requests;
};

// How trustworthy the shown token count is.
enum class CountKind {
    Down,       // the server is down
    None,       // no request in the log yet
    Exact,      // idle, and the log says how many tokens the context holds
    Processing, // a request is loading its prompt: the count is the last known one
    Estimate,   // a request is generating: prompt size plus what /slots reports
};

// The name the Python original uses for a kind ("down", "none", "exact", "processing",
// "estimate").
const char* count_kind_name(CountKind kind);

// A warning raised by update. level is the context percentage that was crossed, absent for the
// time-to-full alert. critical marks the highest level and the time-to-full alert.
struct Alert {
    std::optional<int> level;
    std::string message;
    bool critical = false;
};

// What update returns. With state Slow nothing else is set: the caller keeps the old numbers.
struct StateView {
    LinkState state = LinkState::Up;
    long long ctx = 0;
    long long shown = 0;
    double frac = 0.0; // shown / ctx, at most 1
    CountKind kind = CountKind::None;
    std::string source; // says which parts of the count are known
    bool estimating = false;
    std::optional<double> live; // live generation speed
    std::optional<double> gen;
    std::optional<double> prompt;
    std::optional<double> eta; // seconds until the context is full at the live speed
    bool busy = false;
    std::optional<std::string> model;
    bool log_found = false;
    std::vector<Alert> alerts; // raised by this snapshot
    int level = 0;             // highest warning level reached now, 0 if none
};

struct CtxPoint {
    double time = 0.0;
    long long tokens = 0;
};

struct SpeedPoint {
    double time = 0.0;
    std::optional<double> tps; // absent: a gap between requests
};

struct RequestRecord {
    FinishedRequest request;
    std::optional<double> time; // absent for seed requests
};

// ServerState of the Python original.
class ServerState {
public:
    // warn_levels are context percentages (80 and 95 in the original); history_seconds is how
    // long the histories reach back.
    explicit ServerState(long long default_ctx, std::vector<int> warn_levels = {80, 95},
                         double history_seconds = 600.0);

    // Digests one snapshot taken at time now (seconds).
    StateView update(const Snapshot& snapshot, double now);

    const std::deque<CtxPoint>& ctx_history() const { return ctx_hist_; }
    const std::deque<SpeedPoint>& speed_history() const { return speed_hist_; }
    const std::deque<RequestRecord>& requests() const { return requests_; }

private:
    void push_ctx(double now, long long tokens);
    void push_speed(double now, std::optional<double> tps);
    void add_requests(const std::vector<SnapshotRequest>& requests, double now);
    void mark_down(StateView& view, double now);
    std::optional<double> track_live_speed(const Snapshot& snapshot, double now);
    void add_warnings(StateView& view);

    long long default_ctx_;
    std::vector<int> levels_; // sorted
    double window_;
    std::deque<CtxPoint> ctx_hist_;
    std::deque<SpeedPoint> speed_hist_;
    std::deque<RequestRecord> requests_;
    std::deque<Sample> samples_;
    long long last_ctx_ = 0; // 0: none yet
    bool was_busy_ = false;
    std::set<int> warned_;    // levels already announced
    bool eta_warned_ = false; // the time-to-full alert was raised for the running request
    bool first_ = true;       // no snapshot seen since start or since the server went down
};

} // namespace vermilion::ctxmon
