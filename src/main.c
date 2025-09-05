#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "WiFi_Scan";

void wifi_scan() {
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 120,
        .scan_time.active.max = 150,
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        ESP_LOGI(TAG, "No access points found");
        return;
    }

    wifi_ap_record_t *ap_info = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_info == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for AP list");
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));

    ESP_LOGI(TAG, "Found %d access points:", ap_count);
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "%-32s %-6s %-6s %-4s", "SSID", "RSSI", "CH", "AUTH");
    ESP_LOGI(TAG, "==================================================");

    for (int i = 0; i < ap_count; i++) {
        const char *auth_mode_str;
        switch (ap_info[i].authmode) {
            case WIFI_AUTH_OPEN:
                auth_mode_str = "OPEN";
                break;
            case WIFI_AUTH_WEP:
                auth_mode_str = "WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                auth_mode_str = "WPA";
                break;
            case WIFI_AUTH_WPA2_PSK:
                auth_mode_str = "WPA2";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                auth_mode_str = "WPA/2";
                break;
            case WIFI_AUTH_WPA3_PSK:
                auth_mode_str = "WPA3";
                break;
            default:
                auth_mode_str = "UNK";
                break;
        }

        ESP_LOGI(TAG, "%-32s %-6d %-6d %-4s", 
                 (char *)ap_info[i].ssid, 
                 ap_info[i].rssi, 
                 ap_info[i].primary, 
                 auth_mode_str);
    }

    free(ap_info);
}

void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Starting WiFi scan...");
    
    while (1) {
        wifi_scan();
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Scan every 10 seconds
    }
}