
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>

#include <nvs_flash.h>
#include <conn_mgr_prov.h>
#include <conn_mgr_prov_mode_ble.h>
#include <conn_mgr_prov_mode_softap.h>

#include "app_priv.h"

static const char *TAG = "app_main";

const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    conn_mgr_prov_event_handler(ctx, event);

    /* Global event handler */
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "Connected with IP Address:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
        esp_wifi_connect();
        break;
    default:
        break;
    }
    return ESP_OK;
}

/* Custom event handler for catching provisioning manager events */
static esp_err_t prov_event_handler(void *user_data, conn_mgr_prov_cb_event_t event)
{
    esp_err_t ret = ESP_OK;
    switch (event) {
        case CMP_PROV_END:
            conn_mgr_prov_deinit();
            break;
        default:
            break;
    }
    return ret;
}

static void wifi_init_sta()
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_start() );
}

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

void app_main()
{
    app_driver_init();

    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ret = nvs_flash_erase();

        ret |= nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NVS");
        return;
    }

    /* Initialize TCP/IP and the event loop */
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );
    wifi_event_group = xEventGroupCreate();

    /* Configuration for the provisioning manager */
    conn_mgr_prov_config_t config = {
        .scheme = conn_mgr_prov_scheme_ble,

        .scheme_event_handler = CMP_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,

        .app_event_handler = {
            .event_cb = prov_event_handler,
            .user_data = NULL
        }
    };

    /* Initialize provisioning manager  */
    if (conn_mgr_prov_init(config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize provisioning manager");
        return;
    }

    bool provisioned = false;
    if (conn_mgr_prov_is_provisioned(&provisioned) != ESP_OK) {
        ESP_LOGE(TAG, "Error getting device provisioning state");
        return;
    }

    if (!provisioned) {
        ESP_LOGI(TAG, "Starting provisioning");

        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));

        int security = 1;

        const char *pop = "abcd1234";

        const char *service_key = NULL;

        conn_mgr_prov_start_provisioning(security, pop, service_name, service_key);

    } else {
        ESP_LOGI(TAG, "Already provisioned, starting station");

        wifi_init_sta();

        conn_mgr_prov_deinit();
    }

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);
    cloud_start();
}
