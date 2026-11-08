#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
# Checks that every source file carries the SPDX and copyright headers, a purpose comment, and stays within size limits.
set -u -o pipefail

if [ "$#" -eq 0 ]; then
  cd "$(dirname "$0")/.." || exit 2
  list=$(find src tests tools -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.sh' \) | LC_ALL=C sort)
else
  list=$(printf '%s\n' "$@")
fi

checked=0
missing_header=0
missing_purpose=0
review=0
fail=0
missing_file=0

while IFS= read -r f; do
  [ -n "$f" ] || continue
  checked=$((checked + 1))
  if [ ! -r "$f" ]; then
    printf 'MISSING-FILE %s\n' "$f"
    missing_file=$((missing_file + 1))
    continue
  fi
  out=$(awk 'NR<=5 && /SPDX-License-Identifier: GPL-3\.0-or-later/ {s=1} NR<=5 && /Copyright \(C\) [0-9][0-9][0-9][0-9].* Adam Hainstock \(@adamjeremy-sk\)/ {c=1; cl=NR; next} cl && NR==cl+1 && /^(\/\/|#) [^ ]/ {p=1} END {print NR, s+0, c+0, p+0}' "$f")
  lines=${out%% *}
  rest=${out#* }
  s=${rest%% *}
  rest=${rest#* }
  c=${rest%% *}
  p=${rest#* }
  case "$f" in
  tests/*) review_limit=500 fail_limit=800 ;;
  *) review_limit=300 fail_limit=500 ;;
  esac
  if [ "$lines" -gt "$fail_limit" ]; then
    status=FAIL
    fail=$((fail + 1))
  elif [ "$lines" -gt "$review_limit" ]; then
    status=REVIEW
    review=$((review + 1))
  else
    status=OK
  fi
  printf '%-6s %5d %s\n' "$status" "$lines" "$f"
  if [ "$s" -eq 0 ] || [ "$c" -eq 0 ]; then
    printf 'MISSING-HEADER %s\n' "$f"
    missing_header=$((missing_header + 1))
  fi
  if [ "$p" -eq 0 ]; then
    printf 'MISSING-PURPOSE %s\n' "$f"
    missing_purpose=$((missing_purpose + 1))
  fi
done < <(printf '%s\n' "$list")

printf 'checked %d files: %d missing header, %d missing purpose, %d review, %d fail, %d missing file\n' \
  "$checked" "$missing_header" "$missing_purpose" "$review" "$fail" "$missing_file"

if [ "$missing_file" -gt 0 ]; then
  exit 2
elif [ "$missing_header" -gt 0 ] || [ "$missing_purpose" -gt 0 ] || [ "$fail" -gt 0 ]; then
  exit 1
else
  exit 0
fi
