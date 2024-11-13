#include "button.h"

ESP_EVENT_DEFINE_BASE(BUTTON_EVENT);

void button_sampler(TickType_t sampling_period, esp_event_loop_handle_t event_loop_handle, gpio_num_t gpio_num)
{
    int level_prev = -1;
    while (pdTRUE)
    {
        int level_cur = gpio_get_level(gpio_num);
        if (level_prev == 1 && level_cur == 0)
        {
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_post_to(event_loop_handle, BUTTON_EVENT, BUTTON_EVENT_PRESSED, NULL, 0, 0));
        }
        level_prev = level_cur;
        vTaskDelay(pdMS_TO_TICKS(sampling_period));
    }
}
