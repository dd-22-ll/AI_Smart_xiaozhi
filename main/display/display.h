#ifndef __DISPLAY_H
#define __DISPLAY_H
#include "Com_Debug.h"

void display_init(void);
void display_show_tts(char *tts);
void display_show_emoji(char *emoji);
void display_show_qrcode(char *data);
void display_del_qrcode(void);
#endif
