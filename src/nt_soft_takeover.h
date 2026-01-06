#pragma once

#include <distingnt/api.h>
#include <cmath>

struct SoftTakeoverState {
    bool potButtonWasPressed[3];
    float lastPotPos[3];
    float normalTarget[3];
    float altTarget[3];
    int activeParam;
    int activeParamValue;
    int displayTimeout;
};

struct PotScaling {
    float min;
    float max;
    bool exponential;
    float expBase;
};

struct PotConfig {
    int normalParam;
    int altParam;
    PotScaling normalScaling;
    PotScaling altScaling;
};

inline void initSoftTakeover(SoftTakeoverState* state) {
    for (int i = 0; i < 3; i++) {
        state->potButtonWasPressed[i] = false;
        state->lastPotPos[i] = 0.5f;
        state->normalTarget[i] = 0.5f;
        state->altTarget[i] = 0.5f;
    }
    state->activeParam = -1;
    state->activeParamValue = 0;
    state->displayTimeout = 0;
}

inline float scalingToValue(const PotScaling& s, float potPos) {
    if (s.exponential) {
        return s.min * powf(s.expBase, potPos);
    }
    return s.min + potPos * (s.max - s.min);
}

inline float valueToScaling(const PotScaling& s, float value) {
    if (s.exponential) {
        if (value < s.min) value = s.min;
        return logf(value / s.min) / logf(s.expBase);
    }
    return (value - s.min) / (s.max - s.min);
}

inline void setupSoftTakeover(
    SoftTakeoverState* state,
    _NT_float3& pots,
    const PotConfig configs[3],
    const int16_t* v
) {
    for (int i = 0; i < 3; i++) {
        float normalValue = (float)v[configs[i].normalParam];
        float altValue = (float)v[configs[i].altParam];
        
        pots[i] = valueToScaling(configs[i].normalScaling, normalValue);
        state->lastPotPos[i] = pots[i];
        state->normalTarget[i] = pots[i];
        state->altTarget[i] = valueToScaling(configs[i].altScaling, altValue);
    }
}

struct PotResult {
    int paramIdx;
    float paramValue;
    bool changed;
};

inline PotResult processPot(
    SoftTakeoverState* state,
    int potIndex,
    const _NT_uiData& data,
    const PotConfig& config,
    int displayTimeoutFrames = 48000
) {
    PotResult result = { -1, 0.0f, false };
    
    uint16_t potFlags[3] = { kNT_potL, kNT_potC, kNT_potR };
    uint16_t buttonFlags[3] = { kNT_potButtonL, kNT_potButtonC, kNT_potButtonR };
    
    bool buttonPressed = (data.controls & buttonFlags[potIndex]) != 0;
    bool potMoved = (data.controls & potFlags[potIndex]) != 0;
    
    if (potMoved) {
        float potPos = data.pots[potIndex];
        float delta = potPos - state->lastPotPos[potIndex];
        
        float* target = buttonPressed ? &state->altTarget[potIndex] : &state->normalTarget[potIndex];
        const PotScaling& scaling = buttonPressed ? config.altScaling : config.normalScaling;
        int paramIdx = buttonPressed ? config.altParam : config.normalParam;
        
        *target += delta;
        if (*target < 0.0f) *target = 0.0f;
        if (*target > 1.0f) *target = 1.0f;
        
        bool inSync = fabsf(potPos - *target) < 0.02f ||
                      potPos <= 0.01f || potPos >= 0.99f;
        
        if (inSync) {
            *target = potPos;
        }
        
        float paramValue = scalingToValue(scaling, *target);
        
        result.paramIdx = paramIdx;
        result.paramValue = paramValue;
        result.changed = true;
        
        state->activeParam = paramIdx;
        state->activeParamValue = (int)paramValue;
        state->displayTimeout = displayTimeoutFrames;
        
        state->lastPotPos[potIndex] = potPos;
    }
    
    state->potButtonWasPressed[potIndex] = buttonPressed;
    
    return result;
}

inline bool processEncoder(
    SoftTakeoverState* state,
    int encoderIndex,
    const _NT_uiData& data,
    int paramIdx,
    int currentValue,
    int minVal,
    int maxVal,
    int step,
    int* newValue,
    int displayTimeoutFrames = 48000
) {
    if (data.encoders[encoderIndex] == 0) {
        return false;
    }
    
    int val = currentValue + data.encoders[encoderIndex] * step;
    if (val < minVal) val = minVal;
    if (val > maxVal) val = maxVal;
    
    *newValue = val;
    
    state->activeParam = paramIdx;
    state->activeParamValue = val;
    state->displayTimeout = displayTimeoutFrames;
    
    return true;
}

inline void decrementDisplayTimeout(SoftTakeoverState* state, int frames = 128) {
    if (state->displayTimeout > 0) {
        state->displayTimeout -= frames;
    }
}

inline bool isDisplayActive(const SoftTakeoverState* state) {
    return state->displayTimeout > 0;
}
