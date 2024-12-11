#ifndef ESP_WEBSOCKET_CLIENT_H
#define ESP_WEBSOCKET_CLIENT_H

#include "esp_err.h"

typedef struct {
    const char *uri;      // URI do servidor WebSocket
    const char *protocol; // Protocolo, se necessário
} websocket_client_config_t;

typedef struct websocket_client *websocket_client_handle_t;

// Inicializa o cliente WebSocket
websocket_client_handle_t websocket_client_init(const websocket_client_config_t *config);

// Inicia a conexão com o servidor
esp_err_t websocket_client_start(websocket_client_handle_t client);

// Envia dados ao servidor
esp_err_t websocket_client_send(websocket_client_handle_t client, const char *data, size_t len);

// Recebe dados do servidor
int websocket_client_receive(websocket_client_handle_t client, char *buffer, size_t len, int timeout_ms);

// Para e limpa o cliente WebSocket
esp_err_t websocket_client_stop(websocket_client_handle_t client);

#endif // ESP_WEBSOCKET_CLIENT_H
