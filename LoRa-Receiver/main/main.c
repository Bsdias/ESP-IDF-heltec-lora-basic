#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lora.h"

#include "ssd1306.h"
#include "font8x8_basic.h"
#define TAG "SSD1306"
SSD1306_t dev;

uint8_t buf[32];

lora_config_t lora_config = {
   .frequency        = 915e6,
   .tx_power         = 17,
   .spreading_factor = 7,
   .bandwidth        = 125000,
   .coding_rate      = 5,
   .preamble_length  = 8,
   .sync_word        = 0x12,
   .crc_enabled      = true,
};

void config_i2c_interface(){ //Config Display
 	//CONFIG_I2C_INTERFACE
	ESP_LOGI(TAG, "INTERFACE is i2c");
	ESP_LOGI(TAG, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(TAG, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(TAG, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
 	//CONFIG_SSD1306_128x64
	ESP_LOGI(TAG, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);
	//clear Display
	ssd1306_clear_screen(&dev,false);
	//CONFIG_SSD1306_128x64
	ssd1306_display_text(&dev, 0, "  AGRO - 2030   ", 18, true);
	//ssd1306_display_text(&dev, 1, "                ", 18, true);
	ssd1306_display_text(&dev, 1, "Kit-01 [Gateway]", 18, true);
  	// CONFIG_SSD1306_128x64
}
void show_RSSI_and_SNR(){
   
   int RSSI, SNR;
   RSSI = lora_packet_rssi();
   SNR  = lora_packet_snr();

   char RSSI_c[20];
   char SNR_c[20];
   char values[40];
   sprintf(RSSI_c, "%d", RSSI);
   sprintf(SNR_c, "%d", SNR);

   strcpy(values, "RSSI:");
   strcat(values, RSSI_c);
   strcat(values, " SRN:");
   strcat(values,SNR_c);
   
   ssd1306_display_text(&dev, 2, values, 18, true);

}
void task_rx(void *p)
{
   int x;
   char buf_c[32]; 
   for(;;) {
      lora_receive();    // put into receive mode
      while(lora_received()) {
         x = lora_receive_packet(buf, sizeof(buf));
         buf[x] = 0;
         sprintf(buf_c, "%s", buf);
         printf("Received: %s\n", buf);

         ssd1306_display_text(&dev, 4, "Recebido    ", 18, false);
         ssd1306_display_text(&dev, 5, buf_c, x, false);
         show_RSSI_and_SNR();

         lora_receive();
      }
      vTaskDelay(1);
   }
}

void app_main()
{
   config_i2c_interface();
   lora_init();
   lora_configure(&lora_config);

   xTaskCreate(&task_rx, "task_rx", 4096, NULL, 5, NULL);
}