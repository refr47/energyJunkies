#pragma once
#include <stdint.h>
#include <algorithm>
#include <cmath>
#include <climits>

struct GridDemandConfig {
    bool enabled = false;
    bool enforceLimit = false;
    uint32_t limitW = 10000;
};
struct GridDemandInterval {
    int64_t start = 0;
    double importEnergyWh = 0;
    double observedSeconds = 0;
    bool complete = false;
};
struct GridDemandState {
    bool enabled = false, enforceLimit = false, limiting = false;
    uint32_t limitW = 10000, measurementAgeMs = 0;
    const char *status = "disabled";
    GridDemandInterval current, lastCompleted;
    double remainingEnergyWh = 0, allowedRemainingImportW = 0;
    int maxHeaterPowerW = -1; // -1: no applicable limit / unavailable
};

// Pure calculation aspect. Call regularly, even without fresh samples.
// UTC quarter boundaries; monotonic elapsed time detects wall-clock corrections.
class GridDemandAspect {
    GridDemandState state_;
    bool running = false, usable = false;
    int64_t lastWall = 0;
    uint32_t lastTick = 0, sequence = 0, sampleAt = 0;
    double grid = 0;
    int heater = 0;
    int heldCap = 0;
public:
    const GridDemandState &state() const { return state_; }
    void update(const GridDemandConfig &c, int64_t wall, uint32_t tick,
                bool night, bool valid, uint32_t seq, uint32_t sampledAt,
                double gridW, int heaterAtSample, uint32_t maxAgeMs) {
        state_.enabled = c.enabled;
        state_.enforceLimit = c.enforceLimit;
        state_.limitW = c.limitW;
        state_.limiting = false;
        state_.measurementAgeMs = seq ? uint32_t(tick - sampledAt) : 0;
        state_.maxHeaterPowerW = -1;
        if (!c.enabled || wall < 1609459200) {
            running = usable = false;
            state_.current = {}; state_.lastCompleted = {};
            state_.status = c.enabled ? "timeInvalid" : "disabled";
            if (c.enabled && c.enforceLimit && !night) state_.maxHeaterPowerW = 0;
            return;
        }
        const bool fresh = seq && seq != sequence;
        const bool clockJump = running && std::abs(double(wall - lastWall) - uint32_t(tick - lastTick) / 1000.0) > 2;
        if (!running || clockJump) {
            state_.current = {};
            state_.current.start = wall / 900 * 900;
            state_.current.complete = wall == state_.current.start && !clockJump;
            lastWall = wall; lastTick = tick;
            usable = false; heldCap = 0;
            running = true;
        }
        // Hold the previous measured power only within its freshness window.
        // Missing time (including night) is never silently counted as zero.
        const bool covered = usable && !night && valid && uint32_t(tick - sampleAt) <= maxAgeMs;
        int64_t cursor = lastWall;
        while (cursor < wall) {
            const int64_t end = std::min(wall, state_.current.start + 900);
            const double seconds = double(end - cursor);
            if (covered) {
                state_.current.importEnergyWh += std::max(0.0, grid) * seconds / 3600.0;
                state_.current.observedSeconds += seconds;
            } else state_.current.complete = false;
            cursor = end;
            if (cursor == state_.current.start + 900) {
                state_.lastCompleted = state_.current;
                state_.current = {};
                state_.current.start = cursor;
                state_.current.complete = true;
            }
        }
        lastWall = wall; lastTick = tick;
        if (fresh) { sequence = seq; sampleAt = sampledAt; grid = gridW; heater = heaterAtSample; }
        usable = !night && valid && seq && std::isfinite(gridW) && uint32_t(tick - sampledAt) <= maxAgeMs;
        state_.status = night ? "night" : !usable ? "stale" : state_.current.complete ? "ready" : "collecting";
        state_.remainingEnergyWh = std::max(0.0, c.limitW / 4.0 - state_.current.importEnergyWh);
        double allowed = state_.remainingEnergyWh * 3600.0 / (state_.current.start + 900 - wall);
        // Never spend unknown pre-start energy; also cap instantaneous import.
        allowed = std::min(double(c.limitW), allowed);
        state_.allowedRemainingImportW = allowed;
        if (night) { heldCap = 0; return; } // preserve configured night pause and heating policy
        if (!usable) { heldCap = 0; state_.maxHeaterPowerW = 0; return; }
        // 5% reserve, growing to 10% as a sample ages. Increases require fresh data.
        double margin = c.limitW * (0.05 + 0.05 * std::min(1.0, double(state_.measurementAgeMs) / std::max<uint32_t>(1, maxAgeMs)));
        int cap = int(std::min(double(INT_MAX), std::max(0.0, allowed - (grid - heater) - margin)));
        // At most 500 W upward per measurement, referenced to actual heating.
        const int riseLimit = std::max(0, heater) + 500;
        heldCap = fresh ? std::min(cap, riseLimit) : std::min(heldCap, cap);
        state_.maxHeaterPowerW = heldCap;
    }
    int limitHeating(int requested) {
        if (!state_.enabled || !state_.enforceLimit || state_.maxHeaterPowerW < 0) return requested;
        int result = std::min(requested, state_.maxHeaterPowerW);
        state_.limiting = result < requested;
        return result;
    }
};
