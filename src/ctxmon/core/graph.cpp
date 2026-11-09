// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Graph geometry: graph_segments, nice_max and graph_span of the Python original.

#include "ctxmon/core/graph.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

namespace vermilion::ctxmon {

std::vector<PixelPath> graph_segments(const std::vector<GraphPoint>& points, double now,
                                      double span, double ymax, double w, double h, bool step) {
    std::vector<PixelPath> segments;
    if (span <= 0 || ymax <= 0 || w <= 0 || h <= 0) {
        return segments;
    }
    const double t0 = now - span;
    const auto x_of = [&](double t) { return std::max(0.0, std::min(w, (t - t0) / span * w)); };
    const auto y_of = [&](double v) { return h - std::min(std::max(v, 0.0) / ymax, 1.0) * h; };

    PixelPath current;
    std::optional<double> before; // the newest value older than the shown span
    for (const GraphPoint& p : points) {
        if (!p.value) {
            if (!current.empty()) {
                if (step) {
                    current.push_back({p.time >= now ? w : x_of(p.time), current.back().y});
                }
                segments.push_back(std::move(current));
            }
            current.clear();
            before.reset();
            continue;
        }
        if (p.time < t0) {
            before = p.value;
            continue;
        }
        if (current.empty() && before) {
            current.push_back({0.0, y_of(*before)}); // the line enters from the left edge
        }
        if (step && !current.empty()) {
            current.push_back({x_of(p.time), current.back().y});
        }
        current.push_back({x_of(p.time), y_of(*p.value)});
    }
    if (!current.empty()) {
        if (step) {
            current.push_back({w, current.back().y});
        }
        segments.push_back(std::move(current));
    }
    return segments;
}

double nice_max(double v, double minimum) {
    v = std::max(v, minimum);
    for (const double step : {10.0, 20.0, 50.0, 100.0, 200.0, 500.0}) {
        if (v <= step * 5) {
            return std::ceil(v / step) * step;
        }
    }
    return std::ceil(v / 1000.0) * 1000.0;
}

double graph_span(const std::vector<GraphPoint>& points, double now, double window,
                  double minimum) {
    if (points.empty()) {
        return minimum;
    }
    return std::max(minimum, std::min(window, now - points.front().time));
}

} // namespace vermilion::ctxmon
