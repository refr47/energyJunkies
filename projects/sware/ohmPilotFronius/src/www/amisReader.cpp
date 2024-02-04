#include "amisReader.h"

static double amisValues[AMIS_VALUE_COUNT];

HTTP_REST_TARGET_t restTarget[REST_TARGET_COUNT] = {
        {"Amis reader", "amisreader", {0}, 80,
         "/rest", "GET /rest HTTP/1.0\r\n\r\n", -1,
         AMIS_VALUE_COUNT, amisValues, amisKeyValueMap}
};





// initialize the rest API targets
int amisReader_initRestTargets(HTTP_REST_TARGET_t target[], int targetCount) {
    for (int i = 0; i < targetCount; i++) {
        struct hostent *server = gethostbyname(target[i].hostname);
        if (server != NULL) {
            memset(&target[i].serverAddr, 0, sizeof(target[i].serverAddr));
            target[i].serverAddr.sin_family = AF_INET;
            target[i].serverAddr.sin_port = htons(target[i].port);
            memcpy(&target[i].serverAddr.sin_addr, server->h_addr, server->h_length);
        }
    }
}