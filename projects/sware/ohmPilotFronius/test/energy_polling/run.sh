#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d /tmp/ohmpilot-energy-tests.XXXXXX)
g++ -std=c++14 -Wall -Wextra -Werror -Iinclude test/energy_polling/test.cpp -o "$test_dir/test"
"$test_dir/test"
