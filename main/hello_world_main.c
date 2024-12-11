#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "ds18b20.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include "esp_http_client.h"
#include "esp_sntp.h"
#include <string.h>
#include <stdlib.h>
#include <driver/i2c.h>
#include "HD44780.h"

// wifi
#define WIFI_SSID "FABIO7270"
#define WIFI_PASS "12345678"

// teste lcd
#define LCD_ADDR 0x27
#define SDA_PIN 17
#define SCL_PIN 16
#define LCD_COLS 16
#define LCD_ROWS 2

// keys para poder usar o thingspeak (erro ao pegar dados, enviar ok)
#define THINGSPEAK_API_WRITE_KEY "5PTPEOYO4H4KT143"
#define THINGSPEAK_API_READ_KEY "C8KYD4W3XNM5D09K"
#define THINGSPEAK_CHANNEL_ID "2780336"
#define THINGSPEAK_URL_WRITE "http://api.thingspeak.com/update"
#define THINGSPEAK_URL_READ "http://api.thingspeak.com/channels/" THINGSPEAK_CHANNEL_ID "/feeds/last.json"

// Pinos dos periferico
#define AQUECEDOR_PIN 27
#define BOLHAS_PIN 26
#define VENTILADOR_PIN 25
#define AZUL_PIN 33
#define VERDE_PIN 32
#define VERMELHO_PIN 22
#define AMARELO_PIN 23
#define BUZZER_PIN 21
#define GPIO_DS18B20 19

static const char *TAG = "APP";

// variavel para estados deles
static bool leds_ativos = false;
static bool buzzer_ativo = false;
static bool aquecedor_ativo = false;
static bool bolhas_ativas = false;
static bool ventilador_ativo = false;
static float temperatura_atual = 0.0;

// temperatura alvo padrão caso não existir
static float temperatura_alvo = 30.0f; // valor padrão inicial
static TickType_t heater_off_timer = 0; // para controle do atraso de desligamento, um timer para poder ver quanto tempo se passou

// troca de água
static int intervalo_troca_minutos = 10; // padrão: 3 minutos (para testes)
static time_t tempo_inicio_troca; 
static time_t tempo_proxima_troca;

// Funções para NVS
esp_err_t init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

void enviar_mensagem_whatsapp(const char* mensagem) {
    char url[512];
    snprintf(url, sizeof(url), 
             "http://api.callmebot.com/whatsapp.php?phone=554796769617&text=%s&apikey=1560975", 
             mensagem);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .skip_cert_common_name_check = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (esp_http_client_perform(client) == ESP_OK) {
        ESP_LOGI(TAG, "Mensagem WhatsApp enviada com sucesso.");
    } else {
        ESP_LOGE(TAG, "Erro ao enviar mensagem pelo WhatsApp.");
    }

    esp_http_client_cleanup(client);
}

//LCD que deu errado, mas ta ai, em partes funciona, só não sei qual o erro de aparecer caracteres diferentes, configuração ou enfim, não deu tempo
void atualizar_lcd_task(void *param) {
    //char linha1[LCD_COLS + 1];
    //char linha2[LCD_COLS + 1];
    LCD_init(LCD_ADDR, SDA_PIN, SCL_PIN, LCD_COLS, LCD_ROWS);

    while (1) {
        int temp_atual = (int)temperatura_atual;
        char num[20];
        LCD_home();
        LCD_clearScreen();
        //LCD_setCursor(8, 2);
        sprintf(num, "%d", temp_atual);
        //LCD_setCursor(0, 1); // Linha superior
        //snprintf(linha1, sizeof(linha1), "Temp.: %d C", temp_atual);
        LCD_writeStr(num);
        
        //LCD_clearScreen();
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Atualiza a cada 2 segundos
    }
}

// salvar o estado atual do sistema
void salvar_estado_sistema(bool leds, bool aquecedor, bool bolhas, int intervalo_troca, time_t ultima_troca, float temperatura, int festa_inicio_em, int festa_duracao) {
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_u8(nvs_handle, "leds_ativos", leds);
        nvs_set_u8(nvs_handle, "aquecedor_ativo", aquecedor);
        nvs_set_u8(nvs_handle, "bolhas_ativas", bolhas);
        nvs_set_i32(nvs_handle, "intervalo_troca", intervalo_troca);
        nvs_set_i64(nvs_handle, "ultima_troca", ultima_troca);

        // Converter float para int
        int32_t temp_as_int = (int32_t)(temperatura * 1000);
        nvs_set_i32(nvs_handle, "temp_alvo", temp_as_int);

        nvs_set_i32(nvs_handle, "festa_inicio_em", festa_inicio_em);
        nvs_set_i32(nvs_handle, "festa_duracao", festa_duracao);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE("NVS", "Erro ao abrir NVS para salvar dados.");
    }
}

// puxar o estado atual do sistema, quando incializa o sistema
void carregar_estado_sistema(bool *leds, bool *aquecedor, bool *bolhas, int *intervalo_troca, time_t *ultima_troca, float *temperatura, int *festa_inicio_em, int *festa_duracao) {
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK) {
        uint8_t u8_temp;
        nvs_get_u8(nvs_handle, "leds_ativos", &u8_temp);
        *leds = u8_temp;

        nvs_get_u8(nvs_handle, "aquecedor_ativo", &u8_temp);
        *aquecedor = u8_temp;

        nvs_get_u8(nvs_handle, "bolhas_ativas", &u8_temp);
        *bolhas = u8_temp;

        nvs_get_i32(nvs_handle, "intervalo_troca", (int32_t *)intervalo_troca);
        nvs_get_i64(nvs_handle, "ultima_troca", (int64_t *)ultima_troca);

        // Recuperar int e converter para float
        int32_t temp_as_int;
        nvs_get_i32(nvs_handle, "temp_alvo", &temp_as_int);
        *temperatura = (float)temp_as_int / 1000;

        nvs_get_i32(nvs_handle, "festa_inicio_em", (int32_t *)festa_inicio_em);
        nvs_get_i32(nvs_handle, "festa_duracao", (int32_t *)festa_duracao);

        nvs_close(nvs_handle);
    } else {
        ESP_LOGE("NVS", "Erro ao abrir NVS para carregar dados.");
        *leds = false;
        *aquecedor = false;
        *bolhas = false;
        *intervalo_troca = 10;
        *ultima_troca = time(NULL);
        *temperatura = 30.0f;
        *festa_inicio_em = 0;
        *festa_duracao = 0;
    }
}

// Pagina do site HTML, tentei fazer com python por fora usando websockets, mas só conseguia enviar dado e não me comunicar com a esp32, usei fontes e templates prontos que encontrei para poder deixar mais clean e bonito
static const volatile char html_page[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Controle do Sistema</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <!-- Importando fontes -->
    <link href="https://fonts.googleapis.com/css?family=Roboto:400,700&display=swap" rel="stylesheet">
    <link href="https://fonts.googleapis.com/css?family=Share+Tech+Mono&display=swap" rel="stylesheet">
    <style>
        body {
            font-family: 'Roboto', sans-serif;
            background: linear-gradient(to right, #0f2027, #203a43, #2c5364);
            color: #fff;
            margin: 0; 
            padding: 0;
        }

        .container {
            text-align: center;
            padding: 40px 20px;
            max-width: 500px;
            margin: auto;
        }

        h1 {
            margin-bottom: 30px;
            font-weight: 700;
            font-size: 2em;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.6);
        }

        .button-container {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
        }

        button {
            background: #4CAF50;
            border: none;
            border-radius: 8px;
            color: #fff;
            padding: 15px 25px;
            margin: 10px;
            font-size: 16px;
            cursor: pointer;
            transition: background 0.3s, transform 0.2s;
            box-shadow: 0 4px 6px rgba(0,0,0,0.3);
        }

        button:hover {
            background: #45a049;
            transform: translateY(-2px);
        }

        button:active {
            background: #3e8e41;
            transform: translateY(1px);
        }

        .status {
            font-size: 1.2em;
            margin-top: 40px;
            font-weight: 700;
        }

        .temp-display {
            font-family: 'Share Tech Mono', monospace;
            font-size: 3em;
            margin-top: 20px;
            display: inline-block;
            padding: 20px 40px;
            background: rgba(0,0,0,0.2);
            border-radius: 10px;
            box-shadow: inset 0 0 8px rgba(255,255,255,0.3), 0 0 8px rgba(0,0,0,0.5);
        }

        .input-row {
            margin-top: 30px;
        }

        .input-row input {
            width: 80px;
            font-size: 1em;
            padding: 5px;
            border-radius: 5px;
            border: none;
            margin-right: 10px;
            text-align: center;
        }

        .small-button {
            font-size: 14px;
            padding: 10px 20px;
        }

        .info-row {
            margin-top: 30px;
            text-align: center;
        }

        .info-row span {
            display: block;
            margin-bottom: 10px;
            font-weight: 700;
        }

    </style>
</head>
<body>
    <div class="container">
        <h1>Controle do Sistema</h1>
        <div class="button-container">
            <button onclick="toggle('leds')">Alternar LEDs</button>
            <button onclick="toggle('buzzer')">Alternar Buzzer</button>
            <button onclick="toggle('aquecedor')">Alternar Aquecedor</button>
            <button onclick="toggle('bolhas')">Alternar Bolhas</button>
            <button onclick="toggle('ventilador')">Alternar Ventilador</button>
        </div>
        <div class="status">Temperatura Atual:</div>
        <div class="temp-display" id="temp">0.00</div> °C

        <div class="input-row">
            <div class="status">Temperatura Alvo: <span id="tempAlvo">30.00</span> °C</div>
            <input type="number" id="newTarget" value="30.00" step="0.1">
            <button onclick="setTargetTemp()">Definir Temperatura Alvo</button>
        </div>

        <div class="input-row">
            <div class="status">Intervalo de Troca (min): <span id="intervaloTroca">3</span></div>
            <input type="number" id="novoIntervalo" value="3" step="1" min="1">
            <button onclick="setTrocaInterval()">Definir Intervalo</button>
        </div>

        <div class="input-row">
            <div class="status">Próxima Troca em:</div>
            <span id="tempoRestanteTroca">Calculando...</span>
            <button class="small-button" onclick="jaTroquei()">Já Troquei</button>
        </div>

        <div class="info-row">
            <span>Data/Hora Atual:</span>
            <span id="dataHoraAtual">--:--:--</span>
        </div>
    </div>
    <script>
        function toggle(device) {
            fetch(`/${device}`)
                .then(response => response.text())
                .then(status => console.log(`${device.toUpperCase()} pressionado ${status}`));
        }

        function updateTemperature() {
            fetch('/temperatura')
                .then(response => response.text())
                .then(temp => document.getElementById('temp').textContent = temp);
        }

        function updateTargetTempDisplay() {
            fetch('/get_temp_alvo')
                .then(response => response.text())
                .then(tempAlvo => {
                    document.getElementById('tempAlvo').textContent = tempAlvo;
                    document.getElementById('newTarget').value = tempAlvo;
                });
        }

        function setTargetTemp() {
            let val = document.getElementById('newTarget').value;
            fetch(`/set_temp?temp=${val}`)
                .then(r => r.text())
                .then(msg => {
                    alert(msg);
                    document.getElementById('tempAlvo').textContent = val;
                });
        }

        function setTrocaInterval() {
            let val = document.getElementById('novoIntervalo').value;
            fetch(`/set_troca?min=${val}`)
                .then(r => r.text())
                .then(msg => {
                    alert(msg);
                    document.getElementById('intervaloTroca').textContent = val;
                });
        }

        function jaTroquei() {
            fetch(`/ja_troquei`)
                .then(r => r.text())
                .then(msg => {
                    alert(msg);
                });
        }

        function updateTrocaInfo() {
            fetch('/get_troca_info')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('tempoRestanteTroca').textContent = data.tempo_restante_str;
                    document.getElementById('intervaloTroca').textContent = data.intervalo_minutos;
                    document.getElementById('dataHoraAtual').textContent = data.data_hora_atual;
                });
        }

        setInterval(updateTemperature, 2000);
        updateTargetTempDisplay();
        setInterval(updateTrocaInfo, 2000);
    </script>
</body>
</html>
)rawliteral";

// enviar aviso wpp, quando precisa de algo ou quando inicia o sistema, por exemplo



// envia os dados das tarefas no thingspeak
void enviar_para_thingspeak_task(void *param) {
    char post_data[256];
    esp_http_client_config_t config = {
        .url = THINGSPEAK_URL_WRITE,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    while (1) {
        int estado_led_bolha_aquecedor = (leds_ativos << 2) | (bolhas_ativas << 1) | aquecedor_ativo;
        snprintf(post_data, sizeof(post_data),
                 "api_key=%s&field2=%03d&field3=%.2f&field4=%.2f&field5=%d&field6=0&field7=0&field8=0",
                 THINGSPEAK_API_WRITE_KEY,
                 estado_led_bolha_aquecedor,
                 temperatura_alvo,
                 temperatura_atual,
                 intervalo_troca_minutos);

        esp_http_client_set_url(client, THINGSPEAK_URL_WRITE);
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_post_field(client, post_data, strlen(post_data));

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI("ThingSpeak", "Dados enviados com sucesso: %s", post_data);
        } else {
            ESP_LOGE("ThingSpeak", "Erro ao enviar dados para ThingSpeak: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(15000));
    }

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

// pega as informações do thingspeak, mas não deu boa
esp_err_t get_thingspeak_data(void) {
    esp_http_client_config_t config = {
        .url = THINGSPEAK_URL_READ,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char url_with_key[256];
    snprintf(url_with_key, sizeof(url_with_key), "%s?api_key=%s", THINGSPEAK_URL_READ, THINGSPEAK_API_READ_KEY);
    esp_http_client_set_url(client, url_with_key);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Código de status HTTP: %d", status_code);

        if (status_code == 200) {
            char response[1024];
            int total_read_len = 0;
            int read_len;
            
            int content_length = esp_http_client_get_content_length(client);
            if (content_length == -1) {
                while ((read_len = esp_http_client_read(client, response + total_read_len, sizeof(response) - total_read_len - 1)) > 0) {
                    total_read_len += read_len;
                }
            } else {
                read_len = esp_http_client_read(client, response, sizeof(response) - 1);
                if (read_len > 0) {
                    total_read_len = read_len;
                }
            }

            response[total_read_len] = '\0';

            if (total_read_len > 0) {
                ESP_LOGI(TAG, "Resposta recebida: %s", response);
            } else {
                ESP_LOGE(TAG, "Nenhum dado foi lido.");
            }
        } else {
            ESP_LOGE(TAG, "Erro na resposta HTTP: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "Erro ao realizar a requisição: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}


// inicia os pinos
void inicializa_pinos() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << AQUECEDOR_PIN) | (1ULL << BOLHAS_PIN) | (1ULL << VENTILADOR_PIN) |
                           (1ULL << AZUL_PIN) | (1ULL << VERDE_PIN) | (1ULL << VERMELHO_PIN) | 
                           (1ULL << AMARELO_PIN) | (1ULL << BUZZER_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Ajustando para lógica invertida do aquecedor e bolhas, porque o 1 é desligaod
    gpio_set_level(AQUECEDOR_PIN, 1);
    gpio_set_level(BOLHAS_PIN, 1);
    gpio_set_level(VENTILADOR_PIN, 0);
    gpio_set_level(AZUL_PIN, 0);
    gpio_set_level(VERDE_PIN, 0);
    gpio_set_level(VERMELHO_PIN, 0);
    gpio_set_level(AMARELO_PIN, 0);
    gpio_set_level(BUZZER_PIN, 0);
}

// leds ficam trocando e alternando entre si
void alternar_leds() {
    gpio_num_t leds[] = {AZUL_PIN, VERDE_PIN, VERMELHO_PIN, AMARELO_PIN};
    int num_leds = sizeof(leds) / sizeof(leds[0]);

    while (1) {
        if (leds_ativos) {
            for (int i = 0; i < num_leds; i++) {
                gpio_set_level(leds[i], 1);
                vTaskDelay(pdMS_TO_TICKS(500));
                gpio_set_level(leds[i], 0);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// controle da temperatura
void controle_temperatura() {
    // margem de 0,5%
    float h = temperatura_alvo * 0.005f;
    float limite_inferior = temperatura_alvo - h;
    float limite_superior = temperatura_alvo + h;

    if (temperatura_atual < limite_inferior) {
        gpio_set_level(AQUECEDOR_PIN, 0);
        aquecedor_ativo = true;
        heater_off_timer = 0; // reset do timer
    } else if (temperatura_atual > limite_superior) {
        if (heater_off_timer == 0) {
            heater_off_timer = xTaskGetTickCount();
        } else {
            if ((xTaskGetTickCount() - heater_off_timer) * portTICK_PERIOD_MS >= 5000) {
                gpio_set_level(AQUECEDOR_PIN, 1);
                aquecedor_ativo = false;
                heater_off_timer = 0;
            }
        }
    } else {
        heater_off_timer = 0;
    }
}

void leitura_temperatura_task() {
    ds18b20_init(GPIO_DS18B20);
    while (1) {
        if (ds18b20_read_temperature(GPIO_DS18B20, &temperatura_atual)) {
            ESP_LOGI(TAG, "Temperatura Atual: %.2f °C", temperatura_atual);
            // faz controle da temp e display
            controle_temperatura();
        } else {
            ESP_LOGE(TAG, "Erro ao ler a temperatura!");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Tarefa para verificar tempo de troca
void verifica_troca_task(void *param) {
    while (1) {
        time_t agora = time(NULL);
        if (agora > tempo_proxima_troca) {
            // Toca o buzzer para avisar
            enviar_mensagem_whatsapp("Troca+De+Agua+Necessaria!");
            for (int i=0; i<5; i++){
                gpio_set_level(BUZZER_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(BUZZER_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            // depois de apitar, vamos esperar até o usuário apertar "já troquei". Porque poderia ser que estivesse fora e não escutasse o aviso, só vai ser bem chato porque vai ficar tocando direto
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// Função para extrair parâmetro da URL
static esp_err_t get_query_param(httpd_req_t *req, const char *param_name, char *buf, size_t buf_len) {
    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len > 1) {
        char *query_str = malloc(query_len);
        if (httpd_req_get_url_query_str(req, query_str, query_len) == ESP_OK) {
            if (httpd_query_key_value(query_str, param_name, buf, buf_len) == ESP_OK) {
                free(query_str);
                return ESP_OK;
            }
        }
        free(query_str);
    }
    return ESP_FAIL;
}

// salva o estado em cada alteração no html pelo isuário
esp_err_t toggle_handler(httpd_req_t *req) {
    const char *uri = req->uri;

    if (strcmp(uri, "/leds") == 0) {
        leds_ativos = !leds_ativos;
        salvar_estado_sistema(leds_ativos, aquecedor_ativo, bolhas_ativas, intervalo_troca_minutos, tempo_inicio_troca, temperatura_alvo, 0, 0);
        
        httpd_resp_send(req, leds_ativos ? "ativados" : "desativados", -1);
    } else if (strcmp(uri, "/buzzer") == 0) {
        buzzer_ativo = !buzzer_ativo;
        salvar_estado_sistema(leds_ativos, aquecedor_ativo, bolhas_ativas, intervalo_troca_minutos, tempo_inicio_troca, temperatura_alvo, 0, 0);
        
        gpio_set_level(BUZZER_PIN, buzzer_ativo ? 1 : 0);
        httpd_resp_send(req, buzzer_ativo ? "ativado" : "desativado", -1);
    } else if (strcmp(uri, "/aquecedor") == 0) {
        aquecedor_ativo = !aquecedor_ativo;
        salvar_estado_sistema(leds_ativos, aquecedor_ativo, bolhas_ativas, intervalo_troca_minutos, tempo_inicio_troca, temperatura_alvo, 0, 0);
        
        gpio_set_level(AQUECEDOR_PIN, aquecedor_ativo ? 1 : 0);
        httpd_resp_send(req, aquecedor_ativo ? "ativado" : "desativado", -1);
    } else if (strcmp(uri, "/bolhas") == 0) {
        bolhas_ativas = !bolhas_ativas;
        salvar_estado_sistema(leds_ativos, aquecedor_ativo, bolhas_ativas, intervalo_troca_minutos, tempo_inicio_troca, temperatura_alvo, 0, 0);
        
        gpio_set_level(BOLHAS_PIN, bolhas_ativas ? 1 : 0);
        httpd_resp_send(req, bolhas_ativas ? "ativadas" : "desativadas", -1);
    } else if (strcmp(uri, "/ventilador") == 0) {
        ventilador_ativo = !ventilador_ativo;
        salvar_estado_sistema(leds_ativos, aquecedor_ativo, bolhas_ativas, intervalo_troca_minutos, tempo_inicio_troca, temperatura_alvo, 0, 0);
        
        gpio_set_level(VENTILADOR_PIN, ventilador_ativo ? 1 : 0);
        httpd_resp_send(req, ventilador_ativo ? "ativado" : "desativado", -1);
    } else if (strcmp(uri, "/temperatura") == 0) {
        char temp_str[16];
        salvar_estado_sistema(leds_ativos, aquecedor_ativo, bolhas_ativas, intervalo_troca_minutos, tempo_inicio_troca, temperatura_alvo, 0, 0);
        
        snprintf(temp_str, sizeof(temp_str), "%.2f", temperatura_atual);
        httpd_resp_send(req, temp_str, -1);
    } else {
        httpd_resp_send_404(req);
    }
    return ESP_OK;
}

// faz o controle da temp para poder ficar acima do limite que colocou
esp_err_t set_temp_handler(httpd_req_t *req) {
    char temp_buf[16];
    if (get_query_param(req, "temp", temp_buf, sizeof(temp_buf)) == ESP_OK) {
        float nova_temp = atof(temp_buf);
        if (nova_temp > 0 && nova_temp < 100) {
            temperatura_alvo = nova_temp;
            ESP_LOGI(TAG, "Nova temperatura alvo: %.2f", temperatura_alvo);
            salvar_estado_sistema(leds_ativos, aquecedor_ativo, bolhas_ativas, intervalo_troca_minutos, tempo_inicio_troca, temperatura_alvo, 0, 0);
            httpd_resp_send(req, "Temperatura alvo atualizada!", -1);
        } else {
            httpd_resp_send(req, "Valor inválido para temperatura alvo!", -1);
        }
    } else {
        httpd_resp_send(req, "Parâmetro temp não encontrado!", -1);
    }
    return ESP_OK;
}

esp_err_t get_temp_alvo_handler(httpd_req_t *req) {
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.2f", temperatura_alvo);
    httpd_resp_send(req, temp_str, -1);
    return ESP_OK;
}

// ajuste do intervalo de troca
esp_err_t set_troca_handler(httpd_req_t *req) {
    char min_buf[16];
    if (get_query_param(req, "min", min_buf, sizeof(min_buf)) == ESP_OK) {
        int novo_min = atoi(min_buf);
        if (novo_min > 0 && novo_min < 10000) {
            intervalo_troca_minutos = novo_min;
            // Recalcula próximo tempo de troca
            time_t agora = time(NULL);
            tempo_inicio_troca = agora;
            tempo_proxima_troca = agora + intervalo_troca_minutos * 60;
            ESP_LOGI(TAG, "Novo intervalo de troca: %d minutos", intervalo_troca_minutos);
            httpd_resp_send(req, "Intervalo de troca atualizado!", -1);
        } else {
            httpd_resp_send(req, "Valor inválido para intervalo!", -1);
        }
    } else {
        httpd_resp_send(req, "Parâmetro min não encontrado!", -1);
    }
    return ESP_OK;
}

// Já troquei: reseta o timer de troca
esp_err_t ja_troquei_handler(httpd_req_t *req) {
    time_t agora = time(NULL);
    tempo_inicio_troca = agora;
    tempo_proxima_troca = agora + intervalo_troca_minutos * 60;
    httpd_resp_send(req, "Troca resetada com sucesso!", -1);
    return ESP_OK;
}

// Retorna infos sobre a troca em JSON
esp_err_t get_troca_info_handler(httpd_req_t *req) {
    time_t agora = time(NULL);
    double segundos_faltam = difftime(tempo_proxima_troca, agora);
    if (segundos_faltam < 0) segundos_faltam = 0; // já estourou o tempo

    int min = (int)(segundos_faltam / 60);
    int sec = (int)((int)segundos_faltam % 60);

    char tempo_restante_str[64];
    snprintf(tempo_restante_str, sizeof(tempo_restante_str), "%dmin %dseg", min, sec);

    // Pega data/hora atual formatada
    struct tm timeinfo;
    localtime_r(&agora, &timeinfo);
    char data_hora_str[64];
    strftime(data_hora_str, sizeof(data_hora_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"intervalo_minutos\":%d,\"tempo_restante_str\":\"%s\",\"data_hora_atual\":\"%s\"}",
             intervalo_troca_minutos, tempo_restante_str, data_hora_str);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

httpd_handle_t start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 13; // Aumentando o número máximo de rotas, se não não podemos ter diversas funções para cada coisa limitando o sistema

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_root);

        httpd_uri_t uri_leds = {
            .uri = "/leds",
            .method = HTTP_GET,
            .handler = toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_leds);

        httpd_uri_t uri_buzzer = {
            .uri = "/buzzer",
            .method = HTTP_GET,
            .handler = toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_buzzer);

        httpd_uri_t uri_aquecedor = {
            .uri = "/aquecedor",
            .method = HTTP_GET,
            .handler = toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_aquecedor);

        httpd_uri_t uri_bolhas = {
            .uri = "/bolhas",
            .method = HTTP_GET,
            .handler = toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_bolhas);

        httpd_uri_t uri_ventilador = {
            .uri = "/ventilador",
            .method = HTTP_GET,
            .handler = toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_ventilador);

        httpd_uri_t uri_temperatura = {
            .uri = "/temperatura",
            .method = HTTP_GET,
            .handler = toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_temperatura);

        httpd_uri_t uri_set_temp = {
            .uri = "/set_temp",
            .method = HTTP_GET,
            .handler = set_temp_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_set_temp);

        httpd_uri_t uri_get_temp_alvo = {
            .uri = "/get_temp_alvo",
            .method = HTTP_GET,
            .handler = get_temp_alvo_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get_temp_alvo);

        httpd_uri_t uri_set_troca = {
            .uri = "/set_troca",
            .method = HTTP_GET,
            .handler = set_troca_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_set_troca);

        httpd_uri_t uri_ja_troquei = {
            .uri = "/ja_troquei",
            .method = HTTP_GET,
            .handler = ja_troquei_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_ja_troquei);

        httpd_uri_t uri_get_troca_info = {
            .uri = "/get_troca_info",
            .method = HTTP_GET,
            .handler = get_troca_info_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get_troca_info);
    }

    return server;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi desconectado. Tentando reconectar...");
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

    ESP_LOGI("WiFi", "Wi-Fi inicializado no modo STA.");
}

void initialize_sntp(void) {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org"); // Servidor NTP
    sntp_init();
}

void app_main(void) {
    ESP_ERROR_CHECK(init_nvs());

    bool leds_lidos, aquecedor_lido, bolhas_lidas;
    int intervalo_lido, festa_inicio_lido, festa_duracao_lida;
    time_t ultima_troca_lida;
    float temperatura_alvo_lida;

    carregar_estado_sistema(&leds_lidos, &aquecedor_lido, &bolhas_lidas, 
                        &intervalo_lido, &ultima_troca_lida, &temperatura_alvo_lida, 
                        &festa_inicio_lido, &festa_duracao_lida);

    // Use os valores carregados para inicializar as variáveis do sistema
    

    inicializa_pinos();

    leds_ativos = leds_lidos;
    aquecedor_ativo = aquecedor_lido;
    bolhas_ativas = bolhas_lidas;
    intervalo_troca_minutos = intervalo_lido;
    tempo_inicio_troca = ultima_troca_lida;
    temperatura_alvo = temperatura_alvo_lida;

    xTaskCreate(alternar_leds, "Alternar LEDs", 4096, NULL, 1, NULL);
    xTaskCreate(leitura_temperatura_task, "Leitura Temperatura", 4096, NULL, 1, NULL);

    ESP_LOGI(TAG, "Inicializando Wi-Fi...");
    wifi_init_sta();

    // Espera conexão Wi-Fi (idealmente, configurar SNTP antes)
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Após estar conectado ao Wi-Fi:
    ESP_LOGI(TAG, "Iniciando sincronização via NTP...");
    initialize_sntp();

    // Aguarda o ajuste do tempo
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Aguardando ajuste do tempo via NTP...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGE(TAG, "Não foi possível ajustar a hora após várias tentativas.");
    } else {
        ESP_LOGI(TAG, "Hora ajustada com sucesso: %s", asctime(&timeinfo));
    }

    ESP_LOGI(TAG, "Lendo os últimos dados do ThingSpeak...");
    esp_err_t err = get_thingspeak_data();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Dados lidos com sucesso.");
    } else {
        ESP_LOGE(TAG, "Erro ao ler dados do ThingSpeak.");
    }

    // Inicializa variáveis de tempo da troca
    setenv("TZ", "UTC3", 1); // Ajuste "UTC-3" conforme seu fuso horário
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);
    time_t agora = localtime_r(&now, &timeinfo);
    tempo_inicio_troca = agora;
    tempo_proxima_troca = agora + intervalo_troca_minutos * 60;

    start_webserver();

    xTaskCreate(atualizar_lcd_task, "Atualizar LCD", 4096, NULL, 1, NULL);

    enviar_mensagem_whatsapp("APP+Aquario+Iniciado!");
    xTaskCreate(enviar_para_thingspeak_task, "Enviar ThingSpeak", 4096, NULL, 1, NULL);
    xTaskCreate(verifica_troca_task, "Verifica Troca", 4096, NULL, 1, NULL);
}

