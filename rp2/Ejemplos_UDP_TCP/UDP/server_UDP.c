#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define STARTCHAR1 0x42
#define STARTCHAR2 0x4D
#define FRAMELEN (2 * 13 + 2)

typedef struct {
	uint8_t  startchar1;      // 0x42 (fixed)
	uint8_t  startchar2;      // 0x4d (fixed)
	uint16_t framelen;        // Frame length = 2 * 13 + 2
	uint16_t pm10_standard;   // PM1.0 concentration unit ug/m3 (CF=1，standard particle)
	uint16_t pm25_standard;   // PM2.5 concentration unit ug/m3 (CF=1，standard particle)
	uint16_t pm100_standard;  // PM10 concentration unit ug/m3 (CF=1，standard particle)
	uint16_t pm10_env;        // PM1.0 concentration unit ug/m3 (under atmospheric environment)
	uint16_t pm25_env;        // PM2.5 concentration unit ug/m3 (under atmospheric environment)
	uint16_t pm100_env;       // PM10 concentration unit ug/m3 (under atmospheric environment)
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

int main(int argc, char *argv[]) {
	// port to start the server on
	int SERVER_PORT = 3333;

	// socket address used for the server
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// htons: host to network short: transforms a value in host byte
	// ordering format to a short value in network byte ordering format
	server_address.sin_port = htons(SERVER_PORT);

	// htons: host to network long: same as htons but to long
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);

	// create a UDP socket, creation returns -1 on failure
	int sock;
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Error en creacion de socket\n");
		return 1;
	}

	// bind it to listen to the incoming connections on the created server
	// address, will return -1 on error
	if ((bind(sock, (struct sockaddr *)&server_address,
	          sizeof(server_address))) < 0) {
		printf("Error en bind\n");
		return 1;
	}

	// socket address used to store client address
	struct sockaddr_in client_address;
	int client_address_len = 0;

	// run indefinitely
	while (true) {
		mensaje data_to_receive;

		int maxlen = 500;
		char buffer[maxlen];

		// read content into buffer from an incoming client
		int len = recvfrom(sock, &data_to_receive, sizeof(data_to_receive), 0,
		                   (struct sockaddr *)&client_address,
		                   &client_address_len);
		if (len <= 0) {
			break;
		}

		if (data_to_receive.startchar1 != STARTCHAR1 || data_to_receive.startchar2 != STARTCHAR2) {
			snprintf(buffer, maxlen, "Error de secuencia de inicio");
			goto response;
		}

		if (data_to_receive.framelen != FRAMELEN) {
			snprintf(buffer, maxlen, "Error de tamaño del mensaje");
			goto response;
		}

		const uint16_t received_checksum = data_to_receive.checksum;
		const uint16_t expected_checksum = calculate_checksum(&data_to_receive);
		if (received_checksum != expected_checksum) {
			snprintf(buffer, maxlen, "Error de checksum: se obtuvo '%x' pero se esperaba '%x'", received_checksum, expected_checksum);
			goto response;
		}

		// inet_ntoa prints user friendly representation of the
		// ip address
		snprintf(buffer, maxlen, "Recibido: PM1.0 (CF=1) = '%d', PM2.5 (CF=1) ='%d', PM10 (CF=1)='%d' [...] desde el cliente %s", data_to_receive.pm10_standard, data_to_receive.pm25_standard, data_to_receive.pm100_standard, inet_ntoa(client_address.sin_addr));

		response:
		sendto(sock, &buffer, strlen(buffer), 0, (struct sockaddr *)&client_address,
		       sizeof(client_address));
	}

	return 0;
}
