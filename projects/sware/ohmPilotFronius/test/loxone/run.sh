#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d /tmp/ohmpilot-loxone-tests.XXXXXX)
g++ -std=c++17 -Wall -Wextra -Werror -DLOXONE_API \
  -Iinclude -I.pio/libdeps/lilygo-t-display-s3/ArduinoJson/src \
  src/devices/loxoneResponse.cpp test/loxone/test.cpp -o "$test_dir/test"
"$test_dir/test"
g++ -std=c++17 -Wall -Wextra -Werror -DLOXONE_API '-DLOXONE="test-host"' \
  -Itest/loxone/stubs -Iinclude -I.pio/libdeps/lilygo-t-display-s3/ArduinoJson/src \
  src/devices/loxoneAsProvider.cpp src/devices/loxoneResponse.cpp \
  test/loxone/provider.cpp -o "$test_dir/provider"
"$test_dir/provider"
