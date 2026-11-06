// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)

#include "core/result.h"

#include <string>

namespace vermilion {

template class Result<int>;
template class Result<std::string>;
template class Result<Unit>;

}  // namespace vermilion
