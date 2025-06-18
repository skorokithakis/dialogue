#include "i2s_dac.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "i2s_tx.pio.h"

static PIO        pio    = pio0;
static uint       sm     = 0;
static uint       dma_ch = 0;
static uint32_t   sys_clk;

#define BUF_WORDS  512
static uint32_t buf[BUF_WORDS];

void i2s_dac_init(uint32_t sample_rate_hz)
{
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
                          buf,           // read addr
                          0,             // transfer count
                          false);

    pio_sm_set_enabled(pio, sm, true);
}

void i2s_dac_write(const int32_t *samples, uint32_t n_words)
{
    while (n_words)
    {
        uint32_t now = n_words > BUF_WORDS ? BUF_WORDS : n_words;
        memcpy(buf, samples, now * sizeof(uint32_t));
        dma_channel_wait_for_finish_blocking(dma_ch);
        dma_channel_set_read_addr(dma_ch, buf, false);
        dma_channel_set_trans_count(dma_ch, now, true);
        samples += now;
        n_words -= now;
    }
}
