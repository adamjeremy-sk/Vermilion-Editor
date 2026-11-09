// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Geometry of the monitor graphs: pixel polylines, tidy axis ceilings and the time span.

#pragma once

#include <optional>
#include <vector>

namespace vermilion::ctxmon {

// One sample of a history: a time in seconds and a value, or no value for a gap in the line.
struct GraphPoint {
    double time = 0.0;
    std::optional<double> value;
};

struct PixelPoint {
    double x = 0.0;
    double y = 0.0;
};

using PixelPath = std::vector<PixelPoint>;

// graph_segments of the Python original: pixel polylines for points, oldest first. x runs over
// the last span seconds before now (clamped to 0..w), y is value / ymax from the bottom of a
// graph h pixels high (clamped to 0..h). A gap (no value) ends the line. With step the value is
// held until the next point and up to now, which is right for "tokens in context". Nothing is
// drawn when span, ymax, w or h is not positive.
std::vector<PixelPath> graph_segments(const std::vector<GraphPoint>& points, double now,
                                      double span, double ymax, double w, double h,
                                      bool step = false);

// nice_max of the Python original: a speed ceiling of at least minimum, rounded up to a tidy
// number (a multiple of 10, 20, 50, 100, 200, 500 or 1000 depending on its size).
double nice_max(double v, double minimum = 10.0);

// graph_span of the Python original: the time span to show, as much history as exists, but not
// less than minimum and not more than window seconds.
double graph_span(const std::vector<GraphPoint>& points, double now, double window,
                  double minimum = 60.0);

} // namespace vermilion::ctxmon
