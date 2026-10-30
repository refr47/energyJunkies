Run from the repository root (no ESP32 hardware required):

```sh
g++ -std=c++14 -Wall -Wextra -Werror -Itest/battery_monitor -Iinclude/pid src/pid/batteryMonitor.cpp test/battery_monitor/test.cpp -o /tmp/battery-monitor-test
/tmp/battery-monitor-test
```

The local headers stub Arduino and logging only; tests compile the production implementation.
