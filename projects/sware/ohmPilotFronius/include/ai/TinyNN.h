#pragma once
#include <math.h>
#include <stdlib.h>
#include <Preferences.h>

#define INPUTS 2
#define HIDDEN 8
#define ACTIONS 5
#define REPLAY_SIZE 200
#define BATCH_SIZE 10

struct Experience
{
    float temp;
    float power;
    int action;
    float reward;
};

class TinyNN
{
public:
    TinyNN(float MAX_POWER, float MIDDLE_Temperature, float TEMP_Ran);

    int chooseAction(float temp, float power);
    void train(float temp, float power, int action, float reward);
    void remember(float temp, float power, int action, float reward);
    void trainReplay();

    void save();
    void load();

private:
    Preferences prefs;

    float w1[INPUTS][HIDDEN];
    float b1[HIDDEN];

    float w2[HIDDEN][ACTIONS];
    float b2[ACTIONS];

    float hidden[HIDDEN];
    float output[ACTIONS];

    float epsilon = 0.2;
    float lr = 0.01;

    int updateCounter = 0;

    float MAX_Power;   // onePhase * 3
    float MIDDLE_Temp; // 65+35 /2
    float TEMP_Range;  // 65-35
    

    float normalizeTemp(float t);
    float normalizePower(float p);

    void forward(float in[INPUTS]);
    void randomInit();
    Experience replay[REPLAY_SIZE];
    int replayIndex = 0;
    int replayCount = 0;
};