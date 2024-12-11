#include "HD44780.h"
#include "driver/i2c.h"
#include "freertos/task.h"

// Bits do PCF8574
#define RS_BIT  0x01
#define RW_BIT  0x02
#define EN_BIT  0x04
#define BL_BIT  0x08  // Backlight

static uint8_t _lcd_addr;
static uint8_t _cols;
static uint8_t _rows;
static uint8_t _backlight = BL_BIT;
static i2c_port_t _i2c_port = I2C_NUM_0;

// Comandos do LCD
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME    0x02
#define LCD_ENTRYMODESET  0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT   0x10
#define LCD_FUNCTIONSET   0x20
#define LCD_SETDDRAMADDR  0x80

// Flags para entrada
#define LCD_ENTRYLEFT     0x02
#define LCD_ENTRYSHIFTDEC 0x00

// Flags display on/off
#define LCD_DISPLAYON     0x04
#define LCD_CURSOROFF     0x00
#define LCD_BLINKOFF      0x00

// Flags function set
#define LCD_4BITMODE      0x00
#define LCD_2LINE         0x08
#define LCD_5x8DOTS       0x00

static uint8_t _display_function;
static uint8_t _display_control;
static uint8_t _display_mode;

static esp_err_t lcd_i2c_write(uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) return ESP_FAIL;
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_lcd_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(_i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err;
}

// Função auxiliar para enviar um nibble (4 bits) com pulso de EN
static void lcd_send_nibble(uint8_t nibble) {
    // Envia com EN alto
    lcd_i2c_write(nibble | EN_BIT | _backlight);
    vTaskDelay(pdMS_TO_TICKS(1));
    // Envia com EN baixo
    lcd_i2c_write((nibble & ~EN_BIT) | _backlight);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void lcd_send(uint8_t value, uint8_t mode) {
    uint8_t high = (value & 0xF0);
    uint8_t low = ((value << 4) & 0xF0);
    // Envia nibble alto
    lcd_send_nibble(high | mode);
    // Envia nibble baixo
    lcd_send_nibble(low | mode);
}

static void lcd_command(uint8_t cmd) {
    lcd_send(cmd, 0x00);
    vTaskDelay(pdMS_TO_TICKS(2)); // pequeno delay após comandos
}

static void lcd_write_char(uint8_t ch) {
    lcd_send(ch, RS_BIT);
}

void LCD_init(uint8_t addr, int sda_pin, int scl_pin, uint8_t cols, uint8_t rows) {
    _lcd_addr = addr;
    _cols = cols;
    _rows = rows;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_pin,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };

    i2c_param_config(_i2c_port, &conf);
    i2c_driver_install(_i2c_port, I2C_MODE_MASTER, 0, 0, 0);

    // Inicialização conforme datasheet
    vTaskDelay(pdMS_TO_TICKS(50));
    // Forçar 8-bit
    lcd_send_nibble(0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_nibble(0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_nibble(0x30);
    vTaskDelay(pdMS_TO_TICKS(1));
    // Modo 4-bit
    lcd_send_nibble(0x20);
    vTaskDelay(pdMS_TO_TICKS(1));

    _display_function = LCD_4BITMODE | LCD_5x8DOTS;
    if (_rows > 1) {
        _display_function |= LCD_2LINE;
    }

    lcd_command(LCD_FUNCTIONSET | _display_function);
    _display_control = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    lcd_command(LCD_DISPLAYCONTROL | _display_control);
    LCD_clearScreen();
    _display_mode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDEC;
    lcd_command(LCD_ENTRYMODESET | _display_mode);
    LCD_home();
    LCD_backlightOn();
}

void LCD_clearScreen(void) {
    lcd_command(LCD_CLEARDISPLAY);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void LCD_home(void) {
    lcd_command(LCD_RETURNHOME);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void LCD_setCursor(uint8_t col, uint8_t row) {
    // Ajustar se o usuário usa 1-based indexing
    if (row < 1) row = 1;
    if (row > _rows) row = _rows;
    if (col < 1) col = 1;
    if (col > _cols) col = _cols;

    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    lcd_command(LCD_SETDDRAMADDR | ((col - 1) + row_offsets[row - 1]));
}

void LCD_writeStr(const char *str) {
    while(*str) {
        lcd_write_char((uint8_t)*str++);
    }
}

void LCD_backlightOn(void) {
    _backlight = BL_BIT;
    lcd_i2c_write(_backlight);
}

void LCD_backlightOff(void) {
    _backlight = 0x00;
    lcd_i2c_write(_backlight);
}
