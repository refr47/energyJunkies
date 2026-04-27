#include "TinyNN.h"

TinyNN::TinyNN(float MAX_POWER, float MIDDLE_T, float TEMP_R)
{
    prefs.begin("tinynn", false);

    if (prefs.getBytesLength("w1") == sizeof(w1))
    {
        load();
    }
    else
    {
        randomInit();
        save();
    }
    MAX_Power = MAX_POWER;
    MIDDLE_Temp = MIDDLE_T;
    TEMP_Range = TEMP_R;
}

void TinyNN::randomInit()
{
    for (int i = 0; i < INPUTS; i++)
        for (int j = 0; j < HIDDEN; j++)
            w1[i][j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;

    for (int j = 0; j < HIDDEN; j++)
        b1[j] = 0;

    for (int j = 0; j < HIDDEN; j++)
        for (int k = 0; k < ACTIONS; k++)
            w2[j][k] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;

    for (int k = 0; k < ACTIONS; k++)
        b2[k] = 0;
}

void TinyNN::save()
{
    prefs.putBytes("w1", w1, sizeof(w1));
    prefs.putBytes("b1", b1, sizeof(b1));
    prefs.putBytes("w2", w2, sizeof(w2));
    prefs.putBytes("b2", b2, sizeof(b2));
}

void TinyNN::load()
{
    prefs.getBytes("w1", w1, sizeof(w1));
    prefs.getBytes("b1", b1, sizeof(b1));
    prefs.getBytes("w2", w2, sizeof(w2));
    prefs.getBytes("b2", b2, sizeof(b2));
}

float TinyNN::normalizeTemp(float t)
{
    float x = (t-MIDDLE_Temp) / TEMP_Range; // Normalisiert auf [-1, 1]
    if (x < -1) x = -1;
    if (x > 1) x = 1;
    return x;
}

float TinyNN::normalizePower(float p)
{
    if (p > MAX_Power)
        p = MAX_Power;
    if (p < -MAX_Power)
        p = -MAX_Power;
    return p / MAX_Power;
}
void TinyNN::remember(float temp, float power, int action, float reward)
{
    replay[replayIndex] = {temp, power, action, reward};

    replayIndex = (replayIndex + 1) % REPLAY_SIZE;

    if (replayCount < REPLAY_SIZE)
        replayCount++;
}
void TinyNN::trainReplay()
{
    if (replayCount < BATCH_SIZE)
        return;

    for (int i = 0; i < BATCH_SIZE; i++)
    {
        int idx = rand() % replayCount;
        Experience &e = replay[idx];

        train(e.temp, e.power, e.action, e.reward);
    }
}

void TinyNN::forward(float in[INPUTS])
{
    for (int j = 0; j < HIDDEN; j++)
    {
        float sum = b1[j];
        for (int i = 0; i < INPUTS; i++)
            sum += in[i] * w1[i][j];

        hidden[j] = sum > 0 ? sum : 0;
    }

    for (int k = 0; k < ACTIONS; k++)
    {
        float sum = b2[k];
        for (int j = 0; j < HIDDEN; j++)
            sum += hidden[j] * w2[j][k];

        output[k] = sum;
    }
}

int TinyNN::chooseAction(float temp, float power)
{
    float in[INPUTS] = {
        normalizeTemp(temp),
        normalizePower(power)};

    forward(in);

    if (((float)rand() / RAND_MAX) < epsilon)
        return rand() % ACTIONS;

    int best = 0;
    for (int i = 1; i < ACTIONS; i++)
        if (output[i] > output[best])
            best = i;

    return best;
}

void TinyNN::train(float temp, float power, int action, float reward)
{
    float in[INPUTS] = {
        normalizeTemp(temp),
        normalizePower(power)};

    forward(in);

    float error = reward - output[action];

    for (int j = 0; j < HIDDEN; j++)
        w2[j][action] += lr * error * hidden[j];

    b2[action] += lr * error;

    for (int j = 0; j < HIDDEN; j++)
    {
        float grad = error * w2[j][action];

        if (hidden[j] <= 0)
            grad = 0;

        for (int i = 0; i < INPUTS; i++)
            w1[i][j] += lr * grad * in[i];

        b1[j] += lr * grad;
    }

    // 🔁 Save every N updates
    updateCounter++;
    if (updateCounter % 200 == 0)
        save();

    // ε decay
    if (epsilon > 0.05f)
        epsilon *= 0.9995f;
}