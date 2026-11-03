Run from the firmware directory (`ohmPilotFronius`) (no ESP32 hardware required):

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

The tests cover 40-second meter updates with 2-second control calls, the logged
4338 W export / 911 W import sequence, identical fresh samples, and setup changes.
The heater reference is captured when a successful sample is published; the
inverter API's own measurement latency cannot be removed by this association.

Battery policy: both priorities block discharge above 30 W and recover after three
fresh samples at or below 30 W. Priority 1 releases at 90% SOC and blocks below
88%; priority 0 can redirect battery charging power. Tests cover SOC hysteresis,
repeated calls, mode changes during recovery, and renewed discharge.
