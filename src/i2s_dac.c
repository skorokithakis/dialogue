#include <string.h>    // memcpy()
#include "tusb.h"      // tud_task() – called while waiting
#include "i2s_dac.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "i2s_tx.pio.h"

static PIO        pio    = pio0;
static uint       sm     = 0;
static uint       dma_ch = 0;
static uint32_t   sys_clk;

#define BUF_WORDS   256
static uint32_t buf0[BUF_WORDS];
static uint32_t buf1[BUF_WORDS];
static bool     use_buf0 = true;

void i2s_dac_init(uint32_t sample_rate_hz) {
    sys_clk = clock_get_hz(clk_sys);

    uint offset = pio_add_program(pio, &i2s_tx_program);
    i2s_tx_program_init(pio, sm, offset,
                        I2S_DAC_PIN_DATA,
                        I2S_DAC_PIN_WS,
                        I2S_DAC_PIN_BCK);

    float clk_div = sys_clk / (sample_rate_hz * 128.0f);
    pio_sm_set_clkdiv(pio, sm, clk_div);

    dma_ch = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
    dma_channel_configure(dma_ch, &c,
                          &pio->txf[sm], // write addr
                          buf0,          // read addr (initial, count = 0)
                          0,             // transfer count
                          false);

    pio_sm_set_enabled(pio, sm, true);
}

void i2s_dac_write(const int32_t *samples, uint32_t n_words) {
    while (n_words) {
        uint32_t chunk = n_words > BUF_WORDS ? BUF_WORDS : n_words;
        uint32_t *dst  = use_buf0 ? buf0 : buf1;
        memcpy(dst, samples, chunk * sizeof(uint32_t));

        /* wait until ≤64 words remain, servicing USB meanwhile */
        if (dma_channel_is_busy(dma_ch))
        {
            while (dma_hw->ch[dma_ch].transfer_count > 64)  // still >0.7 ms head-room
            {
                tud_task();
            }
            /* final short wait (≤64 words) – tight loop */
            while (dma_channel_is_busy(dma_ch)) { }
        }

        dma_channel_set_read_addr(dma_ch, dst, false);
        dma_channel_set_trans_count(dma_ch, chunk, true);

        use_buf0 = !use_buf0;
        samples += chunk;
        n_words -= chunk;
    }
}
