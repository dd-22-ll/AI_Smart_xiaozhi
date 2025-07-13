#include "audio_encoder.h"

audio_encoder_t my_encoder;

esp_audio_enc_handle_t encoder = NULL;
void                   audio_encoder_init(void)
{
    // 编码器配置
    esp_opus_enc_config_t aac_cfg = {
        // 配置
        .sample_rate      = 16000,                               // 采样率
        .channel          = 1,                                   // 通道数
        .bits_per_sample  = 16,                                  // 采样位深度
        .frame_duration   = ESP_OPUS_ENC_FRAME_DURATION_60_MS,   // 帧时长
        .bitrate          = 32000,                               // 码率
        .application_mode = ESP_OPUS_ENC_APPLICATION_AUDIO,      // 应用模式
        .complexity       = 5,                                   // 复杂度: 0-10
        .enable_fec       = false,
        .enable_dtx       = false,   // 静音检测
        .enable_vbr       = true     // 可变波特兰
    };

    // 打开编码器
    esp_opus_enc_open(&aac_cfg, sizeof(esp_opus_enc_config_t), &encoder);
}

void encoder_task(void *);
void audio_encoder_start(RingbufHandle_t in, RingbufHandle_t out)
{
    my_encoder.is_running = true;
    assert(in != NULL);
    my_encoder.in_buffer = in;
    assert(out != NULL);
    my_encoder.out_buffer = out;

    xTaskCreatePinnedToCoreWithCaps(encoder_task,
                                    "encoder_task",
                                    64 * 1024,
                                    NULL,
                                    5,
                                    NULL,
                                    1,
                                    MALLOC_CAP_SPIRAM);
}

void audio_encoder_stop(void)
{
    my_encoder.is_running = false;
}

void copy_from_ringbuffer(RingbufHandle_t ringbuffer,
                          uint8_t        *to_buffer,
                          int             size)
{
    while(size > 0)
    {
        size_t   real_read_size = 0;
        uint8_t *data           = (uint8_t *)xRingbufferReceiveUpTo(ringbuffer, &real_read_size, portMAX_DELAY, size);
        memcpy(to_buffer, data, real_read_size);   // 拷贝数据
        vRingbufferReturnItem(ringbuffer, data);
        size -= real_read_size;        // 减去实际读取的长度, 剩余
        to_buffer += real_read_size;   // 移动指针
    }
}

// 编码任务: 从输入缓冲区读数据, 然后编码, 输出缓冲区写入数据
void encoder_task(void *args)
{
    // 获取 输入帧和输出帧的长度
    int in_size = 0, out_size = 0;
    esp_opus_enc_get_frame_size(encoder, &in_size, &out_size);

    // 给输入和输出分配内存
    uint8_t *in_data  = heap_caps_malloc(in_size, MALLOC_CAP_SPIRAM);   // 只缓冲一帧数据
    uint8_t *out_data = heap_caps_malloc(out_size, MALLOC_CAP_SPIRAM);

    // 输入帧和输出帧
    esp_audio_enc_in_frame_t in_frame = {
        .buffer = in_data,
        .len    = in_size,
    };
    esp_audio_enc_out_frame_t out_frame = {
        .buffer = out_data,
        .len    = out_size,
    };

    while(my_encoder.is_running)
    {
        // 读取 in_buf 环形缓冲区数据, copy 到in_data
        copy_from_ringbuffer(my_encoder.in_buffer, in_frame.buffer, in_frame.len);

        // 编码
        ESP_ERROR_CHECK(esp_opus_enc_process(encoder, &in_frame, &out_frame));

        // 读取编码后的数据后的数据, 存入到 out_buufer 环形缓冲区
        xRingbufferSend(my_encoder.out_buffer, out_frame.buffer, out_frame.encoded_bytes, portMAX_DELAY);
    }

    free(in_data);
    free(out_data);

    vTaskDelete(NULL);
}
