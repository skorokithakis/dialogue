#pragma once
#include <stdint.h>

#define I2S_DAC_PIN_DATA  10    // SD / DIN
#define I2S_DAC_PIN_BCK   11   // BCLK / SCK
#define I2S_DAC_PIN_WS     9   // LRCLK / WS

#ifdef __cplusplus
extern "C" {
#endif

void i2s_dac_init(uint32_t sample_rate_hz);                   // call once
void i2s_dac_write(const int32_t *samples, uint32_t n_words); // 32-bit L,R…

#ifdef __cplusplus
}       // extern "C"
#endif
