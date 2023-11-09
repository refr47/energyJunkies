
#include <SD.h>
#include <SPI.h>
#include <FS.h>

#include "cardRW.h"
#include "utils.h"
#include "SD_MMC.h"
#include "pin_config.h"
#include <time.h>

// https://randomnerdtutorials.com/esp32-spi-communication-arduino/

#ifdef LOGFILE_SYS
const char *logFileSys = LOGFILE_SYS;
#else
const char *logFileSys = "logSys.txt";
#endif

static File loggingFile;
static bool loggingAvailable = false;

void appendFile(fs::FS &fs, const char *path, const char *message);

bool cardRW_setup()
{
    DBGf("CardReader %s", "BEGIN");
    // Serialprintln("CLOCK: %d, MISO: %d, MOSI: %d, CS: %d", SCK, MISO, MOSI, SS);

    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH); //  enable CS pin to read from peripheral 1
    SPI.begin(SCK, MISO, MOSI, SS);

    try
    {
        if (!SD.begin(SS))
        {

            DBGf("Card Mount Failed");
            return false;
        }
    }
    catch (...)
    {
        DBGf("Card Mount Failed");
        return false;
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
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    return true;
}

bool cardRW_createLoggingFile()
{
    DBGf("cardRW_createLoggingFile  file: %s\n", logFileSys);

    loggingFile = SD.open(logFileSys, FILE_WRITE);
    /*  if (!loggingFile)
     {
         Serial.printf("Failed to open file: %s - error: %s", logFileSys, strerror(errno));
         return false;
     } */
    loggingFile.close();
    if (SD.exists(logFileSys))
    {
        DBGf("Logfile existes on SD card!");
        loggingFile = SD.open(logFileSys, FILE_APPEND);
        loggingAvailable = true;

        return true;
    }

    return false;
}

bool cardRW_flushLoggingFile()
{
    loggingFile.flush();

    loggingFile = SD.open(logFileSys, FILE_APPEND);
    if (!loggingFile)
    {
        Serial.printf("Failed to open file: %s - error: %s", logFileSys, strerror(errno));
        loggingAvailable = false;
        return false;
    }
    return true;
}

bool cardRW_closeLoggingFile()
{

    loggingFile.close();
    return true;
}

int sdCardLogOutput(const char *format, va_list args)
{
    DBGf("Callback running, logging available: %x", loggingAvailable);
    if (!loggingAvailable)
        return 0;
    char buf[128];
    int ret = vsnprintf(buf, sizeof(buf), format, args);
    if (SD.exists(logFileSys))
    {
        loggingFile.print(buf);
        loggingFile.flush();
    }
    return ret;
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
