#!/usr/bin/env bash
# Usage: run-logged.sh <title> <command...>
# Runs the command, streams its output, and on failure publishes the key lines as a
# GitHub annotation (::error::) plus a step summary, because raw logs are not available
# through the unauthenticated API.
title="$1"
shift
log="$(mktemp)"
set +e
"$@" 2>&1 | tee "$log"
rc=${PIPESTATUS[0]}
if [ "$rc" -ne 0 ]; then
  key="$(grep -E 'error|Error|FAILED|Failed|\*\*\*|Sanitizer|SUMMARY|CMake Error|not found' "$log" | tail -n 25 | cut -c1-300)"
  [ -z "$key" ] && key="$(tail -n 25 "$log" | cut -c1-300)"
  # vcpkg build logs (configure step builds the dependencies).
  if [ -n "${VCPKG_ROOT:-}" ] && [[ "$title" == *configure* ]]; then
    for f in "$VCPKG_ROOT"/buildtrees/*/*-err.log; do
      [ -s "$f" ] && key="$key"$'\n'"[vcpkg] $f"$'\n'"$(tail -n 15 "$f" | cut -c1-300)"
    done
  fi
  msg="$(printf '%s' "$key" | head -c 3500 | tr -d '\r' | sed -e 's/%/%25/g' -e ':a;N;$!ba;s/\n/%0A/g')"
  echo "::error title=${title} failed (exit ${rc})::${msg}"
  if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    { echo "### ${title} failed (exit ${rc})"; echo '```'; tail -n 80 "$log"; echo '```'; } >> "$GITHUB_STEP_SUMMARY"
  fi
fi
exit "$rc"
