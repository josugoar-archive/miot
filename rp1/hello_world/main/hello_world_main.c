/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

static TickType_t xMsToDelay1 = CONFIG_TASK_1_MS_TO_DELAY;
static TickType_t xMsToDelay2 = CONFIG_TASK_2_MS_TO_DELAY;

void hello_task(void *pvParameter)
{
    TickType_t *xMsToDelay = (TickType_t *)pvParameter;

    printf("Hello world!\n");
    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(*xMsToDelay / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

void app_main()
{
    xTaskCreate( &hello_task, "hello_task_1", 2048, &xMsToDelay1, 5, NULL );
    xTaskCreate( &hello_task, "hello_task_2", 2048, &xMsToDelay2, 5, NULL );
}
