
#include <modbus/modbus.h>
#include <string.h>
#include <errno.h>

#define DEVICE_NAME_LEN 24

typedef struct
{
    int deviceId;               // modbus device id: inverter: 1, smart meter: 200
    int baseAddr;               // register number (already reduced by 1)
    int count;                  // number of registers to read
    char text[DEVICE_NAME_LEN]; // device name
} modbus_read_t;

// number of register blocks to read
#define REG_BLOCK_COUNT 2
#define INVERTER_ID 1
#define SMART_METER_ID 200
int powerganzzahl = 0;
float power = 0;

// inverter is device id 1, storage registers start at 400345, read 24 registers
// smart meter is device 200, meter registers start at 40087, read 5 registers
modbus_read_t regsToRead[REG_BLOCK_COUNT] = {{INVERTER_ID, 40345, 24, "Inverter"}, {SMART_METER_ID, 40087, 5, "Smart Meter"}};
const int regsCount = 24; // höchste Registeranzahl aus obiger Lesezugriffstabelle

modbus_t *inverter = NULL;

char text[256];
char buffer[32]; // Buffer zum umrechnen einstellig auf zweistellig
int maxpower = 5950;
int minpower = 5900;
int phasenanschnitt = 0;
int pwmpin = 12;
int leistungsstuffe1 = 0;
int leistungsstuffe2 = 0;
uint16_t res = 0;
int16_t resArr[REG_BLOCK_COUNT][regsCount];

int main(int argc, char **argv)
{

    static int readIndex = 0;
    int rc = 0;
    char ip[30];
    uint16_t port = 502;
    strcpy(ip, "10.0.0.7");
    inverter = modbus_new_tcp(ip, port);

    if (inverter == NULL)
    {
        fprintf(": Unable to initialize Modbus slave %s at port %d\n", ip, port);
        return -1;
    }
    // set response timeout to 5.2 seconds
    modbus_set_response_timeout(inverter, 5, 200000);
    rc = modbus_connect(inverter);
    if (rc == -1)
    {
        fprintf(" Connect failed: %s\n",  modbus_strerror(errno));
        modbus_close(inverter); // close failed connection??
    }

    uint16_t transId = 0;
    if (mb.isConnected(remote))
    { // Check if connection to Modbus Slave is established
        //   //Serial.println("Modbus/TCP connected");
        // mb.readHreg(remote, REG, &res);  // Initiate Read Coil from Modbus Slave
        transId = mb.readHreg(remote, regsToRead[readIndex].baseAddr, (uint16_t *)resArr[readIndex], regsToRead[readIndex].count, NULL, regsToRead[readIndex].deviceId); // Initiate Read Holding Register from Modbus Slave
        if (transId == 0)
        {
            Serial.println("Modbus/TCP register read failed");
        }
        else
        {
            Serial.println("Modbus/TCP register read succeeded");
        }
    }
    else
    {
        bool success = mb.connect(remote); // Try to connect if no connection
        Serial.println(success ? "Successfully connected to Modbus server" : "Failed to connect to Modbus server");
    }
    mb.task(); // Common local Modbus task
    if (transId != 0)
    {
        delay(300); // Pulling interval
        printf("---------------------------------\n");
        printf("%s", regsToRead[readIndex].text);
        for (int i = 0; i < regsToRead[readIndex].count; i++)
        {
            printf("%d\n", regsToRead[readIndex].baseAddr + i);

            if (regsToRead[readIndex].deviceId == SMART_METER_ID && i == 0)
            {
                power = resArr[readIndex][i] * pow(10, resArr[readIndex][4]);
                //       //############# Anzeige ##########################
                int powerganzzahl = (int)(power + .5); // Umwandeln in ganze Zahl
                powerganzzahl = powerganzzahl * -1;    // Umwandeln Negativ ins positive

                if (powerganzzahl <= 0)
                {
                    phasenanschnitt = 0;
                }

                if (powerganzzahl >= 0)
                {

                    if (powerganzzahl >= maxpower)
                    {

                        phasenanschnitt++;
                        if (phasenanschnitt >= 255)
                        {
                            phasenanschnitt = 255;
                        }
                        printf("Phasenschnitt: %d\n", phasenanschnitt);
                    }

                    if (powerganzzahl <= minpower)
                    {
                        phasenanschnitt--;
                        if (phasenanschnitt <= 0)
                        {
                            phasenanschnitt = 0;
                        }
                        printf("Phasenschnitt: %d\n", phasenanschnitt);
                    }

                    if (phasenanschnitt >= 255)
                    {
                        leistungsstuffe1 = 1;
                    }
                }

                //       // scale real power by it's scale factor
                printf("%d\n", resArr[readIndex][i] * pow(10, resArr[readIndex][4]));
            }
            else
            {
                printf("%d\n", resArr[readIndex][i]);
            }
        }
        readIndex = (readIndex + 1) % REG_BLOCK_COUNT;
    }
}