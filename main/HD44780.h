#ifndef HD44780_H
#define HD44780_H

#include <stdint.h>

/**
 * Inicializa o LCD em modo 4-bits via I2C.
 * @param addr Endereço I2C do display (ex: 0x27)
 * @param sda_pin GPIO do SDA
 * @param scl_pin GPIO do SCL
 * @param cols Número de colunas
 * @param rows Número de linhas
 */
void LCD_init(uint8_t addr, int sda_pin, int scl_pin, uint8_t cols, uint8_t rows);

/**
 * Limpa a tela do LCD.
 */
void LCD_clearScreen(void);

/**
 * Retorna o cursor para a posição inicial (0,0).
 */
void LCD_home(void);

/**
 * Define a posição do cursor (1-based).
 * @param col coluna (1-based)
 * @param row linha (1-based)
 */
void LCD_setCursor(uint8_t col, uint8_t row);

/**
 * Escreve uma string no LCD a partir da posição atual do cursor.
 * @param str Ponteiro para a string
 */
void LCD_writeStr(const char *str);

/**
 * Liga a luz de fundo do LCD.
 */
void LCD_backlightOn(void);

/**
 * Desliga a luz de fundo do LCD.
 */
void LCD_backlightOff(void);

#endif // HD44780_H
