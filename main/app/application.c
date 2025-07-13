#include "application.h"
#include "ota.h"
#include "audio_sr.h"
#include "bsp_sound.h"
#include "audio_encoder.h"
#include "audio_decoder.h"
#include "ws.h"
#include "display.h"

#define RECV_HELLO_BIT (1 << 0)

typedef enum
{
    APP_IDLE = 0,
    APP_CONNECTING,
    APP_SPEAKING,
    APP_LISTENING,
} app_state_t;

typedef struct
{
    RingbufHandle_t sr_2_encoder_buffer;   // 4=>four=>for log4j
    RingbufHandle_t encoder_2_ws_buffer;
    RingbufHandle_t ws_2_decoder_buffer;

    app_state_t state;

    EventGroupHandle_t event_group;
} app_t;

static app_t my_app;

void wifi_succ(void);
void wifi_fail(void);
void application_create_buffer(void);

void wake_cb(void);
void vad_change_cb(vad_state_t state);

void recv_txt_cb(char *data, int len);
void recv_bin_cb(char *data, int len);
void ws_finish_cb(void);
void application_set_app_state(app_state_t state);

void audio_upload_task(void *args);
void application_start(void)
{
    my_app.event_group = xEventGroupCreate();

    // 0 初始化显示模块
    display_init();

    /* 1. 初始化wifi, 并启动 */
    bsp_wifi_Init();
    bsp_wifi_Start(wifi_succ, wifi_fail);
}

void test(void *args)
{
    while(1)
    {
        size_t len  = 0;
        void  *data = xRingbufferReceive(my_app.encoder_2_ws_buffer, &len, portMAX_DELAY);

        xRingbufferSend(my_app.ws_2_decoder_buffer, data, len, portMAX_DELAY);

        vRingbufferReturnItem(my_app.encoder_2_ws_buffer, data);
        vTaskDelay(1);
    }
}

void wifi_succ(void)
{
    display_del_qrcode();
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

    // 4. 初始化编码器和启动编码器
    audio_encoder_init();
    audio_encoder_start(my_app.sr_2_encoder_buffer, my_app.encoder_2_ws_buffer);

    // 5. 初始化解码器和启动解码器
    audio_decoder_init();
    audio_decoder_start(my_app.ws_2_decoder_buffer);

    // 6. 初始化 ws
    ws_init(recv_txt_cb, recv_bin_cb, ws_finish_cb);

    // 7. 创建一个任务: 上传语音数据
    xTaskCreateWithCaps(audio_upload_task, "audio_upload_task", 4 * 1024, NULL, 5, NULL, MALLOC_CAP_SPIRAM);

    display_show_tts("说: \"你好,小智\"唤醒设备,开始语音对话");
    // 创建测试编码和解码的任务
    // xTaskCreate(test, "test", 2 * 1024, NULL, 5, NULL);
}

void wifi_fail(void)
{
    MY_LOGI("wifi 连接失败");
}

void application_create_buffer(void)
{
    // 缓冲区类型未来要改: 必须使用bytebuffer, 因为读取的时候,要读取固定大小字节的数据
    my_app.sr_2_encoder_buffer = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
    my_app.encoder_2_ws_buffer = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    my_app.ws_2_decoder_buffer = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
}

void wake_cb(void)
{
    MY_LOGI("唤醒成功...");

    switch(my_app.state)
    {
        case APP_IDLE:
        {
            if(!my_ws.is_connected)
            {
                // 建立连接
                my_app.state = APP_CONNECTING;
                MY_LOGI("正在建立连接...");
                ws_open_audio_channel();
                MY_LOGI("连接建立成功...");
                // 发送hello
                ws_send_hello();
                // 等待hello响应
                xEventGroupWaitBits(my_app.event_group, RECV_HELLO_BIT, true, true, portMAX_DELAY);
            }
            break;
        }

        case APP_SPEAKING:
        {
            // 放弃说话
            ws_send_abort();
            break;
        }

        default:
            break;
    }
    // 发送唤醒词
    application_set_app_state(APP_IDLE);
    ws_send_wake();
}
void vad_change_cb(vad_state_t state)
{
    // MY_LOGI("人声: %s", state == VAD_SPEECH ? "说话" : "静音");
    if(state == VAD_SPEECH && my_app.state != APP_SPEAKING)
    {
        application_set_app_state(APP_LISTENING);
        ws_send_start_listen();
    }
    else if(state == VAD_SILENCE && my_app.state == APP_LISTENING)
    {
        application_set_app_state(APP_IDLE);
        ws_send_stop_listen();
    }
}

void recv_txt_cb(char *data, int len)
{
    // MY_LOGI("收到文本数据: %s", data);
    cJSON *root = cJSON_ParseWithLength(data, len);
    if(root == NULL)
    {
        MY_LOGE("json数据解析失败");
        return;
    }

    cJSON *typeItem = cJSON_GetObjectItem(root, "type");
    if(!cJSON_IsString(typeItem))
    {
        MY_LOGE("收到未知数据: %s", data);
        return;
    }
    MY_LOGI("文本信息: %.*s", len, data);
    char *type = typeItem->valuestring;

    if(strcmp(type, "hello") == 0)
    {
        // 设置收到hello响应的标志位
        xEventGroupSetBits(my_app.event_group, RECV_HELLO_BIT);
        ws_send_iot_descriptor();
        ws_send_iot_status();
    }
    else if(strcmp(type, "tts") == 0)
    {
        char *state = cJSON_GetObjectItem(root, "state")->valuestring;
        if(strcmp(state, "sentence_start") == 0)
        {
            application_set_app_state(APP_SPEAKING);

            cJSON *textItem = cJSON_GetObjectItem(root, "text");
            if(textItem)
            {
                char *ttl = textItem->valuestring;
                display_show_tts(ttl);
            }
        }
        else if(strcmp(state, "sentence_end") == 0)
        {
            application_set_app_state(APP_IDLE);
        }
    }
    else if(strcmp(type, "iot") == 0)
    {
        cJSON *cmdsItem       = cJSON_GetObjectItem(root, "commands");
        cJSON *cmdItem        = cJSON_GetArrayItem(cmdsItem, 0);
        cJSON *parametersItem = cJSON_GetObjectItem(cmdItem, "parameters");

        char *method = cJSON_GetObjectItem(cmdItem, "method")->valuestring;

        if(strcmp(method, "SetVolume") == 0)
        {
            int volumne = cJSON_GetObjectItem(parametersItem, "volume")->valueint;

            bsp_sound_set_volume(volumne);
        }
        else if(strcmp(method, "SetMute") == 0)
        {
            cJSON *muteItem = cJSON_GetObjectItem(parametersItem, "mute");
            bool   is_mute  = false;
            if(cJSON_IsBool(muteItem))
            {
                is_mute = cJSON_IsTrue(muteItem);
            }
            bsp_sound_set_mute(is_mute);
        }
    }
    else if(strcmp(type, "llm") == 0)
    {
        cJSON *emotionItem = cJSON_GetObjectItem(root, "emotion");
        if(emotionItem)
        {
            char *emoji = emotionItem->valuestring;
            display_show_emoji(emoji);
        }
    }

    cJSON_Delete(root);
}

void recv_bin_cb(char *data, int len)
{
    // MY_LOGI("收到二进制数据: %s", data);
    audio_decoder_write((uint8_t *)data, len);
}

void ws_finish_cb(void)
{
    MY_LOGI("websocket连接已关闭");
    my_sr.is_waken = false;   // 连接关闭后, 把sr的唤醒状态关闭
}

void application_set_app_state(app_state_t state)
{
    if(state != my_app.state)
    {
        my_app.state = state;
    }
}

void audio_upload_task(void *args)
{
    MY_LOGI("开始上传音频数据");

    while(1)
    {
        size_t   len  = 0;
        uint8_t *data = (uint8_t *)xRingbufferReceive(my_app.encoder_2_ws_buffer, &len, portMAX_DELAY);
        if(my_app.state == APP_LISTENING && data && len > 0)
        {
            // MY_LOGI("上传音频数据 %d", len);
            ws_send_bin(data, len);
        }

        vRingbufferReturnItem(my_app.encoder_2_ws_buffer, data);
    }
}
