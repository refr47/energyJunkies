#ifdef AMIS_READER_DEV

#include <ArduinoJson.h>
#include "amisReader.h"
#include "debugConsole.h"

#define RESPONSE_LENGTH 1024
#define AMIS_VALUE_COUNT 3

static KEY_VALUE_MAP_t amisKeyValueMap[AMIS_VALUE_COUNT] = {
    {"1.8.0", 0},
    {"2.8.0", 1},
    {"saldo", 2}

};

static HTTP_REST_TARGET_t restTarget[REST_TARGET_COUNT] = {
    {"Amis reader", "amisreader", {0}, 80, "/rest", "GET /rest HTTP/1.0\r\n\r\n", -1, AMIS_VALUE_COUNT, amisKeyValueMap}};

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
    restTarget[0].serverAddr.sin_port = htons(restTarget[0].port);
    restTarget[0].serverAddr.sin_family = AF_INET;
    inet_ntop(AF_INET, &(restTarget[0].serverAddr.sin_addr), str, INET_ADDRSTRLEN);

    restTarget[0].socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (restTarget[0].socketFd >= 0)
    {
        int retVal = connect(restTarget[0].socketFd, (struct sockaddr *)&restTarget[0].serverAddr,
                             sizeof(restTarget[0].serverAddr));
        close(restTarget[0].socketFd);
        DBGf("amisReader_initRestTargets - AmisReader:: IP: %s, RetVal open socket %d", str, retVal);
        return true;
    }
    return false;
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
            DBGf("amisReader_readRestTarget:: Socket error %s mit retVal: %d", strerror(errno), retVal);
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
    // DBGf("readJsonResponse() - payload: %s", response);
    if (jsonStart != NULL)
    {
        mapJsonValues(target, jsonStart, webSockData);
    }
    return true;
}

void mapJsonValues(HTTP_REST_TARGET_t *target, char *jsonStart, WEBSOCK_DATA &webSockData)
{
    StaticJsonDocument<512> jsonBuffer;

    // DBGf("mapJsonValues   ENTER  %s", jsonStart);

    deserializeJson(jsonBuffer, jsonStart);
    // DBGf("mapJsonValues %s", jsonStart);

    for (int i = 0; i < target->valueCount; i++)
    {
        /*  DBGf("map, key: %s", target->mapping[i].key);
         DBGf("map, jsonObj: %d", jsonBuffer[target->mapping[i].key]); */
        switch (i)
        {
        case 0:
            webSockData.amisReader.absolutImportInkWh = jsonBuffer[target->mapping[i].key];
            break;
        case 1:
            webSockData.amisReader.absolutExportInkWh = jsonBuffer[target->mapping[i].key];
            break;
        case 2:
            webSockData.amisReader.saldo = jsonBuffer[target->mapping[i].key];
            break;
        default:
            DBGf("amisReader::mapJsonValues() no mapping found for index: %d", i);
        }
    }

    DBGf("mapJsonValues: Wirkleistung P+ %.3f", webSockData.amisReader.absolutImportInkWh);
}

#endif