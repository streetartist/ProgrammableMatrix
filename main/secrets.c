#include "secrets.h"
#include "nvs.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_check.h"
#include <string.h>
static uint8_t key[32];
static char pop[13];
static const char *TAG="device-secrets";

const uint8_t *secrets_key(void){return key;}
const char *secrets_pop(void){return pop;}
void secrets_init(void)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("jbox-sec",NVS_READWRITE,&h));
    size_t len=sizeof(key);
    esp_err_t rc=nvs_get_blob(h,"api-hmac",key,&len);
    if(rc!=ESP_OK || len!=sizeof key){
        esp_fill_random(key,sizeof key);
        ESP_ERROR_CHECK(nvs_set_blob(h,"api-hmac",key,sizeof key));
    }
    size_t poplen=sizeof pop;
    rc=nvs_get_str(h,"ble-pop",pop,&poplen);
    if(rc!=ESP_OK || poplen!=sizeof pop){
        uint8_t r[6];esp_fill_random(r,sizeof r);
        const char hx[]="0123456789ABCDEF";
        for(int i=0;i<6;i++){pop[2*i]=hx[r[i]>>4];pop[2*i+1]=hx[r[i]&15];}
        pop[12]=0;
        ESP_ERROR_CHECK(nvs_set_str(h,"ble-pop",pop));
    }
    ESP_ERROR_CHECK(nvs_commit(h));nvs_close(h);
    // Intended for bring-up; in production distribute via a private label / secure station.
    char hex[65];static const char hx[]="0123456789abcdef";
    for(int i=0;i<32;i++){hex[2*i]=hx[key[i]>>4];hex[2*i+1]=hx[key[i]&15];}
    hex[64]=0;
    ESP_LOGW(TAG,"DEVICE KEY (copy to client and keep private): %s",hex);
    ESP_LOGW(TAG,"BLE SECURITY1 POP (keep private): %s",pop);
}
