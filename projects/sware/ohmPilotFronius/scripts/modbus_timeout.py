"""Allow build flags to override modbus-esp8266 4.1.0's timeout default.

Run after dependency discovery, before compilation. Reapply after a library
reinstall; fail explicitly if upstream changes the expected settings block.
"""

from pathlib import Path

Import("env")

settings = (
    Path(env.subst("$PROJECT_LIBDEPS_DIR"))
    / env.subst("$PIOENV")
    / "modbus-esp8266/src/ModbusSettings.h"
)
source = settings.read_text()
# Include the closing comment delimiter to avoid patching the documentation.
original = "*/\n#define MODBUSIP_TIMEOUT 1000\n"
guarded = "*/\n#ifndef MODBUSIP_TIMEOUT\n#define MODBUSIP_TIMEOUT 1000\n#endif\n"
if guarded not in source:
    if source.count(original) != 1:
        raise RuntimeError(f"Review Modbus timeout override: unexpected contents in {settings}")
    settings.write_text(source.replace(original, guarded, 1))
    print("Modbus: enabled MODBUSIP_TIMEOUT build-flag override")
