#include "esp_websocket_client.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "WEBSOCKET_CLIENT"
#define BUFFER_SIZE 1024

struct websocket_client {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
};

websocket_client_handle_t websocket_client_init(const websocket_client_config_t *config) {
    if (config == NULL || config->uri == NULL) {
        ESP_LOGE(TAG, "Invalid configuration or URI");
        return NULL;
    }

    websocket_client_handle_t client = malloc(sizeof(struct websocket_client));
    if (!client) {
        ESP_LOGE(TAG, "Failed to allocate memory for WebSocket client");
        return NULL;
    }
    memset(client, 0, sizeof(struct websocket_client));

    // Copia o URI para um buffer local para manipulação segura
    char uri_copy[128];  // Ajuste o tamanho conforme necessário
    strncpy(uri_copy, config->uri, sizeof(uri_copy) - 1);
    uri_copy[sizeof(uri_copy) - 1] = '\0';  // Garante que a string seja terminada corretamente

    // Extrai a porta
    char *port_str = strrchr(uri_copy, ':');
    if (port_str == NULL) {
        ESP_LOGE(TAG, "Invalid URI format: Missing port");
        free(client);
        return NULL;
    }
    *port_str = '\0';  // Divide a string na posição do ':'
    port_str++;        // Avança para o número da porta

    client->server_addr.sin_port = htons(atoi(port_str));  // Converte a porta para formato de rede

    // Extrai o IP
    char *ip_str = strstr(uri_copy, "//");  // Ignora "ws://"
    if (ip_str == NULL) {
        ESP_LOGE(TAG, "Invalid URI format: Missing '//' after scheme");
        free(client);
        return NULL;
    }
    ip_str += 2;  // Avança para o IP

    if (inet_pton(AF_INET, ip_str, &client->server_addr.sin_addr) <= 0) {
        ESP_LOGE(TAG, "Invalid IP address in URI");
        free(client);
        return NULL;
    }

    client->server_addr.sin_family = AF_INET;

    ESP_LOGI(TAG, "WebSocket client initialized: IP=%s, Port=%s", ip_str, port_str);
    return client;
}


esp_err_t websocket_client_start(websocket_client_handle_t client) {
    client->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client->sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return ESP_FAIL;
    }

    if (connect(client->sock, (struct sockaddr *)&client->server_addr, sizeof(client->server_addr)) != 0) {
        ESP_LOGE(TAG, "Failed to connect to WebSocket server");
        close(client->sock);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Connected to WebSocket server");
    return ESP_OK;
}

esp_err_t websocket_client_send(websocket_client_handle_t client, const char *data, size_t len) {
    if (send(client->sock, data, len, 0) < 0) {
        ESP_LOGE(TAG, "Failed to send data");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Sent data: %s", data);
    return ESP_OK;
}

int websocket_client_receive(websocket_client_handle_t client, char *buffer, size_t len, int timeout_ms) {
    struct timeval tv = { .tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000 };
    setsockopt(client->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int bytes = recv(client->sock, buffer, len, 0);
    if (bytes < 0) {
        ESP_LOGE(TAG, "Failed to receive data");
        return -1;
    }
    buffer[bytes] = '\0';
    ESP_LOGI(TAG, "Received data: %s", buffer);
    return bytes;
}

esp_err_t websocket_client_stop(websocket_client_handle_t client) {
    close(client->sock);
    free(client);
    ESP_LOGI(TAG, "WebSocket client stopped");
    return ESP_OK;
}
