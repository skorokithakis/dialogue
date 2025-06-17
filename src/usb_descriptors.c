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

#include "tusb.h"
#include "usb_descriptors.h"
#include "class/audio/audio_device.h"   // <-- provides TWO_CH descriptor +
/* ------------------------------------------------------------------
 * TinyUSB versions that ship with the Pico-SDK (≤ 1.5.0) define only
 * TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR / _DESC_LEN.
 * Provide 2-channel aliases so the rest of this source compiles on
 * all TinyUSB revisions.
 * The generic builder macros exist since v0.15, therefore we just
 * wrap them; if they are also missing we fail with #error.
 * ------------------------------------------------------------------*/
#if !defined(TUD_AUDIO_MIC_TWO_CH_DESCRIPTOR)

/* generic helper present in every TinyUSB that has Audio support */
#  if defined(TUD_AUDIO_MIC_DESCRIPTOR) && defined(TUD_AUDIO_MIC_DESCRIPTOR_LEN)

#define TUD_AUDIO_MIC_TWO_CH_DESC_LEN \
            TUD_AUDIO_MIC_DESCRIPTOR_LEN(2)

#define TUD_AUDIO_MIC_TWO_CH_DESCRIPTOR(_itfnum, _stridx,        \
                                            _nBytesPerSubslot,        \
                                            _nBitResolution,          \
                                            _epin, _epsize)           \
            TUD_AUDIO_MIC_DESCRIPTOR(_itfnum, _stridx, 2,             \
                                      _nBytesPerSubslot,              \
                                      _nBitResolution,                \
                                      _epin, _epsize)

#  else
/* ----------------------------------------------------------------
 * Old TinyUSB releases have only the ONE-CH descriptor helpers.
 * Map the 2-channel names to the mono equivalents so the project
 * still compiles.  (You will enumerate as one-channel audio.)
 * ----------------------------------------------------------------*/
#define TUD_AUDIO_MIC_TWO_CH_DESC_LEN \
            TUD_AUDIO_MIC_ONE_CH_DESC_LEN

#define TUD_AUDIO_MIC_TWO_CH_DESCRIPTOR(_itfnum, _stridx,          \
                                            _nBytesPerSubslot,          \
                                            _nBitResolution,            \
                                            _epin, _epsize)             \
            TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(_itfnum, _stridx,           \
                                            _nBytesPerSubslot,          \
                                            _nBitResolution,            \
                                            _epin, _epsize)
#  endif

#endif /* fallback for TWO_CH macros */

/* ---------- compat: ONE-channel speaker descriptor helper ------------- */
#if !defined(TUD_AUDIO_SPEAKER_ONE_CH_DESCRIPTOR)
# if defined(TUD_AUDIO_SPEAKER_DESCRIPTOR) && defined(TUD_AUDIO_SPEAKER_DESCRIPTOR_LEN)
#  define TUD_AUDIO_SPEAKER_ONE_CH_DESC_LEN \
           TUD_AUDIO_SPEAKER_DESCRIPTOR_LEN(1)
#  define TUD_AUDIO_SPEAKER_ONE_CH_DESCRIPTOR(_itfnum, _stridx,          \
                                               _nBytesPerSubslot,          \
                                               _nBitResolution,            \
                                               _epout, _epsize)            \
           TUD_AUDIO_SPEAKER_DESCRIPTOR(_itfnum, _stridx, 1,              \
                                        _nBytesPerSubslot, _nBitResolution,\
                                        _epout, _epsize)
# else
/* if even the generic builder is missing fall back to MIC macro so     */
/* compilation still works (enumerates as “another” microphone)         */
#  define TUD_AUDIO_SPEAKER_ONE_CH_DESC_LEN \
           TUD_AUDIO_MIC_ONE_CH_DESC_LEN
#  define TUD_AUDIO_SPEAKER_ONE_CH_DESCRIPTOR  TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR
# endif
#endif

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_PID (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                 _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) | _PID_MAP(AUDIO, 5))

#define USB_VID 0xcafe
#define USB_BCD 0x0200

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_hid_report[] = {TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD))};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum {
    ITF_NUM_AUDIO_CONTROL = 0,       // Single audio control interface
    ITF_NUM_AUDIO_STREAMING_SPK,     // 1 - Speaker streaming interface
    ITF_NUM_AUDIO_STREAMING_MIC,     // 2 - Microphone streaming interface

    ITF_NUM_HID,                     // 3
    ITF_NUM_TOTAL                    // 4 interfaces in this config
};

// Calculate the exact length of our custom headset descriptor
// IAD(8) + STD_AC(9) + CS_AC_HDR(10) + IN_TERM(12) + FEAT_UNIT(9) + OUT_TERM(9) +
// IN_TERM(12) + OUT_TERM(9) + SPK_AS_ALT0(9) + SPK_AS_ALT1(9) + SPK_CS_AS(7) +
// SPK_FORMAT(11) + SPK_EP(9) + SPK_CS_EP(7) + MIC_AS_ALT0(9) + MIC_AS_ALT1(9) +
// MIC_CS_AS(7) + MIC_FORMAT(11) + MIC_EP(9) + MIC_CS_EP(7) = 182 bytes
#define TUD_AUDIO_HEADSET_MONO_DESC_LEN  182

#define CONFIG_TOTAL_LEN  ( TUD_CONFIG_DESC_LEN + \
                            TUD_AUDIO_HEADSET_MONO_DESC_LEN + \
                            TUD_HID_DESC_LEN )


#define EPNUM_HID 0x81

#define EPNUM_AUDIO_IN   0x82   // ISO IN endpoint 2
#define EPNUM_AUDIO_OUT  0x03     // ISO OUT endpoint 3

uint8_t const desc_configuration[] = {
    // -------- configuration header --------
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0,
                          CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    /* --- Custom UAC1 headset descriptor (single audio function) --- */
    // IAD (Interface Association Descriptor)
    TUD_AUDIO_DESC_IAD(ITF_NUM_AUDIO_CONTROL, 3, 0),

    // Audio Control Interface
    TUD_AUDIO_DESC_STD_AC(ITF_NUM_AUDIO_CONTROL, 0, 0),

    // Class-Specific AC Interface Header
    0x0A,                                    // bLength
    TUSB_DESC_CS_INTERFACE,                  // bDescriptorType
    AUDIO_CS_AC_INTERFACE_HEADER,            // bDescriptorSubtype
    U16_TO_U8S_LE(0x0100),                   // bcdADC (Audio Device Class 1.0)
    U16_TO_U8S_LE(0x0047),                   // wTotalLength (71 bytes for AC descriptors)
    0x02,                                    // bInCollection (2 streaming interfaces)
    ITF_NUM_AUDIO_STREAMING_SPK,             // baInterfaceNr(1) - Speaker
    ITF_NUM_AUDIO_STREAMING_MIC,             // baInterfaceNr(2) - Microphone

    // Input Terminal - USB streaming to speaker
    0x0C,                                    // bLength
    TUSB_DESC_CS_INTERFACE,                  // bDescriptorType
    AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL,    // bDescriptorSubtype
    0x01,                                    // bTerminalID
    U16_TO_U8S_LE(AUDIO_TERM_TYPE_USB_STREAMING), // wTerminalType
    0x00,                                    // bAssocTerminal
    0x01,                                    // bNrChannels (mono)
    U16_TO_U8S_LE(AUDIO_CHANNEL_CONFIG_NON_PREDEFINED), // wChannelConfig
    0x00,                                    // iChannelNames
    0x00,                                    // iTerminal

    // Feature Unit for speaker volume/mute
    0x09,                                    // bLength
    TUSB_DESC_CS_INTERFACE,                  // bDescriptorType
    AUDIO_CS_AC_INTERFACE_FEATURE_UNIT,      // bDescriptorSubtype
    0x02,                                    // bUnitID
    0x01,                                    // bSourceID (Input Terminal 1)
    0x01,                                    // bControlSize
    0x03,                                    // bmaControls(0) - master mute + volume
    0x00,                                    // bmaControls(1) - channel 1 controls
    0x00,                                    // iFeature

    // Output Terminal - speaker
    0x09,                                    // bLength
    TUSB_DESC_CS_INTERFACE,                  // bDescriptorType
    AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL,   // bDescriptorSubtype
    0x03,                                    // bTerminalID
    U16_TO_U8S_LE(AUDIO_TERM_TYPE_OUT_HEADPHONES), // wTerminalType
    0x00,                                    // bAssocTerminal
    0x02,                                    // bSourceID (Feature Unit 2)
    0x00,                                    // iTerminal

    // Input Terminal - microphone
    0x0C,                                    // bLength
    TUSB_DESC_CS_INTERFACE,                  // bDescriptorType
    AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL,    // bDescriptorSubtype
    0x04,                                    // bTerminalID
    U16_TO_U8S_LE(AUDIO_TERM_TYPE_IN_GENERIC_MIC), // wTerminalType
    0x00,                                    // bAssocTerminal
    0x01,                                    // bNrChannels (mono)
    U16_TO_U8S_LE(AUDIO_CHANNEL_CONFIG_NON_PREDEFINED), // wChannelConfig
    0x00,                                    // iChannelNames
    0x00,                                    // iTerminal

    // Output Terminal - USB streaming from microphone
    0x09,                                    // bLength
    TUSB_DESC_CS_INTERFACE,                  // bDescriptorType
    AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL,   // bDescriptorSubtype
    0x05,                                    // bTerminalID
    U16_TO_U8S_LE(AUDIO_TERM_TYPE_USB_STREAMING), // wTerminalType
    0x00,                                    // bAssocTerminal
    0x04,                                    // bSourceID (Input Terminal 4)
    0x00,                                    // iTerminal

    // Speaker Streaming Interface (Alternate 0 - no endpoints)
    0x09,                                    // bLength
    TUSB_DESC_INTERFACE,                     // bDescriptorType
    ITF_NUM_AUDIO_STREAMING_SPK,             // bInterfaceNumber
    0x00,                                    // bAlternateSetting
    0x00,                                    // bNumEndpoints
    TUSB_CLASS_AUDIO,                        // bInterfaceClass
    AUDIO_SUBCLASS_STREAMING,                // bInterfaceSubClass
    0x00,                                    // bInterfaceProtocol (UAC1)
    0x00,                                    // iInterface

    // Speaker Streaming Interface (Alternate 1 - with endpoint)
    0x09,                                    // bLength
    TUSB_DESC_INTERFACE,                     // bDescriptorType
    ITF_NUM_AUDIO_STREAMING_SPK,             // bInterfaceNumber
    0x01,                                    // bAlternateSetting
    0x01,                                    // bNumEndpoints
    TUSB_CLASS_AUDIO,                        // bInterfaceClass
    AUDIO_SUBCLASS_STREAMING,                // bInterfaceSubClass
    0x00,                                    // bInterfaceProtocol (UAC1)
    0x00,                                    // iInterface

    // Class-Specific AS Interface Descriptor
    0x07,                                    // bLength
    TUSB_DESC_CS_INTERFACE,                  // bDescriptorType
    AUDIO_CS_AS_INTERFACE_AS_GENERAL,        // bDescriptorSubtype
    0x01,                                    // bTerminalLink (Input Terminal 1)
    0x01,                                    // bDelay
    U16_TO_U8S_LE(AUDIO_DATA_FORMAT_TYPE_I_PCM), // wFormatTag

    // Type I Format Type Descriptor
    0x0B,                                    // bLength
    TUSB_DESC_CS_INTERFACE,                  // bDescriptorType
    AUDIO_CS_AS_INTERFACE_FORMAT_TYPE,       // bDescriptorSubtype
    0x01,                                    // bFormatType (Format Type I)
    0x01,                                    // bNrChannels (mono)
    0x02,                                    // bSubFrameSize (2 bytes)
    0x10,                                    // bBitResolution (16 bits)
    0x01,                                    // bSamFreqType (1 frequency)
    0x80, 0xBB, 0x00,                       // tSamFreq (48000 Hz in 3 bytes)

    // Standard AS Isochronous Audio Data Endpoint Descriptor
    0x09,                                    // bLength
    TUSB_DESC_ENDPOINT,                      // bDescriptorType
    EPNUM_AUDIO_OUT,                         // bEndpointAddress
    0x09,                                    // bmAttributes (Isochronous, Adaptive)
    U16_TO_U8S_LE(96),                       // wMaxPacketSize
    0x01,                                    // bInterval (1 ms)
    0x00,                                    // bRefresh
    0x00,                                    // bSynchAddress

    // Class-Specific AS Isochronous Audio Data Endpoint Descriptor
    0x07,                                    // bLength
    TUSB_DESC_CS_ENDPOINT,                   // bDescriptorType
    AUDIO_CS_EP_SUBTYPE_GENERAL,             // bDescriptorSubtype
    0x00,                                    // bmAttributes
    0x00,                                    // bLockDelayUnits
    U16_TO_U8S_LE(0x0000),                   // wLockDelay

    // Microphone Streaming Interface (Alternate 0 - no endpoints)
    0x09,                                    // bLength
    TUSB_DESC_INTERFACE,                     // bDescriptorType
    ITF_NUM_AUDIO_STREAMING_MIC,             // bInterfaceNumber
    0x00,                                    // bAlternateSetting
    0x00,                                    // bNumEndpoints
    TUSB_CLASS_AUDIO,                        // bInterfaceClass
    AUDIO_SUBCLASS_STREAMING,                // bInterfaceSubClass
    0x00,                                    // bInterfaceProtocol (UAC1)
    0x00,                                    // iInterface

    // Microphone Streaming Interface (Alternate 1 - with endpoint)
    0x09,                                    // bLength
    TUSB_DESC_INTERFACE,                     // bDescriptorType
    ITF_NUM_AUDIO_STREAMING_MIC,             // bInterfaceNumber
    0x01,                                    // bAlternateSetting
    0x01,                                    // bNumEndpoints
    TUSB_CLASS_AUDIO,                        // bInterfaceClass
    AUDIO_SUBCLASS_STREAMING,                // bInterfaceSubClass
    0x00,                                    // bInterfaceProtocol (UAC1)
    0x00,                                    // iInterface

    // Class-Specific AS Interface Descriptor
    0x07,                                    // bLength
    TUSB_DESC_CS_INTERFACE,                  // bDescriptorType
    AUDIO_CS_AS_INTERFACE_AS_GENERAL,        // bDescriptorSubtype
    0x05,                                    // bTerminalLink (Output Terminal 5)
    0x01,                                    // bDelay
    U16_TO_U8S_LE(AUDIO_DATA_FORMAT_TYPE_I_PCM), // wFormatTag

    // Type I Format Type Descriptor
    0x0B,                                    // bLength
    TUSB_DESC_CS_INTERFACE,                  // bDescriptorType
    AUDIO_CS_AS_INTERFACE_FORMAT_TYPE,       // bDescriptorSubtype
    0x01,                                    // bFormatType (Format Type I)
    0x01,                                    // bNrChannels (mono)
    0x02,                                    // bSubFrameSize (2 bytes)
    0x10,                                    // bBitResolution (16 bits)
    0x01,                                    // bSamFreqType (1 frequency)
    0x80, 0xBB, 0x00,                       // tSamFreq (48000 Hz in 3 bytes)

    // Standard AS Isochronous Audio Data Endpoint Descriptor
    0x09,                                    // bLength
    TUSB_DESC_ENDPOINT,                      // bDescriptorType
    EPNUM_AUDIO_IN,                          // bEndpointAddress
    0x05,                                    // bmAttributes (Isochronous, Asynchronous)
    U16_TO_U8S_LE(96),                       // wMaxPacketSize
    0x01,                                    // bInterval (1 ms)
    0x00,                                    // bRefresh
    0x00,                                    // bSynchAddress

    // Class-Specific AS Isochronous Audio Data Endpoint Descriptor
    0x07,                                    // bLength
    TUSB_DESC_CS_ENDPOINT,                   // bDescriptorType
    AUDIO_CS_EP_SUBTYPE_GENERAL,             // bDescriptorSubtype
    0x00,                                    // bmAttributes
    0x00,                                    // bLockDelayUnits
    U16_TO_U8S_LE(0x0000),                   // wLockDelay

    /* --- HID keyboard (interface 3) --- */
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0,
                       HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report),
                       EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 5),
};

#if TUD_OPT_HIGH_SPEED
// Per USB specs: high speed capable device must report device_qualifier and other_speed_configuration

// other speed configuration
uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
tusb_desc_device_qualifier_t const desc_device_qualifier = {
    .bLength = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB = USB_BCD,

    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
uint8_t const *tud_descriptor_device_qualifier_cb(void) {
    return (uint8_t const *)&desc_device_qualifier;
}

// Invoked when received GET OTHER SEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index) {
    (void)index; // for multiple configurations

    // other speed config is basically configuration with type = OHER_SPEED_CONFIG
    memcpy(desc_other_speed_config, desc_configuration, CONFIG_TOTAL_LEN);
    desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

    // this example use the same configuration for both high and full speed mode
    return desc_other_speed_config;
}

#endif // highspeed

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index; // for multiple configurations

    // This example use the same configuration for both high and full speed mode
    return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "Stavros",                  // 1: Manufacturer
    "Dialogue",         // 2: Product
    "123456",                   // 3: Serials, should use chip ID
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }

        const char *str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}
