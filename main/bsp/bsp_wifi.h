#ifndef __bsp_WIFI_H
#define __bsp_WIFI_H
#include "Com_Debug.h"
#include "display.h"

typedef void (*wifi_cb)(void);

void bsp_wifi_Init(void);
void bsp_wifi_Start(wifi_cb succ, wifi_cb fail);
#endif
