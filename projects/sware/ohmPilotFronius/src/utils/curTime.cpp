#include <Arduino.h>
#include <vector>

#include <WiFiUdp.h>

#include "time.h"
#include "esp_sntp.h"
#include "curTime.h"
// #include "debugConsole.h"
#include "defines.h"

using namespace std;

typedef struct
{
    short month;
    short hourFrom;
    short hourTo;
} DATE_TIME;

const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
static unsigned int currentMonth = 0;
// Define NTP Client to get time

static struct tm currentTime;
static long currentSecs = 0;
static vector<DATE_TIME> table;

bool time_init()
{
    // esp_sntp_servermode_dhcp(1); // (optional)
    LOG_INFO("time_init()");
    configTime(gmtOffset_sec, daylightOffset_sec, NtpServer1, NtpServer2);
    setenv("TZ", EUROPE_VIENNA_TZ, 1);
    tzset();

    /* DATE_TIME cell;
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
    table.push_back(cell); */
    LOG_DEBUG("time_init()");

    delay(2000);
    struct tm timeinfo;

    // Warten, bis die Zeit erfolgreich abgerufen wurde
    if (!getLocalTime(&timeinfo))
    {
        LOG_DEBUG("curTime::time_init() - Zeit konnte nicht abgerufen werden");
        return false;
    }
    currentMonth = timeinfo.tm_mon + 1;
    currentSecs = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;

    LOG_INFO("curTime::time_init() current month: %d, currSecs: %d", currentMonth, currentSecs);
    time_print();
    return true;
}

unsigned int time_getOffset()
{
    return currentSecs;
}

int time_currentHourLocal()
{
    time_t now;
    time(&now);
    struct tm tinfo;
    localtime_r(&now, &tinfo);
    return tinfo.tm_hour;
}

// Local minute
int time_currentMinuteLocal()
{
    time_t now;
    time(&now);
    struct tm tinfo;
    localtime_r(&now, &tinfo);
    return tinfo.tm_min;
}

bool time_print()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return false;
    }
    // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    time_t timestamp = time(NULL);
    // DBGf("TimeStamp: %d", timestamp);

    return true;
}

time_t time_getTimeStamp()
{
    return time(NULL);
}

bool getCurrentTime(char *buffer, const unsigned len)
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {

        return false;
    }
    strftime(buffer, len, "%F  %T", &timeinfo);
    return true;
}
bool time_storeCurrentTime()
{
    if (!getLocalTime(&currentTime))
        return false;
    return true;
}

static char buf[20];
char *time_currentTimeStamp()
{
    time_t t = time(NULL);
    struct tm *tmp = gmtime(&t);

    /*     int h = (t / 3600) % 24;
        int m = (t / 60) % 60;
        int s = t % 60; */

    sprintf(buf, "%02d:%02d:%02d\n", (t / 3600) % 24, (t / 60) % 60, t % 60);

    // Serial.println(buf);
    return buf;
}

int time_GetDayOfYear()
{
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_yday + 1;
}