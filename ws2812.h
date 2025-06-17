#ifndef WS2812_H
#define WS2812_H

#include "pico/stdlib.h"

#define WS2812_PIN 16

#ifdef __cplusplus
extern "C" {
#endif

void ws2812_init(void);
void ws2812_set(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif // WS2812_H