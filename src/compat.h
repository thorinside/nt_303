/*
 * NT-303: Open303 TB-303 Emulator for Expert Sleepers Disting NT
 * MIT License - Copyright (c) 2025
 */

#ifndef NT_303_COMPAT_H
#define NT_303_COMPAT_H

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cmath>
#include <cstdint>

inline int cvToMidiNote(float cv) {
    float note = 60.0f + cv * 12.0f;
    if (note < 0.0f) note = 0.0f;
    if (note > 127.0f) note = 127.0f;
    return static_cast<int>(note + 0.5f);
}

inline float midiNoteToFreq(int note, float tuning = 440.0f) {
    return tuning * std::pow(2.0f, (note - 69) / 12.0f);
}

inline float cvToFreq(float cv, float tuning = 440.0f) {
    return tuning * std::pow(2.0f, (cv * 12.0f - 9.0f) / 12.0f);
}

#endif
