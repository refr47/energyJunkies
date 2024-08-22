#pragma once

bool time_init();
unsigned int time_getOffset();
bool time_print();
bool time_storeCurrentTime();
bool getCurrentTime(char *buffer, const unsigned len);
time_t time_getTimeStamp();
char *time_currentTimeStamp();
