# CLAUDE.md

This file provides guidance to AI coding tools when working with code in this repository.

## Project Overview

This is "The Dialogue" - a rotary phone controller that converts a traditional rotary phone into a USB handset for meetings. The project uses a Raspberry Pi Pico with TinyUSB to provide:

- **Rotary Dial Input**: Detects pulse trains from rotary phone dial and converts them to keyboard input
- **USB Audio**: Acts as a USB microphone/headphone device for computer audio
- **Hang-up Detection**: Monitors phone hang-up state and sends keyboard shortcuts

## Architecture

### Core Components

- **main.cpp**: Main application loop with USB HID, pulse counting, and hang-up detection
- **audio.cpp/h**: USB audio device implementation using TinyUSB audio class
- **keyboard.h**: HID keyboard interface definitions
- **usb_descriptors.c/h**: USB device descriptors for HID and audio interfaces

### Key Hardware Interfaces

- **GPIO 27 (PULSE_PIN)**: Rotary dial pulse train input with debouncing
- **GPIO 13 (HANGUP_PIN)**: Hang-up switch detection
- **GPIO 26 (ADC)**: Audio input from phone microphone
- Built-in LED for status indication

### USB Device Configuration

The device presents as a composite USB device with:
- HID keyboard interface for dial input
- USB audio device (microphone and speaker)
- Multiple interface descriptors in usb_descriptors.c

## Development Commands

### Build and Upload

```bash
# Set environment variables (done automatically by upload script)
export PICO_PLATFORM=rp2040
export PICO_BOARD=pico

# Build and upload to Pico (requires BOOTSEL mode)
./upload
```

The upload script handles:
- Creating build directory
- Running CMake configuration and build
- Copying .uf2 file to mounted Pico (macOS and Linux)

### Manual Build

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target keyboard
```

## Special Features

### Rotary Dial Processing

- Pulses are counted and debounced with 5ms timing
- End-of-digit detected after 400ms silence
- Special sequence "1234" triggers USB bootloader reset

### Hang-up Sequence

On hang-up detection, sends multi-key sequence:
1. Alt+Q (20ms delay)
2. Enter (20ms delay)
3. Ctrl+W

Rate-limited to once per second.

## Dependencies

- Raspberry Pi Pico SDK (v1.3.0+)
- TinyUSB (included with Pico SDK)
- CMake 3.13+

Requires PICO_SDK_PATH environment variable to be set to Pico SDK location.
