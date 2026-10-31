# Energy polling

Run `bash test/energy_polling/run.sh` from the firmware directory.

The defaults in `platformio.ini` are:

- Local time 23:00 inclusive to 05:00 exclusive: no energy reads or source initialization.
- Otherwise poll every 40 seconds. Only fresh successful samples establish five minutes below 20 W; then poll every five minutes.
- Leave low-PV mode on a fresh sample above 50 W. Values between 20 and 50 W retain an established low-PV mode.
- Invalid clock: use PV-based polling. Equal night start/end disables the night window.
- Night exit requests an immediate fresh sample. Failed or expired samples cannot drive automatic surplus heating.

The PID blocks automatic surplus heating during night/low-PV mode, so normal temperature maintenance stops at the existing minimum temperature. Existing minimum-temperature heating (including its current power), manual commands, legionella cycles and sensor/overtemperature protection retain their priorities. The configured maximum temperature is not overwritten.

WebSocket live data includes `energyValid`, `energySaving`, `energyNight`, and `energyAgeMs`; retained measurement values are not replaced with fake zero readings. Existing UI consumers must use the validity flag to distinguish historical values.

Tests cover midnight and same-day windows, exact thresholds, hysteresis, fresh-sample counting, failed reads, night exit, non-finite PV, and millis rollover. Temperature/legionella regression tests are in `test/weather/run.sh`.
