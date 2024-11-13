#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "gp2y0a41sk0f.h"

const static char *TAG = "EXAMPLE";

static struct gp2y0a41sk0f gp2y0a41sk0f;

static size_t dist_avg_idx = 0;
static float dist_avg[CONFIG_EXAMPLE_N] = {0};

static void gp2y0a41sk0f_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    float *dist = data;
    ESP_LOGI(TAG, "Distance: %f cm", *dist);

    dist_avg[dist_avg_idx] = *dist;
    dist_avg_idx = (dist_avg_idx + 1) % CONFIG_EXAMPLE_N;
    float sum = 0;
    for (size_t i = 0; i < CONFIG_EXAMPLE_N; i++)
    {
        sum += dist_avg[i];
    }
    float avg = sum / CONFIG_EXAMPLE_N;
    ESP_LOGI(TAG, "Distance average: %f cm", avg);
}

void app_main(void)
{
    esp_event_loop_args_t event_loop_args = {
        .queue_size = 32,
        .task_name = "esp_event_loop_run_task",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY,
    };
    esp_event_loop_handle_t event_loop;
    ESP_ERROR_CHECK(esp_event_loop_create(&event_loop_args, &event_loop));
    ESP_ERROR_CHECK(esp_event_handler_register_with(event_loop, GP2Y0A41SK0F_EVENT_BASE, GP2Y0A41SK0F_EVENT_DATA, gp2y0a41sk0f_event_handler, NULL));

    ESP_ERROR_CHECK(gp2y0a41sk0f_init(&gp2y0a41sk0f, event_loop));
    ESP_ERROR_CHECK(gp2y0a41sk0f_start(&gp2y0a41sk0f));
}
