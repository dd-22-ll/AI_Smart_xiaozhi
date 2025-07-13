#ifndef __BSP_SOUND_H
#define __BSP_SOUND_H
#include "Com_Debug.h"
#include "driver/i2c_master.h"

#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

void bsp_sound_Init(void);
int  bsp_sound_WriteData(uint8_t *data, uint32_t len);
int  bsp_sound_ReadData(uint8_t data[], uint32_t len);
void bsp_sound_set_volume(int volume);
void bsp_sound_set_mute(bool mute);
#endif
