#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <modbus/modbus.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <float.h>
#include <string.h>
#include <errno.h>
#include "pvclient.h"

// beginning of cut off (10:30)
#define CUT_OFF_BEGIN 630
// duration of cut off (3 hours 30 minutes)
#define CUT_OFF_DURATION 210
// cut off rate minimum: 0.7kW
// #define CUT_OFF_MIN 7

// modbus object, global for use in CTRL+C signal handler
modbus_t *inverter = NULL;

// the signal handler for CTRL+C
void catchIntSignal(int sig_num);

// cli argument evaluation
int evalArgs(int argc, char **argv, config_t *config);

// read smart meter values
int readSmartMeter(modbus_t *inverter, config_t *config, values_t *values, char timeStamp[]);

// read inverter values
int readInverter(modbus_t *inverter, config_t *config, values_t *values, char timeStamp[]);

// calculate new charge rate in %
void calcChrgPercTarg(config_t *config, values_t *values, struct tm tm, int minute);

// reconnect to server
int reconnect(modbus_t *inverter, config_t *config, char timeStamp[]);

int main(int argc, char **argv)
{
    // ip address of inverters: "10.0.0.13" GEN24 @ home, "172.17.68.202" Symo @ HTL
    // modbus unit id of smart meter: 200 GEN24 @ home, 240 Symo @ HTL

    config_t config = {
        "10.0.0.7", 502, CUT_OFF_DURATION, CUT_OFF_BEGIN, CUT_OFF_BEGIN + CUT_OFF_DURATION,
        CUT_OFF_BEGIN + CUT_OFF_DURATION + 10, 10, 30, 7,
        60, 92,
        200, 30000000, 10, MODBUS_ERROR_RECOVERY_NONE};

    // values read from inverter and smart meter
    values_t values = {0};
    // charge or discharge power (discharge: negative)
    float chrgDisChrgPwr;

    // return code
    int rc;

    // marker for successful reads
    bool success = true;

    // timestamp of current values
    time_t T;
    struct tm tm;
    // minute of the day, starts with 0 to enter while loop
    int minute = 0;
    // number of elapsed iterations
    int elapsed = 0;

    // log line
    char logLine[128];
    // time stamp
    char timeStamp[24];

    // set the INT (Ctrl-c) signal handler to 'catchIntSignal'
    signal(SIGINT, catchIntSignal);

    // evaluate command line arguments
    rc = evalArgs(argc, argv, &config);

    // get current time
    T = time(NULL);
    tm = *localtime(&T);
    // build text from time stamp values
    sprintf(timeStamp,
            "%04d-%02d-%02d %02d:%02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    // create modbus object for inverter
    inverter = modbus_new_tcp(config.ip, config.port);
    if (inverter == NULL)
    {
        fprintf(stderr, "%s: Unable to initialize Modbus slave %s at port %d\n", timeStamp, config.ip, config.port);
        return -1;
    }

    // set response timeout to 5.2 seconds
    modbus_set_response_timeout(inverter, 5, 200000);
    // set reconnect on failure bits, if set in command line arguments
    modbus_set_error_recovery(inverter, config.errorRecoveryMode);
    // connect to inverter
    rc = modbus_connect(inverter);
    if (rc == -1)
    {
        fprintf(stderr, "%s: Connect failed: %s\n", timeStamp, modbus_strerror(errno));
        modbus_close(inverter); // close failed connection??
                                // continue and retry all the time
                                // modbus_free(inverter); // stay here, don't leave
        // return -2;
    }

    // print table header for inverter (MPPT, storage) and smart meter values
    fputs("      date     time\t   dcPwr\t mpptPwr\tmppt1Pwr\tmppt2Pwr\t chrgPwr\t socPerc\t   chrg%\tchrg%New\t realPwr\t   acPwr\n",
          stdout);

    // read inverter registers (mppt, storage, ...) until end of cut off period
    while (minute < config.progEnd)
    {
        // assume succesful read operations from inverter and smart meter
        success = true;
        // get current time
        T = time(NULL);
        tm = *localtime(&T);
        sprintf(timeStamp,
                "%04d-%02d-%02d %02d:%02d:%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        // printf("System Date is: %02d/%02d/%04d\n", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
        // printf("System Time is: %02d:%02d:%02d\n", tm.tm_hour, tm.tm_min, tm.tm_sec);
        //  calculate the minute of the day
        minute = tm.tm_hour * 60 + tm.tm_min;

        // try to read values from smart meter
        rc = readSmartMeter(inverter, &config, &values, timeStamp);
        if (rc != 0)
        {
            success = false;
        }

        // try to read values from inverter
        rc = readInverter(inverter, &config, &values, timeStamp);
        if (rc != 0)
        {
            success = false;
        }

        // process values read in case of successful read operations
        if (success)
        {
            // calculate new charge rate in %
            calcChrgPercTarg(&config, &values, tm, minute);

            // combine charge and discharge power (discharge: negative)
            chrgDisChrgPwr = values.disChrgPwr != 0.0 ? values.disChrgPwr : -values.chrgPwr;
            // print row of current values
            sprintf(logLine,
                    "%s\t%8.2f\t%8.2f\t%8.2f\t%8.2f\t%8.2f\t%8.2f\t%8.2f\t%8.2f\t%8.2f\t%8.2f\n",
                    timeStamp, values.dcPwr, values.mpptPwr, values.mppt1Pwr, values.mppt2Pwr, chrgDisChrgPwr,
                    values.socPerc, values.chrgPerc, values.chrgPercTarg, values.realPwr, values.acPwr);
            fputs(logLine, stdout);

            // set recently changed charge rate at the inverter with slowed down frequency
            if (elapsed == 0 &&
                values.chrgPercTarg != values.chrgPerc)
            {
                // scale charge percent value
                values.chrgPercTarg /= values.chrgDisChrgSf;
                // write new storage charge rate in %
                //////rc = modbus_write_register(inverter, STRG_ADDR + CHRG_PERC, (uint16_t)values.chrgPercTarg);
                fprintf(stderr, "%s: modbus_write_register(%d, %d) ", timeStamp,
                        STRG_ADDR + CHRG_PERC, (uint16_t)values.chrgPercTarg);
                if (rc == -1)
                {
                    fprintf(stderr, "%s (%s)\n", "failed", modbus_strerror(errno));
                }
                else
                {
                    fprintf(stderr, "%s\n", "succeeded");
                }
            } // write modified charge rate?
        }
        else
        {
            rc = reconnect(inverter, &config, timeStamp); // read(s) failed, thus reconnect
        }

        elapsed = (elapsed + 1) % config.elapsedLimit; // increment iterations done

        // wait a little, except at program end
        if (minute < config.progEnd)
        {
            usleep(config.refreshMicroSeconds);
        }
    } // while minute ...

    // shut down modbus connection
    modbus_close(inverter);
    modbus_free(inverter);

    return 0;
}

// the signal handler for CTRL+C
void catchIntSignal(int sig_num)
{
    // reinstall the signal handler for CTRL+C
    signal(SIGINT, catchIntSignal);
    if (inverter != NULL)
    {
        modbus_close(inverter);
        modbus_free(inverter);
    }
    fputs("Terminated by CTRL+c\n", stderr);
}

// cli argument evaluation
int evalArgs(int argc, char **argv, config_t *config)
{
    // command line options handling
    int cliArg;

    opterr = 0;
    while ((cliArg = getopt(argc, argv, "i:p:s:n:u:t:m:c:e:d:b:o:lrh")) != -1)
    {
        switch (cliArg)
        {
        case 'i': // IP address
            strncpy(config->ip, optarg, IP_LENGTH);
            break;
        case 'p': // port
            config->port = atoi(optarg);
            break;
        case 's': // percent value for charging slowly
            config->chrgPercSlow = atoi(optarg);
            break;
        case 'n': // percent value for charging normally
            config->chrgPercNormal = atoi(optarg);
            break;
        case 'u': // percent value for charging normally
            config->chrgPercMin = atoi(optarg);
            break;
        case 't': // SoC limit to stop charging normally
            config->socPercThresh = atoi(optarg);
            break;
        case 'm': // modbus unit id of smart meter
            config->smartMeter = atoi(optarg);
            break;
        case 'c': // period length of one cycle
            config->refreshMicroSeconds = atol(optarg);
            break;
        case 'e': // number of cycles between modifications
            config->elapsedLimit = atoi(optarg);
            break;
        case 'd': // duration of cut off in minutes
            config->cutOffDuration = atoi(optarg);
            config->cutOffEnd = config->cutOffBegin + config->cutOffDuration;
            config->progEnd = config->cutOffEnd + 10;
            break;
        case 'b': // begin of cut off
            config->cutOffBegin = atoi(optarg);
            config->cutOffEnd = config->cutOffBegin + config->cutOffDuration;
            config->progEnd = config->cutOffEnd + 10;
            break;
        case 'o': // begin of cut off
            config->socPercMax = atoi(optarg);
            break;
        case 'l':
            config->errorRecoveryMode |= MODBUS_ERROR_RECOVERY_LINK;
            break;
        case 'r':
            config->errorRecoveryMode |= MODBUS_ERROR_RECOVERY_PROTOCOL;
            break;
        case 'h':
            fprintf(stderr,
                    "usage: %s -i <IP> -p <PORT> -s <SLOW %%> -n <NORMAL %%> -u <MINIMUM %%> -t <SoC THRESHOLD %%> -m <SMART METER> -c <CYCLE MICROSECS> -e <WRITE REDUCTION> -d <duration in minutes> -b <begin at minute> -o <max SoC> -l -r -h\n",
                    argv[0]);
            fprintf(stderr,
                    "\t-l ... recover from link error, -r ... recover from protocol error -h usage information\n");
            fprintf(stderr,
                    "usage: %s -i 10.0.0.13 -p 502 -s 9 -n 30 -u 7 -t 50 -m 200 -c 10000000 -e 30 -d 180 -b 645 -o 92 -l -r\n",
                    argv[0]);
            fprintf(stderr,
                    "connects to 10.0.0.13 at port 502, slow rate 9%%, normal rate 30%%, minimum rate 7%%, storage SoC threshold 50%%, smart meter Modbus id 200, cycle duration 10000000 microseconds, changes ervery 30th iteration, duration 180 minutes, begin at 10:45, max SoC 92%%\n");
            return 0;
        case '?':
            if (optopt == 'i' || optopt == 'p' || optopt == 's' || optopt == 'n' || optopt == 'u' ||
                optopt == 't' ||
                optopt == 'm' || optopt == 'c' || optopt == 'e' || optopt == 'd' || optopt == 'b' || optopt == 'o')
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

int readSmartMeter(modbus_t *inverter, config_t *config, values_t *values, char timeStamp[])
{
    int rc;
    // register values of smart meter
    uint16_t meterRegs[METER_REGS_COUNT];
    // use modbus unit smart meter as defined by config
    rc = modbus_set_slave(inverter, config->smartMeter);
    if (rc == -1)
    {
        fprintf(stderr, "%s: Smart meter select failed: %s\n", timeStamp, modbus_strerror(errno));
    }
    else
    {

        // read modbus registers of smart meter
        rc = modbus_read_registers(inverter, METER_ADDR, METER_REGS_COUNT, meterRegs);
        if (rc == -1)
        {
            fprintf(stderr, "%s: Smart meter read failed: %s\n", timeStamp, modbus_strerror(errno));
            values->realPwrSf = 0;
            values->realPwr = FLT_MAX;
            rc = -2;
        }
        else
        {
            // scale smart meter values read
            values->realPwrSf = (float)pow(10, meterRegs[METER_REAL_PWR_SF]);
            values->realPwr = (int16_t)meterRegs[METER_REAL_PWR] * values->realPwrSf;
            rc = 0;
        }
    }          // smart meter selected successfully
    return rc; // 0 ok, -1 device select error, -2 read error
}

int readInverter(modbus_t *inverter, config_t *config, values_t *values, char timeStamp[])
{
    int rc;
    // inverter successfully read?
    bool readSuccess = true;

    // register values of MPPT
    uint16_t mpptRegs[MPPT_REGS_COUNT];
    // register values of storage
    // uint16_t strgRegs[STRG_REGS_COUNT];
    // register values of smart meter
    uint16_t invrtRegs[INVRT_REGS_COUNT];

    // use modbus unit INVERTER
    rc = modbus_set_slave(inverter, INVERTER);
    if (rc == -1)
    {
        fprintf(stderr, "%s: Inverter select failed: %s\n", timeStamp,
                modbus_strerror(errno));
    }
    else
    {
        // read MPPT values (DC power)
        rc = modbus_read_registers(inverter, MPPT_ADDR, MPPT_REGS_COUNT, mpptRegs);
        if (rc == -1)
        {
            fprintf(stderr, "%s: Inverter read failed: %s\n", timeStamp,
                    modbus_strerror(errno));
            readSuccess = false;
        }

        // read storage values (SoC)
        /*
      rc = modbus_read_registers(inverter, STRG_ADDR, STRG_REGS_COUNT, strgRegs);
      if (rc == -1)
      {
          fprintf(stderr, "%s: Inverter read failed: %s\n", timeStamp,
                  modbus_strerror(errno));
          readSuccess = false;
      }
*/
        // read AC power values
        rc = modbus_read_registers(inverter, AC_PWR_ADDR, INVRT_REGS_COUNT, invrtRegs);
        if (rc == -1)
        {
            fprintf(stderr, "%s: Inverter read failed: %s\n", timeStamp,
                    modbus_strerror(errno));
            readSuccess = false;
        }

        // in case of successful read operations
        if (readSuccess)
        {
            rc = 0;
            printf("SF: %f\n", (float)pow(10, invrtRegs[AC_PWR_SF]));
            printf("AC_PWR_SF: %d, DC_WOR_SF: %d\n", AC_PWR_SF, DC_PWR_SF);
            // calculate scale factors
            // values->chrgDisChrgSf = (float)pow(10, strgRegs[CHRG_DIS_CHRG_SF]);
            values->mpptPwrSf = (float)pow(10, mpptRegs[MPPT_PWR_SF]);
            // values->socPercSf = (float)pow(10, strgRegs[SOC_PERC_SF]);
            values->acPwrSf = (float)pow(10, invrtRegs[AC_PWR_SF]);
            values->dcPwrSf = (float)pow(10, invrtRegs[DC_PWR_SF]);
            // scale inverter values read
            values->mppt1Pwr = (uint16_t)mpptRegs[MPPT_1_PWR] * values->mpptPwrSf;
            values->mppt2Pwr = (uint16_t)mpptRegs[MPPT_2_PWR] * values->mpptPwrSf;
            values->mpptPwr = values->mppt1Pwr + values->mppt2Pwr;
            values->acPwr = invrtRegs[AC_PWR] * values->acPwrSf;
            values->dcPwr = invrtRegs[DC_PWR] * values->dcPwrSf;
            values->chrgPwr = mpptRegs[MPPT_3_PWR] * values->mpptPwrSf;
            values->disChrgPwr = mpptRegs[MPPT_4_PWR] * values->mpptPwrSf;
            // values->socPerc = strgRegs[SOC_PERC] * values->socPercSf;
            // values->chrgPerc = strgRegs[CHRG_PERC] * values->chrgDisChrgSf;
        }
        else
        {
            rc = -2; // read failure
        }
    }
    return rc; // 0 ok, -1 device select error, -2 read error
}

void calcChrgPercTarg(config_t *config, values_t *values, struct tm tm, int minute)
{
    // mark slow charge percent multiplier to 'not calculated'
    static bool chrgPercSlowFactorCalculated = false;
    // charge percent scale factor
    float chrgPercSlowFactor;
    // to avoid cut off between 11:45 and 14:45:
    // limit Soc to 50% until 11:45, limit charge rate to 13% beyond 50% until 14:45
    // use previous charge percent as default target value
    values->chrgPercTarg = values->chrgPerc;
    // try to avoid cut off from April to August
    if (tm.tm_mon >= APRIL && tm.tm_mon <= AUGUST)
    {
        if (values->socPerc < config->socPercThresh)
        {
            // use normal charge rate below threshold
            values->chrgPercTarg = config->chrgPercNormal;
            // recalculate charge % scale factor next time
            chrgPercSlowFactorCalculated = false;
            // fprintf(stderr, "%s:\n", "values->socPerc < config->socPercThresh - 2");
        }
        else if (minute >= config->cutOffEnd)
        {
            // use normal charge rate after the end of the cut off period
            values->chrgPercTarg = config->chrgPercNormal;
            // fprintf(stderr, "%s:\n", "minute >= config->cutOffEnd");
        }
        else if (minute < config->cutOffBegin)
        {
            if (values->socPerc >= config->socPercThresh + 1)
            {
                // avoid charging before cut off begin with SoC high enough
                values->chrgPercTarg = 0;
                // fprintf(stderr, "%s:\n", "minute < config->cutOffBegin && values->socPerc >= config->...Thresh + 2");
            }
        }
        else
        { // within cut off period => charge slowly
            // calculate target SoC according to current minute realativ to cut off period length
            // 50% + (92% - 50%) * ((current - begin) / duration)
            values->socPercTarget = config->socPercThresh +
                                    (config->socPercMax - config->socPercThresh) *
                                        ((minute - config->cutOffBegin) / (float)config->cutOffDuration);
            if (!chrgPercSlowFactorCalculated)
            { // need to (re)calculate charge scale factor?
                chrgPercSlowFactorCalculated = true;
                // how much of available SoC range is already stored at this moment?
                //                chrgPercSlowFactor = 1.0 - (values->socPerc - values->socPercTarget) / //config->socPercThresh) /
                //                                           (config->socPercMax - config->socPercThresh);
                //                chrgPercSlowFactor = ((minute - config->cutOffBegin) / (float) config->cutOffDuration)
                //                                     / ((values->socPerc - config->socPercThresh) /
                //                                        (config->socPercMax - config->socPercThresh));
                chrgPercSlowFactor = ((config->socPercMax - values->socPerc) /
                                      (config->socPercMax - config->socPercThresh)) /
                                     ((config->cutOffEnd - minute) / (float)config->cutOffDuration);
                // reduce/increase charge percent slow accordingly
                values->chrgPercCalc = config->chrgPercSlow * chrgPercSlowFactor;
                if (values->chrgPercCalc < config->chrgPercMin)
                {
                    values->chrgPercCalc = config->chrgPercMin;
                }
                else if (values->chrgPercCalc > config->chrgPercNormal)
                {
                    values->chrgPercCalc = config->chrgPercNormal;
                }
                // fprintf(stderr, "%s: %f\n", "!chrgPercSlowFactorCalculated", values->chrgPercCalc);
            } // first run, calculate target charge %
            if ((int)values->chrgPercTarg != (int)values->chrgPercCalc)
            {
                values->chrgPercTarg = values->chrgPercCalc;
            }
        } // within cut off period
    }     // within cut off months?
}

// reconnect to server
int reconnect(modbus_t *inverter, config_t *config, char timeStamp[])
{
    // connect to inverter
    int rc = modbus_connect(inverter);
    if (rc == -1)
    {
        fprintf(stderr, "%s: Connect failed: %s\n", timeStamp, modbus_strerror(errno));
        modbus_close(inverter); // close failed connection??
    }
    return rc;
}
