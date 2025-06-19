/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

#include "usb_descriptors.h"
#include "keyboard.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
extern "C" {
#include "pico/bootrom.h"
}

// -----------------------------------------------------------------
// Arduino-compat helpers for reset_usb_boot() call
#ifndef LED_BUILTIN
#include "pico/stdlib.h"
#define LED_BUILTIN PICO_DEFAULT_LED_PIN   // map to Pico's on-board LED
#endif

static inline uint digitalPinToPinName(uint pin) { return pin; }
// -----------------------------------------------------------------


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+


// ---------------  PULSE-COUNT INPUT ----------------
#define PULSE_PIN          27      // free GPIO used for pulse train
#define PULSE_DEBOUNCE_MS    5      // match back-ported debounce
volatile uint32_t pulse_count      = 0;   // #edges since last send
volatile uint32_t last_pulse_time  = 0;   // ms timestamp of last **accepted** edge
// ---------------------------------------------------

// ---------------  HANG-UP INPUT -----------------
#define HANGUP_PIN          13        // unused GPIO, pulled-up HIGH
// ------------------------------------------------

void hid_task(void);
void pulse_task(void);
void hangup_task(void);
void gpio_irq_callback(uint gpio, uint32_t events);

KeyBoard keyboard;

void gpio_irq_callback(uint gpio, uint32_t events)
{
  if (gpio == PULSE_PIN && (events & GPIO_IRQ_EDGE_FALL))
  {
    uint32_t now = board_millis();

    // Accept this edge only if it is at least PULSE_DEBOUNCE_MS after
    // the previous accepted one
    if ((uint32_t)(now - last_pulse_time) >= PULSE_DEBOUNCE_MS)
    {
      ++pulse_count;          // valid pulse
      last_pulse_time = now;  // remember when it occurred
    }
  }
}

/*------------- MAIN -------------*/
int main(void)
{
    board_init();
    // ---------- pulse counter pin --------------
    gpio_init(PULSE_PIN);
    gpio_pull_up(PULSE_PIN);                       // idle = high, pulse = low
    gpio_set_dir(PULSE_PIN, GPIO_IN);
    // -------------------------------------------
    // ---------- hang-up pin --------------
    gpio_init(HANGUP_PIN);
    gpio_pull_up(HANGUP_PIN);            // idle = HIGH, active = LOW
    gpio_set_dir(HANGUP_PIN, GPIO_IN);
    // ------------------------------------
    tusb_init();

    while (1)
    {
        tud_task(); // tinyusb device task

        pulse_task();            // NEW : converts pulse train to one keystroke
        hangup_task();         // NEW : sends Ctrl-W on off-hook
        // hid_task(); // keyboard implementation
    }

    return 0;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(bool keys_pressed)
{
    // skip if hid is not ready yet
    if (!tud_hid_ready())
    {
        return;
    }

    // avoid sending multiple zero reports
    static bool send_empty = false;

    if (keys_pressed)
    {
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keyboard.key_codes);
        send_empty = true;
    }
    else
    {
        // send empty key report if previously has key pressed
        if (send_empty)
        {
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        }
        send_empty = false;
    }
}

// Every 10ms, we poll the pins and send a report
void hid_task(void)
{
    // Poll every 10ms
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms)
    {
        return; // not enough time
    }
    start_ms += interval_ms;

    // Check for keys pressed
    bool const keys_pressed = keyboard.update();

    // Remote wakeup
    if (tud_suspended() && keys_pressed)
    {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    }
    else
    {
        // send a report
        send_hid_report(keys_pressed);
    }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint8_t len)
{
    // not implemented, we only send REPORT_ID_KEYBOARD
    (void)instance;
    (void)len;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;

    if (report_type == HID_REPORT_TYPE_OUTPUT)
    {
        // Set keyboard LED e.g Capslock, Numlock etc...
        if (report_id == REPORT_ID_KEYBOARD)
        {
            // bufsize should be (at least) 1
            if (bufsize < 1)
                return;

            uint8_t const kbd_leds = buffer[0];
            (void)kbd_leds;
        }
    }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

void tud_mount_cb(void) {}
void tud_umount_cb(void) {}
void tud_suspend_cb(bool) {}
void tud_resume_cb(void) {}

static inline uint8_t ascii_to_key(char c)            // lower/upper A-Z only
{
  if (c >= 'a' && c <= 'z') return HID_KEY_A + (c - 'a');
  if (c >= 'A' && c <= 'Z') return HID_KEY_A + (c - 'A');
  return 0;
}

void pulse_task(void)
{
  /* ---------- debouncer state --------------------------------- */
  static bool     init             = false;
  static bool     debounced_state  = true;   // last stable level
  static bool     instant_state    = true;   // last raw sample
  static uint32_t debounce_start   = 0;      // ms when a change started
  /* ---------- key-sending state -------------------------------- */
  /* ---------- rolling "1234" detector -------------------------- */
  static uint8_t last4[4] = { 0xFF, 0xFF, 0xFF, 0xFF };   // history of digits
  /* ------------------------------------------------------------- */
  static bool     send_pending = false;      // need to send digit ?
  static uint8_t  digit_key    = 0;          // HID key to emit
  static bool     pressed      = false;      // phase-tracker for HID send
  /* ------------------------------------------------------------- */

  // one-time initialisation
  if (!init)
  {
    debounced_state = gpio_get(PULSE_PIN);   // should be HIGH
    instant_state   = debounced_state;
    debounce_start  = board_millis();
    last_pulse_time = board_millis();
    init = true;
  }

  // Abort dialling when handset is hung up (HANGUP_PIN is HIGH)
  if (gpio_get(HANGUP_PIN))
  {
    pulse_count  = 0;
    send_pending = false;
    pressed      = false;
    return;                    // nothing else while on-hook
  }

  /* --------- sample pin & drive onboard LED ------------------- */
  bool sample = gpio_get(PULSE_PIN);

  /* --------- debounce identical to original C example --------- */
  if (sample != instant_state)
  {
    instant_state  = sample;
    debounce_start = board_millis();
  }

  if ((uint32_t)(board_millis() - debounce_start) >= PULSE_DEBOUNCE_MS)
  {
    if (instant_state != debounced_state)          // stable change
    {
      debounced_state = instant_state;
      last_pulse_time = board_millis();

      if (!debounced_state) ++pulse_count;         // LOW edge counted
    }
  }

  /* --------- detect end-of-digit ( >400 ms silence ) ---------- */
  uint32_t now_ms = board_millis();
  if (!send_pending && pulse_count &&
      (now_ms - last_pulse_time) > 400)
  {
    uint32_t cnt = pulse_count;
    pulse_count  = 0;

    /* ---- update rolling buffer with the new digit --------------- */
    uint8_t new_digit = (cnt == 10) ? 0 : (cnt <= 9 ? cnt : 0xFF);
    last4[0] = last4[1];
    last4[1] = last4[2];
    last4[2] = last4[3];
    last4[3] = new_digit;

    if (last4[0] == 1 && last4[1] == 2 && last4[2] == 3 && last4[3] == 4)
    {
      reset_usb_boot(1 << digitalPinToPinName(LED_BUILTIN), 0);
    }
    /* ------------------------------------------------------------- */

    if      (cnt == 10) digit_key = HID_KEY_0;
    else if (cnt <= 9)  digit_key = HID_KEY_1 + (cnt - 1);
    else                digit_key = 0;

    if (digit_key) send_pending = true;
  }

  /* --------- 2-phase HID send (press / release) --------------- */
  if (send_pending && tud_hid_ready())
  {
    if (!pressed)                         // phase 1 : press
    {
      uint8_t kc[6] = { digit_key, 0,0,0,0,0 };
      tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, kc);
      pressed = true;
    }
    else                                  // phase 2 : release
    {
      tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
      pressed      = false;
      send_pending = false;
    }
  }
}

void hangup_task(void)
{
  // ------------- multi-key sequence control --------------------
  enum HangState {
    HS_IDLE,
    HS_PRESS_ALTQ,  HS_RELEASE_ALTQ,  HS_WAIT1,
    HS_PRESS_ENTER, HS_RELEASE_ENTER, HS_WAIT2,
    HS_PRESS_CTRLW, HS_RELEASE_CTRLW, HS_WAIT3,
    HS_PRESS_CSH,   HS_RELEASE_CSH
  };
  static HangState state   = HS_IDLE;
  static uint32_t  last_ms = 0;           // used for both 1-s rate-limit and 20-ms waits

  // --- 50-ms debouncer -------------------------------------------------
  static bool     debounced_state  = gpio_get(HANGUP_PIN);
  static bool     instant_state    = debounced_state;
  static uint32_t debounce_start   = board_millis();
  // --------------------------------------------------------------------

  // ----------- sample & debounce ( ≥50 ms stable ) --------------------
  bool     sample   = gpio_get(HANGUP_PIN);
  uint32_t now_ms   = board_millis();

  if (sample != instant_state)
  {                               // level changed → restart timer
    instant_state  = sample;
    debounce_start = now_ms;
  }

  if ((uint32_t)(now_ms - debounce_start) >= 50)     // 50-ms stable ?
  {
    if (instant_state != debounced_state)            // accept new state
    {
      debounced_state = instant_state;

      // rising edge (LOW → HIGH) initiates sequence, but not more than once/sec
      if (debounced_state &&
          state == HS_IDLE &&
          (uint32_t)(now_ms - last_ms) >= 1000)
      {
        state   = HS_PRESS_ALTQ;   // start the 3-key sequence
        // last_ms keeps its old value for the 1-s limit; it will be
        // updated when the sequence is finished
      }
    }
  }

  // ---------------- send Alt-Q, Enter, Ctrl-W sequence -----------------
  if (tud_hid_ready())
  {
    switch (state)
    {
      case HS_PRESS_ALTQ:
      {
        uint8_t kc[6] = { HID_KEY_Q, 0,0,0,0,0 };
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD,
                                KEYBOARD_MODIFIER_LEFTALT, kc);
        state = HS_RELEASE_ALTQ;
        break;
      }

      case HS_RELEASE_ALTQ:
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        last_ms = board_millis();
        state   = HS_WAIT1;
        break;

      case HS_WAIT1:
        if (board_millis() - last_ms >= 20) state = HS_PRESS_ENTER;
        break;

      case HS_PRESS_ENTER:
      {
        uint8_t kc[6] = { HID_KEY_ENTER, 0,0,0,0,0 };
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, kc);
        state = HS_RELEASE_ENTER;
        break;
      }

      case HS_RELEASE_ENTER:
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        last_ms = board_millis();
        state   = HS_WAIT2;
        break;

      case HS_WAIT2:
        if (board_millis() - last_ms >= 20) state = HS_PRESS_CTRLW;
        break;

      case HS_PRESS_CTRLW:
      {
        uint8_t kc[6] = { HID_KEY_W, 0,0,0,0,0 };
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD,
                                KEYBOARD_MODIFIER_LEFTCTRL, kc);
        state = HS_RELEASE_CTRLW;
        break;
      }

      case HS_RELEASE_CTRLW:
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        last_ms = board_millis();     // start 20-ms pause before C-S-H
        state   = HS_WAIT3;
        break;

      case HS_WAIT3:                         // 20-ms pause after Ctrl-W
        if (board_millis() - last_ms >= 20) state = HS_PRESS_CSH;
        break;

      case HS_PRESS_CSH:                     // Ctrl-Shift-H  (press)
      {
        uint8_t kc[6] = { HID_KEY_H, 0,0,0,0,0 };
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD,
                                KEYBOARD_MODIFIER_LEFTCTRL |
                                KEYBOARD_MODIFIER_LEFTSHIFT, kc);
        state = HS_RELEASE_CSH;
        break;
      }

      case HS_RELEASE_CSH:                   // Ctrl-Shift-H  (release)
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        last_ms = board_millis();            // restart 1-s rate-limit timer
        state   = HS_IDLE;
        break;

      default:
        break;
    }
  }
  // --------------------------------------------------------------------
}

