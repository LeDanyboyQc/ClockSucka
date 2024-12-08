/* 
Bouzin first espressif project
*/
/*****************************************************************************************
*   INCLUSION
*****************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif.h"

#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver/gpio.h"


/*****************************************************************************************
*   DEFINITION
*****************************************************************************************/
#define WIFI_SSID       "Mori-Coulemz"
#define WIFI_PASS       "Poutine123"
#define MAXIMUM_RETRY   10

#define LED_PIN         GPIO_NUM_2              // GPIO2 is typically the onboard LED

// #if CONFIG_ESP_WIFI_AUTH_OPEN
// #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
// #elif CONFIG_ESP_WIFI_AUTH_WEP
// #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
// #elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
// #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
// #elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
// #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
// #elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
// #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
// #elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
// #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
// #elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
// #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
// #elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
// #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
// #endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char* WIFI_TAG = "wifi station";
static const char* SNTP_TAG = "sntp protocol";

static int s_retry_num = 0;

#define SNTP_SERVER     "pool.ntp.org"
#define TIMEZONE        "EST5EDT,M3.2.0/2,M11.1.0/2"


/*****************************************************************************************
*   FUNCTION DEFINITION
*****************************************************************************************/
static void boot_message(void){
    printf("My sick ass`s BOOT!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());
}


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        } 
        else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG,"connect to the AP fail");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    ESP_LOGI(WIFI_TAG, "ESP_WIFI_MODE_STA");

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	        //.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "connected to ap SSID: %s password: %s", WIFI_SSID, WIFI_PASS);
    } 
    else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID: %s, password: %s", WIFI_SSID, WIFI_PASS);
    } 
    else {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
    }
}


void configTime(long gmtOffset_sec, int daylightOffset_sec, const char* server1){
    //tcpip_adapter_init();  // Should not hurt anything if already inited
    esp_netif_init();
    if(sntp_enabled()){
        sntp_stop();
    }
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*)server1);
    sntp_init();
    setenv("TZ", TIMEZONE, 1);
    tzset();
}


/*****************************************************************************************
*   TASK DEFINITION
*****************************************************************************************/

void task_led_blink(void){
    /* LED flash */
    // Configure GPIO2 as an output
    gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(LED_PIN, 1);  // Turn the LED on
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Wait 1 second
        gpio_set_level(LED_PIN, 0);  // Turn the LED off
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Wait 1 second
    }
}


void task_tell_time(void) {
    /*  TELL time
        Fetch SNTP server to get current time of North america timezone, and print it to 
        the screen every second
    */ 
    configTime(3600,3600, SNTP_SERVER);

    struct timeval time;
    struct tm timeinfo;

    while (1) {
        if (gettimeofday(&time, NULL) == 0) {
            // Convert the timestamp to local time (timeinfo structure)
            localtime_r(&time.tv_sec, &timeinfo);

            // Print the date and time (Year-Month-Day Hour:Minute:Second)
            ESP_LOGI(SNTP_TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d",
                        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            ESP_LOGE(SNTP_TAG, "Failed to get time");
        }
        fflush(stdout);
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for 1 second
    }
}


/*****************************************************************************************
*   MAIN PROGRAM
*****************************************************************************************/
void app_main(void)
{

    // Welcome
    boot_message();

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialise Wi-Fi
    wifi_init_sta();

    // vTaskDelay(5000 / portTICK_PERIOD_MS);
    // printf("timer fini");

    /* LED blinking task */
    xTaskCreate(
        (void*)&task_led_blink,         // Task function
        "TASK_LED_blink",               // Task name (for debugging)
        2048,                           // Stack size in words (4 bytes per word)
        NULL,                           // Parameters for the task (none here)
        6,                              // Priority (higher = more urgent)
        NULL                            // Task handle (not used here)
    );


    /* tell time task */
    xTaskCreate(
        (void*)&task_tell_time,           // Task function
        "TASK_tell_time",    // Task name (for debugging)
        2048,                   // Stack size in words (4 bytes per word)
        NULL,                   // Parameters for the task (none here)
        5,                      // Priority (higher = more urgent)
        NULL                    // Task handle (not used here)
    );

    // We be chilling bro
    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}