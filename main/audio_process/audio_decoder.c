#include "audio_decoder.h"

audio_decoder_t my_decoder;

esp_audio_dec_handle_t dec_handle;

void decoder_task(void *args);
void audio_decoder_init(void)
{
    esp_opus_dec_cfg_t opus_cfg = {
        .sample_rate    = 16000,
        .channel        = 1,
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_INVALID,   // invalid : 解码器会自动识别每帧长度
        .self_delimited = false                                  // 自描述分隔符: 如果是标志的opus流,则不需要
    };

    // 创建解码器
    ESP_ERROR_CHECK(esp_opus_dec_open(&opus_cfg, sizeof(opus_cfg), &dec_handle));
}

void audio_decoder_start(RingbufHandle_t in_buffer)
{
    assert(dec_handle);
    my_decoder.is_running = true;

    my_decoder.in_buffer = in_buffer;

    xTaskCreatePinnedToCoreWithCaps(decoder_task,
                                    "decoder_task",
                                    32 * 1024,
                                    NULL,
                                    5,
                                    NULL,
                                    0,
                                    MALLOC_CAP_SPIRAM);
}

void decoder_task(void *args)
{
    // 从输入环形缓冲区读取数据, 给输入帧: 这里面的数据可能不止一帧
    esp_audio_dec_in_raw_t in_frame;

    // 输出帧: 解码之后数据存储到out_frame->buffer,   decoded_size: 解码后数据的长度
    esp_audio_dec_out_frame_t out_frame;
    out_frame.buffer = heap_caps_malloc(8 * 1024, MALLOC_CAP_SPIRAM);
    out_frame.len    = 8 * 1024;

    esp_audio_dec_info_t info;
    while(my_decoder.is_running)
    {
        in_frame.buffer = (uint8_t *)xRingbufferReceive(my_decoder.in_buffer, &in_frame.len, portMAX_DELAY);

        void *raw_buffer = in_frame.buffer;   // 存储最初指针, 用来释放内存
        while(in_frame.len)
        {
            // 一次只能解码一帧, 来的可能会有多帧数据, 需要循环解码
            esp_opus_dec_decode(dec_handle, &in_frame, &out_frame, &info);

            in_frame.len -= in_frame.consumed;      // 待解码的长度
            in_frame.buffer += in_frame.consumed;   // 待解码的指针移动

            // 播放解码后的数据
            bsp_sound_WriteData(out_frame.buffer, out_frame.decoded_size);
        }

        // 循环缓冲区数据用完: 释放
        vRingbufferReturnItem(my_decoder.in_buffer, raw_buffer);
    }

    free(out_frame.buffer);
    vTaskDelete(NULL);
}

void audio_decoder_stop(void)
{
    my_decoder.is_running = false;
}

void audio_decoder_write(uint8_t *audio, int len)
{
    // 向输入环形缓冲区写入数据
    if(my_decoder.in_buffer)
    {
        xRingbufferSend(my_decoder.in_buffer, audio, (size_t)len, 0);
    }
}
