Run from the project directory:

```sh
python3 test/energy_service/run.py
```

The host harness compiles the actual service function bodies with mock locks,
WLAN state and an energy adapter. It checks partial lock acquisition cleanup,
WLAN versus energy failure handling, initialization retries, deferred publication
and retaining the active adapter across failed reads and supervisor lock contention.
It does not simulate FreeRTOS scheduling or the inverter's Modbus responses.
