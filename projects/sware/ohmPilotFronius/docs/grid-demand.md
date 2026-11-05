# Grid demand aspect

Backend setup fields (existing setup patch/read API and NVS persistence):

- `grid_demand_enabled`: default false; enables calculation.
- `grid_demand_enforce_limit`: default false; enables output limiting.
- `grid_demand_limit_w`: default 10000; accepted range 100..1000000 W.

Old schema-2 snapshots with none of these fields migrate to these defaults.
No GUI or MQTT publisher is added. WebSocket `live.gridDemand` supplies one
snapshot for future consumers. Interval start/end values are UTC epoch seconds.
`lastCompleted` is the most recently closed interval, including incomplete ones;
it is not a historical archive. `averageImportW` is null for incomplete intervals.
`importEnergyWh` is observed energy only, not an extrapolation across missing data.

The pure GridDemandAspect runs in the existing PID update under its existing
locks and consumes ENERGY_STATE. It holds the last measured p_grid between
updates, counts import only, splits at UTC quarter boundaries and detects clock
jumps against millis(). Measurements remain estimates, not billing meter values.
An interval started partway through, with stale/invalid data or during the night
pause is incomplete. Night polling is unchanged. During night, calculation has
no measurement coverage and demand limiting is bypassed, including forced heat.

During daytime enforcement caps all operational heating modes (auto, manual,
minimum temperature and legionella). It may therefore delay thermal programs.
Missing/invalid time or stale samples stop heating when enforcement is enabled.
The existing thermal stop remains authoritative. Raw diagnostic pin tests are
not an operational heating mode and are outside this limiter.

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
