/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
/*****************************************************************************************
*   INCLUSION
*****************************************************************************************/
#include <stdio.h>
#include <time.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"  //for delay,mutexs,semphrs rtos operations
#include "freertos/task.h"

#include "esp_system.h"         //esp_init funtions esp_err_t 
#include "esp_spi_flash.h"
#include "esp_wifi.h"           //esp_wifi_init functions and wifi operations
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_attr.h"
#include "esp_sntp.h"

#include "lwip/err.h"           //light weight ip packets error handling
#include "lwip/sys.h"           //system applications for light weight ip apps
#include "lwip/ip_addr.h"

#include "driver/gpio.h"

#include "nvs_flash.h"          //non volatile storage

/*****************************************************************************************
*   DEFINITION
*****************************************************************************************/

// LED blinking
#define LED_PIN GPIO_NUM_2  // GPIO2 is typically the onboard LED

// Wifi-connection
#define WIFI_SSID "Mori-Coulemz"
#define WIFI_PASS "Poutine123"

// Tell time
#define NTP_TAG     "NTP"
#define NTP_SERVER  "pool.ntp.org"

/*****************************************************************************************
*   GLOBALE VARIABLE
*****************************************************************************************/

int retry_num                   =0;

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
    struct timeval time;
    struct tm timeinfo;

    while (1) {
        if (gettimeofday(&time, NULL) == 0) {
            // Convert the timestamp to local time (timeinfo structure)
            localtime_r(&time.tv_sec, &timeinfo);

            // Print the date and time (Year-Month-Day Hour:Minute:Second)
            ESP_LOGI(NTP_TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d",
                        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            ESP_LOGE(NTP_TAG, "Failed to get time");
        }
        fflush(stdout);
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for 1 second
    }
}

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


static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data){
    if(event_id == WIFI_EVENT_STA_START){
        printf("WIFI CONNECTING....\n");
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED){
        printf("WiFi CONNECTED\n");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED){
        printf("WiFi lost connection\n");
        if(retry_num < 5){
            esp_wifi_connect();
            retry_num++;
            printf("Retrying to Connect...\n");
        }
    }
    else if (event_id == IP_EVENT_STA_GOT_IP){
        printf("Wifi got IP...\n\n");
    }
}


static void wifi_connection(void){

    // 1 - Wi-Fi Configuration Phase
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());       // event loop   
    esp_netif_create_default_wifi_sta();                    // WiFi station   

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid       = WIFI_SSID,
            .password   = WIFI_PASS,
           }
        };

    esp_log_write(ESP_LOG_INFO, "Kconfig", "SSID=%s, PASS=%s\n\n", WIFI_SSID, WIFI_PASS);
    
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg);

    // 2 - Wi-Fi Start Phase
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);

    // 3 - Wi-Fi Connect Phase
    esp_wifi_connect();
}


/*****************************************************************************************
*   MAIN PROGRAM
*****************************************************************************************/
void app_main(void)
{
    // Welcome
    boot_message();

    // System initialization
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_connection();

    /* LED blinking task */
    xTaskCreate(
        (void*)&task_led_blink,          // Task function
        "TASK_LED_blink",         // Task name (for debugging)
        2048,               // Stack size in words (4 bytes per word)
        NULL,               // Parameters for the task (none here)
        1,                  // Priority (higher = more urgent)
        NULL                // Task handle (not used here)
    );

    /* tell time task */
    xTaskCreate(
        (void*)&task_tell_time,           // Task function
        "TASK_tell_time",    // Task name (for debugging)
        2048,                   // Stack size in words (4 bytes per word)
        NULL,                   // Parameters for the task (none here)
        2,                      // Priority (higher = more urgent)
        NULL                    // Task handle (not used here)
    );

    // Wait for connection or events (you can put other tasks here)
    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
