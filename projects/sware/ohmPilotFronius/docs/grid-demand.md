# Grid demand aspect

The Vue setup page exposes these settings through the existing `/getSetup` and
`/storeSetup` API (saved in NVS):

- `grid_demand_enabled`: default false; enables measurement and interval calculation.
- `grid_demand_enforce_limit`: default false; additionally caps heater output when
  measurement is enabled. Turning off measurement also disables effective capping;
  the saved capping preference is retained.
- `grid_demand_limit_w`: default 10000; accepted range 100..1000000 W.

Old schema-2 snapshots with none of these fields migrate to these defaults.
WebSocket `live.gridDemand` supplies the GUI's live snapshot; there is no MQTT
publisher. Interval start/end values are UTC epoch seconds, displayed in the
browser's local time. The GUI shows the current and most recently closed
interval, observed import energy and seconds of measurement coverage, remaining
energy budget, allowed remaining import power, and whether heating is currently
being limited. It also shows the current heater cap, measurement age, and the
last complete interval's average import power. A dash means the corresponding
live value is unavailable.
`lastCompleted` is the most recently closed interval, including incomplete ones;
it is not a historical archive. `averageImportW` is null for incomplete intervals.
`importEnergyWh` is observed energy only, not an extrapolation across missing data.
The setup page receives a new live snapshot on each WebSocket update; settings
still have to be saved explicitly. The status values are `disabled`,
`timeInvalid`, `night`, `stale`, `collecting`, and `ready`.

The pure GridDemandAspect runs in the existing PID update under its existing
locks and consumes ENERGY_STATE. It holds the last measured p_grid between
updates, counts import only, splits at UTC quarter boundaries and detects clock
jumps against millis(). Measurements remain estimates, not billing meter values.
An interval started partway through, with stale/invalid data or during the night
pause is incomplete. During a configured night polling pause, calculation has
no measurement coverage and demand limiting is bypassed. An active heating-plan
tariff window overrides that pause and keeps normal polling active. Emergency
heating outside the tariff window also resumes polling when demand enforcement
is enabled; the cap then applies to emergency heating as well.

Whenever grid measurements are active, enforcement caps all operational heating modes (auto, manual,
minimum temperature and legionella). It may therefore delay thermal programs.
Missing/invalid time or stale samples stop heating when enforcement is enabled.
The existing thermal stop remains authoritative. Raw diagnostic pin tests are
not an operational heating mode and are outside this limiter.

The minimum-temperature mode does not depend on PV surplus, WLAN, or a working
energy adapter. A failed energy read disables automatic surplus heating, while
minimum-temperature control continues. With demand enforcement enabled, a lost
grid measurement immediately stops the heater because no safe quarter-hour
headroom can be calculated. Heating resumes under the calculated cap once
measurements return. A running legionella cycle cannot suppress the minimum
temperature mode. Sensor faults and the absolute temperature limit still stop
all heating.

Allowed import is the remaining quarter energy divided by remaining time, capped
at the configured instantaneous limit. A 5..10% power reserve increases with
sample age. The heater budget subtracts p_grid minus heaterPowerAtSample, avoiding
repeated subtraction of heating changes against the same measurement. Budget
increases require a fresh sample and allow at most 500 W above the measured
heater output; reductions are immediate. Relay dwell cannot
retain power above an enforced cap. Partial intervals never permit import above
the configured instantaneous limit; unknown consumption before startup cannot
be recovered and the quarter limit cannot be guaranteed in that case.

No new grid measurements are requested. The inverter's unknown measurement
latency and uncontrollable household loads remain limitations. Energy is not
persisted across reboot; the first interval is marked incomplete instead.
