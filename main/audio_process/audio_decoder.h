#ifndef __AUDIO_DECODER_H
#define __AUDIO_DECODER_H
#include "Com_Debug.h"
#include "esp_audio_dec.h"
#include "esp_opus_dec.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "bsp_sound.h"

typedef struct
{
    bool            is_running;
    RingbufHandle_t in_buffer;
} audio_decoder_t;

void audio_decoder_init(void);

void audio_decoder_start(RingbufHandle_t in_buffer);
void audio_decoder_stop(void);

void audio_decoder_write(uint8_t *audio, int len);

#endif
