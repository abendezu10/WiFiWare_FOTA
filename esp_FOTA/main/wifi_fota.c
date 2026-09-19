#include "wifi_fota.h"

static const char *WIFI_TAG = "wifi tag";

static EventGroupHandle_t sta_wifi_event_group;
static int sta_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
        esp_wifi_connect();
    } else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
        if(sta_retry_num < FOTA_WIFI_MAX_RETRY){
            esp_wifi_connect();
            sta_retry_num++;
            ESP_LOGI(WIFI_TAG, "attempting to connecting to the WiFi");
        } else {
            xEventGroupSetBits(sta_wifi_event_group,WIFI_FAILED_BIT);
        }
        ESP_LOGI(WIFI_TAG, "failed to connect to the WiFi");
   } else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
      ESP_LOGI(WIFI_TAG, "GOT IP EVENT - test");
      sta_retry_num = 0;
      xEventGroupSetBits(sta_wifi_event_group, WIFI_CONNECTED_BIT);
   }
}

void wifi_init(void){

    sta_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = FOTA_WIFI_SSID,
            .password = FOTA_WIFI_PASS
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished");

    EventBits_t event_bits = xEventGroupWaitBits(sta_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if(event_bits & WIFI_CONNECTED_BIT){
        ESP_LOGI(WIFI_TAG, "connected to WiFi, SSID: %s Password: %s",
                FOTA_WIFI_SSID, FOTA_WIFI_PASS);
    } else if(event_bits & WIFI_FAILED_BIT){
        ESP_LOGI(WIFI_TAG, "failed to connect to WiFi, SSID %s Password: %s",
                FOTA_WIFI_SSID, FOTA_WIFI_PASS);
    } else {
        ESP_LOGI(WIFI_TAG, "some other problem has occured.");
    }

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));

    esp_wifi_connect();

}
