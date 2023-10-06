#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_http_client.h"
#include "connect_wifi.h"
#include "lcd.h"

static const char *TAG = "HTTP_CLIENT";

char api_key[] = "IWCH9QP5IUIQOI0Z";

char message[] = "Hello This is a test message";

int pulseData[] = {70, 72, 68, 74, 76, 73, 71};
int oximetryData[] = {97, 98, 96, 99, 95, 96, 98};
int dataSize = sizeof(pulseData) / sizeof(pulseData[0]);
int currentIndex = 0;

unsigned int getPulseData(void)
{
	int data = pulseData[currentIndex];
	currentIndex = (currentIndex + 1) % dataSize;
    return data;
}

unsigned int getOximetryData(void)
{
	int data = oximetryData[currentIndex];
	currentIndex = (currentIndex + 1) % dataSize;
    return data;
}

void lcd_task(void *pvParameters)
{
    /* Create LCD object */
    lcd_t lcd;

    /* Set default pinout */
    lcdDefault(&lcd);

    /* Initialize LCD object */
    lcdInit(&lcd);

    /* Clear previous data on LCD */
    lcdClear(&lcd);
    while (1)
    {
        char buffer[16];
        
        unsigned int pulse = getPulseData();
        unsigned int oximetry = getOximetryData();
        
        sprintf(buffer, "Pulse: %u", pulse);
        lcd_err_t ret = lcdSetText(&lcd, buffer, 0, 0);
        assert_lcd(ret);

        sprintf(buffer, "Oximetry: %u", oximetry);
        ret = lcdSetText(&lcd, buffer, 0, 1);
        assert_lcd(ret);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    // Free LCD if and when task is being deleted
    lcdFree(&lcd);
}


void thingspeak_send_data(void *pvParameters)
{
    while (1) // Ensuring repeated data sending
    {
        unsigned int pulse = getPulseData();
        unsigned int oximetry = getOximetryData();
        
        char thingspeak_url[200];
        snprintf(thingspeak_url,
                 sizeof(thingspeak_url),
                 "%s%s%s%d%s%d",
                 "https://api.thingspeak.com/update?api_key=",
                 api_key,
                 "&field1=",
                 pulse,
                 "&field2=",
                 oximetry);

        esp_http_client_config_t config = {
            .url = thingspeak_url,
            .method = HTTP_METHOD_GET
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");

        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK)
        {
            int status_code = esp_http_client_get_status_code(client);
            if (status_code == 200) // Note: ThingSpeak typically returns a 200 status on successful updates
            {
                ESP_LOGI(TAG, "Message sent Successfully");
            }
            else
            {
                ESP_LOGI(TAG, "Message sent Failed");
            }
        }
        else
        {
            ESP_LOGE(TAG, "HTTP client error: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);

        vTaskDelay(16000 / portTICK_PERIOD_MS); // Delay 16 seconds as per ThingSpeak rate limits
    }
}

void app_main(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	connect_wifi();
	if (wifi_connect_status)
	{
		xTaskCreate(thingspeak_send_data, "thingspeak_send_data", 8192, NULL, 6, NULL);
	}
    // Start the LCD task regardless of WiFi status
    xTaskCreate(lcd_task, "lcd_task", 2048, NULL, 4, NULL);
}