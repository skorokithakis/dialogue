#include "i2s.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "i2s.pio.h"
#include <string.h>

static PIO i2s_pio = pio0;
static uint i2s_sm = 0;
static int i2s_dma_chan = -1;

// Double buffering for smooth playback - now 32-bit for stereo pairs
static uint32_t i2s_buffer[2][I2S_BUFFER_SIZE/2];
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
            static uint32_t silence[I2S_BUFFER_SIZE/2] = {0};
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
    // The PIO program takes 2 cycles per bit (one for BCLK low, one for BCLK high)
    // We need 32 bits per stereo sample (16 bits * 2 channels)
    // So we need: BCLK = sample_rate * 32
    // PIO runs at 2 * BCLK (since it takes 2 cycles per bit)
    float clk_div = (float)clock_get_hz(clk_sys) / (I2S_SAMPLE_RATE * 32 * 2);
    pio_sm_set_clkdiv(i2s_pio, i2s_sm, clk_div);
    
    // Configure DMA
    i2s_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_config = dma_channel_get_default_config(i2s_dma_chan);
    
    // Use 32-bit transfers for stereo pairs
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pio_get_dreq(i2s_pio, i2s_sm, true));
    
    dma_channel_configure(
        i2s_dma_chan,
        &dma_config,
        &i2s_pio->txf[i2s_sm],      // Write to PIO TX FIFO
        i2s_buffer[0],              // Read from first buffer
        I2S_BUFFER_SIZE/2,          // Number of 32-bit transfers
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
    // Pack left and right channels into 32-bit words
    size_t i;
    for (i = 0; i < sample_count && i < I2S_BUFFER_SIZE/2; i++) {
        uint16_t sample = (uint16_t)samples[i];
        // Pack stereo pair: [right_channel:left_channel] in memory
        // This will be sent as left first, then right due to MSB-first transmission
        i2s_buffer[target_buffer][i] = ((uint32_t)sample << 16) | sample;
    }
    
    // Fill rest with silence if needed
    for (; i < I2S_BUFFER_SIZE/2; i++) {
        i2s_buffer[target_buffer][i] = 0;
    }
    
    // Mark buffer as ready
    buffer_ready[target_buffer] = true;
    
    // Start DMA if not already running
    if (!dma_channel_is_busy(i2s_dma_chan)) {
        dma_channel_set_read_addr(i2s_dma_chan, i2s_buffer[target_buffer], true);
    }
}

bool i2s_is_ready(void) {
    uint target_buffer = 1 - current_buffer;
    return !buffer_ready[target_buffer];
}