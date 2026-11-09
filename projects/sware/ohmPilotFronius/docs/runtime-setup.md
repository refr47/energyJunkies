# Runtime configuration (schema 2)

MQTT and Influx are compiled by `FEATURE_MQTT` / `FEATURE_INFLUX`. These are capability switches, not addresses or activation flags. Both are enabled in the standard PlatformIO build. Board, pins, task/watchdog and compiler settings remain build configuration.

The field registry in `setupFields.cpp` is shared by `/getSetup`, `/storeSetup` and persisted schema-2 records. Existing HTTP field names are retained.

| Field | Default | Validation / meaning |
| --- | --- | --- |
| mqtt_enabled | false | Enable without deleting credentials when disabled |
| mqtt_port | 1883 | Integer 1–65535 |
| influx_enabled | false | Enabled service requires URL, organization, token and bucket |
| energy_day_seconds | 40 | Integer 5–3600 seconds |
| energy_saving_seconds | 300 | At least normal interval, at most 86400 seconds |
| energy_night_enabled | true | Enable scheduled suspension of energy requests |
| energy_night_start | 23 | Local hour 0–23, inclusive |
| energy_night_end | 5 | Local hour 0–23, exclusive; equal hours disable pause |
| energy_low_pv_watts | 20 | Finite 0–1000000 W; enter saving below threshold |
| energy_wake_pv_watts | 50 | Finite, greater than low threshold, at most 1000000 W |
| energy_hold_seconds | 300 | Integer 1–86400 seconds; distinct valid samples must establish low PV |
| heating_plan_enabled | false | Optional tariff heating window; disabled preserves immediate minimum-temperature behavior |
| heating_plan_start_minute / heating_plan_end_minute | 1260 / 360 | Local minute of day, 0–1439; unequal when enabled; overnight allowed |
| heating_plan_emergency_c | 2 | Must be below `heizstab_temp_min` when the plan is enabled |
| heating_plan_boiler_liters | 200 | Estimated effective volume, 20–5000 litres |

The slow interval is for low PV, not the night window. During the night window no energy request is made unless an active tariff window or demand-limited emergency heating needs grid measurements. Missing valid local time disables the scheduled window. Freshness thresholds use twice the corresponding runtime interval. Changing the energy configuration invalidates the old sample, resets the polling policy, and schedules a new read when allowed by the night window. All seconds-to-milliseconds conversions fit uint32_t under the validated bounds.

## Persistence and migration

`setupLegacy.h` freezes the original schema-0/1 ESP32 binary layout and string capacities. Legacy settings are loaded into the new model with defaults for new fields, MQTT port 1883, and enabled flags inferred from nonempty/non-`---` hosts. Reads do not modify flash. The next explicit successful save writes a single `setup_v2` NVS JSON blob containing `schema`, `revision` and the complete registered `settings`. No new binary `sizeof(Setup)` dependency is introduced. The legacy blob is retained; firmware rollback would see the older configuration, not subsequent schema-2 edits.

Malformed or unsupported schema-2 records do not silently fall back to stale legacy values. They return safe defaults in memory and an error to the storage caller. `/getSetup` identifies defaults using `source: "defaults"` and a warning; the normal-mode save handler refuses to overwrite unreadable NVS. AP mode permits explicit recovery by saving validated defaults plus user edits.

## API and application

`GET /getSetup` retains `result` and `error.code` (1 = successful response), and adds `schema`, `source`, `capabilities`, and `state`. State reports saved/applied revision, MQTT/Influx consumed revisions, pending application, and connection states. Connection success is separate from consumption of a configuration.

`POST /storeSetup` accepts a partial flat settings object. Omitted fields retain their stored values. Optional text fields accept null to clear; boolean/numeric fields do not. Disabled integrations retain credentials and do not require a working host/complete connection configuration. Invalid input rejects the whole patch and returns all `errors` with field names and messages. Success reports `done: 1`, `revision` and `apply: "pending"` or `"restart_required"`.

The clock task applies stored revisions without holding application locks during flash reads. Application lock order is Data → Config → PID; each acquired lock is released on every failure. Later revisions remain pending if a lock fails or another save arrives. WiFi credentials and connection settings of compiled energy providers require reboot. AMIS address/key changes apply without reboot when `AMIS_READER_DEV` is not compiled. AP mode alone does not force a reboot. Required restarts are owned by the clock task and deferred until after the save response can be sent. MQTT/Influx changes do not require reboot.

The clock task owns the MQTT client; other tasks enqueue bounded messages. Reconfiguration disconnects the old connection and drops queued old messages. Failed connections are retried with a delay, without a blocking retry loop. The energy task owns Influx configuration, connection validation and normal writes, and runs its connection service even during a scheduled energy pause. No broker/database connection is made under shared configuration locks.

The Vue form uses the activation flags, displays seconds/watts/hours explicitly, and polls application status without overwriting ongoing form edits. Unsupported firmware capabilities are disabled in the form. Device upload is a separate step: both firmware and updated LittleFS assets are needed.

## Diagnosing a save crash

The firmware logs `storeSetup phase=request`, `jsonParsed`, `nvsReadBegin`,
`nvsReadDone`, `validated`, `nvsWriteBegin`, and `nvsWriteDone`, followed by
`Setup saved`. The first missing phase narrows down the failing operation. The
boot log also prints `Reset reason`; capture that line together with any
`Guru Meditation`, stack canary, or backtrace lines. These phase logs contain
body size and free heap, but never credentials or the JSON body.
