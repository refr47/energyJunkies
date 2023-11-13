#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <modbus/modbus.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "energy_mgr.h"

// storage manager's configuration file
#define STR_MNGR_CONF_FILE_NAME "strg-mngr.conf"
// consumption tariffs fetched from aWATTar
#define AWATTAR_TARIFF_FILE_NAME "current.yaml"

// max. length of input file lines
#define TEXT_LEN 256

// buffer to hold config/tariff file line read
char text[TEXT_LEN];

// modbus object, global for use in CTRL+C signal handler
modbus_t *inverter = NULL;


int main(int argc, char *argv[]) {
    // ip address of inverters: "10.0.0.13" GEN24 @ home, "172.17.68.202" Symo @ HTL
    // modbus unit id of smart meter: 200 GEN24 @ home, 240 Symo @ HTL
    config_t config = {
            "10.0.0.13", 502, STR_MNGR_CONF_FILE_NAME, AWATTAR_TARIFF_FILE_NAME, MODBUS_ERROR_RECOVERY_NONE, 13};

    // inverterValues read from inverter
    values_t inverterValues = {0};

    // return code, number of retryCount, retries limit, wait time, ...
    int rc;
    int retryCount;
    int maxRetries = 5;

    // current timestamp
    struct tm timeNow;
    // time stamp as tetxt
    char timeNowText[24];
    // minute of the day
    int minuteNow;
    // hour of most recent evaluation/change
    int prevHour;
    // most recent discharge rate
    int prevDischrgRate;
    // most recent discharge rate
    int prevChargeFromGrid;
    // length of one cycle in minutes
    int minutesPerCycle = 1;
    // delay between retries in minutes
    int retryDelayMinutes = 5;
    // minutes to wait
    int minutesToWait;


    // set the INT (Ctrl-c) signal handler to 'catchIntSignal'
    signal(SIGINT, catchIntSignal);

    // evaluate command line arguments
    evalArgs(argc, argv, &config);

    // get current time
    refreshTimeNow(&timeNow, config.timeOffset, timeNowText);

    // create modbus object for inverter
    inverter = modbus_new_tcp(config.ip, config.port);
    if (inverter == NULL) {
        if (config.verbosityLevel >= VERBOSITY_ERROR) {
            fprintf(stderr, "%s: Unable to initialize Modbus/TCP slave %s at port %d\n", timeNowText, config.ip,
                    config.port);
        }
        return -1;
    }

    // set response timeout to 5.2 seconds
    modbus_set_response_timeout(inverter, 5, 200000);
    // set reconnect on failure bits, if set in command line arguments
    modbus_set_error_recovery(inverter, config.errorRecoveryMode);
    // connect to inverter
    rc = modbus_connect(inverter);

    if (rc == -1) {
        if (config.verbosityLevel >= VERBOSITY_ERROR) {
            fprintf(stderr, "%s: Connect failed: %s\n", timeNowText, modbus_strerror(errno));
        }
        modbus_close(inverter); // close failed connection??
        // continue and retryCount all the time
        //modbus_free(inverter); // stay here, don't leave
        //return -2;
    }

    // import configuration from config file
    readConfigFile(&config);

    // get current time and set minute of the day
    /*minuteNow = */refreshTimeNow(&timeNow, config.timeOffset, timeNowText);

    // wait until begin of dawn (beginning of low PV production)
    minuteNow = timeNow.tm_hour * 60 +
                timeNow.tm_min; //calcMinute(timeNow); //config.pvMinutes[timeNow.tm_mon].end + 1; //calcMinuteNow(timeNow);
    minutesToWait = config.pvMinutes[timeNow.tm_mon].end - minuteNow;
    if (config.verbosityLevel >= VERBOSITY_INFO) {
        fprintf(stderr, "%s: sleeping for %d minutes from now.\n", timeNowText, minutesToWait);
    }
    waitForMinutes(minutesToWait);

    // read cunsumption tariffs from aWATTar tariff file (usually named current.yaml)
    readConsumTariff(&config, config.gridTariff, config.vatPercent, timeNow.tm_hour);

    // calculate charge and discharge net consumtion tariff threshold (reduced by grid tariff)
    config.chargeThresholdNet = config.dischargeThresholdNet =
            (config.feedTariff / (1.0f + (float) config.vatPercent / 100.0f)) - config.gridTariff;
    config.dischargeThresholdNet += config.feedTariffDischargePlus;
    config.chargeThresholdNet -= config.feedTariffChargeMinus;
    config.chargeThresholdNet *= config.batteryEfficiency;


    refreshTimeNow(&timeNow, config.timeOffset, timeNowText);
    minuteNow = timeNow.tm_hour * 60 + timeNow.tm_min;

    rc = readInverterRetrying(&inverterValues, retryDelayMinutes, timeNowText);

//    // for testing: feedTariff = 25.528f;
//    calcDischargeHours(&config, HOUR_COUNT, timeNow.tm_mon, config.pvMinutes[timeNow.tm_mon].begin,
//                       config.pvMinutes[timeNow.tm_mon].end, inverterValues.kWhEnergy);
//    showDischargeHours(&config, timeNow.tm_mon, inverterValues.kWhEnergy);
//    showDischargeHoursCron(&config, timeNow.tm_mon, inverterValues.kWhEnergy);

    // refresh time stamp to now
    /*minuteNow = */refreshTimeNow(&timeNow, config.timeOffset, timeNowText);
    minuteNow = timeNow.tm_hour * 60 + timeNow.tm_min;

    // force first inverter configuration change
    prevHour = timeNow.tm_hour - 1;
    prevDischrgRate = (int) inverterValues.dischrgPerc;
    prevChargeFromGrid = inverterValues.chargeFromGrid;

//    fprintf(stderr, "%s: in or outside pv production window (%d-%d)?\n", timeNowText,
//            config.pvMinutes[timeNow.tm_mon].begin,
//            config.pvMinutes[timeNow.tm_mon].end);

    // repeat inverter discharge configuration refresh until begin of PV production
//    while (minuteNow >= config.pvMinutes[timeNow.tm_mon].end || minuteNow <= config.pvMinutes[timeNow.tm_mon].begin) {
    do {
        // refresh inverter discharge configuration every hour
        if (timeNow.tm_hour != prevHour) {
            rc = readInverterRetrying(&inverterValues, retryDelayMinutes, timeNowText);

            // for testing: feedTariff = 25.528f;
            calcDischargeHours(&config, HOUR_COUNT, timeNow.tm_mon,
                               timeNow.tm_hour, config.pvMinutes[timeNow.tm_mon].begin,
                               config.pvMinutes[timeNow.tm_mon].end, inverterValues.kWhEnergy);
            if (config.verbosityLevel >= VERBOSITY_INFO) {
                printf("\n%s: Calculation results:", timeNowText);
                showDischargeHours(&config, timeNow.tm_mon, timeNow.tm_hour, config.pvMinutes[timeNow.tm_mon].begin,
                                   config.pvMinutes[timeNow.tm_mon].end, inverterValues.kWhEnergy);
                showDischargeHoursCron(&config, timeNow.tm_mon, timeNow.tm_hour, inverterValues.kWhEnergy);

                fprintf(stderr, "%s: discharge (%d%% -> %d%%), charge from grid (%c -> %c)\n",
                        timeNowText,
                        prevDischrgRate, config.dischargeRate[timeNow.tm_hour],
                        prevChargeFromGrid ? 'y' : 'n', config.chargeFromGrid[timeNow.tm_hour] ? 'y' : 'n');
            }
            // mark current hour as prcessed
            prevHour = timeNow.tm_hour;
            // only if discharge rate should change compared to just before
            if (config.dischargeRate[timeNow.tm_hour] != prevDischrgRate) {
                prevDischrgRate = config.dischargeRate[timeNow.tm_hour];
                // scale charge percent value
                config.dischargeRate[timeNow.tm_hour] = (int) ((float) config.dischargeRate[timeNow.tm_hour] /
                                                               inverterValues.chrgDisChrgSf);
                // write new storage discharge rate in % into inverter discharge control register
                writeInverter(inverter, STRG_ADDR + DIS_CHRG_PERC, config.dischargeRate[timeNow.tm_hour], timeNowText,
                              config.timeOffset);
            } // different discharge rate from just before?
            // only if charge from grid setting should change compared to just before
            if (config.chargeFromGrid[timeNow.tm_hour] != prevChargeFromGrid) {
                prevChargeFromGrid = config.chargeFromGrid[timeNow.tm_hour];
                // write new setting regarding charge from grid into inverter charge from grid control register
                //TODO: activate and test the functionality to charge from grid
                writeInverter(inverter, STRG_ADDR + CHARGE_FROM_GRID, config.chargeFromGrid[timeNow.tm_hour],
                              timeNowText, config.timeOffset);
            } // different discharge rate from just before?
        } // hour != previous hour?
        waitForMinutes(minutesPerCycle);
        // get current time and calculate the minute of the day
        /*minuteNow = */refreshTimeNow(&timeNow, config.timeOffset, timeNowText);
        minuteNow = timeNow.tm_hour * 60 + timeNow.tm_min;
//    } // while outside of PV production period
    } while (inverterValues.kWhEnergy > 0 && timeNow.tm_hour !=
                                             config.lastHour); // while minuteNow ..., from end of PV production period to begin on next day

    // write default discharge rate into inverter at beginning of PV production
    writeInverter(inverter, STRG_ADDR + DIS_CHRG_PERC,
                  (int) ((float) config.dischargePercent / inverterValues.chrgDisChrgSf), timeNowText,
                  config.timeOffset);
// shut down modbus connection
    modbus_close(inverter);
    modbus_free(inverter);

    fprintf(stderr, "%s: Accu empty %.3f or termination Time %d reached. Good Bye.\n",
            timeNowText, inverterValues.kWhEnergy, config.lastHour);
    return 0;
} // main


int readInverterRetrying(values_t *inverterValues, int retryDelayMinutes, char timeNowText[]) {
    // return code, number of retryCount, retries limit, wait time, ...
    int rc;
    int retryCount;
    int maxRetries = 5;

    // try to read inverterValues from inverter at end of daily PV production period
    rc = readInverter(inverter, inverterValues, timeNowText);
    retryCount = 0;
    while (rc != 0 && retryCount < maxRetries) {
        retryCount++;
        waitForMinutes(retryDelayMinutes); // try again in 5 minutes
        rc = reconnect(inverter, timeNowText); // read(s) failed, thus reconnect
        if (rc == 0) {
            rc = readInverter(inverter, inverterValues, timeNowText);
        }
    }
    return rc;
}

void waitForMinutes(int minutes) {
    if (minutes > 0) {
        unsigned int seconds = minutes * 60;
        sleep(seconds);
    }
}


// refresh current time and add time offset
int refreshTimeNow(struct tm *tm, long timeOffset, char timeNowText[]) {
    time_t T;
    time(&T);
    T += timeOffset; // add time offset given as command line argument (unifi pi: 1 hour too late, GMT!)
    *tm = *localtime(&T);
    // build text from time stamp inverterValues
    fillTimeNowText(timeNowText, *tm);
    return calcMinuteNow(*tm);
}


// fill timestamp string
void fillTimeNowText(char timeStamp[], struct tm tm) {
    sprintf(timeStamp,
            "%04d-%02d-%02d %02d:%02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

int calcMinuteNow(struct tm tm) {
    return (tm.tm_hour * 60 + tm.tm_min) % (24 * 60);
}

// the signal handler for CTRL+C
void catchIntSignal(int sig_num) {
// reinstall the signal handler for CTRL+C
    signal(SIGINT, catchIntSignal);
    if (inverter != NULL) {
        modbus_close(inverter);
        modbus_free(inverter);
    }
    fputs("Terminated by CTRL+c\n", stderr);
}

// cli argument evaluation
int evalArgs(int argc, char **argv, config_t *config) {
// command line options handling
    int cliArg;
    char *ptr;

    opterr = 0;
    while ((cliArg = getopt(argc, argv, "i:p:c:t:o:x:d:f:s:v:lrh")) != -1) {
        switch (cliArg) {
            case 'i': // IP address
                strncpy(config->ip, optarg, IP_LENGTH);
                break;
            case 'c': // config file name
                strncpy(config->configFile, optarg, IP_LENGTH);
                break;
            case 't': // tariff file name
                strncpy(config->tariffFile, optarg, IP_LENGTH);
                break;
            case 'p': // port
                config->port = strtol(optarg, &ptr, 10);
                break;
            case 'o': // server time offset
                config->timeOffset = strtol(optarg, &ptr, 10);
                config->timeOffset *= 60; // Zeitverschiebung (unifi pi: -1h) in Sekunden
                break;
            case 'x': // feed tariff surcharge (added to feed tariff to influence rentability check
                config->feedTariffDischargePlus = strtof(optarg, &ptr);
                break;
            case 'd': // feed tariff reduction (subtracted from feed tariff to influence rentability check
                config->feedTariffChargeMinus = strtof(optarg, &ptr);
                break;
            case 'f': // battery efficiency factor, used to reduced charge threshold
                config->batteryEfficiency = strtof(optarg, &ptr);
                break;
            case 's': // hour to process as last, then terminate
                config->lastHour = strtol(optarg, &ptr, 10);
                break;
            case 'v': // verbosity level: 3 info, 2 warning, 1 error
                config->verbosityLevel = (int) strtol(optarg, &ptr, 10);
                break;
            case 'l':
                config->errorRecoveryMode |= MODBUS_ERROR_RECOVERY_LINK;
                break;
            case 'r':
                config->errorRecoveryMode |= MODBUS_ERROR_RECOVERY_PROTOCOL;
                break;
            case 'h':
                fprintf(stderr,
                        "usage: %s -i <IP> -p <PORT> -c <config file> -t <aWATTar current.yaml> -x <feed tariff discharge extra ct/kWh> -d <feed tariff charge extra ct/kWh> -f <battery efficiency> -o <minutes offset> -s <last hour to control> -l -r -h\n",
                        argv[0]);
                fprintf(stderr,
                        "\t-l ... recover from link error, -r ... recover from protocol error -h usage information\n");
                fprintf(stderr,
                        "usage: %s -i 10.0.0.13 -p 502 -c strg-mngr.conf -t current.yaml -x 5.50 -d 8.00 -f 0.75 -o 60 -s 13 -l -r\n",
                        argv[0]);
                fprintf(stderr,
                        "connects to 10.0.0.13 at port 502 using strg-mngr.conf and current.yaml, adding 5.5 ct/kWh to feed tariff, subtracting 8.0 ct/kWh from feed tariff, multiplying charge threshold by 0.75 and 60 minutes to local time, terminate at 13:00\n");
                return 0;
            case '?':
                if (optopt == 'i' || optopt == 'p' || optopt == 'c' || optopt == 't' || optopt == 'o' ||
                    optopt == 'x' || optopt == 'd' || optopt == 'f' || optopt == 's' || optopt == 'v')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                return -9;
            default:
                abort();
        }
    }
    return 0;
}

int writeInverter(modbus_t *invertr, int registerNumber, int value, char timeNowText[], int timeOffset) {
    int rc;
    struct tm tm;
    int retry;
    int maxRetries = 5;
    rc = modbus_write_register(invertr, registerNumber, (uint16_t) value);
    fprintf(stderr, "%s: modbus_write_register(%d, %d) ", timeNowText,
            registerNumber, (uint16_t) value);
    if (rc == -1) {
        fprintf(stderr, "%s (%s)\n", "failed", modbus_strerror(errno));
    } else {
        fprintf(stderr, "%s\n", "succeeded");
    }
    retry = 0;
    while (rc == -1 && retry < maxRetries) {
        retry++;
        waitForMinutes(5); // try again in 5 minutes
        refreshTimeNow(&tm, timeOffset, timeNowText);
        /*rc = */reconnect(invertr, timeNowText); // write(s) failed, thus reconnect
        rc = modbus_write_register(invertr, registerNumber, (uint16_t) value);
    } // retry loop
    return rc;
}


int readInverter(modbus_t *invertr, values_t *values, char timeStamp[]) {
    int rc;
// invertr successfully read?
    bool readSuccess = true;

// register values of storage
    int16_t strgRegs[STRG_REGS_COUNT];

// use modbus unit INVERTER
    rc = modbus_set_slave(invertr, INVERTER);
    if (rc == -1) {
        fprintf(stderr, "%s: Inverter select failed: %s\n", timeStamp,
                modbus_strerror(errno));
    } else {
// read storage values (SoC)
        rc = modbus_read_registers(invertr, STRG_ADDR, STRG_REGS_COUNT, (uint16_t *) strgRegs);
        if (rc == -1) {
            fprintf(stderr, "%s: Inverter read failed: %s\n", timeStamp,
                    modbus_strerror(errno));
            readSuccess = false;
        }

// in case of successful read operations
        if (readSuccess) {
            rc = 0;
// calculate scale factors
            values->chrgDisChrgSf = (float) pow(10, strgRegs[CHRG_DIS_CHRG_SF]);
            values->socPercSf = (float) pow(10, strgRegs[SOC_PERC_SF]);
            float capacitySf = (float) pow(10, strgRegs[CAPACITY_SF]);
            float reservePercSf = (float) pow(10, strgRegs[RESERVE_PERC_SF]);
// scale invertr values read
            values->socPerc = (float) strgRegs[SOC_PERC] * values->socPercSf;
            values->dischrgPerc = (float) strgRegs[DIS_CHRG_PERC] * values->chrgDisChrgSf;
            float whCapacity = (float) strgRegs[CAPACITY] * capacitySf;
            float reservePerc = (float) strgRegs[RESERVE_PERC] * reservePercSf;
            values->kWhEnergy = (whCapacity / 1000.0f) * ((values->socPerc - reservePerc) / 100.0f);
        } else {
            rc = -2; // read failure
        }
    }
    return rc; // 0 ok, -1 device select error, -2 read error
}

// reconnect to server
int reconnect(modbus_t *invertr, char timeStamp[]) {
// connect to invertr
    int rc = modbus_connect(invertr);
    if (rc == -1) {
        fprintf(stderr, "%s: Connect failed: %s\n", timeStamp, modbus_strerror(errno));
        modbus_close(invertr); // close failed connection??
    }
    return rc;
}

void showDischargeHours(config_t *cfg, int month, int hourNow, int begin, int end, float kWhAvail) {
    printf("\nVAT%%\tDiChg%%\tGrid ct\tFeed ct\tAdd ct\tRed ct\tDiCh ct\tChrg ct\tBattEff\tAcc kWh\n%d\t%d\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\n",
           cfg->vatPercent, cfg->dischargePercent, cfg->gridTariff, cfg->feedTariff, cfg->feedTariffDischargePlus,
           cfg->feedTariffChargeMinus, cfg->dischargeThresholdNet, cfg->chargeThresholdNet,
           cfg->batteryEfficiency, kWhAvail);
    printf("Hour\tNrg ct\tCons ct\tDiCh%%\tChgGri\tCon kWh\tRem kWh\n");
    for (int i = 0; i < HOUR_COUNT && kWhAvail >= 0 /*&& dischargeRate[consTariff[i].hour] > 0*/; i++) {
        if (cfg->dischargeRate[cfg->consTariff[i].hour] > 0 && cfg->realDischarge[cfg->consTariff[i].hour]) {
            kWhAvail -= cfg->consumption[month][cfg->consTariff[i].hour];
            printf("%3d\t%7.3f\t%7.3f\t%2d\t%d\t%7.3f\t%7.3f\n",
                   cfg->consTariff[i].hour, cfg->consTariff[i].energyTariff, cfg->consTariff[i].consTariff,
                   cfg->dischargeRate[cfg->consTariff[i].hour],
                   cfg->chargeFromGrid[cfg->consTariff[i].hour] ? 1 : 0,
                   cfg->consumption[month][cfg->consTariff[i].hour],
                   kWhAvail);
        }
    }
    fflush(stdout);
}

int findOrigHour(config_t *cfg, int hour) {
    for (int i = 0; i < HOUR_COUNT; i++) {
        if (cfg->consTariff[i].hour == hour) {
            return i;
        }
    }
    return -1;
}

void showDischargeHoursCron(config_t *cfg, int month, int hourNow, float kWhAvail) {
    printf("\nVAT%%\tDiChg%%\tGrid ct\tFeed ct\tAdd ct\tRed ct\tDiCh ct\tChrg ct\tBattEff\tAcc kWh\n%d\t%d\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\n",
           cfg->vatPercent, cfg->dischargePercent, cfg->gridTariff, cfg->feedTariff, cfg->feedTariffDischargePlus,
           cfg->feedTariffChargeMinus, cfg->dischargeThresholdNet, cfg->chargeThresholdNet,
           cfg->batteryEfficiency, kWhAvail);
    printf("Hour\tPos\tNrg ct\tCons ct\tDiChg%%\tChgGri\tCon kWh\tRem kWh\n");
    for (int j = 0; j < HOUR_COUNT; j++) {
        int i = (j + hourNow) % HOUR_COUNT; // hour absolute (0..23)
        int k = findOrigHour(cfg, i);   // index of entry for real hour
        if (k >= 0) {
            if (cfg->dischargeRate[i] > 0 && cfg->realDischarge[i]) {
                kWhAvail -= cfg->consumption[month][i];
            }
            if (cfg->realDischarge[i]) {
                printf("%3d\t%3d\t%7.3f\t%7.3f\t%2d\t%d\t%7.3f\t%7.3f\n",
                       i, k, cfg->consTariff[k].energyTariff, cfg->consTariff[k].consTariff,
                       cfg->dischargeRate[cfg->consTariff[k].hour],
                       cfg->chargeFromGrid[cfg->consTariff[k].hour] ? 1 : 0,
                       cfg->consumption[month][i],
                       kWhAvail);
            }
        }
    }
    fflush(stdout);
}

int consTariffCmp(const void *a, const void *b) {
    consTariff_t *aa = (consTariff_t *) a;
    consTariff_t *bb = (consTariff_t *) b;
    if (aa->consTariff > bb->consTariff) {
        return -1;
    } else if (aa->consTariff < bb->consTariff) {
        return 1;
    } else {
        return 0;
    }
}

int calcDischargeHours(config_t *cfg, int count, int month, int hourNow, int begin, int end,
                       float kWh) {
    int i = 0;
    int minuteNow = hourNow * 60;   // current hour in minutes as base time for calculation

    if (hourNow < end / 60) {   // are we already in the next day before end of PV production?
        minuteNow += 24 * 60;   // add one full day to avoid wrap around
    }

    begin += 24 * 60;   // make begin minute one day ahead ( 9 o'Clock becomes '33' o'Clock)
//    printf("Index\tHour\tNrg ct\tCons ct\tDiCh%%\tChgGri\tCon kWh\tRem kWh\n");
    memset(cfg->dischargeRate, 0, sizeof(cfg->dischargeRate[0]) * HOUR_COUNT);
    memset(cfg->realDischarge, 0, sizeof(cfg->realDischarge[0]) * HOUR_COUNT);
    while (i < count && cfg->consTariff[i].energyTariff > cfg->dischargeThresholdNet && kWh > 0) {
        cfg->dischargeRate[cfg->consTariff[i].hour] = cfg->dischargePercent;
        int indexMinute = cfg->consTariff[i].index * 60;
        if (indexMinute >= end && // after end of PV production
            indexMinute < begin &&    // before begin of PV production
            indexMinute >= minuteNow) {   // in the future
            kWh -= cfg->consumption[month][cfg->consTariff[i].hour];
            cfg->realDischarge[cfg->consTariff[i].hour] = true;
        }
//        printf("%3d\t%3d\t%7.3f\t%7.3f\t%2d\t%d\t%7.3f\t%7.3f\n",
//               cfg->consTariff[i].index, cfg->consTariff[i].hour, cfg->consTariff[i].energyTariff,
//               cfg->consTariff[i].consTariff,
//               cfg->dischargeRate[cfg->consTariff[i].hour],
//               cfg->chargeFromGrid[cfg->consTariff[i].hour] ? 1 : 0,
//               cfg->consumption[month][cfg->consTariff[i].hour],
//               kWh);
        i++;
    }
    // find lowest consumption tariff and enable charging from grid, if it is low enough
//    if (cfg->consTariff[count - 1].energyTariff < cfg->chargeThresholdNet) {
//        cfg->chargeFromGrid[cfg->consTariff[count - 1].hour] = 1;
//    }
    return 0;
}

int readConsumTariff(config_t *cfg, float gridTarff, int vatPerc, int hourFrom) {
    char pricePattern[32];
    FILE *file = fopen(cfg->tariffFile, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening '%s'. Error: (%s)\n", cfg->tariffFile, strerror(errno));
        return 1;
    }
    sprintf(pricePattern, "data_price_hour_abs_%02d_amount", hourFrom);
    while (readLine(file, text, TEXT_LEN) != NULL) {
        //fputs(text, stdout);
        if (strncmp(pricePattern, text, 29) == 0) { // data_price_hour_abs_<hourFrom>_amount
            extractConsTariff(file, text, TEXT_LEN, cfg->consTariff, HOUR_COUNT, gridTarff, vatPerc, hourFrom);
            qsort(cfg->consTariff, HOUR_COUNT, sizeof(cfg->consTariff[0]), consTariffCmp);
            break;
        }
    }
    fclose(file);
    return 0;
}


int readConfigFile(config_t *cfg) {
    FILE *file = fopen(cfg->configFile, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening '%s'. Error: (%s)\n", cfg->configFile, strerror(errno));
        return 1;
    }
    while (readLine(file, text, TEXT_LEN) != NULL) {
        //fputs(text, stdout);
        if (strncmp("[vatpercent]", text, 12) == 0) {
            extractVatPercent(file, text, TEXT_LEN, &cfg->vatPercent);
        } else if (strncmp("[daylight]", text, 10) == 0) {
            extractDayLight(file, text, TEXT_LEN, cfg->pvMinutes, MONTH_COUNT);//, cfg->timeOffset);
        } else if (strncmp("[consumption]", text, 13) == 0) {
            extractConsumption(file, text, TEXT_LEN, cfg->consumption, MONTH_COUNT);
        } else if (strncmp("[gridtariff]", text, 12) == 0) {
            extractGridTariff(file, text, TEXT_LEN, &cfg->gridTariff);
        } else if (strncmp("[feedtariff]", text, 12) == 0) {
            extractFeedTariff(file, text, TEXT_LEN, &cfg->feedTariff);
        } else if (strncmp("[dischargepercent]", text, 18) == 0) {
            extractDisChargePercent(file, text, TEXT_LEN, &cfg->dischargePercent);
        }
    }
    fclose(file);
    return 0;
}

int extractVatPercent(FILE *file, char txt[], int length, int *vatPerc) {
    char *ptr;
    if (readLine(file, txt, length) == txt) {
        *vatPerc = (int) strtol(txt, &ptr, 10);
        return 0;
    } else {
        return -1;
    }
}

int extractDisChargePercent(FILE *file, char txt[], int length, int *dischargePerc) {
    char *ptr;
    if (readLine(file, txt, length) == txt) {
        *dischargePerc = (int) strtol(txt, &ptr, 10);
        return 0;
    } else {
        return -1;
    }
}

int extractDayLight(FILE *file, char txt[], int length, daylight_t pvProd[], int count)/*, int timeOffset)*/ {
    int i = 0;
    char *ptr;
    char *ptr2;
    while (readLine(file, txt, length) == txt && i < count) {
        ptr = txt;
        int month = (int) strtol(ptr, &ptr2, 10);
        ptr = ptr2 + 1; // next number
        pvProd[month].begin = (int) strtol(ptr, &ptr2, 10);// + timeOffset;
        ptr = ptr2 + 1; // next number
        pvProd[month].end = (int) strtol(ptr, &ptr2, 10);// + timeOffset;
        i++;
    }
    if (i == count) {
        return 0;
    } else {
        return -1;
    }
}

int extractConsumption(FILE *file, char txt[], int length, float consumptn[][HOUR_COUNT], int count) {
    int i = 0;
    char *ptr;
    char *ptr2;
    while (readLine(file, txt, length) == txt && i < count) {
        ptr = txt;
        int month = (int) strtol(ptr, &ptr2, 10);
        for (int j = 0; j < HOUR_COUNT; j++) {
            ptr = ptr2 + 1; // next number
            consumptn[month][j] = strtof(ptr, &ptr2);
        }
        i++;
    }
    if (i == count) {
        return 0;
    } else {
        return -1;
    }
}

int extractConsTariff(FILE *file, char txt[], int length, consTariff_t consTarff[], int count, float gridTarff,
                      int vatPerc, int hourFrom) {
    int i = 0;
    char *ptr;
    char *ptr2;
    do {
        ptr = txt + 31;
        consTarff[i].index = i + hourFrom;  // ascending, starting from begin of time period
        consTarff[i].hour = (i + hourFrom) % 24;    // real hour, wrapping around at midnight
        consTarff[i].energyTariff = strtof(ptr, &ptr2);
        consTarff[i].consTariff = (consTarff[i].energyTariff + gridTarff) * ((100.0f + (float) vatPerc) / 100.0f);
        i++;
        if (i < (24 - hourFrom)) {
            readLine(file, txt, length); // ignore +xy/-xy entry between hourFrom and 23
        }
    } while (readLine(file, txt, length) == txt && i < count);
    if (i == count) {
        return 0;
    } else {
        return -1;
    }
}

int extractGridTariff(FILE *file, char txt[], int length, float *gridTarff) {
    char *ptr;
    if (readLine(file, txt, length) == txt) {
        *gridTarff = strtof(txt, &ptr);
        return 0;
    } else {
        return -1;
    }
}

int extractFeedTariff(FILE *file, char txt[], int length, float *feedTarff) {
    char *ptr;
    char *ptr2;
    while (readLine(file, txt, length) == txt && txt[0] != '\n') {
        ptr = txt;
        /*int year = */strtol(ptr, &ptr2, 10);
        ptr = ptr2 + 1; // next number
        /*int month = */strtol(ptr, &ptr2, 10);
        ptr = ptr2 + 1; // next number
        *feedTarff = strtof(ptr, &ptr2);
    }
    return 0;
}

char *readLine(FILE *file, char txt[], int length) {
    char *result = fgets(txt, length, file);
    while (result == txt && txt[0] == '#') {
        result = fgets(txt, length, file);
    }
    return result;
}

// zusätzlicher, aktuell unbenutzter Quell-Code
/*daylight_t pvMinutes[MONTH_COUNT] = {
        {540, 930},
        {510, 960},
        {450, 1020},
        {420, 1080},
        {390, 1140},
        {360, 1200},
        {390, 1170},
        {450, 1080},
        {480, 1020},
        {510, 960},
        {540, 930},
        {570, 900}}*/

/*float consumption[MONTH_COUNT][HOUR_COUNT] = {
        {0.232f, 0.242f, 0.256f, 0.225f, 0.260f, 0.328f, 0.355f, 0.794f, 0.786f, 0.660f, 1.031f, 1.133f, 1.224f, 0.731f, 0.708f, 0.533f, 0.770f, 0.882f, 0.807f, 1.180f, 0.735f, 0.687f, 0.592f, 0.318f},
        {0.231f, 0.217f, 0.228f, 0.251f, 0.276f, 0.325f, 0.430f, 0.807f, 0.726f, 0.661f, 0.858f, 1.237f, 1.272f, 0.747f, 0.633f, 0.551f, 0.511f, 0.636f, 0.856f, 0.798f, 0.712f, 0.689f, 0.585f, 0.317f},
        {0.183f, 0.180f, 0.213f, 0.247f, 0.238f, 0.344f, 0.396f, 0.674f, 0.754f, 0.771f, 0.814f, 1.060f, 1.126f, 0.764f, 0.491f, 0.501f, 0.412f, 0.521f, 0.792f, 0.674f, 0.745f, 0.775f, 0.493f, 0.298f},
        {0.241f, 0.219f, 0.207f, 0.206f, 0.231f, 0.281f, 0.330f, 0.354f, 0.570f, 0.758f, 0.710f, 1.306f, 1.110f, 0.805f, 0.789f, 0.568f, 0.470f, 0.621f, 0.593f, 0.671f, 0.697f, 0.658f, 0.488f, 0.309f},
        {0.201f, 0.185f, 0.190f, 0.170f, 0.176f, 0.219f, 0.235f, 0.358f, 0.408f, 0.515f, 0.805f, 1.059f, 1.166f, 0.813f, 0.609f, 0.566f, 0.513f, 0.587f, 0.596f, 0.619f, 0.601f, 0.549f, 0.455f, 0.322f},
        {0.232f, 0.159f, 0.151f, 0.157f, 0.175f, 0.196f, 0.206f, 0.307f, 0.427f, 0.468f, 0.835f, 0.936f, 1.069f, 0.718f, 0.547f, 0.556f, 0.643f, 0.705f, 0.503f, 0.533f, 0.622f, 0.605f, 0.484f, 0.351f},
        {0.200f, 0.159f, 0.156f, 0.159f, 0.167f, 0.174f, 0.231f, 0.461f, 0.563f, 0.499f, 0.727f, 1.061f, 0.882f, 0.510f, 0.477f, 0.363f, 0.446f, 0.436f, 0.434f, 0.375f, 0.563f, 0.480f, 0.421f, 0.419f},
        {0.201f, 0.185f, 0.190f, 0.170f, 0.176f, 0.219f, 0.235f, 0.358f, 0.408f, 0.515f, 0.805f, 1.059f, 1.166f, 0.813f, 0.609f, 0.566f, 0.513f, 0.587f, 0.596f, 0.619f, 0.601f, 0.549f, 0.455f, 0.322f},
        {0.241f, 0.219f, 0.207f, 0.206f, 0.231f, 0.281f, 0.330f, 0.354f, 0.570f, 0.758f, 0.710f, 1.306f, 1.110f, 0.805f, 0.789f, 0.568f, 0.470f, 0.621f, 0.593f, 0.671f, 0.697f, 0.658f, 0.488f, 0.309f},
        {0.166f, 0.151f, 0.166f, 0.153f, 0.211f, 0.316f, 0.382f, 0.516f, 0.610f, 0.662f, 0.808f, 0.876f, 0.778f, 0.657f, 0.585f, 0.457f, 0.539f, 0.660f, 0.856f, 0.810f, 0.620f, 0.572f, 0.438f, 0.288f},
        {0.201f, 0.204f, 0.194f, 0.216f, 0.257f, 0.292f, 0.316f, 0.710f, 0.755f, 0.795f, 1.010f, 1.365f, 1.419f, 0.925f, 0.799f, 0.747f, 0.719f, 0.927f, 0.870f, 0.972f, 0.933f, 0.843f, 0.602f, 0.399f},
        {0.237f, 0.221f, 0.248f, 0.253f, 0.278f, 0.373f, 0.381f, 0.860f, 0.776f, 0.620f, 1.099f, 1.577f, 1.383f, 0.767f, 0.732f, 0.529f, 0.762f, 0.892f, 0.929f, 0.965f, 0.863f, 0.732f, 0.602f, 0.342f}}*/
