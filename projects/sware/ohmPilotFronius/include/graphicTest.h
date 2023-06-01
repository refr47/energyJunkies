#ifndef __GRAPH_TEST
#define __GRAPH_TEST

// Define meter size as 1 for tft.rotation(0) or 1.3333 for tft.rotation(1)
#define M_SIZE 1.3333

#include <TFT_eSPI.h>
#include <SPI.h>

void meterPlotNeedle(int value, byte ms_delay);
void meterAnalogMeter();
void meterSim();

#endif