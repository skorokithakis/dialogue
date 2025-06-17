#include "i2s.h"
#include "pico/stdlib.h"
#include <math.h>

// Generate a 440Hz test tone
void i2s_test_tone(void) {
    static uint32_t phase = 0;
    int16_t samples[256];
    
    // Generate 256 samples of a 440Hz sine wave
    for (int i = 0; i < 256; i++) {
        float angle = 2.0f * M_PI * phase / (48000.0f / 440.0f);
        samples[i] = (int16_t)(sin(angle) * 16000.0f);  // ~50% volume
        phase++;
        if (phase >= (48000 / 440)) phase = 0;
    }
    
    // Send to I2S
    i2s_write_samples(samples, 256);
}

// Simple test that outputs a continuous tone
void i2s_test_continuous(void) {
    i2s_init();
    
    // Toggle LED to show we're running
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    
    uint32_t count = 0;
    while (true) {
        i2s_test_tone();
        
        // Blink LED
        if (++count > 100) {
            gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_DEFAULT_LED_PIN));
            count = 0;
        }
    }
}