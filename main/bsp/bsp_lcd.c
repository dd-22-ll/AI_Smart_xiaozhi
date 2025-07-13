#include "bsp_lcd.h"

#define TAG "bsp_lcd"
#define LCD_HOST SPI2_HOST

#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL 1
#define LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define PIN_NUM_SCLK 47
#define PIN_NUM_MOSI 48
#define PIN_NUM_MISO 18
#define PIN_NUM_LCD_DC 45
#define PIN_NUM_LCD_RST 16
#define PIN_NUM_LCD_CS 21
#define PIN_NUM_BK_LIGHT 40

bsp_lcd_t my_lcd;

void bsp_lcd_init(void)
{
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    // 配置spi
    spi_bus_config_t buscfg = {
        .sclk_io_num   = PIN_NUM_SCLK,
        .mosi_io_num   = PIN_NUM_MOSI,
        .miso_io_num   = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1};
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    // 在spi总线上添加从机
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t     io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num       = PIN_NUM_LCD_DC,
        .cs_gpio_num       = PIN_NUM_LCD_CS,
        .pclk_hz           = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10};
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // 对lcd做配置
    esp_lcd_panel_handle_t     panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);

    // 充值lcd
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    // 初始lcd
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));

    // 开启屏幕
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL);

    my_lcd.io_handle    = io_handle;
    my_lcd.panel_handle = panel_handle;
}
