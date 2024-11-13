/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "esp_timer.h"
#include "shtc3.h"

static const char *TAG = "example";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

static uint8_t s_led_state = 0;

#ifdef CONFIG_BLINK_LED_STRIP

static led_strip_handle_t led_strip;

static void blink_led(void)
{
    /* If the addressable LED is enabled */
    if (s_led_state)
    {
        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        /* Refresh the strip to send data */
        led_strip_refresh(led_strip);
    }
    else
    {
        /* Set all LED off to clear all pixels */
        led_strip_clear(led_strip);
    }
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };
#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#else
#error "unsupported LED type"
#endif

float temp;
float hum;

shtc3_t tempSensor;
i2c_master_bus_handle_t bus_handle;

void init_i2c(void)
{
    uint16_t id;
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = 8,
        .sda_io_num = 10,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    shtc3_init(&tempSensor, bus_handle, 0x70);
}

static void periodic_timer_callback_1(void *arg)
{
    ESP_ERROR_CHECK(shtc3_get_temp_and_hum(&tempSensor, &temp, &hum));
}

static void periodic_timer_callback_2(void *arg)
{
    ESP_LOGI(TAG, "Temperature: %.2f, Humidity: %.2f", temp, hum);
}

#define GPIO_INPUT_IO 9
#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_INPUT_IO)
#define ESP_INTR_FLAG_DEFAULT 0

static void gpio_isr_handler(void *arg)
{
    ESP_LOGI(TAG, "Button pressed");
}

void app_main(void)
{
    configure_led();
    init_i2c();

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    gpio_isr_handler_add(GPIO_INPUT_IO, gpio_isr_handler, (void *)GPIO_INPUT_IO);

    const esp_timer_create_args_t periodic_timer_args_1 = {
        .callback = &periodic_timer_callback_1,
        .name = "periodic_1"};
    const esp_timer_create_args_t periodic_timer_args_2 = {
        .callback = &periodic_timer_callback_2,
        .name = "periodic_2"};

    esp_timer_handle_t periodic_timer_1;
    esp_timer_handle_t periodic_timer_2;

    esp_timer_create(&periodic_timer_args_1, &periodic_timer_1);
    esp_timer_create(&periodic_timer_args_2, &periodic_timer_2);

    esp_timer_start_periodic(periodic_timer_1, 1000000);
    esp_timer_start_periodic(periodic_timer_2, 10000000);

    while (1)
    {
        if (temp < 20)
        {
#ifdef CONFIG_BLINK_LED_STRIP
            led_strip_clear(led_strip);
#elif CONFIG_BLINK_LED_GPIO
            gpio_set_level(BLINK_GPIO, 0);
#endif
        }
        else
        {
            uint32_t rgb = temp - 20;
#ifdef CONFIG_BLINK_LED_STRIP
            led_strip_set_pixel(led_strip, 0, 16, 16, 16);
            led_strip_refresh(led_strip);
#elif CONFIG_BLINK_LED_GPIO
            gpio_set_level(BLINK_GPIO, 1);
#endif
        }

        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }

    esp_timer_stop(periodic_timer_2);
    esp_timer_stop(periodic_timer_1);

    esp_timer_delete(periodic_timer_2);
    esp_timer_delete(periodic_timer_1);
}
