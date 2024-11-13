#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

static const char *TAG = "i2c-tools";

static gpio_num_t i2c_gpio_sda = CONFIG_EXAMPLE_I2C_MASTER_SDA;
static gpio_num_t i2c_gpio_scl = CONFIG_EXAMPLE_I2C_MASTER_SCL;

static i2c_port_t i2c_port = I2C_NUM_0;

typedef unsigned short crc;

static crc crcSlow(unsigned char const message[], int nBytes)
{
    uint8_t crc = 0x00;
    uint8_t poly = 0x31;

    for (size_t i = 0; i < nBytes; i++)
    {
        crc ^= message[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x80)
            {
                crc = (crc << 1) ^ poly;
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void app_main(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = i2c_port,
        .scl_io_num = i2c_gpio_scl,
        .sda_io_num = i2c_gpio_sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    i2c_new_master_bus(&i2c_bus_config, &bus_handle);

    const i2c_device_config_t i2c_dev_cfg = {
        .device_address = 0x40,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t dev_handle;
    i2c_master_bus_add_device(bus_handle, &i2c_dev_cfg, &dev_handle);

    while (true)
    {
        uint8_t buf[3] = {0xE3, 0, 0};
        i2c_master_transmit(dev_handle, buf, 1, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(20));
        i2c_master_receive(dev_handle, buf, 3, pdMS_TO_TICKS(100));

        uint16_t temp_raw = buf[0] << 8 | buf[1];
        double temp = 175.72 * temp_raw / 65536.0 - 46.85;
        ESP_LOGI(TAG, "Temperature: %f", temp);

        uint8_t chxsum = buf[2];
        ESP_LOGI(TAG, "Checksum: %u", chxsum);

        crc crc = crcSlow(buf, 2);
        ESP_LOGI(TAG, "CRC: %u", crc);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
