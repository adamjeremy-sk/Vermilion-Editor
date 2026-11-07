// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Explicit instantiations of Result<T>, so the template is compiled with the core flags.

#include "core/result.h"

#include <string>

namespace vermilion {

template class Result<int>;
template class Result<std::string>;
template class Result<Unit>;

} // namespace vermilion
