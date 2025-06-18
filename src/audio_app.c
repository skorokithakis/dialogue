#include <stdio.h>
#include <string.h>
#include <math.h>      // powf()
#include <limits.h>    // INT16_MIN
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "i2s_dac.h"

extern uint32_t blink_interval_ms;     // provided by main.cpp

/* must match enum value in main.cpp */
#define BLINK_MOUNTED  1000u

/* Sample rates */
const uint32_t sample_rates[] = {44100, 48000};
uint32_t current_sample_rate  = 44100;
#define N_SAMPLE_RATES  TU_ARRAY_SIZE(sample_rates)

/* Audio controls: mute and volume states */
int8_t  mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1];
int16_t volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1];

/* Buffers for microphone and speaker data */
int32_t mic_buf[CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ  / 4];
int32_t spk_buf[CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ / 4];
/* NEW – local copy used by audio_task() to decouple USB IRQ and DMA */
static int32_t spk_copy_buf[CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ / 4];
volatile int     spk_data_size;

/* previous gains – used to make gain changes pop-free */
static float prev_gainL = 1.0f;
static float prev_gainR = 1.0f;
/* Resolutions per format */
const uint8_t resolutions_per_format[CFG_TUD_AUDIO_FUNC_1_N_FORMATS] = {
    CFG_TUD_AUDIO_FUNC_1_FORMAT_2_RESOLUTION_RX   // only 24/32-bit left
};
uint8_t current_resolution;

/* Volume control constants */
enum {
    VOLUME_CTRL_0_DB    = 0,
    VOLUME_CTRL_10_DB   = 2560,
    VOLUME_CTRL_20_DB   = 5120,
    VOLUME_CTRL_30_DB   = 7680,
    VOLUME_CTRL_40_DB   = 10240,
    VOLUME_CTRL_50_DB   = 12800,
    VOLUME_CTRL_60_DB   = 15360,
    VOLUME_CTRL_70_DB   = 17920,
    VOLUME_CTRL_80_DB   = 20480,
    VOLUME_CTRL_90_DB   = 23040,
    VOLUME_CTRL_100_DB  = 25600,
    VOLUME_CTRL_SILENCE = 0x8000,
};

/* Forward declarations of callbacks */
static bool tud_audio_clock_get_request(uint8_t rhport, audio_control_request_t const *request);
static bool tud_audio_clock_set_request(uint8_t rhport, audio_control_request_t const *request, uint8_t const *buf);
static bool tud_audio_feature_unit_get_request(uint8_t rhport, audio_control_request_t const *request);
static bool tud_audio_feature_unit_set_request(uint8_t rhport, audio_control_request_t const *request, uint8_t const *buf);

// Convert USB volume value (1/256 dB units, negative = attenuation) to linear gain
static inline float db256_to_gain(int16_t db256) {
    return powf(10.0f, (float)db256 / (20.0f * 256.0f));
}

/* Clock get request handler */
static bool tud_audio_clock_get_request(uint8_t rhport, audio_control_request_t const *request) {
    TU_ASSERT(request->bEntityID == UAC2_ENTITY_CLOCK);
    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
        if (request->bRequest == AUDIO_CS_REQ_CUR) {
            audio_control_cur_4_t curf = { (int32_t) tu_htole32(current_sample_rate) };
            return tud_audio_buffer_and_schedule_control_xfer(rhport,
                    (tusb_control_request_t const *)request, &curf, sizeof(curf));
        } else if (request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_4_n_t(N_SAMPLE_RATES) rangef = {
                .wNumSubRanges = tu_htole16(N_SAMPLE_RATES)
            };
            for (uint8_t i = 0; i < N_SAMPLE_RATES; i++) {
                rangef.subrange[i].bMin = (int32_t) sample_rates[i];
                rangef.subrange[i].bMax = (int32_t) sample_rates[i];
                rangef.subrange[i].bRes = 0;
            }
            return tud_audio_buffer_and_schedule_control_xfer(rhport,
                    (tusb_control_request_t const *)request, &rangef, sizeof(rangef));
        }
    } else if (request->bControlSelector == AUDIO_CS_CTRL_CLK_VALID &&
               request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_1_t cur_valid = { .bCur = 1 };
        return tud_audio_buffer_and_schedule_control_xfer(rhport,
                (tusb_control_request_t const *)request, &cur_valid, sizeof(cur_valid));
    }
    return false;
}

/* Clock set request handler */
static bool tud_audio_clock_set_request(uint8_t rhport, audio_control_request_t const *request, uint8_t const *buf) {
    TU_ASSERT(request->bEntityID == UAC2_ENTITY_CLOCK);
    TU_VERIFY(request->bRequest == AUDIO_CS_REQ_CUR);
    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
        current_sample_rate = (uint32_t)((audio_control_cur_4_t const *)buf)->bCur;
        i2s_dac_init(current_sample_rate);   // re-configure I²S clocks
        return true;
    }
    return false;
}

/* Feature unit get request handler */
static bool tud_audio_feature_unit_get_request(uint8_t rhport, audio_control_request_t const *request) {
    TU_ASSERT(request->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT);
    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_1_t mute1 = { .bCur = mute[request->bChannelNumber] };
        return tud_audio_buffer_and_schedule_control_xfer(rhport,
                (tusb_control_request_t const *)request, &mute1, sizeof(mute1));
    } else if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME) {
        if (request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_2_n_t(1) range_vol = {
                .wNumSubRanges = tu_htole16(1),
                .subrange[0] = {
                    .bMin = tu_htole16(-VOLUME_CTRL_50_DB),
                    .bMax = tu_htole16(VOLUME_CTRL_0_DB),
                    .bRes = tu_htole16(256)
                }
            };
            return tud_audio_buffer_and_schedule_control_xfer(rhport,
                    (tusb_control_request_t const *)request, &range_vol, sizeof(range_vol));
        } else if (request->bRequest == AUDIO_CS_REQ_CUR) {
            audio_control_cur_2_t cur_vol = { .bCur = tu_htole16(volume[request->bChannelNumber]) };
            return tud_audio_buffer_and_schedule_control_xfer(rhport,
                    (tusb_control_request_t const *)request, &cur_vol, sizeof(cur_vol));
        }
    }
    return false;
}

/* Feature unit set request handler */
static bool tud_audio_feature_unit_set_request(uint8_t rhport, audio_control_request_t const *request, uint8_t const *buf) {
    TU_ASSERT(request->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT);
    TU_VERIFY(request->bRequest == AUDIO_CS_REQ_CUR);
    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE) {
        mute[request->bChannelNumber] = ((audio_control_cur_1_t const *)buf)->bCur;
        return true;
    } else if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME) {
        volume[request->bChannelNumber] = ((audio_control_cur_2_t const *)buf)->bCur;
        return true;
    }
    return false;
}

/* Application callback: GET request */
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    audio_control_request_t const *request = (audio_control_request_t const *)p_request;
    if (request->bEntityID == UAC2_ENTITY_CLOCK) {
        return tud_audio_clock_get_request(rhport, request);
    }
    if (request->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT) {
        return tud_audio_feature_unit_get_request(rhport, request);
    }
    return false;
}

/* Application callback: SET request */
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf) {
    audio_control_request_t const *request = (audio_control_request_t const *)p_request;
    if (request->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT) {
        return tud_audio_feature_unit_set_request(rhport, request, buf);
    }
    if (request->bEntityID == UAC2_ENTITY_CLOCK) {
        return tud_audio_clock_set_request(rhport, request, buf);
    }
    return false;
}

/* Interface-close endpoint callback */
bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;
    uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex));
    uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));
    if (ITF_NUM_AUDIO_STREAMING_SPK == itf && alt == 0) {
        blink_interval_ms = BLINK_MOUNTED;
    }
    return true;
}

/* Interface set callback */
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;
    uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex));
    uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));
    if (ITF_NUM_AUDIO_STREAMING_SPK == itf && alt != 0) {
        blink_interval_ms = 25;          // existing line

        /* start / re-start I²S when the host enables streaming */
        static bool i2s_started = false;
        if (!i2s_started) {
            i2s_dac_init(current_sample_rate);
            i2s_started = true;
        }
    }
    spk_data_size = 0;
    if (alt != 0) {
        current_resolution = resolutions_per_format[alt - 1];
    }
    return true;
}

/* RX done pre-read callback */
bool tud_audio_rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id,
                                   uint8_t ep_out, uint8_t cur_alt_setting) {
    (void)rhport;
    (void)func_id;
    (void)ep_out;
    (void)cur_alt_setting;
    spk_data_size = tud_audio_read(spk_buf, n_bytes_received);
    return true;
}

/* TX done pre-load callback */
bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf,
                                   uint8_t ep_in, uint8_t cur_alt_setting) {
    (void)rhport;
    (void)itf;
    (void)ep_in;
    (void)cur_alt_setting;
    return true;
}

/* Audio processing task */
void audio_task(void) {
    uint32_t bytes = spk_data_size;
    if (bytes) {
        memcpy(spk_copy_buf, spk_buf, bytes);   // grab packet
        spk_data_size = 0;                      // free USB buffer

        uint32_t n_words = bytes / 4;           // 32-bit samples
        /* --- combine master + per-channel attenuation (dB values add) --- */
        int32_t dbL = (mute[0] || mute[1]) ? INT16_MIN : (int32_t)volume[0] + volume[1];
        int32_t dbR = (mute[0] || mute[2]) ? INT16_MIN : (int32_t)volume[0] + volume[2];

        /* safeguard: clamp to representable range (≤ –128 dB) */
        if (dbL < -32768) {
            dbL = -32768;
        }
        if (dbR < -32768) {
            dbR = -32768;
        }

        float gainL = (dbL == INT16_MIN) ? 0.0f : db256_to_gain((int16_t)dbL);
        float gainR = (dbR == INT16_MIN) ? 0.0f : db256_to_gain((int16_t)dbR);

        // Removed –6 dB head-room attenuation

        /* ----------- smooth gain change to eliminate pops ------------- */
        uint32_t n_frames = n_words / 2u;
        float    stepL    = (gainL - prev_gainL) / (float)n_frames;
        float    stepR    = (gainR - prev_gainR) / (float)n_frames;
        float    gL       = prev_gainL;
        float    gR       = prev_gainR;

        for (uint32_t i = 0; i < n_words; i += 2)
        {
            /* sign-extend real 24-bit sample that is already left-justified */
            int32_t rawL = spk_copy_buf[i]     >> 8;
            int32_t rawR = spk_copy_buf[i + 1] >> 8;

            /* apply smoothly changing gain */
            int32_t sL = (int32_t)((float)rawL * gL);
            int32_t sR = (int32_t)((float)rawR * gR);

            /* clip to ±0x7FFFFF (24-bit full scale) */
            if (sL >  0x7FFFFF) sL =  0x7FFFFF;
            if (sL < -0x7FFFFF) sL = -0x7FFFFF;
            if (sR >  0x7FFFFF) sR =  0x7FFFFF;
            if (sR < -0x7FFFFF) sR = -0x7FFFFF;

            /* left-justify 24-bit sample for the I²S transmitter */
            spk_copy_buf[i]     = sL << 8;
            spk_copy_buf[i + 1] = sR << 8;

            /* advance gain for next frame */
            gL += stepL;
            gR += stepR;
        }

        prev_gainL = gainL;
        prev_gainR = gainR;

        /* push to DAC */
        i2s_dac_write(spk_copy_buf, n_words);
    }
}

/* Audio control task */
void audio_control_task(void) {
    const uint32_t interval_ms = 50;
    static uint32_t start_ms = 0;
    static uint32_t btn_prev = 0;
    if (board_millis() - start_ms < interval_ms) {
        return;
    }
    start_ms += interval_ms;
    uint32_t btn = board_button_read();
    if (!btn_prev && btn) {
        for (int i = 0; i < CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1; i++) {
            volume[i] = (volume[i] == 0) ? -VOLUME_CTRL_30_DB : 0;
        }
        const audio_interrupt_data_t data = {
            .bInfo            = 0,
            .bAttribute       = AUDIO_CS_REQ_CUR,
            .wValue_cn_or_mcn = 0,
            .wValue_cs        = AUDIO_FU_CTRL_VOLUME,
            .wIndex_ep_or_int = 0,
            .wIndex_entity_id = UAC2_ENTITY_SPK_FEATURE_UNIT,
        };
        tud_audio_int_write(&data);
    }
    btn_prev = btn;
}
