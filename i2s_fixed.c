#include "i2s.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "i2s.pio.h"
#include <string.h>
#include <stdio.h>

static PIO i2s_pio = pio0;
static uint i2s_sm = 0;
static int i2s_dma_chan = -1;

// Double buffering for smooth playback
static int16_t i2s_buffer[2][I2S_BUFFER_SIZE];
static volatile uint current_buffer = 0;
static volatile bool buffer_ready[2] = {false, false};

// DMA interrupt handler
static void dma_irq_handler(void) {
    if (dma_hw->ints0 & (1u << i2s_dma_chan)) {
        dma_hw->ints0 = 1u << i2s_dma_chan;

        // Switch buffers
        buffer_ready[current_buffer] = false;
        current_buffer = 1 - current_buffer;

        if (buffer_ready[current_buffer]) {
            // Start DMA transfer from the next buffer
            dma_channel_set_read_addr(i2s_dma_chan, i2s_buffer[current_buffer], true);
        } else {
            // No data ready, output silence
            static int16_t silence[I2S_BUFFER_SIZE] = {0};
            dma_channel_set_read_addr(i2s_dma_chan, silence, true);
        }
    }
}

void i2s_init(void) {
    // Load PIO program
    uint offset = pio_add_program(i2s_pio, &i2s_program);

    // Initialize PIO program
    i2s_program_init(i2s_pio, i2s_sm, offset, I2S_DATA_PIN, I2S_BCLK_PIN);

    // Calculate clock divider for desired sample rate
    // The PIO program has 8 instructions total, each taking 2 cycles (one for instruction, one for side-set)
    // Total cycles per stereo sample = 8 * 2 = 16 cycles
    // But we output 32 bits (16 left + 16 right), and each bit takes 2 instructions
    // So total cycles = 32 bits * 2 instructions/bit * 1 cycle/instruction = 64 cycles
    // Required SYSCLK frequency = sample_rate * 64
    float clk_div = (float)clock_get_hz(clk_sys) / (I2S_SAMPLE_RATE * 128);
    pio_sm_set_clkdiv(i2s_pio, i2s_sm, clk_div);
    /* ---- generate ≈12.5 MHz MCLK on I2S_MCLK_PIN ------------------ */
    uint32_t sys_hz  = clock_get_hz(clk_sys);
    uint32_t div     = (sys_hz + 6250000) / 12500000;   // nearest ÷ that
    clock_gpio_init(I2S_MCLK_PIN, CLOCKS_CLK_GPOUT0_CTRL_SRC_CLK_SYS, div);
    // clock_gpio_init() already selects GPIO_FUNC_CLOCK for the pin

    printf("I2S: System clock = %u Hz\n", clock_get_hz(clk_sys));
    printf("I2S: Clock divider = %.4f\n", clk_div);
    printf("I2S: Effective BCLK = %.0f Hz\n", (float)clock_get_hz(clk_sys) / clk_div / 2);

    // Configure DMA
    i2s_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_config = dma_channel_get_default_config(i2s_dma_chan);

    // Transfer 32-bit words (left + right sample pairs)
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pio_get_dreq(i2s_pio, i2s_sm, true));

    dma_channel_configure(
        i2s_dma_chan,
        &dma_config,
        &i2s_pio->txf[i2s_sm],      // Write to PIO TX FIFO
        i2s_buffer[0],              // Read from first buffer
        I2S_BUFFER_SIZE / 2,        // Number of 32-bit transfers (stereo pairs)
        false                       // Don't start yet
    );

    // Configure DMA interrupt
    dma_channel_set_irq0_enabled(i2s_dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Enable PIO state machine
    pio_sm_set_enabled(i2s_pio, i2s_sm, true);
}

void i2s_write_samples(const int16_t* samples, size_t sample_count) {
    // Wait for a buffer to become available
    uint target_buffer = 1 - current_buffer;
    while (buffer_ready[target_buffer]) {
        tight_loop_contents();
    }

    // Copy samples to the buffer (convert mono to stereo)
    // Important: We're dealing with interleaved stereo data
    size_t stereo_samples = 0;
    for (size_t i = 0; i < sample_count && stereo_samples < I2S_BUFFER_SIZE; i++) {
        // For mono input, duplicate to both channels
        uint32_t word = ((uint32_t)samples[i]) << 16;      // bits 31…16 = data
        i2s_buffer[target_buffer][stereo_samples++] = word;  // left
        i2s_buffer[target_buffer][stereo_samples++] = word;  // right
    }

    // Fill rest with silence if needed
    while (stereo_samples < I2S_BUFFER_SIZE) {
        i2s_buffer[target_buffer][stereo_samples++] = 0;
    }

    // Mark buffer as ready
    buffer_ready[target_buffer] = true;

    // Start DMA if not already running
    if (!dma_channel_is_busy(i2s_dma_chan)) {
        // Configure for 32-bit transfers (stereo pairs)
        dma_channel_set_read_addr(i2s_dma_chan, i2s_buffer[target_buffer], false);
        dma_channel_set_trans_count(i2s_dma_chan, I2S_BUFFER_SIZE / 2, true);
    }
}

bool i2s_is_ready(void) {
    uint target_buffer = 1 - current_buffer;
    return !buffer_ready[target_buffer];
}
