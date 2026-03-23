#include "system.h"
#include "debugConsole.h"
#include <Arduino.h>
#include "freertos/task.h"

void system_checkStack(const char *msg)
{
    UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
    LOG_INFO("   %s: Free Stack: %d bytes\n", msg, watermark * sizeof(StackType_t));
}
void system_checkHeap(const char *msg)
{
    size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    LOG_INFO("   %s: Free Heap: %d bytes, Largest Block: %d bytes\n", msg, freeHeap, largestBlock);
}