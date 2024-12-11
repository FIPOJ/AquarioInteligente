#ifndef DS18B20_H
#define DS18B20_H

#include <stdbool.h>
#include <stdint.h>

// Inicializa o sensor DS18B20 no GPIO especificado
void ds18b20_init(int gpio_num);

// Lê a temperatura do sensor DS18B20
bool ds18b20_read_temperature(int gpio_num, float *temperature);

#endif // DS18B20_H
