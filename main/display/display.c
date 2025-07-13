#include "display.h"
#include "bsp_lcd.h"
#include "esp_lvgl_port.h"
#include "font_emoji.h"
#include "font_puhui.h"
#include "lv_examples.h"

#define TAG "display"

#define LCD_H_RES 240
#define LCD_V_RES 320

typedef struct
{
    char *text;
    char *emoji;
} emoji_t;

static emoji_t emoji_list[] = {
    {"neutral", "😶"},
    {"happy", "🙂"},
    {"laughing", "😆"},
    {"funny", "😂"},
    {"sad", "😔"},
    {"angry", "😠"},
    {"crying", "😭"},
    {"loving", "😍"},
    {"embarrassed", "😳"},
    {"surprised", "😯"},
    {"shocked", "😱"},
    {"thinking", "🤔"},
    {"winking", "😉"},
    {"cool", "😎"},
    {"relaxed", "😌"},
    {"delicious", "🤤"},
    {"kissy", "😘"},
    {"confident", "😏"},
    {"sleepy", "😴"},
    {"silly", "😜"},
    {"confused", "🙄"}};

lv_obj_t       *
    tts_label;
lv_obj_t *emoji_label;
lv_obj_t *scr;

LV_FONT_DECLARE(font_puhui_14_1)
void display_init(void)
{
    // 初始化lcd
    bsp_lcd_init();

    // 初始化lvgl
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority     = 4,    /* LVGL task priority */
        .task_stack        = 6144, /* LVGL task stack size */
        .task_affinity     = -1,   /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500,  /* Maximum sleep in LVGL task */
        .timer_period_ms   = 5     /* LVGL timer tick period in ms */
    };
    lvgl_port_init(&lvgl_cfg);

    ESP_LOGI(TAG, "Add LCD screen to lvgl");
    uint32_t buff_size = LCD_H_RES * 10;   // 缓冲10行数据

    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle  = my_lcd.panel_handle,
        .io_handle     = my_lcd.io_handle,
        .buffer_size   = buff_size,
        .double_buffer = true,                     // 释放开启双缓冲
        .hres          = LCD_H_RES,                // 水平分辨率
        .vres          = LCD_V_RES,                // 处置分辨率
        .monochrome    = false,                    // 释放单色显示
        .color_format  = LV_COLOR_FORMAT_RGB565,   // 颜色格式
        .rotation      = {
                 .swap_xy  = false,
                 .mirror_x = true,   // 是否镜像
                 .mirror_y = true,
        },
        .flags = {
            .buff_dma    = true,   // 是否启用dma
            .buff_spiram = true,   // 缓冲区使用spiram
            .swap_bytes  = true,   // 字节序调整

            .full_refresh = false,
            .direct_mode  = false,
        }};

    lv_display_t *lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    scr = lv_scr_act();

    // 在多任务操作系统中 给屏幕操作上锁
    lvgl_port_lock(0);

    tts_label = lv_label_create(scr);
    lv_obj_align(tts_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_width(tts_label, LCD_H_RES);
    lv_obj_set_style_text_align(tts_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(tts_label, &font_puhui_14_1, 0);
    lv_label_set_text(tts_label, "尚硅谷智能语音聊天机器人");

    emoji_label = lv_label_create(scr);
    lv_obj_align(emoji_label, LV_ALIGN_CENTER, 0, -50);
    lv_obj_set_width(emoji_label, LCD_H_RES);
    lv_obj_set_style_text_align(emoji_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(emoji_label, font_emoji_64_init(), 0);
    lv_label_set_text(emoji_label, "😆");

    lvgl_port_unlock();
}

void display_show_tts(char *tts)
{
    lvgl_port_lock(0);
    lv_label_set_text(tts_label, tts);
    lvgl_port_unlock();
}

void display_show_emoji(char *txt)
{
    char *emoji = "😆";
    for(int i = 0; i < sizeof(emoji_list) / sizeof(emoji_list[0]); i++)
    {
        if(strcmp(emoji_list[i].text, txt) == 0)
        {
            emoji = emoji_list[i].emoji;
            break;
        }
    }

    lvgl_port_lock(0);
    lv_label_set_text(emoji_label, emoji);
    lvgl_port_unlock();
}

lv_obj_t *qr = NULL;
void      display_show_qrcode(char *data)
{
    if(qr) return;
    MY_LOGI("显示二维码");
    lvgl_port_lock(0);
    lv_color_t bg_color = lv_color_white();
    lv_color_t fg_color = lv_color_black();

    qr = lv_qrcode_create(scr);
    lv_qrcode_set_size(qr, 220);
    lv_qrcode_set_dark_color(qr, fg_color);
    lv_qrcode_set_light_color(qr, bg_color);

    /*Set data*/

    lv_qrcode_update(qr, data, strlen(data));
    lv_obj_center(qr);

    /*Add a border with bg_color*/
    lv_obj_set_style_border_color(qr, lv_color_black(), 0);
    lv_obj_set_style_border_width(qr, 2, 0);
    lvgl_port_unlock();
}

void display_del_qrcode(void)
{
    if(qr != NULL)
    {
        lvgl_port_lock(0);
        lv_obj_del(qr);
        lvgl_port_unlock();
        qr = NULL;
    }
}
