#pragma once

#ifdef AMIS_READER_DEV

#include <Arduino.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "defines.h"

// length of ip address string
#define IP_LENGTH 64
// host name length
#define HOSTNAME_LENGTH 64
// length of the resource identifier
#define RESOURCE_LENGTH 64
// HTTP rest API request length
#define REQUEST_LENGTH 64
// length of json object key
#define KEY_LENGTH 32
#define TARGET_NAME_LEN 24
#define AMIS_VALUE_COUNT 5
#define REST_TARGET_COUNT 1

typedef struct
{
    // (start) index of source register
    char key[KEY_LENGTH];
    // index of destination field element
    int valueIndex;
} KEY_VALUE_MAP_t;

typedef struct
{
    // rest API target name
    char text[TARGET_NAME_LEN];
    // host name of the rest API target
    char hostname[HOSTNAME_LENGTH];
    // server details
    struct sockaddr_in serverAddr;
    // rest API port
    uint16_t port;
    // rest API resource identifier
    char resource[RESOURCE_LENGTH];
    // complete rest API HTTP request
    char request[REQUEST_LENGTH];
    // file descriptor of connection socket
    int socketFd;
    // number of values to build from json object
    int valueCount;
    // translated values (maybe scaled) array
    long *values;
    // json object key to values mapping array
    KEY_VALUE_MAP_t *mapping;
} HTTP_REST_TARGET_t;

bool amisReader_initRestTargets(Setup &setup);
bool amisReader_readRestTarget(WEBSOCK_DATA &);

#endif