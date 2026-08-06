#ifndef INC_WIFI_FOTA_
#define INC_WIFI_FOTA_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"

/* ---- FILL THESE IN with your actual network credentials ---- */
#define FOTA_WIFI_SSID          "YOUR_SSID_HERE"
#define FOTA_WIFI_PASS          "YOUR_PASSWORD_HERE"

/* How many times to retry connecting before giving up */
#define FOTA_WIFI_MAX_RETRY     5

/* Event group bits used to signal connection result */
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAILED_BIT         BIT1

void wifi_init(void);

#endif
