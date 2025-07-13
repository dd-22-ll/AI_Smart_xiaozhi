#ifndef __OTA_H
#define __OTA_H
#include "Com_Debug.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


typedef struct
{
    bool is_activation;  // 是否已经激活的标志

    char *devce_id;
    char *client_id;

    char *ws_url;
    char *token;
    char *code;
} ota_t;

extern ota_t my_ota;

void ota_check_version(void);
#endif
