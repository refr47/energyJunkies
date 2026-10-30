Run from the repository root after PlatformIO has installed ArduinoJson:

```sh
bash test/weather/run.sh
```

The tests compile production forecast parsing, temperature control, and neural
network code. GPIO/Arduino and NVS storage are host stubs. Both enabled and disabled
TinyNN builds are tested. Cases include complete and malformed forecasts, absent
sunshine tomorrow/overmorgen, insufficient remaining energy, DST and year changes,
sensor failure, legionella completion, neural learning from external targets,
training throttling, persistence throttling, and bounded temperature corrections.

Implementation assumptions:

- Open-Meteo hourly radiation describes the preceding hour. Unix timestamps and
  local calendar boundaries are used (including DST). The fourth requested day
  supplies the final midnight interval needed for three complete local days.
  Documentation: https://open-meteo.com/en/docs
- Until panel orientation is confirmed, PV power is estimated from horizontal
  irradiation with the existing 36 x 405 W / 10 kW inverter parameters. It is an
  approximate trend model, not a calibrated production forecast.
- The reserve rule compares today's full-day forecast with the weaker of the
  next two days and scales the bonus down below 2 kWh remaining today. The rule
  permits up to +20 C, with a separate absolute 80 C limit in every heating mode.
  Actual reserve heating requires PV production and the BatteryMonitor budget.
- TinyNN learns explicit rule/headroom targets every 30 minutes. It is not yet a
  model of measured household demand. For the first 100 samples, only the rule
  controls the reserve; afterwards ML may correct intermediate bonuses by at
  most +/-2 C. Zero and full reserve remain deterministic.
- Old incorrectly normalized weights are not loaded (new NVS namespace tinynn-v2).
  New weights are saved at most once per six hours of uptime during training.
- A failed weather fetch retains the last valid forecast until expiry; expired
  forecasts reset all weather inputs and disable the reserve.
