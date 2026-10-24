#pragma once
#include <cstdint>

// TTPlayer 5.7.9's integer mixer uses different interpolation tables from
// libmodplug 0.8.9.0. Generated from its formulas, never copied from a DLL.
namespace ttp_mod_legacy {
void InitializeTables();
extern short kaiser[3][4096 * 8];
extern short spline[256 * 4];
extern std::uint32_t dither_a, dither_b;
void Dither(int *buffer, unsigned count, unsigned bits);
unsigned long Convert16(void *destination, int *buffer, unsigned long count, long *minimum, long *maximum);

template<class Sample>
inline int Fir(const Sample *samples, int position, int increment) {
    const unsigned magnitude = increment < 0 ? 0u - unsigned(increment) : unsigned(increment);
    const unsigned bank = magnitude <= 0x13000 ? 0 : magnitude <= 0x18000 ? 1 : 2;
    const short *coefficients = kaiser[bank] + ((unsigned(position) & 0xfff0) >> 1);
    const Sample *input = samples + (position >> 16) - 3;
    int sum = 0;
    for (unsigned tap = 0; tap != 8; ++tap) sum += input[tap] * coefficients[tap];
    return sum >> (sizeof(Sample) == 1 ? 6 : 14);
}

template<class Sample>
inline int StereoSpline(const Sample *samples, int position) {
    const short *coefficients = spline + ((unsigned(position) >> 6) & 0x3fc);
    const Sample *input = samples + (position >> 16) * 2 - 2;
    int sum = 0;
    for (unsigned tap = 0; tap != 4; ++tap) sum += input[tap * 2] * coefficients[tap];
    return sum >> (sizeof(Sample) == 1 ? 6 : 14);
}
}
