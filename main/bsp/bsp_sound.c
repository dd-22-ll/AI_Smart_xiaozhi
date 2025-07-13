#include "bsp_sound.h"

#define SCL_PIN 1
#define SDA_PIN 0

#define I2S_MCK_PIN 3
#define I2S_BCK_PIN 2
#define I2S_DATA_WS_PIN 5
#define I2S_DATA_OUT_PIN 6
#define I2S_DATA_IN_PIN 4

static i2c_master_bus_handle_t i2c_bus_handle;

static i2s_chan_handle_t speakHandle;
static i2s_chan_handle_t micHandle;

static esp_codec_dev_handle_t codec_dev;

static int s_volume = 60;
void       bsp_sound_I2C_Init(void)
{
    i2c_master_bus_config_t i2c_bus_config      = {0};
    i2c_bus_config.clk_source                   = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.i2c_port                     = I2C_NUM_0;
    i2c_bus_config.scl_io_num                   = SCL_PIN;
    i2c_bus_config.sda_io_num                   = SDA_PIN;
    i2c_bus_config.glitch_ignore_cnt            = 7;
    i2c_bus_config.flags.enable_internal_pullup = true;
    i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
}

void bsp_sound_I2S_Init(void)
{
    /* i2s通道配置 */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_std_config_t  std_cfg  = {
          .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),                           /* 时钟配置: 参数是采样率 16K  */
          .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_MONO), /* 采样深度: 16位  单声道*/
          .gpio_cfg = {
              .mclk = I2S_MCK_PIN,
              .bclk = I2S_BCK_PIN,
              .ws   = I2S_DATA_WS_PIN,
              .dout = I2S_DATA_OUT_PIN,
              .din  = I2S_DATA_IN_PIN,
        },
    };

    /* 创建通道 */
    i2s_new_channel(&chan_cfg, &speakHandle, &micHandle);

    /* 初始化通道模式 */
    i2s_channel_init_std_mode(speakHandle, &std_cfg);
    i2s_channel_init_std_mode(micHandle, &std_cfg);

    /* 通道使能 */
    i2s_channel_enable(speakHandle);
    i2s_channel_enable(micHandle);
}

void bsp_sound_ES8311_Init(void)
{
    /* 1. 创建配置接口 */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .rx_handle = micHandle,
        .tx_handle = speakHandle,
    };
    /* 2. 创建i2s数据接口 */
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    /* 3. 创建控制接口 */
    audio_codec_i2c_cfg_t i2c_cfg = {
        .addr       = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus_handle};
    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

    /* 4. 创建 gpio接口 */
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    /* 5. 创建 codec 接口 */
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode  = ESP_CODEC_DEV_WORK_MODE_BOTH, /* 支持播放和录音 */
        .ctrl_if     = out_ctrl_if,                  /* 控制即可 */
        .gpio_if     = gpio_if,                      /* gpio 接口*/
        .pa_pin      = 7,                            /* pa引脚: 控制功放开关 */
        .use_mclk    = true,                         /* 是否使用主时钟 */
        .digital_mic = false                         /* 是否使用数字麦克风 */
    };
    const audio_codec_if_t *out_codec_if = es8311_codec_new(&es8311_cfg);

    /* 6. 创建设备: 音频设备 */
    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = out_codec_if,               // codec interface from es8311_codec_new
        .data_if  = data_if,                    // data interface from audio_codec_new_i2s_data
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT   // codec support both playback and record
    };
    codec_dev = esp_codec_dev_new(&dev_cfg);

    /* 其他配置 */
    /* 播放音量 */
    esp_codec_dev_set_out_vol(codec_dev, 60.0);
    /* 麦克风增益 */
    esp_codec_dev_set_in_gain(codec_dev, 20.0);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate     = 16000,                              /* 要播放的音频的采样率 16000*/
        .channel         = 1,                                  /* 要播放的音频的通道数 */
        .bits_per_sample = 16,                                 /* 采样深度 16位 */
        .channel_mask    = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), /* 通道掩码 */
    };
    esp_codec_dev_open(codec_dev, &fs);
}

void bsp_sound_Init(void)
{
    /* 1. i2c的初始化 */
    bsp_sound_I2C_Init();

    /* 2. i2s的初始化 */
    bsp_sound_I2S_Init();

    /* 3. es8311的初始化 */
    bsp_sound_ES8311_Init();
}

/**
 * @description: 播放 给es8311发送音频数据
 * @param {uint8_t} *data 具体的音频数据
 * @param {uint32_t} len 音频数据的长度
 * @return {*}
 */
int bsp_sound_WriteData(uint8_t *data, uint32_t len)
{
    if(codec_dev && data && len)
    {
        return esp_codec_dev_write(codec_dev, data, len);
    }

    return ESP_CODEC_DEV_WRITE_FAIL;
}

/**
 * @description: 录音: 读取mic的数据
 * @param {uint8_t} data
 * @param {uint32_t} len
 * @return {*}
 */
int bsp_sound_ReadData(uint8_t data[], uint32_t len)
{
    if(codec_dev && data && len)
    {
        return esp_codec_dev_read(codec_dev, data, len);
    }
    return ESP_CODEC_DEV_READ_FAIL;
}

void bsp_sound_set_volume(int volume)
{
    s_volume = volume;
    esp_codec_dev_set_out_vol(codec_dev, volume);
}

void bsp_sound_set_mute(bool mute)
{
    esp_codec_dev_set_out_vol(codec_dev, mute == true ? 0 : s_volume);
}
