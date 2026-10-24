#include "legacy_mixer.h"
#include <cmath>

namespace ttp_mod_legacy {
short kaiser[3][4096 * 8];
short spline[256 * 4];
std::uint32_t dither_a{}, dither_b{};
namespace {
double bessel(double beta) {
    double sum = 1.0, term = 1.0, order = 0.0;
    do {
        order += 2.0;
        term = term * beta * beta / (order * order);
        sum += term;
    } while (sum * 1e-7 < term);
    return sum;
}
double cubic(double x, double a, double b, double c, double d) {
    const double delta = c - b;
    const double right = (d - b) * 0.5;
    const double left = (delta - (a - b)) * 0.5;
    return ((((right + left) - delta * 2.0) * x +
        ((delta * 3.0 - left * 2.0) - right)) * x + left) * x + b;
}
void make_kaiser(short *table, double beta, double cutoff) {
    const double normalization = bessel(beta);
    const double frequency = std::atan(1.0) * 4.0 * cutoff;
    for (unsigned i = 0; i != 4096 * 8; ++i) {
        const int coordinate = (7 - int(i & 7)) * 4096 + int(i >> 3);
        double value = 1.0;
        if (coordinate != 16384) {
            const double x = double(coordinate - 16384) / 4096.0;
            const double window = bessel(std::sqrt(1.0 - x * x * 0.0625) * beta);
            value = std::sin(x * frequency) * window / (x * normalization * frequency);
        }
        // Original 60467d25 truncates before the unsigned shift (not floor).
        const std::uint32_t scaled = 0x80u - std::uint32_t(int(value * cutoff * -4194304.0));
        table[i] = short(scaled >> 8);
    }
}
}
void InitializeTables() {
    // Called once under the reader engine initialization gate.
    make_kaiser(kaiser[0], 9.6377, 1.0);
    make_kaiser(kaiser[1], 8.5, 0.75);
    for (unsigned phase = 0; phase != 4096; ++phase) {
        const double x = double(phase) / 8192.0;
        for (unsigned sample = 0; sample != 4; ++sample) {
            const double a = sample == 0, b = sample == 1;
            const double c = sample == 2, d = sample == 3;
            kaiser[2][phase * 8 + sample * 2] = short(int(cubic(x + 0.5, a, b, c, d) * 8192.0));
            kaiser[2][phase * 8 + sample * 2 + 1] = short(int(cubic(x, a, b, c, d) * 8192.0));
        }
    }
    for (unsigned phase = 0; phase != 256; ++phase) {
        const double x = double(phase) / 256.0;
        for (unsigned sample = 0; sample != 4; ++sample)
            spline[phase * 4 + sample] = short(int(cubic(x, sample == 0, sample == 1,
                sample == 2, sample == 3) * 16384.0));
    }
}
void Dither(int *buffer, unsigned count, unsigned bits) {
    for (unsigned i = 0; i != count; ++i) {
        dither_a = ((dither_a << 1 | dither_a >> 31) ^ 0x10204080u) + 0x78649e7du + dither_b * 4;
        dither_b += (dither_a << 16 | dither_a >> 16) * 5;
        const int noise = std::int32_t(dither_b) >> ((bits + 5) & 31);
        buffer[i] = int(std::uint32_t(buffer[i]) + std::uint32_t(noise));
    }
}
unsigned long Convert16(void *destination, int *buffer, unsigned long count, long *minimum, long *maximum) {
    auto output = static_cast<short *>(destination);
    for (unsigned long i = 0; i != count; ++i) {
        // 60469d7e rounds by +0x800 before clipping and shifting. Upstream's
        // x86 converter instead truncates toward zero, changing signed LSBs.
        std::int64_t value = std::int64_t(buffer[i]) + 0x800;
        if (value < -0x8000000) value = -0x8000000;
        if (value > 0x7ffffff) value = 0x7ffffff;
        if (value < *minimum) *minimum = long(value);
        if (value > *maximum) *maximum = long(value);
        output[i] = short(value >> 12);
    }
    return count * 2;
}
}
