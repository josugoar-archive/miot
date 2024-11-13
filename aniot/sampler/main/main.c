#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#include "button.h"
#include "mock-flash.h"
#include "mock_wifi.h"
#include "shtc3.h"

enum mode
{
    MODE_MONITORING,
    MODE_CONSOLE
};

ESP_EVENT_DECLARE_BASE(SENSOR_EVENT);

enum
{
    SENSOR_EVENT_NEWSAMPLE
};

struct sensor_data
{
    float temp;
    float hum;
};

ESP_EVENT_DEFINE_BASE(SENSOR_EVENT);

static enum mode mode = MODE_MONITORING;

static enum mock_wifi_state wifi_state = NOT_INITIALIZED;

static esp_event_loop_handle_t event_loop_handle;

static shtc3_t sensor;

static void wifi_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_MOCK_EVENT_WIFI_CONNECTED:
        wifi_state = CONNECTED;
        break;
    case WIFI_MOCK_EVENT_WIFI_GOT_IP:
        wifi_state = CONNECTED_WITH_IP;
        for (size_t i = 0; i < getDataLeft(); ++i)
        {
            float *data = readFromFlash(sizeof(*data));
            send_data_wifi(data, sizeof(*data));
        }
        break;
    case WIFI_MOCK_EVENT_WIFI_DISCONNECTED:
        wifi_state = DISCONNECTED;
        if (mode != MODE_CONSOLE)
        {
            wifi_connect();
        }
        break;
    default:
        break;
    }
}

static void sensor_newsample_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    struct sensor_data *data = (struct sensor_data *)event_data;
    if (wifi_state == CONNECTED_WITH_IP)
    {
        send_data_wifi(data, sizeof(*data));
    }
    else
    {
        writeToFlash(data, sizeof(*data));
    }
}

static void sensor_task(void *pvParameter)
{
    TickType_t sampling_period = (TickType_t)pvParameter;
    while (pdTRUE)
    {
        if (mode == MODE_MONITORING)
        {
            struct sensor_data data;
            if (shtc3_get_temp_and_hum(&sensor, &data.temp, &data.hum) == ESP_OK)
            {
                esp_event_post_to(event_loop_handle, SENSOR_EVENT, SENSOR_EVENT_NEWSAMPLE, &data, sizeof(data), 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(sampling_period));
    }
    vTaskDelete(NULL);
}

static void button_pressed_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    mode = MODE_CONSOLE;
    if (wifi_state != DISCONNECTED)
    {
        wifi_disconnect();
    }
}

static void button_task(void *pvParameter)
{
    TickType_t sampling_period = (TickType_t)pvParameter;
    button_sampler(sampling_period, event_loop_handle, GPIO_NUM_9);
    vTaskDelete(NULL);
}

static int monitor_cmd(int argc, char **argv)
{
    if (mode != MODE_CONSOLE)
    {
        return EXIT_FAILURE;
    }
    mode = MODE_MONITORING;
    wifi_connect();
    return EXIT_SUCCESS;
}

static int quota_cmd(int argc, char **argv)
{
    if (mode != MODE_CONSOLE)
    {
        return EXIT_FAILURE;
    }
    printf("%zu\n", getDataLeft());
    return EXIT_SUCCESS;
}

void app_main(void)
{
    esp_event_loop_args_t event_loop_args = {
        .queue_size = CONFIG_ESP_SYSTEM_EVENT_QUEUE_SIZE,
        .task_name = "esp_event_loop_run_task",
        .task_priority = ESP_TASKD_EVENT_PRIO,
        .task_stack_size = ESP_TASKD_EVENT_STACK,
        .task_core_id = tskNO_AFFINITY,
    };
    esp_event_loop_create(&event_loop_args, &event_loop_handle);
    esp_event_handler_instance_register_with(event_loop_handle, WIFI_MOCK, ESP_EVENT_ANY_ID, wifi_handler, NULL, NULL);
    esp_event_handler_instance_register_with(event_loop_handle, SENSOR_EVENT, SENSOR_EVENT_NEWSAMPLE, sensor_newsample_handler, NULL, NULL);
    esp_event_handler_instance_register_with(event_loop_handle, BUTTON_EVENT, BUTTON_EVENT_PRESSED, button_pressed_handler, NULL, NULL);

    mock_flash_init(configMINIMAL_STACK_SIZE);

    wifi_mock_init(event_loop_handle);
    wifi_state = INITIALIZED;
    wifi_connect();

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 10,
        .scl_io_num = 8,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true};
    i2c_master_bus_handle_t bus_handle;
    i2c_new_master_bus(&bus_config, &bus_handle);
    shtc3_init(&sensor, bus_handle, 0x70);
    xTaskCreate(sensor_task, "sensor_task", configMINIMAL_STACK_SIZE, (void *)(CONFIG_SENSOR_SAMPLING_PERIOD * 1000), uxTaskPriorityGet(NULL), NULL);

    gpio_config_t io_config = {
        .pin_bit_mask = 1ULL << GPIO_NUM_9,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE};
    gpio_config(&io_config);
    xTaskCreate(button_task, "button_task", configMINIMAL_STACK_SIZE, (void *)(CONFIG_BUTTON_SAMPLING_PERIOD * 1000), uxTaskPriorityGet(NULL), NULL);

    esp_console_register_help_command();
    esp_console_cmd_t cmd_monitor = {
        .command = "monitor",
        .func = monitor_cmd};
    esp_console_cmd_register(&cmd_monitor);
    esp_console_cmd_t cmd_quota = {
        .command = "quota",
        .func = quota_cmd};
    esp_console_cmd_register(&cmd_quota);
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_repl_t *repl;
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl);
#else
#error Unsupported console type
#endif
    esp_console_start_repl(repl);
}
