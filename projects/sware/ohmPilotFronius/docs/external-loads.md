# External loads for the grid demand controller

The initial table is compiled into `src/pid/gridLoads.cpp`: `load1` through
`load4`, with priorities 1 through 4. Rename the IDs and labels there for the
installation. A nominal rating can be recorded, but decisions use only the
currently reported `powerW`. The boiler is controlled first and is not in the
table. The table has fixed-size fields and a separate runtime state so a later
GUI/NVS loader can replace the compiled defaults.

For each ID the ESP subscribes to `energy/loads/<id>/state`. Loxone or the
device publishes JSON on every change and at least every 30 seconds:

```json
{"on":true,"available":true,"powerW":1800,"ts":1795000000,"commandId":0}
```

`powerW` is the actual current consumption in whole watts; report zero when
off. `available` means the device can currently be switched. `ts` is Unix
epoch seconds from a synchronized clock. Status older than 90 seconds is
ignored, including retained MQTT status after reconnect. The ESP does not
infer availability from a nominal rating. The device must keep publishing
heartbeats even when off.

The ESP sends a non-retained command on `energy/loads/<id>/set`:

```json
{"on":false,"commandId":42,"validUntil":1795000015}
```

The receiver must ignore expired commands, apply the desired state, then
publish `state` with the same `commandId`. A duplicate command with the same
ID must be safe. The ESP retries an unconfirmed command after 10 seconds;
queued commands older than 10 seconds are discarded before MQTT publication.
The measured grid import, rather than the acknowledgement alone, determines
whether further shedding is needed. Restoration uses reverse priority and
requires spare import capacity for at least 60 seconds after a shed.

The quarter controller starts only after SNTP has synchronized. It uses fixed
UTC 900-second boundaries, which coincide with local quarter hours in Vienna.
A boot within a quarter leaves that interval incomplete; the first complete
interval begins at the next boundary. The dashboard marks its projection as
unavailable until then. If the configured night rest is active, the quarter
controller pauses and releases loads it switched off; future cheap-night
charging is outside this controller. Adjustable loads are represented by a
control type in the table but are not commanded by this first version.
