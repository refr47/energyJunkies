Run from the repository root (no ESP32 hardware required):

```sh
g++ -std=c++14 -Wall -Wextra -Werror -Itest/battery_monitor -Iinclude/pid src/pid/batteryMonitor.cpp test/battery_monitor/test.cpp -o /tmp/battery-monitor-test
/tmp/battery-monitor-test
```

The local headers stub Arduino and logging only; tests compile the production implementation.

Control-loop regression with simulated GPIO, relay dwell, and inverter power redistribution:

```sh
g++ -std=c++14 -Wall -Wextra -Werror -Itest/battery_monitor -Iinclude/pid src/pid/batteryMonitor.cpp src/pid/phaseController.cpp test/battery_monitor/test_control.cpp -o /tmp/battery-control-test
/tmp/battery-control-test
```
