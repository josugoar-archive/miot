/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "protocol_examples_common.h"

#include "esp_log.h"
#include "mqtt_client.h"

#define TOPIC "/EDIFICIO_3/P_4/N/12"
#define TOPIC_COUNT 4
#define TOPIC_MAX_LEN 32

struct mqtt_publish_args
{
    esp_mqtt_client_handle_t client;
    char *topic;
};

struct mqtt_event_handler_args
{
    esp_timer_handle_t timer;
    char *topic;
    uint64_t period;
};

static const char *TAG = "mqtt_example";

static char *sensor_topics[TOPIC_COUNT] = {"/TEMP", "/HUM", "/LUX", "/VIBR"};

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    struct mqtt_event_handler_args *mqtt_event_handler_args = (struct mqtt_event_handler_args *)handler_args;
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        char topic[TOPIC_MAX_LEN];
        strcpy(topic, mqtt_event_handler_args->topic);
        strcat(topic, "/+");
        msg_id = esp_mqtt_client_subscribe(client, topic, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        char *filter = event->topic + strlen(mqtt_event_handler_args->topic);
        size_t filter_len = event->topic_len - strlen(mqtt_event_handler_args->topic);
        if (strncmp(filter, "/interval", filter_len) == 0)
        {
            mqtt_event_handler_args->period = strtoul(event->data, NULL, 10) * 1000;
            esp_timer_restart(mqtt_event_handler_args->timer, mqtt_event_handler_args->period);
            ESP_LOGI(TAG, "interval=%" PRIu64 "", mqtt_event_handler_args->period);
        }
        else if (strncmp(filter, "/disable", filter_len) == 0)
        {
            esp_timer_stop(mqtt_event_handler_args->timer);
            ESP_LOGI(TAG, "timer disabled");
        }
        else if (strncmp(filter, "/enable", filter_len) == 0)
        {
            esp_timer_start_periodic(mqtt_event_handler_args->timer, mqtt_event_handler_args->period);
            ESP_LOGI(TAG, "timer enabled");
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_publish_timer(void *pvParameters)
{
    struct mqtt_publish_args *mqtt_publish_args = (struct mqtt_publish_args *)pvParameters;
    char data = esp_random();
    int msg_id = esp_mqtt_client_publish(mqtt_publish_args->client, mqtt_publish_args->topic, &data, sizeof(data), 0, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .credentials.set_null_client_id = true,
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0)
    {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128)
        {
            int c = fgetc(stdin);
            if (c == '\n')
            {
                line[count] = '\0';
                break;
            }
            else if (c > 0 && c < 127)
            {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    }
    else
    {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    for (size_t i = 0; i < TOPIC_COUNT; ++i)
    {

        static char topics[TOPIC_COUNT][TOPIC_MAX_LEN];
        strcpy(topics[i], TOPIC);
        strcat(topics[i], sensor_topics[i]);

        esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_start(client);

        static struct mqtt_publish_args mqtt_publish_args[TOPIC_COUNT];
        mqtt_publish_args[i].topic = topics[i];
        mqtt_publish_args[i].client = client;
        esp_timer_create_args_t mqtt_timer_args = {
            .callback = mqtt_publish_timer,
            .arg = &mqtt_publish_args[i],
        };
        esp_timer_handle_t mqtt_publish_timer;
        esp_timer_create(&mqtt_timer_args, &mqtt_publish_timer);

        static struct mqtt_event_handler_args mqtt_event_handler_args[TOPIC_COUNT];
        mqtt_event_handler_args[i].timer = mqtt_publish_timer;
        mqtt_event_handler_args[i].topic = topics[i];
        mqtt_event_handler_args[i].period = 0;
        esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, &mqtt_event_handler_args[i]);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
}
