#ifndef __TEMP
#define __TEMP

typedef struct
{
    unsigned int numberOfDevices;
    float sensor1;
    float sensor2;
} TEMPERATURE;

void temp_init();
int temp_getNumberOfDevices();
bool temp_getTemperature(TEMPERATURE &container);
#endif
