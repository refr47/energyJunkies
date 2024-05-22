#include "logReader.h"

#define BUFFER_SIZE_READER 1024

static char ringBuffer[BUFFER_SIZE_READER];
static size_t writeIndex = 0;
static size_t readIndex = 0;
static size_t dataLength = 0;
static bool redirect = false;

// prototypes
static void addToBuffer(char data);

void logReader_init()
{
    DBGf("logReader initialized with buffer size: %d", BUFFER_SIZE_READER);
    memset(ringBuffer, 0, BUFFER_SIZE_READER);
}
void logReader_enDisableRedirect(bool enDis)
{
    redirect = enDis;
}
String logReader_getBufferAsString()
{
    String result = "";
    for (size_t i = 0; i < dataLength; i++)
    {
        size_t index = (readIndex + i) % BUFFER_SIZE_READER;
        result += ringBuffer[index];
    }
    DBGf("logReader_getBufferAsString: %s", result.c_str());
    return result;
}

void logReader_captureSerialOutput()
{
    if (!redirect)
        return;
    while (Serial.available())
    {
        char ch = (char)Serial.read();
        addToBuffer(ch);
        Serial.print(ch); // Sende Zeichen zur PlatformIO-Konsole zurück
    }
}

static void addToBuffer(char data)
{
    ringBuffer[writeIndex] = data;
    writeIndex = (writeIndex + 1) % BUFFER_SIZE_READER;
    if (dataLength < BUFFER_SIZE_READER)
    {
        dataLength++;
    }
    else
    {
        // buffer full, oldest data is overwritten
        readIndex = (readIndex + 1) % BUFFER_SIZE_READER;
    }
}