#include "wifi_ble.h"
#include "secrets.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "nvs_flash.h"
#include <stdio.h>
static EventGroupHandle_t wifi_bits;
#define GOT_IP_BIT BIT0
static const char *TAG="BLE+WIFI";
static bool provisioning_active;
static bool credentials_saved;
static esp_timer_handle_t reconnect_timer;
static unsigned reconnect_attempts;
static void reconnect_callback(void *unused) {
    (void)unused;
    if(!provisioning_active && credentials_saved && !wifi_is_ready())
        esp_wifi_connect();
}

bool wifi_is_ready(void) {return wifi_bits && (xEventGroupGetBits(wifi_bits)&GOT_IP_BIT);}

static void on_event(void *arg,esp_event_base_t event,int32_t id,void *data)
{
    (void)arg;
    if(event==WIFI_PROV_EVENT) {
        switch(id){
        case WIFI_PROV_START:ESP_LOGI(TAG,"BLE provisioning started");break;
        case WIFI_PROV_CRED_RECV: ESP_LOGI(TAG,"credentials received over BLE");break;
        case WIFI_PROV_CRED_SUCCESS:ESP_LOGI(TAG,"BLE provisioning successful");credentials_saved=true;break;
        case WIFI_PROV_CRED_FAIL:ESP_LOGW(TAG,"BLE Wi-Fi authentication failed; power-cycle or reset credentials to retry");break;
        case WIFI_PROV_END:
            ESP_LOGI(TAG,"BLE provisioning finished; free BLE resources");
            provisioning_active=false;
            wifi_prov_mgr_deinit();
            break;
        default:break;
        }
    } else if(event==WIFI_EVENT){
        if(id==WIFI_EVENT_STA_START) {
            esp_wifi_set_ps(WIFI_PS_NONE);
            if(!provisioning_active)esp_wifi_connect();
        }
        if(id==WIFI_EVENT_STA_DISCONNECTED) {
            xEventGroupClearBits(wifi_bits,GOT_IP_BIT);
            if(!provisioning_active && credentials_saved) {
                unsigned delay_s=1u<<(reconnect_attempts<5?reconnect_attempts:5);
                if(delay_s>30)delay_s=30;
                if(reconnect_attempts<10)reconnect_attempts++;
                ESP_LOGW(TAG,"Wi-Fi disconnected; retry after %us",delay_s);
                if(esp_timer_is_active(reconnect_timer))esp_timer_stop(reconnect_timer);
                esp_timer_start_once(reconnect_timer,(uint64_t)delay_s*1000000ULL);
            }
        }
    } else if(event==IP_EVENT && id==IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t *e=data;
        ESP_LOGI(TAG,"device IP " IPSTR,IP2STR(&e->ip_info.ip));
        reconnect_attempts=0;
        if(esp_timer_is_active(reconnect_timer))esp_timer_stop(reconnect_timer);
        xEventGroupSetBits(wifi_bits,GOT_IP_BIT);
    }
}
void wifi_ble_start(void)
{
    wifi_bits=xEventGroupCreate();
    const esp_timer_create_args_t rargs={.callback=reconnect_callback,.name="wifi-retry"};
    ESP_ERROR_CHECK(esp_timer_create(&rargs,&reconnect_timer));
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT,ESP_EVENT_ANY_ID,&on_event,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&on_event,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&on_event,NULL));
    wifi_prov_mgr_config_t prov={
        .scheme=wifi_prov_scheme_ble,
        .scheme_event_handler=WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov));
    bool provisioned=false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
    credentials_saved=provisioned;
    if(!provisioned){
        uint8_t mac[6];ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_STA));
        char name[32];snprintf(name,sizeof name,"JBOX-%02X%02X%02X",mac[3],mac[4],mac[5]);
        provisioning_active=true;
        ESP_LOGW(TAG,"BLE service name: %s, security mode 1",name);
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
            WIFI_PROV_SECURITY_1,secrets_pop(),name,NULL));
    }else{
        provisioning_active=false;
        wifi_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}
