#ifndef __WS_H
#define __WS_H
#include "Com_Debug.h"
#include "esp_websocket_client.h"
#include "ota.h"
#include "freertos/event_groups.h"
#include "iot.h"

#define WS_CONNECTED_BIT (1 << 0)
typedef struct
{
    bool is_connected;

    EventGroupHandle_t event_group;

    void (*recv_txt_cb)(char *, int);
    void (*recv_bin_cb)(char *, int);
    void (*ws_finish_cb)(void);
} ws_t;

extern ws_t my_ws;

void ws_init(void (*recv_txt_cb)(char *, int),
             void (*recv_bin_cb)(char *, int),
             void (*ws_finish_cb)(void));
void ws_open_audio_channel(void);

void ws_close_audio_channel(void);

void ws_send_bin(uint8_t *data, uint32_t len);

void ws_send_hello(void);

void ws_send_wake(void);

void ws_send_abort(void);

void ws_send_start_listen(void);

void ws_send_stop_listen(void);

void ws_send_iot_descriptor(void);

void ws_send_iot_status();

#endif
