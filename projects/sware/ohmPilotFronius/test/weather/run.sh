#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo_dir"
test_dir=$(mktemp -d /tmp/ohmpilot-weather-tests.XXXXXX)
flags=(-std=c++17 -Wall -Wextra -Werror -Itest/weather/stubs -Itest/battery_monitor -Iinclude/pid -Iinclude/weather -Iinclude/ai -I.pio/libdeps/lilygo-t-display-s3/ArduinoJson/src)
sources=(src/weather/weatherModel.cpp src/pid/thermalController.cpp src/pid/mlPredictor.cpp test/weather/test.cpp)
g++ "${flags[@]}" "${sources[@]}" -o "$test_dir/weather-no-ml"
"$test_dir/weather-no-ml"
g++ "${flags[@]}" -DTINYNN_ENABLE "${sources[@]}" src/ai/TinyNN.cpp -o "$test_dir/weather-ml"
"$test_dir/weather-ml"
