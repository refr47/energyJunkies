#ifdef CARRD_READER
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include <time.h>
#include <string.h>

#include "cardRW.h"
#include "utils.h"
#include "SD_MMC.h"
#include "pin_config.h"

#include "debugConsole.h"
// https://randomnerdtutorials.com/esp32-spi-communication-arduino/

#ifdef LOGFILE_SYS
const char *LOG_FILE_SYSTEM_NAME = LOGFILE_SYS;
#else
const char *LOG_FILE_SYSTEM_NAME = "logSys.txt";
#endif

// #define MAX_LOG_SIZE 1024 * 1024
#define MAX_LOG_SIZE 200
#define MIN_SPACE_ON_SD_CARD_IN_BYTES 1025 * 512
#define MAX_WRITES_BEFORE_FLASH 20
#define COUNTER_MAX_FOR_MOUNT 5

static File loggingFile;
static bool loggingAvailable = false;
static unsigned int counterForFlush = 0;
static bool logEventsToCard = false, logInverterToCard = false;

void appendFile(fs::FS &fs, const char *path, const char *message);

bool cardRW_setup(bool logToCard, bool logInverter)
{
    DBGf("CardReader, logToCard: %x, logInverterToCard: %x", logToCard, logInverter);
    logEventsToCard = logToCard;
    logInverterToCard = logInverter;
    // Serialprintln("CLOCK: %d, MISO: %d, MOSI: %d, CS: %d", SCK, MISO, MOSI, SS);
    unsigned int counter = 1;
    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH); //  enable CS pin to read from peripheral 1
    SPI.begin(SCK, MISO, MOSI, SS);

    while (!SD.begin(SS))
    {

        delay(2000);
        DBGf("Try to mount card (%d)", counter);
        if (++counter > COUNTER_MAX_FOR_MOUNT)
        {
            DBGf("Card Mount Failed:  %d trials", counter);
            ESP_LOGE(TAG, "SD Card Reader kann nicht initialisiert werden!");
            return false;
        }
    }

    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        DBGf("No SD card attached");

        return false;
    }

    DBG("SD Card Type: ");
    if (cardType == CARD_MMC)
    {
        DBGf("MMC");
    }
    else if (cardType == CARD_SD)
    {
        DBGf("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        DBGf("SDHC");
    }
    else
    {
        DBGf("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    // DBGln("SD Card Size: %lluMB\n", cardSize);
    DBGf("SD Card Size: %dMB\n", cardSize);

    return true;
}

bool cardRW_createLoggingFile()
{
    DBGf("cardRW_createLoggingFile  file: %s\n", LOG_FILE_SYSTEM_NAME);
    if (SD.exists(LOG_FILE_SYSTEM_NAME))
    {
        DBGf("Logfile existes on SD card!");
    }
    loggingFile = SD.open(LOG_FILE_SYSTEM_NAME, FILE_APPEND);
    if (!loggingFile)
    {
        DBGf("Failed to open file: %s - error: %s", LOG_FILE_SYSTEM_NAME, strerror(errno));
        return false;
    }
    loggingAvailable = true;
    return true;
}

bool cardRW_flushLoggingFile()
{
    if (!loggingAvailable)
    {
        DBGf("Failed to update logging file");
        return false;
    }
    if (loggingFile)
        loggingFile.flush();

    loggingFile = SD.open(LOG_FILE_SYSTEM_NAME, FILE_APPEND);
    if (!loggingFile)
    {
        Serial.printf("Failed to open file: %s - error: %s", LOG_FILE_SYSTEM_NAME, strerror(errno));
        loggingAvailable = false;
        return false;
    }
    loggingAvailable = true;
    return true;
}

bool cardRW_closeLoggingFile()
{

    loggingFile.close();
    return true;
}

inline int printLog(bool writeSD, const char *format, va_list args)
{
    char buf[128];
    int ret = vsnprintf(buf, sizeof(buf), format, args);
    // DBGf("printLog, write: %x, SD_Exists: %x, counter: %d", writeSD, SD.exists(LOG_FILE_SYSTEM_NAME), counterForFlush);
    if (writeSD)
    {
        if (SD.exists(LOG_FILE_SYSTEM_NAME))
        {
            loggingFile.print(buf);
            if (++counterForFlush > MAX_WRITES_BEFORE_FLASH)
            {
                loggingFile.flush();
                counterForFlush = 0;
                // DBGf("cardRW_flush:");
            }
        }
    }

    DBGf("%s\n", buf);
    return ret;
}

// interface for ESP Logging System
int cardRW_LogOutput(const char *format, va_list args)
{
    // DBGf("Callback running, logging available: %x, Size: %d", loggingAvailable, loggingFile.size());

    if (logEventsToCard)
    {

        if (loggingAvailable)
        {
            if (loggingFile.size() > MAX_LOG_SIZE)
            {
                time_t current_time = time(NULL);
                // Convert the current time to a struct tm.
                struct tm *time_info = localtime(&current_time);
                // Print the current time in the format "HH:MM:SS".
                printf("%02d:%02d:%02d\n", time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
                // SD.cardSize() / (1024 * 1024)
                char newF[128];
                memset(newF, 0, 128);
                strncpy(newF, LOG_FILE_SYSTEM_NAME, strlen(LOG_FILE_SYSTEM_NAME) - 4);

                char buf[30];
                sprintf(buf, "-%d_%d_%d.txt", time_info->tm_mday, time_info->tm_hour, time_info->tm_min);
                strncat(newF, buf, strlen(buf));
                if (!cardRW_renameFile(LOG_FILE_SYSTEM_NAME, newF))
                {
                    loggingAvailable = false;
                    DBGf("Cannot rename Logging file: %s to %s", LOG_FILE_SYSTEM_NAME, newF);
                    printLog(false, format, args);
                    return 0;
                }
                // DBGf("Logfile renamed to %s", newF);
                if (SD.cardSize() - SD.usedBytes() < MIN_SPACE_ON_SD_CARD_IN_BYTES)
                {
                    loggingAvailable = false;
                    DBGf("There is only a little space (%d) left on the map", SD.cardSize() - SD.usedBytes());
                    printLog(false, format, args);
                    return 0;
                }
                loggingFile.close();
                if (!cardRW_createLoggingFile())
                {
                    return 0;
                }
            }
        }
    }
    return printLog(true, format, args);
}

/* ****************************************************************************************** */
// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        DBGf("Failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        DBGf("File written");
    }
    else
    {
        DBGf("Write failed");
    }
    file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
bool cardRW_appendFile(const char *path, const char *message)
{
    DBGf("Appending to file: %s\n", path);

    File file = SD.open(path, FILE_APPEND);
    if (!file)
    {
        DBGf("Failed to open file for appending");
        return false;
    }
    if (file.print(message))
    {
        DBGf("Message appended");
    }
    else
    {
        DBGf("Append failed");
    }
    file.close();
    return true;
}

void cardRW_listDir(const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\n", dirname);

    File root = SD.open(dirname);
    if (!root)
    {
        DBGf("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        DBGf("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            DBGf("  DIR : %s", file.name());
            if (levels)
            {
                cardRW_listDir(file.name(), levels - 1);
            }
        }
        else
        {
            DBGf("  FILE: %s, Size: %d", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}

bool cardRW_createDir(const char *path)
{
    DBGf("Creating Dir: %s\n", path);
    if (SD.mkdir(path))
    {
        DBGf("Dir created");
        return true;
    }
    else
    {
        DBGf("mkdir failed");
        return false;
    }
}

bool cardRW_removeDir(const char *path)
{
    DBGf("Removing Dir: %s\n", path);
    if (SD.rmdir(path))
    {
        DBGf("Dir removed");
        return true;
    }
    else
    {
        DBGf("rmdir failed");
        return false;
    }
}

bool cardRW_renameFile(const char *path1, const char *path2)
{
    DBGf("Renaming file %s to %s\n", path1, path2);
    if (SD.rename(path1, path2))
    {
        DBGf("File renamed");
        return true;
    }
    else
    {
        DBGf("Rename failed");
        return false;
    }
}

bool cardRW_deleteFile(const char *path)
{
    DBGf("Deleting file: %s\n", path);
    if (SD.remove(path))
    {
        DBGf("File deleted");
        return true;
    }
    else
    {
        DBGf("Delete failed");
        return false;
    }
}

void cardRW_testFileIO(const char *path)
{
    File file = SD.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if (file)
    {
        len = file.size();
        size_t flen = len;
        start = millis();
        while (len)
        {
            size_t toRead = len;
            if (toRead > 512)
            {
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        DBGf("%u bytes read for %u ms\n", flen, end);
        file.close();
    }
    else
    {
        DBGf("Failed to open file for reading");
    }

    file = SD.open(path, FILE_WRITE);
    if (!file)
    {
        DBGf("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for (i = 0; i < 2048; i++)
    {
        file.write(buf, 512);
    }
    end = millis() - start;
    DBGf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}

#endif