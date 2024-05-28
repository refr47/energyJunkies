#include "debugConsole.h"
#include <Arduino.h>
#include "logReader.h"

static char buf[200];
int debug_LogOutput(const char *format, va_list args)
{

    int ret = vsnprintf(buf, sizeof(buf), format, args);
    logReader_captureSerialOutput((const char *)buf);
    Serial.printf("%s\n", buf);
    return ret;
}
