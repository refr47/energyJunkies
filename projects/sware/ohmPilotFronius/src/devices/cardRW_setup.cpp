
#include <SD.h>
#include <SPI.h>
#include <FS.h>

#include "cardRW.h"
#include "utils.h"
#include "SD_MMC.h"
#include "pin_config.h"
// https://randomnerdtutorials.com/esp32-spi-communication-arduino/

void appendFile(fs::FS &fs, const char *path, const char *message);

bool cardRW_setup()
{
    DBGln("CardReader -- Setup");
    Serialprintln("CLOCK: %d, MISO: %d, MOSI: %d, CS: %d", SCK, MISO, MOSI, SS);

    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH); //  enable CS pin to read from peripheral 1
    SPI.begin(SCK, MISO, MOSI, SS);

    try
    {
        if (!SD.begin(SS))
        {

            DBGln("Card Mount Failed");
            return false;
        }
    }
    catch (...)
    {
        DBGln("Card Mount Failed");
        return false;
    }

    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        DBGln("No SD card attached");
        return false;
    }

    DBG("SD Card Type: ");
    if (cardType == CARD_MMC)
    {
        DBGln("MMC");
    }
    else if (cardType == CARD_SD)
    {
        DBGln("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        DBGln("SDHC");
    }
    else
    {
        DBGln("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    return true;
}

// Write the sensor readings on the SD card
void logSDCard()
{
    // dataMessage = String(readingID) + "," + String(dayStamp) + "," + String(timeStamp) + "," +
    //               String(temperature) + "\r\n";
    const char *dataMessage = "Hello World \n";
    DBG("Save data: ");
    DBGln(dataMessage);
    appendFile(SD, "/data1.txt", (const char *)dataMessage);
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        DBGln("Failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        DBGln("File written");
    }
    else
    {
        DBGln("Write failed");
    }
    file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
bool cardRW_appendFile(const char *path, const char *message)
{
    Serial.printf("Appending to file: %s\n", path);

    File file = SD.open(path, FILE_APPEND);
    if (!file)
    {
        DBGln("Failed to open file for appending");
        return false;
    }
    if (file.print(message))
    {
        DBGln("Message appended");
    }
    else
    {
        DBGln("Append failed");
    }
    file.close();
}

void cardRW_listDir(const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\n", dirname);

    File root = SD.open(dirname);
    if (!root)
    {
        DBGln("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        DBGln("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            DBG("  DIR : ");
            DBGln(file.name());
            if (levels)
            {
                cardRW_listDir(file.name(), levels - 1);
            }
        }
        else
        {
            DBG("  FILE: ");
            DBG(file.name());
            DBG("  SIZE: ");
            DBGln(file.size());
        }
        file = root.openNextFile();
    }
}

bool cardRW_createDir(const char *path)
{
    Serial.printf("Creating Dir: %s\n", path);
    if (SD.mkdir(path))
    {
        DBGln("Dir created");
        return true;
    }
    else
    {
        DBGln("mkdir failed");
        return false;
    }
}

bool cardRW_removeDir(const char *path)
{
    Serial.printf("Removing Dir: %s\n", path);
    if (SD.rmdir(path))
    {
        DBGln("Dir removed");
        return true;
    }
    else
    {
        DBGln("rmdir failed");
        return false;
    }
}

bool cardRW_renameFile(const char *path1, const char *path2)
{
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (SD.rename(path1, path2))
    {
        DBGln("File renamed");
        return true;
    }
    else
    {
        DBGln("Rename failed");
        return false;
    }
}

bool cardRW_deleteFile(const char *path)
{
    Serial.printf("Deleting file: %s\n", path);
    if (SD.remove(path))
    {
        DBGln("File deleted");
        return true;
    }
    else
    {
        DBGln("Delete failed");
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
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    }
    else
    {
        DBGln("Failed to open file for reading");
    }

    file = SD.open(path, FILE_WRITE);
    if (!file)
    {
        DBGln("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for (i = 0; i < 2048; i++)
    {
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}
