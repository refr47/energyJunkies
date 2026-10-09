// =============================================================================
// TinyNN.cpp – Energiebudget-Prognose (Regression-MSE)
// =============================================================================
#include "TinyNN.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// ── Konstruktor ────────────────────────────────────────────────────────────
TinyNN::TinyNN(float, float min_t, float max_t)
{
    prefs.begin("tinynn", false);

    if (static_cast<int>(prefs.getBytesLength("w1")) == static_cast<int>(sizeof(w1)))
    {
        load();
    }
    else
    {
        randomInit();
        save();
    }

    MIN_Temp = min_t;
    MAX_Temp = max_t;
}

// ── Initialisierung (Xavier/Glorot) ───────────────────────────────────────
void TinyNN::randomInit()
{
    float s1 = sqrtf(2.0f / (TINYNN_INPUTS + TINYNN_HIDDEN));
    float s2 = sqrtf(2.0f / (TINYNN_HIDDEN + TINYNN_OUTPUTS));

    for (int i = 0; i < TINYNN_INPUTS;  i++)
        for (int j = 0; j < TINYNN_HIDDEN; j++)
            w1[i][j] = ((float)rand() / RAND_MAX - 0.5f) * s1;

    for (int j = 0; j < TINYNN_HIDDEN; j++)
        b1[j] = 0.01f;

    for (int j = 0; j < TINYNN_HIDDEN; j++)
        for (int k = 0; k < TINYNN_OUTPUTS; k++)
            w2[j][k] = ((float)rand() / RAND_MAX - 0.5f) * s2;

    for (int k = 0; k < TINYNN_OUTPUTS; k++)
        b2[k] = 0.5f;    // Mitte [0..1] für Sigmoid-Start
}

// ── Forward-Pass ─────────────────────────────────────────────────────────
// Layer 1: INPUTS → HIDDEN (ReLU)
// Layer 2: HIDDEN → OUTPUTS (Sigmoid → [0..1])
void TinyNN::forward(float in[TINYNN_INPUTS])
{
    // Layer 1
    for (int j = 0; j < TINYNN_HIDDEN; j++)
    {
        float sum = b1[j];
        for (int i = 0; i < TINYNN_INPUTS; i++)
            sum += in[i] * w1[i][j];
        hidden[j] = sum > 0 ? sum : 0;  // ReLU
    }

    // Layer 2 + Sigmoid → [0..1]
    for (int k = 0; k < TINYNN_OUTPUTS; k++)
    {
        float sum = b2[k];
        for (int j = 0; j < TINYNN_HIDDEN; j++)
            sum += hidden[j] * w2[j][k];
        output[k] = 1.0f / (1.0f + expf(-sum));
    }
}

// ── Prädiktion ───────────────────────────────────────────────────────────
TinyNNPrediction TinyNN::predict(float temp, float pv_ratio,
                                 float cloud_avg, float temp_outside)
{
    float in[TINYNN_INPUTS] = { temp, pv_ratio, cloud_avg, temp_outside };

    forward(in);

    return { output[0], output[1] };  // preheat_score, buffer_pct
}

// ── Training (MSE-Regression) ────────────────────────────────────────────
void TinyNN::train(float in[TINYNN_INPUTS], float target[TINYNN_OUTPUTS])
{
    forward(in);

    // MSE Gradient für beide Outputs (Sigmoid-Derivativ: out*(1-out))
    float delta2[TINYNN_OUTPUTS];
    for (int k = 0; k < TINYNN_OUTPUTS; k++)
    {
        float mse    = target[k] - output[k];
        delta2[k]    = mse * output[k] * (1.0f - output[k]);  // dSigmoid
    }

    // Layer 2: w2 += lr * hidden.T * delta2
    for (int j = 0; j < TINYNN_HIDDEN; j++)
        for (int k = 0; k < TINYNN_OUTPUTS; k++)
            w2[j][k] += lr * delta2[k] * hidden[j];

    for (int k = 0; k < TINYNN_OUTPUTS; k++)
        b2[k] += lr * delta2[k];

    // Backprop nach Layer 1
    for (int j = 0; j < TINYNN_HIDDEN; j++)
    {
        float delta1 = 0;
        for (int k = 0; k < TINYNN_OUTPUTS; k++)
            delta1 += delta2[k] * w2[j][k];

        if (hidden[j] <= 0)  // ReLU-Derivativ
            delta1 = 0;

        // w1 update
        for (int i = 0; i < TINYNN_INPUTS; i++)
            w1[i][j] += lr * delta1 * in[i];

        b1[j] += lr * delta1;
    }

    // 🔁 Persist alle N Updates
    updateCounter++;
    if ((updateCounter % 500) == 0)
        save();
}

// ── Replay-Buffer ────────────────────────────────────────────────────────
void TinyNN::remember(const TinyNNExperience &exp)
{
    replay[replayIndex] = exp;
    replayIndex = (replayIndex + 1) % TINYNN_REPLAY;
    if (replayCount < TINYNN_REPLAY)
        replayCount++;
}

void TinyNN::trainReplay()
{
    if (replayCount < TINYNN_BATCH)
        return;

    for (int i = 0; i < TINYNN_BATCH; i++)
    {
        int idx = rand() % replayCount;
        const auto &e = replay[idx];

        float in[TINYNN_INPUTS]  = { e.temp, e.pv_ratio, e.cloud_avg, e.temp_outside };
        float tgt[TINYNN_OUTPUTS] = { e.target_preheat, e.target_buffer };

        train(in, tgt);
    }
}

// ── Persistenz (NVS) ────────────────────────────────────────────────────
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
