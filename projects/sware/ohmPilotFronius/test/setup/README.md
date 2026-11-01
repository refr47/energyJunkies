Run `python3 test/setup/run.py` from the firmware directory after PlatformIO has installed ArduinoJson.

The host harness uses the production Setup declarations, field templates, validation and setup persistence implementation. Only hardware headers and Preferences are substituted. It covers atomic rejection of invalid patches, aggregated errors, optional/null/omitted values, string bounds and embedded NULs, host/URL validation, required Influx fields, NVS open/write/schema/corruption failures, and bounded diagnostic output.

Optional sanitizer run:

```
SETUP_TEST_CXXFLAGS='-fsanitize=address,undefined -g' python3 test/setup/run.py
```

When running under ptrace, LeakSanitizer may require `ASAN_OPTIONS=detect_leaks=0`; address and undefined-behavior checks remain enabled.

Runtime policy: omitted fields retain their values; optional text fields accept null as empty. Empty or legacy `---` hosts disable MQTT/Influx. Invalid supplied values reject the whole patch with all field errors. Influx requires organization, token and bucket when enabled. MQTT credentials may be empty. Network client settings are applied after reboot and require the corresponding firmware feature to be compiled in. Invalid NVS data is retained for recovery; safe AP defaults are returned in memory instead.
