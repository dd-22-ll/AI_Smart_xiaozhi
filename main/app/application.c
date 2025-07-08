#include "application.h"
#include "ota.h"
#include "audio_sr.h"
#include "bsp_sound.h"

typedef struct
{
    RingbufHandle_t sr_2_encoder_buffer;   // 4=>four=>for log4j
} app_t;

static app_t my_app;

void wifi_succ(void);
void wifi_fail(void);
void application_create_buffer(void);

void wake_cb(void);
void vad_change_cb(vad_state_t state);
void application_start(void)
{
    /* 1. 初始化wifi, 并启动 */
    bsp_wifi_Init();
    bsp_wifi_Start(wifi_succ, wifi_fail);
}

void wifi_succ(void)
{
    MY_LOGI("wifi 连接成功");
    // 1. ota检测
    ota_check_version();

    // 2. 启动es8311 音频设备
    bsp_sound_Init();

    // 3. 创建需要的环形缓冲区
    application_create_buffer();

    // 3. 初始化语音识别 和启动
    audio_sr_init();
    audio_sr_start(my_app.sr_2_encoder_buffer, wake_cb, vad_change_cb);
}

void wifi_fail(void)
{
    MY_LOGI("wifi 连接失败");
}

void application_create_buffer(void)
{
    // 缓冲区类型未来要改
    my_app.sr_2_encoder_buffer = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
}

void wake_cb(void)
{
    MY_LOGI("唤醒成功...");
}
void vad_change_cb(vad_state_t state)
{
    MY_LOGI("人声: %s", state == VAD_SPEECH ? "说话" : "静音");
}
