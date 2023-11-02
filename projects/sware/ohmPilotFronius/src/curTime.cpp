#include <Arduino.h>
#include <vector>
#include "time.h"
#include "esp_sntp.h"
#include "curTime.h"
#include "debugConsole.h"

#define EUROPE_VIENNA_TZ "CET-1CEST,M3.5.0,M10.5.0/3" // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

using namespace std;

typedef struct
{
    short month;
    short hourFrom;
    short hourTo;
} DATE_TIME;

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

static struct tm currentTime;
static vector<DATE_TIME> table;

void time_init()
{
    // esp_sntp_servermode_dhcp(1); // (optional)
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
    setenv("TZ", EUROPE_VIENNA_TZ, 1);
    tzset();
    DATE_TIME cell;
    cell.month = 1;
    cell.hourFrom = 16;
    cell.hourTo = 8;
    table.push_back(cell);
    cell.month = 2;
    cell.hourFrom = 17;
    cell.hourTo = 8;
    table.push_back(cell);
    cell.month = 3;
    cell.hourFrom = 18;
    cell.hourTo = 8;
    table.push_back(cell);
    cell.month = 4;
    cell.hourFrom = 18;
    cell.hourTo = 7;
    table.push_back(cell);
    cell.month = 5;
    cell.hourFrom = 19;
    cell.hourTo = 7;
    table.push_back(cell);
    cell.month = 6;
    cell.hourFrom = 20;
    cell.hourTo = 7;
    table.push_back(cell);
    cell.month = 7;
    cell.hourFrom = 22;
    cell.hourTo = 5;
    table.push_back(cell);
    cell.month = 8;
    cell.hourFrom = 22;
    cell.hourTo = 5;
    table.push_back(cell);
    cell.month = 9;
    cell.hourFrom = 21;
    cell.hourTo = 6;
    table.push_back(cell);
    cell.month = 10;
    cell.hourFrom = 19;
    cell.hourTo = 6;
    table.push_back(cell);
    cell.month = 11;
    cell.hourFrom = 17;
    cell.hourTo = 8;
    table.push_back(cell);
    cell.month = 12;
    cell.hourFrom = 17;
    cell.hourTo = 8;
    table.push_back(cell);
}

bool time_print()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return false;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    return true;
}

bool time_storeCurrentTime()
{
    if (!getLocalTime(&currentTime))
        return false;
    return true;
}
