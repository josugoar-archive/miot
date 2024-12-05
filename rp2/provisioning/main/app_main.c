#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "cJSON.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "mqtt_client.h"
#include "protocol_examples_common.h"

#define HOST "demo.thingsboard.io"
#define PORT 1883

#define PROVISION_DEVICE_KEY "0ncxadzw2po3trlpn3v8"
#define PROVISION_DEVICE_SECRET "xvqb14ytsildkpggzyx3"
#define PROVISION_REQUEST "{\"provisionDeviceKey\":\"" PROVISION_DEVICE_KEY "\",\"provisionDeviceSecret\":\"" PROVISION_DEVICE_SECRET "\"}"

#define EDIFICIO "EDIFICIO_3"
#define PLANTA "P_4"
#define ALA "N"
#define SALA "12"
#define ATTRS "{\"edificio\":\"" EDIFICIO "\",\"planta\":\"" PLANTA "\",\"ala\":\"" ALA "\",\"sala\":" SALA "}"

#define PROVISION_REQUEST_TOPIC "/provision/request"
#define PROVISION_RESPONSE_TOPIC "/provision/response"
#define DATA_TOPIC "v1/devices/me/telemetry"
#define ATTR_TOPIC "v1/devices/me/attributes"

#define BUF_SIZE 64

static const char *TAG = "mqtt_example";

static char *RESULT_CODES[] = {
    [MQTT_CONNECTION_REFUSE_PROTOCOL] = "incorrect protocol version",
    [MQTT_CONNECTION_REFUSE_ID_REJECTED] = "invalid client identifier",
    [MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE] = "server unavailable",
    [MQTT_CONNECTION_REFUSE_BAD_USERNAME] = "bad username or password",
    [MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED] = "not authorised",
};

static nvs_handle_t nvs;

static TaskHandle_t mqtt_publish_task_handle;

static bool enabled = true;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_publish_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_client_config_t *mqtt_cfg = handler_args;
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        if (event->error_handle->connect_return_code != MQTT_CONNECTION_ACCEPTED)
        {
            ESP_LOGE(TAG, "[ThingsBoard client] Cannot connect to ThingsBoard!, result: %s", RESULT_CODES[event->error_handle->connect_return_code]);
            break;
        }
        ESP_LOGI(TAG, "[ThingsBoard client] Connected to ThingsBoard with credentials: %s", mqtt_cfg->credentials.username);
        esp_mqtt_client_subscribe(client, ATTR_TOPIC, 0);
        ESP_LOGI(TAG, "[ThingsBoard client] Sending attributes %s", ATTRS);
        esp_mqtt_client_publish(event->client, ATTR_TOPIC, ATTRS, strlen(ATTRS), 0, 0);
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
        cJSON *decoded_data = cJSON_Parse(event->data);
        enabled = cJSON_GetObjectItem(decoded_data, "enabled")->valueint;
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

static void mqtt_publish_task(void *pvParameters)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    char credentials_value[BUF_SIZE];
    size_t credentials_length;
    if (nvs_get_str(nvs, "credentialsVal", credentials_value, &credentials_length) != ESP_OK)
    {
        ESP_LOGE(TAG, "[ThingsBoard client] Cannot read credentials from nvs!");
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "[ThingsBoard client] Read credentials from nvs.");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.hostname = HOST,
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .broker.address.port = PORT,
        .credentials.username = credentials_value,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_publish_event_handler, &mqtt_cfg);
    esp_mqtt_client_start(client);

    while (1)
    {
        if (enabled)
        {
            char data[BUF_SIZE];
            snprintf(data, sizeof(data), "{\"temperature\":%ld}", (int32_t)esp_random());
            esp_mqtt_client_publish(client, DATA_TOPIC, data, 0, 0, 0);
            ESP_LOGI(TAG, "[ThingsBoard client] Published data: %s", data);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        if (event->error_handle->connect_return_code != MQTT_CONNECTION_ACCEPTED)
        {
            ESP_LOGE(TAG, "[Provisioning client] Cannot connect to ThingsBoard!, result: %s", RESULT_CODES[event->error_handle->connect_return_code]);
            break;
        }
        ESP_LOGI(TAG, "[Provisioning client] Connected to ThingsBoard");
        esp_mqtt_client_subscribe(client, PROVISION_RESPONSE_TOPIC, 0);
        ESP_LOGI(TAG, "[Provisioning client] Sending provisioning request %s", PROVISION_REQUEST);
        esp_mqtt_client_publish(client, PROVISION_REQUEST_TOPIC, PROVISION_REQUEST, strlen(PROVISION_REQUEST), 0, 0);
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
        ESP_LOGI(TAG, "[Provisioning client] Received data from ThingsBoard: %.*s", event->data_len, event->data);
        cJSON *decoded_data = cJSON_Parse(event->data);
        char *provision_device_status = cJSON_GetObjectItem(decoded_data, "status")->valuestring;
        if (strcmp(provision_device_status, "SUCCESS") == 0)
        {
            ESP_ERROR_CHECK(nvs_set_str(nvs, "credentialsVal", cJSON_GetObjectItem(decoded_data, "credentialsValue")->valuestring));
            ESP_ERROR_CHECK(nvs_commit(nvs));
        }
        else
        {
            ESP_LOGE(TAG, "[Provisioning client] Provisioning was unsuccessful with status %s and message: %s", provision_device_status, cJSON_GetObjectItem(decoded_data, "errorMsg")->valuestring);
        }
        esp_mqtt_client_disconnect(client);
        xTaskNotifyGive(mqtt_publish_task_handle);
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

static void mqtt_app_start(void)
{
    ESP_LOGI(TAG, "[Provisioning client] Connecting to ThingsBoard (provisioning client)");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.hostname = HOST,
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .broker.address.port = PORT,
        .credentials.username = "provision",
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client)
    {
        ESP_LOGE(TAG, "Client was not created!");
        return;
    }
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    xTaskCreate(mqtt_publish_task, "mqtt_publish_task", 4096, NULL, uxTaskPriorityGet(NULL), &mqtt_publish_task_handle);
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

    ESP_ERROR_CHECK(example_connect());

    nvs_open("credentials", NVS_READWRITE, &nvs);
    mqtt_app_start();
}
