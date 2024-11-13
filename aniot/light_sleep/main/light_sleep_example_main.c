/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "light_sleep_example.h"
#include "nvs_flash.h"
#include "esp_random.h"

static nvs_handle_t nvs;

static void sensor_timer(void *arg)
{
    uint32_t data = esp_random();
    nvs_set_u32(nvs, "data", data);
}

static void sleep_timer(void *arg)
{
    const char *wakeup_reason;
    switch (esp_sleep_get_wakeup_cause())
    {
    case ESP_SLEEP_WAKEUP_TIMER:
        wakeup_reason = "timer";
        break;
    case ESP_SLEEP_WAKEUP_GPIO:
        wakeup_reason = "pin";
        break;
    case ESP_SLEEP_WAKEUP_UART:
        wakeup_reason = "uart";
        vTaskDelay(1);
        break;
#if EXAMPLE_TOUCH_LSLEEP_WAKEUP_SUPPORT
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        wakeup_reason = "touch";
        break;
#endif
    default:
        wakeup_reason = "other";
        break;
    }
    printf("Wakeup reason: %s\n", wakeup_reason);
    printf("Writing wakeup reason to NVS\n");
    nvs_set_str(nvs, "wakeup_reason", wakeup_reason);

    printf("Delaying timer\n");
    vTaskDelay(pdMS_TO_TICKS(3000));

    static size_t count = 0;
    ++count;

    if (count % 5 == 0)
    {
        printf("Entering deep sleep\n");
        esp_deep_sleep_start();
        return;
    }

    printf("Entering light sleep\n");
    esp_light_sleep_start();

    int64_t t_after_us = esp_timer_get_time();

    printf("Returned from light sleep, t=%lld ms\n", t_after_us / 1000);
}

void app_main(void)
{
    /* Enable wakeup from light sleep by gpio */
    example_register_gpio_wakeup();
    /* Enable wakeup from light sleep by timer */
    example_register_timer_wakeup();
    /* Enable wakeup from light sleep by uart */
    example_register_uart_wakeup();
#if EXAMPLE_TOUCH_LSLEEP_WAKEUP_SUPPORT
    /* Enable wakeup from light sleep by touch element */
    example_register_touch_wakeup();
#endif

    nvs_flash_init();
    nvs_open("storage", NVS_READWRITE, &nvs);

    esp_timer_create_args_t sensor_timer_args = {
        .callback = sensor_timer,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sensor_timer"};
    esp_timer_handle_t sensor_timer_handle;
    esp_timer_create(&sensor_timer_args, &sensor_timer_handle);
    esp_timer_start_periodic(sensor_timer_handle, 1000000);

    esp_timer_create_args_t sleep_timer_args = {
        .callback = sleep_timer,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sleep_timer"};
    esp_timer_handle_t sleep_timer_handle;
    esp_timer_create(&sleep_timer_args, &sleep_timer_handle);
    esp_timer_start_periodic(sleep_timer_handle, 500000);
}
