#include "audio_sr.h"
#include "assert.h"

audio_sr_t my_sr;

static esp_afe_sr_iface_t *afe_handle;
esp_afe_sr_data_t         *afe_data;
void                       audio_sr_init(void)
{
    // 初始 sr 模型
    srmodel_list_t *models = esp_srmodel_init("model");
    // afe 配置初始化 参数1:输入类型 M 表示一个mic 参数2: 前端类型 语音识别
    afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    afe_config->aec_init     = false;   // 禁用回声消除
    afe_config->se_init      = false;   // 语音增强
    afe_config->ns_init      = false;   // 噪音消除

    afe_config->vad_init         = true;         // vad 使能
    afe_config->vad_mode         = VAD_MODE_2;   // vad 模式 数字越小对声音越敏感
    afe_config->vad_min_noise_ms = 500;          // 噪声检测阈值

    afe_config->wakenet_init      = true;
    afe_config->wakenet_mode      = DET_MODE_90;                   // 唤醒模式 值越大越容易识别到唤醒词, 但是更易误唤醒
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;   // 内存分配模式: 尽量分配到外部

    // 获取前端获取句柄
    afe_handle = esp_afe_handle_from_config(afe_config);
    // 创建实例: 通过这个实例进行数据的读写
    afe_data = afe_handle->create_from_config(afe_config);
}

void feed_task(void *arg);
void fetch_task(void *arg);
void audio_sr_start(RingbufHandle_t out_buffer,
                    void (*wake_cb)(void),
                    void (*vad_change_cb)(vad_state_t))
{
    assert(out_buffer);

    my_sr.out_buffer = out_buffer;

    my_sr.is_running    = true;
    my_sr.wake_cb       = wake_cb;
    my_sr.vad_change_cb = vad_change_cb;

    xTaskCreateWithCaps(feed_task, "feed_task", 3 * 1024, NULL, 5, NULL, MALLOC_CAP_SPIRAM);
    xTaskCreateWithCaps(fetch_task, "fetch_task", 3 * 1024, NULL, 5, NULL, MALLOC_CAP_SPIRAM);
}

void audio_sr_stop(void)
{
    my_sr.is_running = false;
}

// 从esp8311读取数据, 交给模型处理
void feed_task(void *arg)
{

    // 获取将来需要给前端音频片段的大小
    int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
    // 通道数据
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);
    // 存储音频片段数据的缓冲区的大小 把内存指定到外部内存
    size_t   feed_size = feed_chunksize * feed_nch * sizeof(int16_t);
    int16_t *feed_buff = (int16_t *)heap_caps_malloc(feed_size, MALLOC_CAP_SPIRAM);

    while(my_sr.is_running)
    {
        // 从es8311读取数据
        bsp_sound_ReadData((uint8_t *)feed_buff, feed_size);

        // feed_buff 里面的数据 喂给sr模型,开始进行识别处理
        afe_handle->feed(afe_data, feed_buff);
    }

    free(feed_buff);
    vTaskDelete(NULL);
}

// 把模型处理后的数据, 存入到环形缓冲区
void fetch_task(void *arg)
{

    while(my_sr.is_running)
    {
        // 获取模式识别后的结果数据  包含识别后的语音和唤醒状态 vad状态
        afe_fetch_result_t *result          = afe_handle->fetch(afe_data);
        int16_t            *processed_audio = result->data;           // 别后的语音语音数据
        vad_state_t         vad_state       = result->vad_state;      // vad状态
        wakenet_state_t     wakeup_state    = result->wakeup_state;   // 唤醒状态

        // 已经唤醒
        if(wakeup_state == WAKENET_DETECTED)
        {
            my_sr.is_waken       = true;          // 已经唤醒
            my_sr.last_vad_state = VAD_SILENCE;   // 唤醒之后, vad的状态应该是silence, 否则会丢语音
            // 调用唤醒后的回调函数
            if(my_sr.wake_cb)
            {
                my_sr.wake_cb();
            }
        }

        if(my_sr.is_waken)
        {
            // 当时唤醒状态的时候,再处理vad
            if(my_sr.last_vad_state != vad_state)
            {
                // vad的状态发送变化
                my_sr.last_vad_state = vad_state;
                if(my_sr.vad_change_cb)
                {
                    my_sr.vad_change_cb(my_sr.last_vad_state);
                }
            }
        }

        // 如果现在是唤醒状态, 并且有人说话,就把音频数据存入到环形缓冲区
        if(my_sr.is_waken && vad_state == VAD_SPEECH)
        {
            // if vad cache is exists, please attach the cache to the front of processed_audio to avoid data loss
            if(result->vad_cache_size > 0)
            {
                int16_t *vad_cache = result->vad_cache;
                if(my_sr.out_buffer)
                {
                    xRingbufferSend(my_sr.out_buffer, vad_cache, result->vad_cache_size, 0);
                }
            }

            if(my_sr.out_buffer)
            {
                xRingbufferSend(my_sr.out_buffer, processed_audio, result->data_size, 0);
            }
        }
    }
    vTaskDelete(NULL);
}
