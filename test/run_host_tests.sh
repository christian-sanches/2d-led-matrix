#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_binary=$(mktemp "${TMPDIR:-/tmp}/led-matrix-tests.XXXXXX")
trap 'rm -f "$test_binary"' EXIT HUP INT TERM

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror -pedantic \
    -I"$repo_root/include" "$repo_root/test/host_tests.cpp" -o "$test_binary"
"$test_binary"
