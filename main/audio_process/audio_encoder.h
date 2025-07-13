#ifndef __AUDIO_ENCODER_H
#define __AUDIO_ENCODER_H
#include "Com_Debug.h"
#include "esp_audio_enc.h"
#include "esp_opus_enc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

typedef struct
{
    bool is_running;

    RingbufHandle_t in_buffer;
    RingbufHandle_t out_buffer;

} audio_encoder_t;

void                   audio_encoder_init(void);

void audio_encoder_start(RingbufHandle_t in, RingbufHandle_t out);

void audio_encoder_stop(void);

#endif
