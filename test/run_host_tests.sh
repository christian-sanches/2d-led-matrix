#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
host_test_binary=$(mktemp "${TMPDIR:-/tmp}/led-matrix-host-tests.XXXXXX")
menu_test_binary=$(mktemp "${TMPDIR:-/tmp}/led-matrix-menu-tests.XXXXXX")
trap 'rm -f "$host_test_binary" "$menu_test_binary"' EXIT HUP INT TERM

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -I"$repo_root/include" \
    "$repo_root/test/host_tests.cpp" -o "$host_test_binary"
"$host_test_binary"

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -I"$repo_root/include" \
    "$repo_root/src/MenuController.cpp" "$repo_root/test/test_menu.cpp" \
    -o "$menu_test_binary"
"$menu_test_binary"
