#ifndef BUTTON_H
#define BUTTON_H

#include "driver/gpio.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"

ESP_EVENT_DECLARE_BASE(BUTTON_EVENT);

enum
{
    BUTTON_EVENT_PRESSED
};

void button_sampler(TickType_t sampling_period, esp_event_loop_handle_t event_loop_handle, gpio_num_t gpio_num);

#endif
