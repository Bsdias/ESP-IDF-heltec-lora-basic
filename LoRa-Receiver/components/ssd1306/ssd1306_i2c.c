#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "ssd1306.h"

#define tag "SSD1306"

#define I2C_MASTER_FREQ_HZ 400000

void i2c_master_init(SSD1306_t * dev, int16_t sda, int16_t scl, int16_t reset)
{
    /*
     * Reset ANTES de criar o bus. Se um boot anterior terminou no meio de uma
     * transação I2C (ex: stack overflow), o SSD1306 pode estar segurando SDA
     * em LOW. O reset de hardware limpa isso antes de inicializar o I2C.
     */
    if (reset >= 0) {
        gpio_reset_pin(reset);
        gpio_set_direction(reset, GPIO_MODE_OUTPUT);
        gpio_set_level(reset, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(reset, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    /* Destrava qualquer byte pendente no slave. */
    i2c_master_bus_reset(bus_handle);

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2CAddress,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev->_I2CHandle));

    dev->_address = I2CAddress;
    dev->_flip = false;
}

void i2c_init(SSD1306_t * dev, int width, int height)
{
    dev->_width = width;
    dev->_height = height;
    dev->_pages = 8;
    if (dev->_height == 32) dev->_pages = 4;

    uint8_t cmd_buf[] = {
        OLED_CONTROL_BYTE_CMD_STREAM,
        OLED_CMD_DISPLAY_OFF,
        OLED_CMD_SET_MUX_RATIO,
        (dev->_height == 64) ? 0x3F : 0x1F,
        OLED_CMD_SET_DISPLAY_OFFSET,
        0x00,
        OLED_CMD_SET_DISPLAY_START_LINE,
        dev->_flip ? OLED_CMD_SET_SEGMENT_REMAP_0 : OLED_CMD_SET_SEGMENT_REMAP_1,
        OLED_CMD_SET_COM_SCAN_MODE,
        OLED_CMD_SET_DISPLAY_CLK_DIV,
        0x80,
        OLED_CMD_SET_COM_PIN_MAP,
        (dev->_height == 64) ? 0x12 : 0x02,
        OLED_CMD_SET_CONTRAST,
        0xFF,
        OLED_CMD_DISPLAY_RAM,
        OLED_CMD_SET_VCOMH_DESELCT,
        0x40,
        OLED_CMD_SET_MEMORY_ADDR_MODE,
        OLED_CMD_SET_PAGE_ADDR_MODE,
        0x00,
        0x10,
        OLED_CMD_SET_CHARGE_PUMP,
        0x14,
        OLED_CMD_DEACTIVE_SCROLL,
        OLED_CMD_DISPLAY_NORMAL,
        OLED_CMD_DISPLAY_ON,
    };

    esp_err_t rc = i2c_master_transmit(dev->_I2CHandle, cmd_buf, sizeof(cmd_buf), 100);
    if (rc == ESP_OK) {
        ESP_LOGI(tag, "OLED configured successfully");
    } else {
        ESP_LOGE(tag, "OLED configuration failed. code: 0x%.2X", rc);
    }
}

void i2c_display_image(SSD1306_t * dev, int page, int seg, uint8_t * images, int width)
{
    if (page >= dev->_pages) return;
    if (seg >= dev->_width) return;

    int _seg = seg + CONFIG_OFFSETX;
    uint8_t columLow  = _seg & 0x0F;
    uint8_t columHigh = (_seg >> 4) & 0x0F;

    int _page = page;
    if (dev->_flip) {
        _page = (dev->_pages - page) - 1;
    }

    /* Transação 1: define página e coluna (4 bytes na stack — OK) */
    uint8_t addr_buf[] = {
        OLED_CONTROL_BYTE_CMD_STREAM,
        (0x00 + columLow),
        (0x10 + columHigh),
        0xB0 | _page,
    };
    i2c_master_transmit(dev->_I2CHandle, addr_buf, sizeof(addr_buf), 100);

    /*
     * Transação 2: byte de controle + pixels em um único frame I2C sem
     * alocar buffer extra na stack. NÃO usar uint8_t data_buf[129] aqui —
     * esta função é chamada ~16x por ssd1306_display_text e uma alocação
     * grande na stack causa overflow na task_rx (stack = 4096 bytes).
     */
    static const uint8_t data_ctrl = OLED_CONTROL_BYTE_DATA_STREAM;
    i2c_master_transmit_multi_buffer_info_t seg_bufs[] = {
        { .write_buffer = &data_ctrl, .buffer_size = 1     },
        { .write_buffer = images,     .buffer_size = width },
    };
    i2c_master_multi_buffer_transmit(dev->_I2CHandle, seg_bufs, 2, 100);
}

void i2c_contrast(SSD1306_t * dev, int contrast)
{
    int _contrast = contrast;
    if (contrast < 0x0) _contrast = 0;
    if (contrast > 0xFF) _contrast = 0xFF;

    uint8_t buf[] = {
        OLED_CONTROL_BYTE_CMD_STREAM,
        OLED_CMD_SET_CONTRAST,
        _contrast,
    };
    i2c_master_transmit(dev->_I2CHandle, buf, sizeof(buf), 100);
}

void i2c_hardware_scroll(SSD1306_t * dev, ssd1306_scroll_type_t scroll)
{
    uint8_t buf[12];
    int len = 0;
    buf[len++] = OLED_CONTROL_BYTE_CMD_STREAM;

    if (scroll == SCROLL_RIGHT) {
        buf[len++] = OLED_CMD_HORIZONTAL_RIGHT;
        buf[len++] = 0x00; buf[len++] = 0x00; buf[len++] = 0x07;
        buf[len++] = 0x07; buf[len++] = 0x00; buf[len++] = 0xFF;
        buf[len++] = OLED_CMD_ACTIVE_SCROLL;
    } else if (scroll == SCROLL_LEFT) {
        buf[len++] = OLED_CMD_HORIZONTAL_LEFT;
        buf[len++] = 0x00; buf[len++] = 0x00; buf[len++] = 0x07;
        buf[len++] = 0x07; buf[len++] = 0x00; buf[len++] = 0xFF;
        buf[len++] = OLED_CMD_ACTIVE_SCROLL;
    } else if (scroll == SCROLL_DOWN) {
        buf[len++] = OLED_CMD_CONTINUOUS_SCROLL;
        buf[len++] = 0x00; buf[len++] = 0x00; buf[len++] = 0x07;
        buf[len++] = 0x00; buf[len++] = 0x3F;
        buf[len++] = OLED_CMD_VERTICAL; buf[len++] = 0x00;
        buf[len++] = (dev->_height == 64) ? 0x40 : 0x20;
        buf[len++] = OLED_CMD_ACTIVE_SCROLL;
    } else if (scroll == SCROLL_UP) {
        buf[len++] = OLED_CMD_CONTINUOUS_SCROLL;
        buf[len++] = 0x00; buf[len++] = 0x00; buf[len++] = 0x07;
        buf[len++] = 0x00; buf[len++] = 0x01;
        buf[len++] = OLED_CMD_VERTICAL; buf[len++] = 0x00;
        buf[len++] = (dev->_height == 64) ? 0x40 : 0x20;
        buf[len++] = OLED_CMD_ACTIVE_SCROLL;
    } else if (scroll == SCROLL_STOP) {
        buf[len++] = OLED_CMD_DEACTIVE_SCROLL;
    }

    esp_err_t rc = i2c_master_transmit(dev->_I2CHandle, buf, len, 100);
    if (rc == ESP_OK) {
        ESP_LOGD(tag, "Scroll command succeeded");
    } else {
        ESP_LOGE(tag, "Scroll command failed. code: 0x%.2X", rc);
    }
}
