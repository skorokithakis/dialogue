# I2S Implementation Analysis

## Issues Found

### 1. PIO Program Bit Count Error
The original PIO program outputs 17 bits per channel instead of 16:
- It outputs 1 bit before the loop
- Loops 15 times (x=14 to x=0)
- Outputs 1 bit after the loop
- Total: 17 bits

### 2. Clock Divider Calculation
The clock divider calculation is incorrect:
```c
float clk_div = (float)clock_get_hz(clk_sys) / (I2S_SAMPLE_RATE * 32 * 4);
```
- The `* 4` factor doesn't match the actual PIO timing
- Should be based on actual PIO instruction cycles

### 3. Entry Point Location
The program starts at instruction 7, which is:
```asm
set x, 14           side 0b01   ; Reset bit counter, BCLK=1, LRCLK=0
```
This is in the middle of right channel processing, causing initial misalignment.

### 4. DMA Configuration Issues
- DMA configured for 16-bit transfers but dealing with stereo pairs
- Buffer size calculations don't account for stereo properly
- Missing proper circular buffer setup for continuous playback

### 5. I2S Protocol Compliance
- LRCLK should change 1 BCLK before MSB (not simultaneously)
- No proper frame sync timing
- May not be compatible with all DACs

## Recommended Solutions

### 1. Fixed PIO Program (`i2s_fixed.pio`)
- Properly outputs exactly 16 bits per channel
- Entry point at the beginning of left channel
- Clear separation between left and right channels
- Uses consistent timing

### 2. Corrected Clock Calculation
```c
// 64 total PIO cycles per stereo frame (32 bits)
float clk_div = (float)clock_get_hz(clk_sys) / (I2S_SAMPLE_RATE * 64);
```

### 3. Improved DMA Setup
- Use 32-bit transfers for stereo pairs
- Proper buffer management
- Circular buffer option for continuous playback

### 4. Test Implementation (`i2s_test.c`)
- Generates 440Hz test tone
- Uses circular DMA for continuous output
- Includes debugging functions
- Simpler setup for testing

## Testing Steps

1. Replace `i2s.pio` with `i2s_fixed.pio`
2. Replace `i2s.c` with `i2s_fixed.c` 
3. Rebuild and test
4. If still no sound, try `i2s_test.c` for a simple 440Hz tone
5. Use an oscilloscope or logic analyzer to verify:
   - BCLK frequency (should be 1.536 MHz for 48kHz)
   - LRCLK frequency (should be 48 kHz)
   - Data pin activity
   - Proper I2S timing relationships

## Additional Debugging

Add to main.cpp to verify I2S output:
```cpp
// Call periodically to check pin states
extern "C" void i2s_debug_pins(void);
i2s_debug_pins();
```

## Hardware Checks

1. Verify CJMCU-1334 connections:
   - VDD to 3.3V
   - GND to ground
   - DIN to GPIO 9
   - BCLK to GPIO 10
   - WSEL to GPIO 11

2. Check if DAC needs:
   - External pull-ups on I2S lines
   - Specific I2S format (standard/left-justified/right-justified)
   - Different bit depth settings

3. Verify audio output connections:
   - VOUTR/VOUTL properly connected
   - Output coupling capacitors if needed
   - Proper load (headphones/amplifier)