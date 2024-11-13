/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#ifdef CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN
#include "addr_from_stdin.h"
#endif

#if defined(CONFIG_EXAMPLE_IPV4)
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
#elif defined(CONFIG_EXAMPLE_IPV6)
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV6_ADDR
#else
#define HOST_IP_ADDR ""
#endif

#define PORT CONFIG_EXAMPLE_PORT

static const char *TAG = "example";

#define STARTCHAR1 0x42
#define STARTCHAR2 0x4D
#define FRAMELEN (2 * 13 + 2)
#define CALCULATE_CHECKSUM true

typedef struct {
	uint8_t  startchar1;      // 0x42 (fixed)
	uint8_t  startchar2;      // 0x4d (fixed)
	uint16_t framelen;        // Frame length = 2 * 13 + 2
	uint16_t pm10_standard;   // PM1.0 concentration unit ug/m3 (CF=1，standard particle)
	uint16_t pm25_standard;   // PM2.5 concentration unit ug/m3 (CF=1，standard particle)
	uint16_t pm100_standard;  // PM10 concentration unit ug/m3 (CF=1，standard particle)
	uint16_t pm10_env;        // M1.0 concentration unit ug/m3 (under atmospheric environment)
	uint16_t pm25_env;        // M2.5 concentration unit ug/m3 (under atmospheric environment)
	uint16_t pm100_env;       // M10 concentration unit ug/m3 (under atmospheric environment)
	uint16_t particles_03um;  // Number of particles with diameter beyond 0.3 um into 0.1 L of air
	uint16_t particles_05um;  // Number of particles with diameter beyond 0.5 um into 0.1 L of air
	uint16_t particles_10um;  // Number of particles with diameter beyond 1.0 um into 0.1 L of air
	uint16_t particles_25um;  // Number of particles with diameter beyond 2.5 um into 0.1 L of air
	uint16_t particles_50um;  // Number of particles with diameter beyond 5.0 um into 0.1 L of air
	uint16_t particles_100um; // Number of particles with diameter beyond 10.0 um into 0.1 L of air
	uint16_t unused;          // Reserved
	uint16_t checksum;        // Check code
} mensaje;

uint16_t calculate_checksum(const mensaje *const data) {
    const uint8_t *const ptr = (const uint8_t *)data;
    uint16_t checksum = 0;
    for (size_t i = 0; i < sizeof(*data) - (sizeof(data->unused) + sizeof(data->checksum)); ++i) {
        checksum += ptr[i];
    }
    return checksum;
}

mensaje generate_sensor_data(void) {
	mensaje data;
	uint8_t *const ptr = (uint8_t *)&data;
	for (size_t i = 0; i < sizeof(data); ++i) {
		ptr[i] = rand() % 256;
	}
    data.startchar1 = STARTCHAR1;
    data.startchar2 = STARTCHAR2;
	data.framelen = FRAMELEN;
#if CALCULATE_CHECKSUM == true
    data.checksum = calculate_checksum(&data);
#endif
	return data;
}


static void udp_client_task(void *pvParameters)
{
    char rx_buffer[500];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1) {

#if defined(CONFIG_EXAMPLE_IPV4)
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
#elif defined(CONFIG_EXAMPLE_IPV6)
        struct sockaddr_in6 dest_addr = { 0 };
        inet6_aton(HOST_IP_ADDR, &dest_addr.sin6_addr);
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        dest_addr.sin6_scope_id = esp_netif_get_netif_impl_index(EXAMPLE_INTERFACE);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
#elif defined(CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN)
        struct sockaddr_storage dest_addr = { 0 };
        ESP_ERROR_CHECK(get_addr_from_stdin(PORT, SOCK_DGRAM, &ip_protocol, &addr_family, &dest_addr));
#endif

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        ESP_LOGI(TAG, "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);

        while (1) {
            const mensaje payload = generate_sensor_data();

            int err = sendto(sock, &payload, sizeof(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Message sent");

            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                if (strncmp(rx_buffer, "OK: ", 4) == 0) {
                    ESP_LOGI(TAG, "Received expected message, reconnecting");
                    break;
                }
            }

            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
}
