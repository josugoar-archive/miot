#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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

int main() {
	const char* server_name = "localhost";
	const int server_port = 3333;

	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// creates binary representation of server name
	// and stores it as sin_addr
	// http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
	inet_pton(AF_INET, server_name, &server_address.sin_addr);

	// htons: port in network order format
	server_address.sin_port = htons(server_port);

	// open socket
	int sock;
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Error en creacion de socket\n");
		return 1;
	}

	while (true) {
		// data that will be sent to the server
		const mensaje data_to_send = generate_sensor_data();

		// send data
		sendto(sock, &data_to_send, sizeof(data_to_send), 0,
			   (struct sockaddr*)&server_address, sizeof(server_address));

		// received data back
		int n = 0;
		int len = 0, maxlen = 500;
		char buffer[maxlen];
		char* pbuffer = buffer;

		n = recvfrom(sock, pbuffer, maxlen, 0, NULL, NULL);
		len += n;

		buffer[len] = '\0';
		printf("Recibido: '%s' desde el servidor\n", buffer);
		fflush(stdout);

		sleep(2);
	}

	// close the socket
	close(sock);
	return 0;
}
