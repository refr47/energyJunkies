
#ifdef SHELLY

#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "shelly.h"

#define RECEIVE_BUFFER_LEN 1024
#define TIME_TO_WAIT_FOR_UDP_RESPONSE 1000

#define STACK_SIZE_FOR_UDP_TASK 3196
#define USE_CORE_FOR_UDP_TASK 0

#define MAX_RETRIES_FOR_UDP_RESPONSE 15
/*
http://10.0.0.31/rpc/Switch.Set?id=0&on=false

*/
static WiFiUDP udp;

static char packetBuffer[RECEIVE_BUFFER_LEN]; // Buffer for incoming packets
static StaticJsonDocument<RECEIVE_BUFFER_LEN> doc;
static unsigned int shellyIndex = 0;
static bool doListen = false;
static unsigned int counter = 0;
static int packetSize = 0;
static SemaphoreHandle_t package_received_semaphore = NULL;
static SemaphoreHandle_t doListenSemaphore = NULL;
static SHELLY_OBJ *pShellyObjArray = NULL;
static ERROR_CONTAINER errorContainer;

static bool sendShellyCommandWithParm(const char *method, JsonObject &params);
static bool sendShellyCommandWithOutParm(const char *method);
static void taskListenForShellyCommand(void *pvParameters);
static bool setListenFlag();
static bool clearListenFlag();

void shelly_init(SHELLY_OBJ *shellyObj)
{
    pShellyObjArray = shellyObj;
    shellyObj[TROCKNER_shellyIndex].id = TROCKNER_SHELLY_ID;
    shellyObj[TROCKNER_shellyIndex].sent = false;
    shellyObj[TROCKNER_shellyIndex].received = false;
    strcpy(shellyObj[TROCKNER_shellyIndex].ip, "10.0.0.110");
    shellyObj[POOL_PUMPE_shellyIndex].id = POOL_PUMPE_SHELLY_ID;
    strcpy(shellyObj[POOL_PUMPE_shellyIndex].ip, "10.0.0.29");
    shellyObj[POOL_PUMPE_shellyIndex].sent = false;
    shellyObj[POOL_PUMPE_shellyIndex].received = false;

    shellyObj[POOL_WPUMPE_shellyIndex].id = POOL_PUMPE_SHELLY_ID;
    strcpy(shellyObj[POOL_WPUMPE_shellyIndex].ip, "10.0.0.13");
    shellyObj[POOL_WPUMPE_shellyIndex].sent = false;
    shellyObj[POOL_WPUMPE_shellyIndex].received = false;

    package_received_semaphore = xSemaphoreCreateBinary();
    doListenSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(doListenSemaphore);
    udp.begin(UDP_SHELLY_DEFAULT_PORT);
    delay(2000);
    xTaskCreatePinnedToCore(&taskListenForShellyCommand, "UDPResponseHandler", STACK_SIZE_FOR_UDP_TASK, NULL, 1, NULL, USE_CORE_FOR_UDP_TASK);
}

bool shelly_resetShelly(unsigned int sIndex)
{
    shellyIndex = sIndex;
    LOG_INFO("shelly::resetShelly::\n");
    pShellyObjArray[shellyIndex].received == false;
    sendShellyCommandWithOutParm(SHELLY_RESET_METHOD);
    LOG_INFO("shelly::resetShelly - after sending, index: %d, retVal: %x\n", shellyIndex, pShellyObjArray[shellyIndex].received);
    if (!pShellyObjArray[shellyIndex].received)
    {
        LOG_ERROR("shelly::switchOnOff::Waiting for response from %s did not come\n", pShellyObjArray[shellyIndex].ip);
    }
    return pShellyObjArray[shellyIndex].received == true;
}

bool shelly_switchOnOff(bool on, unsigned int ind)
{

    shellyIndex = ind;
    LOG_INFO("shelly::switchOnOff::%s\n", on ? "ON" : "OFF");
    if (pShellyObjArray[shellyIndex].sent && pShellyObjArray[shellyIndex].received == false)
    {
        LOG_ERROR("shelly::switchOnOff::Waiting for response from %s\n", pShellyObjArray[shellyIndex].ip);
        return false;
    }
    StaticJsonDocument<64> paramsDoc;
    JsonObject params = paramsDoc.to<JsonObject>();
    params["id"] = 0; // Relay ID
    if (!on)
    {
        params["on"] = false; // Turn off the relay
    }
    else
    {
        params["on"] = true;
    }

    pShellyObjArray[shellyIndex].received == false;
    sendShellyCommandWithParm(SHELLY_SWITCH_METHOD, params);
    LOG_INFO("shelly::switchOnOff - after sending, index: %d, retVal: %x, doListen: %x\n", shellyIndex, pShellyObjArray[shellyIndex].received, doListen);

    // delay(TIME_TO_WAIT_FOR_UDP_RESPONSE);
    if (!pShellyObjArray[shellyIndex].received)
    {
        LOG_ERROR("shelly::switchOnOff::Waiting for response from %s did not come\n", pShellyObjArray[shellyIndex].ip);
    }
    return pShellyObjArray[shellyIndex].received == true;
}

bool shelly_getStatus(unsigned int sIndex)
{
    shellyIndex = sIndex;
    if (pShellyObjArray[shellyIndex].sent && pShellyObjArray[shellyIndex].received == false)
    {
        LOG_ERROR("shelly::getStatus::Waiting for response from %s\n", pShellyObjArray[shellyIndex].ip);
        return false;
    }

    pShellyObjArray[shellyIndex].received == false;
    sendShellyCommandWithOutParm(SHELLY_STATUS_METHOD);
    LOG_INFO("shelly::getStatus - after sending, index: %d, retVal: %x\n", shellyIndex, pShellyObjArray[shellyIndex].received);
    if (!pShellyObjArray[shellyIndex].received)
    {
        LOG_ERROR("shelly::shelly_getStatus::Waiting for response from %s did not come\n", pShellyObjArray[shellyIndex].ip);
    }
    return pShellyObjArray[shellyIndex].received == true;
}

/* private methodes*/
static bool setListenFlag()
{

    if (xSemaphoreTake(doListenSemaphore, (TickType_t)100) == pdTRUE)
    {
        // Critical section
        doListen = true;
        //  End of critical section
        xSemaphoreGive(doListenSemaphore);
        return true;
    }
    return false;
}

static bool clearListenFlag()
{
    if (xSemaphoreTake(doListenSemaphore, (TickType_t)100) == pdTRUE)
    {
        // Critical section
        doListen = false;
        xSemaphoreGive(doListenSemaphore);
        return true;
    }
    return false;
}

static bool sendShellyCommandWithParm(const char *method, JsonObject &params)
{
    // Prepare the JSON payload
    StaticJsonDocument<256> doc;
    doc["id"] = pShellyObjArray[shellyIndex].id; // Unique message ID
    doc["src"] = method;                         // Source name
    doc["method"] = method;
    doc["params"] = params;

    char payload[256];
    serializeJson(doc, payload);

    // Send UDP packet
    udp.beginPacket(pShellyObjArray[shellyIndex].ip, UDP_SHELLY_DEFAULT_PORT);
    udp.print(payload);
    udp.endPacket();
    udp.flush();
    LOG_INFO("shelly::sendShellyCommandWithParm: %s", payload);
    pShellyObjArray[shellyIndex].sent = true;
    pShellyObjArray[shellyIndex].timestamp64Sent = millis();
    setListenFlag();
    xSemaphoreTake(package_received_semaphore, portMAX_DELAY);
    clearListenFlag();
    pShellyObjArray[shellyIndex].sent = false;
    return true;
}
static bool sendShellyCommandWithOutParm(const char *method)
{
    // Prepare the JSON payload
    StaticJsonDocument<256> doc;
    doc["id"] = pShellyObjArray[shellyIndex].id; // Unique message ID
    doc["src"] = method;                         // Source name
    doc["method"] = method;

    char payload[256];
    serializeJson(doc, payload);

    // Send UDP packet
    udp.beginPacket(pShellyObjArray[shellyIndex].ip, UDP_SHELLY_DEFAULT_PORT);
    udp.print(payload);
    udp.endPacket();
    LOG_INFO("shelly::sendShellyCommandWithOutParm: %s", payload);
    pShellyObjArray[shellyIndex].sent = true;
    pShellyObjArray[shellyIndex].timestamp64Sent = millis();
    setListenFlag();
    xSemaphoreTake(package_received_semaphore, portMAX_DELAY); // wait for response
    pShellyObjArray[shellyIndex].sent = false;
    clearListenFlag();
    return true;
}

static void taskListenForShellyCommand(void *pvParameters)
{
    while (1)
    {
        while (!doListen)
        {
            vTaskDelay(pdMS_TO_TICKS(1000)); // Simulate work
                                             // break;
        }

        packetSize = udp.parsePacket();

        // Serial.printf("taskListenForShellyCommand - packetSize: %d\n", packetSize);
        /* size_t freeHeap = esp_get_free_heap_size();
        Serial.printf("Free heap space: %u bytes\n", freeHeap); */
        if (packetSize)
        {
            packetBuffer[0] = 0;
            counter = 0;
            int len = udp.read(packetBuffer, RECEIVE_BUFFER_LEN);
            if (len > 0)
            {
                packetBuffer[len] = 0; // Null-terminate the string
            }
            // Serial.printf("Received packet: %s\n", packetBuffer);
            pShellyObjArray[shellyIndex].received = true;
            // Parse the JSON payload

            doc.clear();

            DeserializationError error = deserializeJson(doc, packetBuffer);

            if (error)
            {
                LOG_ERROR("shelly::deserializeJson() failed: ", error.c_str());
                pShellyObjArray[shellyIndex].received = false;
                memset(&errorContainer, 0, sizeof(errorContainer));
                errorContainer.errorHappend = true;
                strncpy(errorContainer.errorMessage, error.c_str(), SHELLY_ERROR_CONTAINER_LEN);
                strncpy(errorContainer.usedMethod, "deserializeJson", SHELLY_METHODE_LEN);
                pShellyObjArray[shellyIndex].errorContainer = &errorContainer;
                vTaskDelay(pdMS_TO_TICKS(500));
                xSemaphoreGive(package_received_semaphore); // Notify t
            }
            const char *src = doc["src"];
            // Serial.printf("Source: %s\n", src);
            unsigned int id = doc["id"];
            // Serial.printf("ID: %u\n", id);
            const char *method = doc["dst"];

            const char *err = doc["error"]["message"];
            if (err != NULL)
            {
                LOG_ERROR("shelly::taskListenForShelly method: %s Error: %s\n", method, err);
                memset(&errorContainer, 0, sizeof(errorContainer));
                errorContainer.errorHappend = true;
                strncpy(errorContainer.errorMessage, err, SHELLY_ERROR_CONTAINER_LEN);
                strncpy(errorContainer.usedMethod, method, SHELLY_METHODE_LEN);
                errorContainer.id = id;
                pShellyObjArray[shellyIndex].errorContainer = &errorContainer;
                pShellyObjArray[shellyIndex].received = false;
                xSemaphoreGive(package_received_semaphore); // Notify t
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            else
            {
                if (strcmp(method, SHELLY_RESET_METHOD) == 0)
                {

                    LOG_INFO("Received packet: %s\n", packetBuffer);
                }
                if (strcmp(method, SHELLY_SWITCH_METHOD) == 0)
                {
                    bool isOn = doc["result"]["was_on"];
                    // Serial.printf("Rela cvccv y is %s\n", isOn ? "ON" : "OFF");
                    pShellyObjArray[shellyIndex].response.switchStatus.wasOn = isOn;
                    LOG_INFO("shelly::SwitchSet, received package%s\n", packetBuffer);
                }
                if (strcmp(method, SHELLY_STATUS_METHOD) == 0)
                {
                    float currentWatt = doc["result"]["switch:0"]["apower"];
                    // Serial.printf("Current consumption: %.2f W\n", currentWatt);
                    float ampere = doc["result"]["switch:0"]["current"];
                    // Serial.printf("Current Ampere: %.2f A\n", ampere);
                    float consume = doc["result"]["switch:0"]["aenergy"]["total"];
                    // Serial.printf("Total consumption: %.2f kWh\n", consume);
                    pShellyObjArray[shellyIndex].response.status.currentConsumption = currentWatt;
                    pShellyObjArray[shellyIndex].response.status.currentAmpere = ampere;
                    pShellyObjArray[shellyIndex].response.status.collectedConsumption = consume / 1000.0;
                }
                pShellyObjArray[shellyIndex].received = true;
                pShellyObjArray[shellyIndex].errorContainer = NULL;
                xSemaphoreGive(package_received_semaphore); // Notify t
            }
        }
        else
        {
            pShellyObjArray[shellyIndex].received = false;
            ++counter;
            if (counter % MAX_RETRIES_FOR_UDP_RESPONSE == 0)
            {
                LOG_ERROR("shelly::taskListenForShellyCommand  max retries: %d  - for receiving answer reached\n", counter);
                counter = 0;
                memset(&errorContainer, 0, sizeof(errorContainer));
                errorContainer.errorHappend = true;
                strncpy(errorContainer.errorMessage, "No response", SHELLY_ERROR_CONTAINER_LEN);
                pShellyObjArray[shellyIndex].errorContainer = &errorContainer;
                xSemaphoreGive(package_received_semaphore); // Notify t
                vTaskDelay(pdMS_TO_TICKS(500));
                // return;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            // break;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS); // Warte 500 ms
    }
}

#endif