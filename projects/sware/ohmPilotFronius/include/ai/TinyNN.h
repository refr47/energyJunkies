// =============================================================================
// TinyNN – Energiebudget-Prognose (Regression)
// ───────────────────────────────────────────
// INPUTS (4): temp_boiler, pv_ratio, cloud_avg, temp_outside
//   ↓
// HIDDEN (8): ReLU (unchanged)
//   ↓
// OUTPUTS (2): preheat_score [0..1], buffer_pct [0..1]
//
// Statt RL-Agent → regressions-Modell: gibt Preheat-Empfehlung + Vorrats-Puffer
// prozentual zurück. Trainiert auf MSE mit Targets aus PV-Wetter + Boiler-Zustand.
// Persistenz: NVS (Preferences)
// =============================================================================
#pragma once
#include <math.h>
#include <stdlib.h>
#include <Preferences.h>

#define TINYNN_INPUTS  4         // temp, pv_ratio, cloud, out_temp
#define TINYNN_HIDDEN  8
#define TINYNN_OUTPUTS 2         // preheat_score, buffer_pct
#define TINYNN_REPLAY  200
#define TINYNN_BATCH   10

struct TinyNNExperience
{
    // ── Normalisierte Inputs ──
    float temp;         // [-1 .. 1]  (Boiler relativ zu Min/Max)
    float pv_ratio;     // [-1 .. 1]  (heute/morgen, 0..2 → -1..1)
    float cloud_avg;    // [0 .. 1]   (Wolkenmittel)
    float temp_outside; // [-1 .. 1]  (Aussentemperatur)

    // ── Targets (Regression) ──
    float target_preheat; // [0 .. 1]  (gewünschte Preheat-Aggressivität)
    float target_buffer;  // [0 .. 1]  (gewünschter Vorrats-Puffer)
};

struct TinyNNPrediction
{
    float preheat_score; // [0.0 .. 1.0]  → 0 = kein Preheat, 1 = aggressiv
    float buffer_pct;    // [0.0 .. 1.0]  → 0 = leer, 1 = volles Budget
};

class TinyNN
{
public:
    // MAX_Power = onePhase*3, MIN_T = tempMin, MAX_T = tempMaxAllowed
    TinyNN(float MAX_POWER, float MIN_Temperature, float MAX_Temperature);

    // ── Prädiktion ──
    TinyNNPrediction predict(float temp, float pv_ratio, float cloud_avg, float temp_outside);

    // ── Training (Regression, MSE) ──
    void remember(const TinyNNExperience &exp);
    void trainReplay();

    // ── Persistenz ──
    void save();
    void load();

private:
    Preferences prefs;

    // ── Gewichtsmatrizen ──
    // Layer 1: INPUTS × HIDDEN
    float w1[TINYNN_INPUTS][TINYNN_HIDDEN];
    float b1[TINYNN_HIDDEN];

    // Layer 2: HIDDEN × OUTPUTS (Regression, keine Actions)
    float w2[TINYNN_HIDDEN][TINYNN_OUTPUTS];
    float b2[TINYNN_OUTPUTS];

    float hidden[TINYNN_HIDDEN];
    float output[TINYNN_OUTPUTS];

    float lr = 0.005;      // niedriger LR für Regression

    int updateCounter = 0;

    // ── Normalisierungs-Parameter ──
    float MIN_Temp;
    float MAX_Temp;

    // ── Replay-Buffer ──
    TinyNNExperience replay[TINYNN_REPLAY];
    int replayIndex = 0;
    int replayCount = 0;

    // ─── private helpers ──
    void forward(float in[TINYNN_INPUTS]);
    void train(float in[TINYNN_INPUTS], float target[TINYNN_OUTPUTS]);
    void randomInit();
};
