#ifndef __BSP_LCD_H
#define __BSP_LCD_H
#include "Com_Debug.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

typedef struct
{
    esp_lcd_panel_handle_t    panel_handle;
    esp_lcd_panel_io_handle_t io_handle;
} bsp_lcd_t;

extern bsp_lcd_t my_lcd;
void bsp_lcd_init(void);

#endif
