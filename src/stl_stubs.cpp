/*
 * Minimal math stubs for bare-metal ARM (newlib-nano lacks these)
 * Used by: BiquadFilter (sinh), MipMappedWaveTable (tanh), FFT (atan2)
 */

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern "C" double sinh(double x) {
    double ex = exp(x);
    return (ex - 1.0/ex) * 0.5;
}

extern "C" double tanh(double x) {
    double ex = exp(2.0 * x);
    return (ex - 1.0) / (ex + 1.0);
}

extern "C" double atan2(double y, double x) {
    if (x > 0) return atan(y / x);
    if (x < 0 && y >= 0) return atan(y / x) + M_PI;
    if (x < 0 && y < 0) return atan(y / x) - M_PI;
    if (x == 0 && y > 0) return M_PI / 2;
    if (x == 0 && y < 0) return -M_PI / 2;
    return 0;
}
