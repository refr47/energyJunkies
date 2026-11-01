Run `python3 test/setup/run.py` from the firmware directory after PlatformIO has installed ArduinoJson.

The host harness uses the production Setup declarations, field templates, validation and setup persistence implementation. Only hardware headers and Preferences are substituted. It covers atomic rejection of invalid patches, aggregated errors, optional/null/omitted values, string bounds and embedded NULs, host/URL validation, required Influx fields, NVS open/write/schema/corruption failures, and bounded diagnostic output.

Optional sanitizer run:

```
SETUP_TEST_CXXFLAGS='-fsanitize=address,undefined -g' python3 test/setup/run.py
```

When running under ptrace, LeakSanitizer may require `ASAN_OPTIONS=detect_leaks=0`; address and undefined-behavior checks remain enabled.

The suite also checks legacy schema migration (retaining original bytes), schema-2 round trips and revisions, activation flags, runtime energy bounds, and hotupdate lock failures, retries, restart decisions and consecutive saves. See `docs/runtime-setup.md` for the API and runtime policy. Energy-policy state transitions are tested separately by `test/energy_polling/run.sh`.
