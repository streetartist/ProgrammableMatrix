#include "esp_log.h"
#include "nvs_flash.h"
#include "board.h"
#include "matrix.h"
#include "peripherals.h"
#include "secrets.h"
#include "wifi_ble.h"
#include "net.h"
#include "bootloader_random.h"

void app_main(void)
{
    esp_err_t e=nvs_flash_init();
    if(e==ESP_ERR_NVS_NO_FREE_PAGES || e==ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());e=nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);
    // VERY FIRST: matrix safe state and all 19 user IOs high-impedance.
    matrix_init();
    // Generate NVS secrets with true hardware entropy BEFORE radio/ADC startup.
    bootloader_random_enable();
    secrets_init();
    bootloader_random_disable();
    peripherals_init();
    wifi_ble_start();
    net_start();
    ESP_LOGI("JBOX","ready: 52 nodes / 8 nets / 19 controllable GPIO; UART0 at GPIO43/44");
}
