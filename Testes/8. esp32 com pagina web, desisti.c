#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

#define WIFI_SSID "FABIO7270"
#define WIFI_PASS "12345678"
#define LED_PIN 33

static const char *TAG = "WiFi";

// Página HTML com interface de controle
static const char html_page[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Controle de LED</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
        button { padding: 10px 20px; font-size: 18px; cursor: pointer; }
        .status { font-size: 24px; margin-top: 20px; }
    </style>
</head>
<body>
    <h1>Controle de LED</h1>
    <button onclick="toggleLed()">Alternar LED</button>
    <div class="status">Estado do LED: <span id="led-status">Desconhecido</span></div>
    <script>
        function toggleLed() {
            fetch('/toggle_led')
                .then(response => response.text())
                .then(state => {
                    document.getElementById('led-status').textContent = state;
                });
        }
    </script>
</body>
</html>
)rawliteral";

// Handler para alternar o LED
esp_err_t toggle_led_handler(httpd_req_t *req) {
    static bool led_on = false;
    led_on = !led_on;
    gpio_set_level(LED_PIN, led_on ? 1 : 0);

    const char *resp = led_on ? "Ligado" : "Desligado";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// Handler para a página principal
esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

// Inicializa o servidor HTTP
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t toggle_led_uri = {
            .uri = "/toggle_led",
            .method = HTTP_GET,
            .handler = toggle_led_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &toggle_led_uri);
    }
    return server;
}

// Configura o Wi-Fi
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Tentando reconectar ao Wi-Fi...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init_sta(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi inicializado no modo STA.");
}

void app_main(void) {
    // Inicializa NVS e Wi-Fi
    ESP_LOGI(TAG, "Inicializando...");
    wifi_init_sta();

    // Configura o GPIO do LED
    esp_rom_gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    // Inicia o servidor web
    start_webserver();
}
