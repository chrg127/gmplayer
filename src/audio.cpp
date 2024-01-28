#include "audio.hpp"

namespace gmplayer {

// gain = unit_div_pow2(x, step, unit)
// gain < 64 when x = 5 * sample_rate * 2 / 512
//
// unit / 2**(secs * sample_rate * channels / block_size / step) = 64
// unit = 64 * 2**(secs * sample_rate * channels / block_size / step) = 64
// 2**shift = 2**fade_shift * 2**(secs * sample_rate * channels / block_size / step) = 64
//
// shift = fade_shift + (secs * sample_rate * channels / block_size / step)
// shift - fade_shift = secs * sample_rate * channels / block_size / step
// (shift - fade_shift) * step = secs * sample_rate * channels / block_size
// step = secs * sample_rate * channels / block_size * (shift - fade_shift)
//
// gain = f(x, step, unit) = 2**(x / step)
// gain = 2**14 when x = length of fade = 5 * sample_rate * 2 / 512
// 2**(5 * sample_rate * 2 / 512 / step) = 2**14
// secs * sample_rate * channels / block_size / step = unit_shift
// secs * sample_rate * channels / block_size = unit_shift * step
// step = secs * sample_rate * channels / block_size / unit_shift
Fade::Fade(Type type, int from, int length, int sample_rate, int num_channels)
{
    step = type == Type::In
         ? (length / 1000) * sample_rate * num_channels / BLOCK_SIZE / UNIT_SHIFT
         : (length / 1000) * sample_rate * num_channels / (BLOCK_SIZE * SHIFT);
    start = millis_to_samples(from, sample_rate, num_channels);
    len   = millis_to_samples(length, sample_rate, num_channels);
    this->type = type;
}

void Fade::put_in(std::span<short> samples, long num_samples)
{
    for (auto i = 0; i < samples.size(); i += BLOCK_SIZE) {
        auto gain = type == Type::Out
                  ? unit_div_pow2((num_samples + i - start) / BLOCK_SIZE, step, UNIT)
                  : short(std::pow(2, double(num_samples + i - start) / BLOCK_SIZE / step));
        // if (gain < (UNIT >> fade_shift)) // when close to 0
        //     return true;
        for (auto j = 0; j < std::min<long>(BLOCK_SIZE, samples.size() - i); j++)
            samples[i+j] = short(samples[i+j] * gain / UNIT);
    }
}

} // namespace gmplayer
