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

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// defined by board.mk
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_DEVICE_RHPORT_NUM
#define BOARD_DEVICE_RHPORT_NUM 0
#endif

// RHPort max operational speed can defined by board.mk
// Default to Highspeed for MCU with internal HighSpeed PHY (can be port specific), otherwise FullSpeed
#ifndef BOARD_DEVICE_RHPORT_SPEED
#if (CFG_TUSB_MCU == OPT_MCU_LPC18XX || CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX || \
     CFG_TUSB_MCU == OPT_MCU_NUC505 || CFG_TUSB_MCU == OPT_MCU_CXD56 || CFG_TUSB_MCU == OPT_MCU_SAMX7X)
#define BOARD_DEVICE_RHPORT_SPEED OPT_MODE_HIGH_SPEED
#else
#define BOARD_DEVICE_RHPORT_SPEED OPT_MODE_FULL_SPEED
#endif
#endif

// Device mode with rhport and speed defined by board.mk
#if BOARD_DEVICE_RHPORT_NUM == 0
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | BOARD_DEVICE_RHPORT_SPEED)
#elif BOARD_DEVICE_RHPORT_NUM == 1
#define CFG_TUSB_RHPORT1_MODE (OPT_MODE_DEVICE | BOARD_DEVICE_RHPORT_SPEED)
#else
#error "Incorrect RHPort configuration"
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif

// CFG_TUSB_DEBUG is defined by compiler in DEBUG build
// #define CFG_TUSB_DEBUG           0

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

    //--------------------------------------------------------------------
    // DEVICE CONFIGURATION
    //--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

//------------- CLASS -------------//
#define CFG_TUD_HID 1
#define CFG_TUD_AUDIO 1
#define CFG_TUD_CDC 0

// --- CDC buffer sizes (required by TinyUSB ≥0.15) -----------------
#ifndef CFG_TUD_CDC_RX_BUFSIZE
#define CFG_TUD_CDC_RX_BUFSIZE 64   // bytes  (may raise later)
#endif

#ifndef CFG_TUD_CDC_TX_BUFSIZE
#define CFG_TUD_CDC_TX_BUFSIZE 64   // bytes  (may raise later)
#endif

#define CFG_TUD_MSC 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

// HID buffer size Should be sufficient to hold ID (if any) + Data
#define CFG_TUD_HID_EP_BUFSIZE 16

/* ----------------  AUDIO function 1 (mono headset @48 kHz) ---------------- */
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX   1      // 1 channel for microphone
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX   1      // 1 channel for speaker
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_TYPE     1      // PCM
#define CFG_TUD_AUDIO_FUNC_1_SUBFORMAT       AUDIO_CS_FORMAT_TYPE_I_PCM
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_BITS 16
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE     48000

/* Endpoint payload size = samples per ms × bytes/sample × channels
   48 kHz × 2 × 1 / 1000  = 96 B  */
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ         96     // mic: 48 frames × 1 ch × 2 bytes
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ        96     // spk: 48 frames × 1 ch × 2 bytes

/* ---- extra parameters the TinyUSB audio driver insists on --------- */
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT      2   /* 2 streaming interfaces: spk + mic */
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ  64   /* control-request buffer */

/* Descriptor length for our custom headset */
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN      182

// --- enable TinyUSB's internal log trace -----------------
#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 2        // 0=off, 1=error, 2=info, 3=verbose
#endif

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
