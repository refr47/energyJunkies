#ifdef AMIS_READER_DEV

#include <ArduinoJson.h>
#include "amisReader.h"
#include "debugConsole.h"

#define RESPONSE_LENGTH 1024

static KEY_VALUE_MAP_t amisKeyValueMap[AMIS_VALUE_COUNT] = {
    {"1.8.0", 0},
    {"2.8.0", 1},
    {"1.7.0", 2},
    {"2.7.0", 3},

};

static double amisValues[AMIS_VALUE_COUNT];

static HTTP_REST_TARGET_t restTarget[REST_TARGET_COUNT] = {
    {"Amis reader", "amisreader", {0}, 80, "/rest", "GET /rest HTTP/1.0\r\n\r\n", -1, AMIS_VALUE_COUNT, amisValues, amisKeyValueMap}};

/* ****************** PROTOTYPES **********************************************/

static bool readJsonResponse(HTTP_REST_TARGET_t *target, WEBSOCK_DATA &webSockData);
static void mapJsonValues(HTTP_REST_TARGET_t *target, char jsonString[], WEBSOCK_DATA &webSockData);

// initialize the rest API targets
bool amisReader_initRestTargets(Setup &setup)
{

    char str[INET_ADDRSTRLEN];
    DBGf("AmisReader - init() ");
    // store this IP address in sa:
    inet_pton(AF_INET, setup.amisReaderHost.c_str(), &(restTarget[0].serverAddr.sin_addr));
    // now get it back and print it
    inet_ntop(AF_INET, &(restTarget[0].serverAddr.sin_addr), str, INET_ADDRSTRLEN);
    DBGf("AmisReader:: IP: %s",str);
    return true;
}

bool amisReader_readRestTarget(WEBSOCK_DATA &webSockData)
{
    restTarget[0].socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (restTarget[0].socketFd >= 0)
    {
        int retVal = connect(restTarget[0].socketFd, (struct sockaddr *)&restTarget[0].serverAddr,
                             sizeof(restTarget[0].serverAddr));
        if (retVal >= 0 || errno == EISCONN)
        {
            if (!readJsonResponse(&restTarget[0], webSockData))
            {
                DBGf("amisReader_readRestTarget:: Cannot read Response of socket communication with amisReader ");
                close(restTarget[0].socketFd);
                return false;
            }
            close(restTarget[0].socketFd);
        }
        else
        {
            DBGf("amisReader_readRestTarget:: Socket error %s ", strerror(errno));
            return false;
        }
    }
    return true;
}

bool readJsonResponse(HTTP_REST_TARGET_t *target, WEBSOCK_DATA &webSockData)
{
    int bytes, sent, received, total;
    char response[RESPONSE_LENGTH];
    int responseLen = sizeof(response);
    char *jsonStart;

    /* send the request */
    total = strlen(target->request);
    sent = 0;
    do
    {
        bytes = write(target->socketFd, target->request + sent, total - sent);
        if (bytes < 0)
            return bytes;
        if (bytes == 0)
            break;
        sent += bytes;
    } while (sent < total);

    /* receive the response */
    memset(response, 0, responseLen);
    total = responseLen - 1;
    received = 0;
    do
    {
        bytes = read(target->socketFd, response + received, total - received);
        if (bytes < 0)
        {
            DBGf("readJsonResponse() - cannot read bytes from socket (bytes <0)");
            return false;
            // return bytes;
        }

        if (bytes == 0)
            break;
        received += bytes;
    } while (received < 1); // total);

    if (received == total)
    {

        DBGf("readJsonResponse() - too much data (receivedd == total)");
        return false;
        return 0;
    }

    jsonStart = strchr(response, '{');
    if (jsonStart != NULL)
    {
        mapJsonValues(target, jsonStart, webSockData);
    }
    return true;
}

void mapJsonValues(HTTP_REST_TARGET_t *target, char jsonString[], WEBSOCK_DATA &webSockData)
{
    StaticJsonDocument<200> jsonBuffer;
    deserializeJson(jsonBuffer, jsonString);
    DBGf("mapJsonValues %s", jsonString);
    for (int i = 0; i < target->valueCount; i++)
    {

        target->values[i] = atof(jsonBuffer[target->mapping[0].key]);
        DBGf("-- formated values %.3f for key: %s", target->values[i], jsonBuffer[target->mapping[0].key]);
    }
    memcpy(&webSockData.amisReader, target->values, sizeof(AMIS_READER));
    DBGf("mapJsonValues: Wirkleistung P+ %.3f", webSockData.amisReader.currentWirkleistungInKwPlus);
    //    printf("\n");
}

#endif