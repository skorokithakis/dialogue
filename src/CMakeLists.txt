cmake_minimum_required(VERSION 3.13)

add_executable(keyboard)

# Generate PIO headers
pico_generate_pio_header(keyboard ${CMAKE_CURRENT_LIST_DIR}/../i2s.pio)
pico_generate_pio_header(keyboard ${CMAKE_CURRENT_LIST_DIR}/../ws2812.pio)

target_sources(keyboard PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/audio.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../i2s.c
    ${CMAKE_CURRENT_LIST_DIR}/../ws2812.c)

# Make sure TinyUSB can find tusb_config.h
target_include_directories(keyboard PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..)  # For i2s.h

# In addition to pico_stdlib required for common PicoSDK functionality, add dependency on tinyusb_device
# for TinyUSB device support and tinyusb_board for the additional board support library used by the example
target_link_libraries(keyboard
    PUBLIC
        pico_stdlib
        tinyusb_device
        tinyusb_board
        hardware_adc
        hardware_pio
        hardware_dma
        hardware_clocks
        m)                        # <-- add this line


pico_add_extra_outputs(keyboard)
