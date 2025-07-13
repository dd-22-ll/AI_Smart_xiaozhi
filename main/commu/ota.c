#include "ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "esp_app_desc.h"

#define OTA_URL "https://api.tenclass.net/xiaozhi/ota/"

#define TAG "ota"

#define OTA_RECV_BIT (1 << 0)

ota_t my_ota;

static esp_http_client_handle_t client;

static EventGroupHandle_t event_group;

void ota_task(void *params);
void parse_ota_result(char *output_buffer, int output_len);
void ota_check_version(void)
{
    event_group = xEventGroupCreate();   // 创建时间标志组
    /* 1. 创建一个任务进行http请求 */

    // 一直等待用户激活成功

    while(1)
    {
        xTaskCreate(ota_task,
                    "ota_task",
                    6 * 1024,
                    NULL,
                    5,
                    NULL);
        // 等待响应
        MY_LOGI("等待服务器响应....");
        xEventGroupWaitBits(event_group, OTA_RECV_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if(my_ota.is_activation == false)
        {
            MY_LOGI("等待用户激活设备... %s", my_ota.code);
        }
        else
        {
            MY_LOGI("设备已经激活....");
            return;
        }
        vTaskDelay(10000);
    }
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;   // Buffer to store response of http request from event handler
    static int   output_len;      // Stores number of bytes read
    switch(evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:   // 收到响应头
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            if(strcmp(evt->header_key, "Content-Length") == 0)
            {
                output_len         = 0;                         // 表示已经收到数据的长度
                int len            = atoi(evt->header_value);   // 把数字字符串转成整数  "123" =>123
                output_buffer      = malloc(len + 1);
                output_buffer[len] = '\0';
            }

            break;
        case HTTP_EVENT_ON_DATA:   // 收到服务器的回应
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // MY_LOGE("%.*s", evt->data_len, (char *)evt->data);
            // 把每次收到的数据拷贝到output_buffer中
            memcpy(output_buffer + output_len, evt->data, evt->data_len);
            output_len += evt->data_len;   // 存储已经copy的长度
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            // 数据接收完毕
            parse_ota_result(output_buffer, output_len);
            // 给对应事件标志位位进行置位
            xEventGroupSetBits(event_group, OTA_RECV_BIT);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");

            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");

            break;
    }
    return ESP_OK;
}

char *ota_get_device_id(void)
{
    uint8_t mac[6];
    memset(mac, 0, sizeof(mac));
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // static uint8_t mac_str[18];
    char *mac_str = (char *)malloc(18);
    sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return mac_str;
}

char *ota_get_client_id(void)   // uuid
{

    nvs_handle_t nvs_handle;
    esp_err_t    err = nvs_open("ota", NVS_READWRITE, &nvs_handle);

    // 格式化为 UUID 字符串
    char *uuid_str = malloc(37);

    size_t len = 0;
    nvs_get_str(nvs_handle, "uuid", NULL, &len);   // 测试字符串长度
    if(len > 0)
    {
        err = nvs_get_str(nvs_handle, "uuid", uuid_str, &len);

        if(err == ESP_OK)
        {
            MY_LOGI("读到uuid");
            return uuid_str;
        }
    }

    MY_LOGI("没有读到uuid");

    uint8_t uuid[16];
    for(int i = 0; i < 16; ++i)
    {
        uuid[i] = esp_random() & 0xFF;
    }

    // 设置版本号：UUID version 4 (随机)
    uuid[6] = (uuid[6] & 0x0F) | 0x40;

    // 设置 variant bits：10xx xxxx
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    sprintf(uuid_str,
            "%02x%02x%02x%02x-"
            "%02x%02x-"
            "%02x%02x-"
            "%02x%02x-"
            "%02x%02x%02x%02x%02x%02x",
            uuid[0],
            uuid[1],
            uuid[2],
            uuid[3],
            uuid[4],
            uuid[5],
            uuid[6],
            uuid[7],
            uuid[8],
            uuid[9],
            uuid[10],
            uuid[11],
            uuid[12],
            uuid[13],
            uuid[14],
            uuid[15]);
    /* 存储uuid */
    nvs_set_str(nvs_handle, "uuid", uuid_str);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    return uuid_str;
}

void ota_add_http_requeset_header(void)
{
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "User-Agent", "bread-compact-wifi-128*64/1.0.1");

    char *device_id = ota_get_device_id();
    esp_http_client_set_header(client, "Device-Id", device_id);
    MY_LOGI("Device-Id: %s", device_id);

    char *client_id = ota_get_client_id();
    esp_http_client_set_header(client, "Client-Id", client_id);
    MY_LOGI("Client-Id: %s", client_id);

    my_ota.devce_id  = strdup(device_id);   // 深copy: 复制内容
    my_ota.client_id = strdup(client_id);

    free(device_id);
    free(client_id);
}

void ota_add_http_requeset_body(void)
{
    cJSON *root = cJSON_CreateObject();

    cJSON *application = cJSON_CreateObject();
    cJSON_AddStringToObject(application, "version", "1.0.1");
    cJSON_AddStringToObject(application, "elf_sha256", esp_app_get_elf_sha256_str());
    cJSON_AddItemToObject(root, "application", application);

    cJSON *board = cJSON_CreateObject();
    cJSON_AddStringToObject(board, "type", "bread-compact-wifi");
    cJSON_AddStringToObject(board, "name", "bread-compact-wifi-128X64");
    cJSON_AddStringToObject(board, "ssid", "atguigu");
    cJSON_AddNumberToObject(board, "rssi", -50);
    cJSON_AddNumberToObject(board, "channel", 1);
    cJSON_AddStringToObject(board, "ip", "192.168.1.10");
    cJSON_AddStringToObject(board, "mac", my_ota.devce_id);

    cJSON_AddItemToObject(root, "board", board);

    char *body = cJSON_PrintUnformatted(root);

    MY_LOGI("请求体: %s", body);

    esp_http_client_set_post_field(client, body, strlen(body));   // 设置请全体

    cJSON_Delete(root);
}
void ota_task(void *params)
{
    esp_http_client_config_t config = {
        .url               = OTA_URL,
        .method            = HTTP_METHOD_POST,
        .event_handler     = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .transport_type    = HTTP_TRANSPORT_OVER_SSL,
        //.buffer_size       = 2048,   // 接收缓冲区大小
    };

    // 初始化客户端
    client = esp_http_client_init(&config);

    // 添加请求头
    ota_add_http_requeset_header();

    // 添加请求体
    ota_add_http_requeset_body();

    // 发送请求
    esp_err_t err = esp_http_client_perform(client);

    vTaskDelete(NULL);
}

void parse_ota_result(char *output_buffer, int output_len)
{
    MY_LOGE("响应内容: %s", output_buffer);

    cJSON *root = cJSON_ParseWithLength(output_buffer, output_len);

    if(root == NULL)
    {
        MY_LOGE("parse json failed");
        free(output_buffer);
        return;
    }

    cJSON *ws = cJSON_GetObjectItem(root, "websocket");
    if(ws == NULL)
    {
        MY_LOGE("websocket is null");
        cJSON_Delete(root);
        free(output_buffer);
        return;
    }

    cJSON *ws_url = cJSON_GetObjectItem(ws, "url");
    my_ota.ws_url = strdup(ws_url->valuestring);

    cJSON *token = cJSON_GetObjectItem(ws, "token");
    my_ota.token = strdup(token->valuestring);

    cJSON *activation    = cJSON_GetObjectItem(root, "activation");
    my_ota.is_activation = false;
    if(activation == NULL)
    {
        my_ota.is_activation = true;
        // 已经激活成功
        // MY_LOGI("已经激活成功!!!");
        cJSON_Delete(root);
        free(output_buffer);
        return;
    }
    cJSON *code = cJSON_GetObjectItem(activation, "code");
    my_ota.code = strdup(code->valuestring);

    // MY_LOGI("得到激活码: %s", my_ota.code);

    MY_LOGI("wsUrl = %s, token = %s, code = %s", my_ota.ws_url, my_ota.token, my_ota.code);

    cJSON_Delete(root);
    free(output_buffer);
}
