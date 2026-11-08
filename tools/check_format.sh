#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
# Checks that every .cpp and .h file under src and tests matches the committed .clang-format style.
set -u -o pipefail

if ! command -v clang-format >/dev/null 2>&1; then
  printf 'SKIP clang-format not installed\n'
  exit 77
fi

version=$(clang-format --version)
printf '%s\n' "${version%%$'\n'*}"

status=0
if [ "$#" -eq 0 ]; then
  cd "$(dirname "$0")/.." || exit 2
  find src tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | LC_ALL=C sort -z | xargs -0 clang-format --dry-run --Werror
  status=$?
else
  clang-format --dry-run --Werror "$@"
  status=$?
fi

if [ "$status" -eq 0 ]; then
  printf 'format check: clean\n'
  exit 0
fi
printf 'format check: DIRTY\n'
exit 1
