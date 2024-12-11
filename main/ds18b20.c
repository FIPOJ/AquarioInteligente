#include "ds18b20.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DS18B20";

// Comandos do DS18B20
#define DS18B20_CMD_CONVERT_TEMP 0x44
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE
#define DS18B20_CMD_SKIP_ROM 0xCC

// Funções auxiliares
static void ds18b20_gpio_set_level(int gpio_num, int level) {
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(gpio_num, level);
}

static int ds18b20_gpio_get_level(int gpio_num) {
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);
    return gpio_get_level(gpio_num);
}

static void ds18b20_delay_us(int us) {
    esp_rom_delay_us(us);
}

// Reset do barramento OneWire
static bool ds18b20_reset(int gpio_num) {
    ds18b20_gpio_set_level(gpio_num, 0);
    ds18b20_delay_us(480);
    ds18b20_gpio_set_level(gpio_num, 1);
    ds18b20_delay_us(70);
    int presence = ds18b20_gpio_get_level(gpio_num);
    ds18b20_delay_us(410);
    return presence == 0;
}

// Envia um byte no barramento OneWire
static void ds18b20_write_byte(int gpio_num, uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        ds18b20_gpio_set_level(gpio_num, 0);
        if (byte & (1 << i)) {
            ds18b20_delay_us(10);
            ds18b20_gpio_set_level(gpio_num, 1);
            ds18b20_delay_us(55);
        } else {
            ds18b20_delay_us(65);
            ds18b20_gpio_set_level(gpio_num, 1);
        }
    }
}

// Lê um byte do barramento OneWire
static uint8_t ds18b20_read_byte(int gpio_num) {
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        ds18b20_gpio_set_level(gpio_num, 0);
        ds18b20_delay_us(2);
        ds18b20_gpio_set_level(gpio_num, 1);
        ds18b20_delay_us(10);
        if (ds18b20_gpio_get_level(gpio_num)) {
            byte |= (1 << i);
        }
        ds18b20_delay_us(55);
    }
    return byte;
}

// Inicializa o sensor DS18B20
void ds18b20_init(int gpio_num) {
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT_OUTPUT);
    ESP_LOGI(TAG, "DS18B20 inicializado no GPIO %d", gpio_num);
}

// Lê a temperatura do sensor DS18B20
bool ds18b20_read_temperature(int gpio_num, float *temperature) {
    if (!ds18b20_reset(gpio_num)) {
        ESP_LOGE(TAG, "Sensor DS18B20 não detectado!");
        return false;
    }

    // Comando para iniciar a conversão de temperatura
    ds18b20_write_byte(gpio_num, DS18B20_CMD_SKIP_ROM);
    ds18b20_write_byte(gpio_num, DS18B20_CMD_CONVERT_TEMP);

    // Aguarda a conversão (~750ms para resolução máxima)
    vTaskDelay(pdMS_TO_TICKS(750));

    if (!ds18b20_reset(gpio_num)) {
        ESP_LOGE(TAG, "Erro ao resetar o barramento OneWire!");
        return false;
    }

    // Comando para ler o scratchpad
    ds18b20_write_byte(gpio_num, DS18B20_CMD_SKIP_ROM);
    ds18b20_write_byte(gpio_num, DS18B20_CMD_READ_SCRATCHPAD);

    // Lê os dois primeiros bytes do scratchpad (temperatura)
    uint8_t lsb = ds18b20_read_byte(gpio_num);
    uint8_t msb = ds18b20_read_byte(gpio_num);

    // Calcula a temperatura (formato 12-bit)
    int16_t temp = (msb << 8) | lsb;
    *temperature = temp * 0.0625; // Cada incremento representa 0.0625 °C

    return true;
}
