#ifndef __EEPROM_H
#define __EEPROM_H

typedef struct
{
    const char *ssid;
    const char *passwd;
} Network;

void eprom_storeNetwork(Network &network);
void eprom_getNetwork(Network &network);

#endif