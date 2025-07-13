#include "ws.h"

#include "esp_crt_bundle.h"
#include "cJSON.h"

ws_t my_ws;

esp_websocket_client_handle_t client;

#define TAG "WS"
static void ws_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch(event_id)
    {
        case WEBSOCKET_EVENT_BEGIN:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
            my_ws.is_connected = false;
            break;
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
            my_ws.is_connected = true;
            // 设置ws连接成功的事件标志位
            xEventGroupSetBits(my_ws.event_group, WS_CONNECTED_BIT);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            my_ws.is_connected = false;
            break;
        case WEBSOCKET_EVENT_DATA:
            my_ws.is_connected = true;
            // ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA  %d", data->op_code);
            if(data->op_code == 0x2)
            {
                // MY_LOGI("收到音频数据");
                if(my_ws.recv_bin_cb)
                {
                    my_ws.recv_bin_cb(data->data_ptr, data->data_len);
                }
            }
            else if(data->op_code == 0x01)
            {
                // MY_LOGI("收到文本数据: %.*s", data->data_len, (char *)data->data_ptr);
                if(my_ws.recv_txt_cb)
                {
                    my_ws.recv_txt_cb(data->data_ptr, data->data_len);
                }
            }
            else if(data->op_code == 0x08)
            {
                MY_LOGI("收到停止帧");
            }

            break;
        case WEBSOCKET_EVENT_ERROR:
            my_ws.is_connected = false;
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");

            break;
        case WEBSOCKET_EVENT_FINISH:
            my_ws.is_connected = false;
            // ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
            if(my_ws.ws_finish_cb)
            {
                my_ws.ws_finish_cb();
            }
            break;
    }
}

void ws_init(void (*recv_txt_cb)(char *, int),
             void (*recv_bin_cb)(char *, int),
             void (*ws_finish_cb)(void))

{

    my_ws.recv_bin_cb  = recv_bin_cb;
    my_ws.recv_txt_cb  = recv_txt_cb;
    my_ws.ws_finish_cb = ws_finish_cb;

    my_ws.event_group = xEventGroupCreate();

    // ws客户端配置
    esp_websocket_client_config_t websocket_cfg = {
        .uri                  = my_ota.ws_url,
        .transport            = WEBSOCKET_TRANSPORT_OVER_SSL,
        .crt_bundle_attach    = esp_crt_bundle_attach,
        .reconnect_timeout_ms = 3000,   // 重新连接时间
        .network_timeout_ms   = 5000,   // 网络超时时间
        .buffer_size          = 2 * 1024};

    // 初始化客户端
    client = esp_websocket_client_init(&websocket_cfg);

    // 设置请求头: 必须在建立连接之前
    char *authorization = NULL;
    asprintf(&authorization, "Bearer %s", my_ota.token);
    esp_websocket_client_append_header(client, "Authorization", strdup(authorization));
    esp_websocket_client_append_header(client, "Protocol-Version", "1");
    esp_websocket_client_append_header(client, "Device-Id", my_ota.devce_id);
    esp_websocket_client_append_header(client, "Client-Id", my_ota.client_id);

    free(authorization);

    // 注册事件回调函数
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, ws_event_handler, (void *)client);
}

// 建立到服务器音频传输通道
void ws_open_audio_channel(void)
{
    if(client && !esp_websocket_client_is_connected(client))
    {
        esp_websocket_client_start(client);   // 建立到服务器的连接
        // 等待事件标志位: 连接建立成功
        xEventGroupWaitBits(my_ws.event_group, WS_CONNECTED_BIT, true, true, portMAX_DELAY);
    }
}

void ws_close_audio_channel(void)
{
    if(client && esp_websocket_client_is_connected(client))
    {
        esp_websocket_client_close(client, portMAX_DELAY);
    }
}

void ws_send_text(char *txt)
{
    if(client && txt && my_ws.is_connected)
    {
        esp_websocket_client_send_text(client, txt, strlen(txt), portMAX_DELAY);
    }
}

void ws_send_bin(uint8_t *data, uint32_t len)
{
    if(client && data && len && my_ws.is_connected)
    {
        esp_websocket_client_send_bin(client, (char *)data, len, portMAX_DELAY);
    }
}

void ws_send_hello(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddStringToObject(root, "transport", "websocket");

    cJSON *audio_params = cJSON_CreateObject();
    cJSON_AddNumberToObject(audio_params, "sample_rate", 16000);
    cJSON_AddNumberToObject(audio_params, "channels", 1);
    cJSON_AddNumberToObject(audio_params, "frame_duration", 60);
    cJSON_AddStringToObject(audio_params, "format", "opus");

    cJSON_AddItemToObject(root, "audio_params", audio_params);

    char *json_str = strdup(cJSON_PrintUnformatted(root));

    ws_send_text(json_str);
    cJSON_Delete(root);
}

void ws_send_wake(void)
{
    MY_LOGI("发送唤醒词");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "detect");
    cJSON_AddStringToObject(root, "text", "你好,小智");

    char *json_str = strdup(cJSON_PrintUnformatted(root));

    ws_send_text(json_str);
    cJSON_Delete(root);
}

void ws_send_abort(void)
{
    MY_LOGI("发送中止信息");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "abort");
    cJSON_AddStringToObject(root, "reason", "wake_word_detected");

    char *json_str = strdup(cJSON_PrintUnformatted(root));

    ws_send_text(json_str);
    cJSON_Delete(root);
}

void ws_send_start_listen(void)
{
    MY_LOGI("发送开始监听信息");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "start");
    cJSON_AddStringToObject(root, "mode", "manual");

    char *json_str = strdup(cJSON_PrintUnformatted(root));

    ws_send_text(json_str);
    cJSON_Delete(root);
}

void ws_send_stop_listen(void)
{
    MY_LOGI("发送开始监听信息");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "stop");

    char *json_str = strdup(cJSON_PrintUnformatted(root));

    ws_send_text(json_str);
    cJSON_Delete(root);
}

void ws_send_iot_descriptor(void)
{
    MY_LOGI("发送iot描述信息");
    ws_send_text(iot_get_descriptor());
}

void ws_send_iot_status()
{
    MY_LOGI("发送iot状态信息");
    ws_send_text(iot_get_state());
}
