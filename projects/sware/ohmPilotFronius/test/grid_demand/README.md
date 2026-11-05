Run from the firmware directory:

```sh
g++ -std=c++14 -Wall -Wextra -Werror -Iinclude test/grid_demand/test.cpp -o /tmp/grid-demand-test
/tmp/grid-demand-test
```
Covers aligned and crossed quarter boundaries, import-only accounting, stale
measurements, night exemption, disable/re-enable, observation mode, clock jumps
and unsynchronized time. No hardware or network is required.
