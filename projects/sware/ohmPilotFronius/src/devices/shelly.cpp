#include "shelly.h"
#include "utils.h"

#ifdef SHELLY

/*
http://10.0.0.31/rpc/Switch.Set?id=0&on=false

*/
#define URL_BUFFER_SIZE 60
static char url[URL_BUFFER_SIZE];

void shelly_init()
{
    DBG("Shelly init");
}

bool shelly_switchOn(char *ip)
{
    int httpResponseCode = 0;
    snprintf(url, URL_BUFFER_SIZE, "http://%s/rpc/Switch.Set?id=0&on=true", ip);
    util_GET_Request((const char *)url, &httpResponseCode);
    return httpResponseCode == 200;
}
bool shelly_switchOff(char *ip)
{
    int httpResponseCode = 0;
    snprintf(url, URL_BUFFER_SIZE, "http://%s/rpc/Switch.Set?id=0&on=false", ip);
    util_GET_Request((const char *)url, &httpResponseCode);
    return httpResponseCode == 200;
}

#endif