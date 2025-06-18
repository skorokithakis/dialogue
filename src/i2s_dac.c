#include "i2s_dac.h"

/*  Minimal stub implementation.
    Replace later with a real PIO/DMA I²S driver if desired.              */

void i2s_dac_init(uint32_t sample_rate_hz)
{
  (void) sample_rate_hz;          // keep compiler quiet
  /* nothing to do for a stub */
}

void i2s_dac_write(const int32_t *samples, uint32_t n_words)
{
  (void) samples;
  (void) n_words;                 // discard data in stub
}
