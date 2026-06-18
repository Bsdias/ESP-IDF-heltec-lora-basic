# LoRa Communication Kit — ESP32 / Heltec WiFi LoRa 32

Two ESP-IDF projects demonstrating point-to-point LoRa communication using the Heltec WiFi LoRa 32 board. Each node runs a FreeRTOS task and shows link diagnostics on an SSD1306 OLED display.

- **LoRa-Sender** — transmits an incrementing counter packet every 2 seconds at 915 MHz.
- **LoRa-Receiver** — listens continuously and displays each received payload along with RSSI and SNR.

## Requirements

- Heltec WiFi LoRa 32 (v1 or v2) — two boards, one per role
- ESP-IDF v6.0.1
- Python 3 with `esptool` (included in ESP-IDF)

## Hardware

The default GPIO assignments match the Heltec WiFi LoRa 32 v1 pinout. No external wiring is needed.

| Signal | GPIO |
|---|---|
| LoRa CS | 18 |
| LoRa RST | 14 |
| SPI MISO | 19 |
| SPI MOSI | 27 |
| SPI SCK | 5 |
| OLED SDA | 4 |
| OLED SCL | 15 |
| OLED RST | 16 |



## Project Structure

Both projects share the same component layout:

```
main/
    main.c          application entry point and FreeRTOS task
    Kconfig         GPIO and display configuration options
components/
    lora/           SX127x driver over SPI
    ssd1306/        SSD1306 OLED driver over I2C
```

## Notes

- The LoRa driver uses `SPI3_HOST` (VSPI on ESP32).
- The OLED driver uses the new ESP-IDF v5+ I2C master API (`driver/i2c_master.h`). The legacy `driver/i2c.h` API is not used.

## References

- [esp32-lora-library](https://github.com/Inteform/esp32-lora-library)
- [esp-idf-ssd1306](https://github.com/nopnop2002/esp-idf-ssd1306)
