//
// Created by ferdinand on 21.10.23.
//

#ifndef ENERGY_MGR_ENERGY_MGR_H
#define ENERGY_MGR_ENERGY_MGR_H


// modbus unit id of inverter
#define INVERTER 1

// used storage starting register number
#define STRG_ADDR 40345 // enable (dis-)charge limits bitfield
#define CAPACITY (40345 - STRG_ADDR)
//#define MAX_DIS_CHRG_RATE (40347 - STRG_ADDR)
//#define LIMIT_MODE (40348 - STRG_ADDR)
#define RESERVE_PERC ((40350 - STRG_ADDR))
#define SOC_PERC (40351 - STRG_ADDR)
#define DIS_CHRG_PERC (40355 - STRG_ADDR)
//#define CHRG_PERC (40356 - STRG_ADDR)
#define CHARGE_FROM_GRID (40360 - STRG_ADDR)
#define CAPACITY_SF (40361 - STRG_ADDR)
//#define MAX_CHRG_DIS_CHRG_SF (40362 - STRG_ADDR)
#define RESERVE_PERC_SF (40364 - STRG_ADDR)
#define SOC_PERC_SF (40365 - STRG_ADDR)
#define CHRG_DIS_CHRG_SF (40368 - STRG_ADDR)

#define STRG_REGS_COUNT (CHRG_DIS_CHRG_SF + 1) // number of registers

#define MONTH_COUNT 12
#define HOUR_COUNT 24

#define IP_LENGTH 64

#define VERBOSITY_INFO 3
#define VERBOSITY_WARN 2
#define VERBOSITY_ERROR 1


typedef struct {
    // state of charge
    float socPerc;
    // SoC scale factor
    float socPercSf;
    // available energy in kWh
    float kWhEnergy;
    // discharge percent limit
    float dischrgPerc;
    // charge percent scale factor
    float chrgDisChrgSf;
    // charge from grid?
    int chargeFromGrid;
} values_t;

typedef struct {
    int begin; // dusk in minutes
    int end; // dawn in minutes
} daylight_t;

typedef struct {
    unsigned int index; // hour + 24 (for first hours of the day)
    unsigned int hour;  // real hour (18, 19, ...23, 0, 1, ...)
    float consTariff;
    float energyTariff;
} consTariff_t;

typedef struct {
    // ip address of the inverter
    char ip[IP_LENGTH];
    // modbus/tcp port
    uint16_t port;
    // config file name
    char configFile[FILENAME_MAX];
    // consumption tariff file name
    char tariffFile[FILENAME_MAX];

    // error recovery mode
    modbus_error_recovery_mode errorRecoveryMode;
    // last hour to set charge/discharge rate
    int lastHour;
    // time offset of server (60 minutes on unifi-pi)
    long timeOffset;
// estimated PV production times (minute from until minute to, per month)
    daylight_t pvMinutes[MONTH_COUNT];
// average recent power consumption, per month calculated, value for each hour extra
    float consumption[MONTH_COUNT][HOUR_COUNT];
// consumption tariff, fetched from aWATTar,grid tariff and VAT added
    consTariff_t consTariff[HOUR_COUNT];
// scheduled discharge rate (discharge percent or 0
    int dischargeRate[HOUR_COUNT];
// scheduled discharge rate counts regarding SoC
    bool realDischarge[HOUR_COUNT];
// enable charging from network
    int chargeFromGrid[HOUR_COUNT];

// grid tariff, manually calculated from invoice, extracted from config file
    float gridTariff;/* = 6.897f;*/
// feed tariff, manually entered into config file and extracted from there
    float feedTariff;/*[] = {51.450f, 26.863f};*/
// feed tariff extra surcharge to influence rentability check/calculation
    float feedTariffDischargePlus;/*[] = {51.450f, 26.863f};*/
// feed tariff extra surcharge to influence rentability check/calculation
    float feedTariffChargeMinus;/*[] = {51.450f, 26.863f};*/
// net consumtion tariff above which the battery should be discharged
    float dischargeThresholdNet;
// net consumtion tariff below which the battery should be charged
    float chargeThresholdNet;
// energy efficiency of the battery charge efficiency * discharge efficiency
    float batteryEfficiency;/* = 20;*/
// VAT percentage, extracted from config file
    int vatPercent;/* = 20;*/
// discharge percent intended to use in high tariff hours
    int dischargePercent;/*=50;*/
// verbosity level
    int verbosityLevel;
} config_t;

// read inverter multiple times, if previous calls failed
int readInverterRetrying(values_t *inverterValues, int retryDelayMinutes, char timeNowText[]);

// wait for a number of minutes, returns immediately if < 0
void waitForMinutes(int minutes);

// refresh current time and modify it by timeOffset (unifi pi: runs in GMT, so add 1 hour)
// returns time of day in minutes
int refreshTimeNow(struct tm *tm, long timeOffset, char timeNowText[]);

// fill time stamp string
void fillTimeNowText(char timeStamp[], struct tm tm);

// calc minute of the day
int calcMinuteNow(struct tm tm);

// the signal handler for CTRL+C
void catchIntSignal(int sig_num);

// cli argument evaluation
int evalArgs(int argc, char **argv, config_t *config);

// read invertr values
int readInverter(modbus_t *invertr, values_t *values, char timeStamp[]);

// write register into inverter
int writeInverter(modbus_t *invertr, int registerNumber, int value, char timeNowText[], int timeOffset);

// reconnect to server
int reconnect(modbus_t *invertr, char timeStamp[]);

void showDischargeHours(config_t *cfg, int month, int hourNow, int begin, int end, float kWhAvail);

// show discharge hours cronological
void showDischargeHoursCron(config_t *cfg, int month, int hourNow, float kWhAvail);

int readConfigFile(config_t *cfg);

int readConsumTariff(config_t *cfg, float gridTarff, int vatPerc, int hourFrom);

char *readLine(FILE *file, char txt[], int length);

int extractVatPercent(FILE *file, char txt[], int length, int *vatPerc);

int extractDisChargePercent(FILE *file, char txt[], int length, int *dischargePerc);

int extractDayLight(FILE *file, char txt[], int length, daylight_t pvProd[], int count);//, int timeOffset);

int extractConsumption(FILE *file, char txt[], int length, float consumptn[][HOUR_COUNT], int count);

int
extractConsTariff(FILE *file, char txt[], int length, consTariff_t consTarff[HOUR_COUNT], int count, float gridTarff,
                  int vatPerc, int hourFrom);

int extractGridTariff(FILE *file, char txt[], int length, float *gridTarff);

int extractFeedTariff(FILE *file, char txt[], int length, float *feedTarff);

int calcDischargeHours(config_t *cfg, int count, int month, int hourNow, int begin, int end, float kWh);

#endif //ENERGY_MGR_ENERGY_MGR_H
