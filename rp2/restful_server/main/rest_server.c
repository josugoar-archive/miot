/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "cbor.h"

static const char *REST_TAG = "esp-rest";
#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Simple handler for light brightness control */
static esp_err_t light_brightness_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    int red = cJSON_GetObjectItem(root, "red")->valueint;
    int green = cJSON_GetObjectItem(root, "green")->valueint;
    int blue = cJSON_GetObjectItem(root, "blue")->valueint;
    ESP_LOGI(REST_TAG, "Light control: red = %d, green = %d, blue = %d", red, green, blue);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

static esp_err_t info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    time_t timer = esp_random();
    struct tm *t = localtime(&timer);
    char s[100];
    strftime(s, sizeof(s), "%d %m %Y %H:%M", t);
    cJSON_AddStringToObject(root, "fecha", s);
    cJSON *information = cJSON_AddObjectToObject(root, "info");
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(information, "version", IDF_VER);
    cJSON_AddStringToObject(information, "hostname", CONFIG_EXAMPLE_MDNS_HOST_NAME);
    cJSON_AddNumberToObject(information, "nucleos", chip_info.cores);
    cJSON *sensores = cJSON_AddArrayToObject(root, "sensores");
    cJSON *temperatura = cJSON_CreateObject();
    uint32_t temp = esp_random() % 20;
    cJSON_AddNumberToObject(temperatura, "temperatura_f", (double)temp * 1.8 + 32);
    cJSON_AddNumberToObject(temperatura, "temperatura_c", temp);
    cJSON_AddItemToArray(sensores, temperatura);
    cJSON *humedad = cJSON_CreateObject();
    cJSON_AddNumberToObject(humedad, "humedad", esp_random() % 20);
    cJSON_AddItemToArray(sensores, humedad);
    const char *info = cJSON_Print(root);
    httpd_resp_sendstr(req, info);
    free((void *)info);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t info_cbor_get_handler(httpd_req_t *req)
{
    // Tipo de respuesta.
    httpd_resp_set_type(req, "application/cbor");

    CborEncoder root_encoder;
    uint8_t buf[256];

    // Codificador CBOR.
    cbor_encoder_init(&root_encoder, buf, sizeof(buf), 0);

    // Codificamos CBOR.
    CborEncoder map_encoder;
    // {
    cbor_encoder_create_map(&root_encoder, &map_encoder, 3);
    time_t timer = esp_random();
    struct tm *t = localtime(&timer);
    char s[100];
    strftime(s, sizeof(s), "%d %m %Y %H:%M", t);
    //      fecha:
    cbor_encode_text_stringz(&map_encoder, "fecha");
    //             <string>
    cbor_encode_text_stringz(&map_encoder, s);
    //      info:
    cbor_encode_text_stringz(&map_encoder, "info");
    CborEncoder info_map_encoder;
    //            {
    cbor_encoder_create_map(&map_encoder, &info_map_encoder, 3);
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    //                 version:
    cbor_encode_text_stringz(&info_map_encoder, "version");
    //                          <string>
    cbor_encode_text_stringz(&info_map_encoder, IDF_VER);
    //                 hostname:
    cbor_encode_text_stringz(&info_map_encoder, "hostname");
    //                           <string>
    cbor_encode_text_stringz(&info_map_encoder, CONFIG_EXAMPLE_MDNS_HOST_NAME);
    //                 nucleos:
    cbor_encode_text_stringz(&info_map_encoder, "nucleos");
    //                          <uint>
    cbor_encode_uint(&info_map_encoder, chip_info.cores);
    // /          }
    cbor_encoder_close_container(&map_encoder, &info_map_encoder);
    //      sensores:
    cbor_encode_text_stringz(&map_encoder, "sensores");
    uint32_t temp = esp_random() % 20;
    CborEncoder array_encoder;
    //                [
    cbor_encoder_create_array(&map_encoder, &array_encoder, 2);
    CborEncoder tenperature_map_encoder;
    //                     {
    cbor_encoder_create_map(&array_encoder, &tenperature_map_encoder, 2);
    //                          temperatura_f:
    cbor_encode_text_stringz(&tenperature_map_encoder, "temperatura_f");
    //                                         <double>
    cbor_encode_double(&tenperature_map_encoder, (double)temp * 1.8 + 32);
    //                          temperatura_c:
    cbor_encode_text_stringz(&tenperature_map_encoder, "temperatura_c");
    //                                         <uint>
    cbor_encode_uint(&tenperature_map_encoder, temp);
    //                     }
    cbor_encoder_close_container(&array_encoder, &tenperature_map_encoder);
    CborEncoder humidity_map_encoder;
    //                     {
    cbor_encoder_create_map(&array_encoder, &humidity_map_encoder, 1);
    //                          humedad:
    cbor_encode_text_stringz(&humidity_map_encoder, "humedad");
    //                                   <int>
    cbor_encode_int(&humidity_map_encoder, esp_random() % 20);
    //                     }
    cbor_encoder_close_container(&array_encoder, &humidity_map_encoder);
    //                ]
    cbor_encoder_close_container(&map_encoder, &array_encoder);
    // }
    cbor_encoder_close_container(&root_encoder, &map_encoder);

    // Enviamos respuesta, consultando previamente el tamaño del buffer codificado.
    httpd_resp_send(req, (char *)buf, cbor_encoder_get_buffer_size(&root_encoder, buf));

    return ESP_OK;
}

/* Simple handler for getting system handler */
static esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "version", IDF_VER);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Simple handler for getting temperature data */
static esp_err_t temperature_data_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "raw", esp_random() % 20);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Simple handler for getting temperature data in fahrenheit */
static esp_err_t temperature_fahrenheit_data_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "fahrenheit", (double)(esp_random() % 20) * 1.8 + 32);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t start_rest_server(const char *base_path)
{
    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    /* URI handler for fetching info */
    httpd_uri_t info_get_uri = {
        .uri = "/api/v1/info",
        .method = HTTP_GET,
        .handler = info_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &info_get_uri);

    /* URI handler for fetching info */
    httpd_uri_t info_cbor_get_uri = {
        .uri = "/api/v1/info/cbor",
        .method = HTTP_GET,
        .handler = info_cbor_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &info_cbor_get_uri);

    /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/v1/system/info",
        .method = HTTP_GET,
        .handler = system_info_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_info_get_uri);

    /* URI handler for fetching temperature data */
    httpd_uri_t temperature_data_get_uri = {
        .uri = "/api/v1/temp/raw",
        .method = HTTP_GET,
        .handler = temperature_data_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &temperature_data_get_uri);

    /* URI handler for fetching temperature data in fahrenheit */
    httpd_uri_t temperature_fahrenheit_data_get_uri = {
        .uri = "/api/v1/temp/fahrenheit",
        .method = HTTP_GET,
        .handler = temperature_fahrenheit_data_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &temperature_fahrenheit_data_get_uri);

    /* URI handler for light brightness control */
    httpd_uri_t light_brightness_post_uri = {
        .uri = "/api/v1/light/brightness",
        .method = HTTP_POST,
        .handler = light_brightness_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &light_brightness_post_uri);

    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &common_get_uri);

    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
