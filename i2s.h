#ifndef I2S_H
#define I2S_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define I2S_DATA_PIN   9     // DIN on CJMCU-1334
#define I2S_BCLK_PIN   10    // BCLK on CJMCU-1334  
#define I2S_LRCLK_PIN  11    // WSEL/LRCLK on CJMCU-1334
#define I2S_MCLK_PIN   12    // SCK / MCLK to CJMCU-1334 (≃12 MHz)

#define I2S_SAMPLE_RATE    48000
#define I2S_BITS_PER_SAMPLE 16
#define I2S_BUFFER_SIZE    1024

#ifdef __cplusplus
extern "C" {
#endif

void i2s_init(void);
void i2s_write_samples(const int16_t* samples, size_t sample_count);
bool i2s_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif // I2S_H
