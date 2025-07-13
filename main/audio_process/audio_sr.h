#ifndef __AUDIO_SR_H
#define __AUDIO_SR_H
#include "Com_Debug.h"

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_aec.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "bsp_sound.h"

typedef struct
{
    bool        is_running;       // sr是否在运行
    bool        is_waken;         // 是否已经唤醒
    vad_state_t last_vad_state;   // 上一次的vad的状态

    RingbufHandle_t out_buffer;   // 输出环形缓冲区

    void (*wake_cb)(void);
    void (*vad_change_cb)(vad_state_t);

} audio_sr_t;

extern audio_sr_t my_sr;


void                       audio_sr_init(void);
void                       audio_sr_start(RingbufHandle_t out_buffer, void (*wake_cb)(void), void (*vad_change_cb)(vad_state_t));
void                       audio_sr_stop(void);
#endif
