#include "sntp_sync.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "SNTP_SYNC";

static volatile bool s_is_synced = false;
static sntp_time_step_cb_t s_time_step_cb = NULL;
static StaticSemaphore_t s_cb_mutex_buf;
static SemaphoreHandle_t s_cb_mutex = NULL;

static void time_sync_notification_cb(struct timeval *tv)
{
    time_t now = tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

    ESP_LOGI(TAG, "Wall-clock synchronized via NTP: %s (GMT+7)", strftime_buf);
    s_is_synced = true;

    // Trigger cron scheduler recalculation hook
    if (s_cb_mutex && xSemaphoreTake(s_cb_mutex, 0) == pdTRUE) {
        if (s_time_step_cb != NULL) {
            s_time_step_cb();
        }
        xSemaphoreGive(s_cb_mutex);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA started, connecting to AP...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected. Reconnecting in 3s (background non-blocking)...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected to AP! IP: " IPSTR, IP2STR(&event->ip_info.ip));

        if (!esp_sntp_enabled()) {
            ESP_LOGI(TAG, "Initializing SNTP client with server: %s", SNTP_SERVER_NAME);
            esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, SNTP_SERVER_NAME);
            sntp_set_time_sync_notification_cb(time_sync_notification_cb);
            esp_sntp_init();
        }
    }
}

esp_err_t sntp_sync_init(const char *wifi_ssid, const char *wifi_password)
{
    if (s_cb_mutex == NULL) {
        s_cb_mutex = xSemaphoreCreateMutexStatic(&s_cb_mutex_buf);
    }

    // Set Timezone to GMT+7 (Vietnam / Indochina Standard Time)
    setenv("TZ", "ICT-7", 1);
    tzset();

    // Initialize NVS (required for Wi-Fi storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, wifi_ssid ? wifi_ssid : CONFIG_ESP_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, wifi_password ? wifi_password : CONFIG_ESP_WIFI_PASS, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) return ret;

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) return ret;

    ret = esp_wifi_start();
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "Non-blocking Wi-Fi & SNTP engine started (SSID: %s)", wifi_config.sta.ssid);
    return ESP_OK;
}

bool sntp_sync_is_synced(void)
{
    return s_is_synced;
}

esp_err_t sntp_sync_get_time_str(char *buf, size_t buf_len, const char *format)
{
    if (buf == NULL || buf_len == 0) return ESP_ERR_INVALID_ARG;

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    if (strftime(buf, buf_len, format ? format : "%H:%M:%S", &timeinfo) == 0) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

time_t sntp_sync_get_time(void)
{
    if (!s_is_synced) return 0;
    time_t now;
    time(&now);
    return now;
}

void sntp_sync_register_time_step_cb(sntp_time_step_cb_t cb)
{
    if (s_cb_mutex && xSemaphoreTake(s_cb_mutex, portMAX_DELAY) == pdTRUE) {
        s_time_step_cb = cb;
        xSemaphoreGive(s_cb_mutex);
    }
}
