#pragma once

void time_init();
bool time_print();
bool time_storeCurrentTime();
bool getCurrentTime(char *buffer, const unsigned len);
time_t time_getTimeStamp();
char *time_currentTimeStamp();
