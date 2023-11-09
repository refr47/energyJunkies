#pragma once

#include "debugConsole.h"

bool cardRW_setup();
bool cardRW_createLoggingFile();
bool cardRW_closeLoggingFile();
bool cardRW_flushLoggingFile();

void cardRW_listDir(const char *dirname, uint8_t levels = 4);
bool cardRW_appendFile(const char *path, const char *message);

bool cardRW_createDir(const char *path);
bool cardRW_removeDir(const char *path);
bool cardRW_renameFile(const char *path1, const char *path2);
bool cardRW_deleteFile(const char *path);
void cardRW_testFileIO(const char *path);
