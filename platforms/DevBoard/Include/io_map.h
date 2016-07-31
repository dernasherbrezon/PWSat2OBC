#ifndef SRC_IO_MAP_H_
#define SRC_IO_MAP_H_

#include <em_gpio.h>
#include <em_system.h>

#define LED_PORT gpioPortE
#define LED0 2
#define LED1 3

// LEUART0
#define LEUART0_PORT gpioPortD
#define LEUART0_TX 4
#define LEUART0_RX 5
#define LEUART0_LOCATION LEUART_ROUTE_LOCATION_LOC0
#define LEUART0_BAUDRATE 9600

// I2C
#define I2C I2C1
#define I2C_PORT gpioPortC
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define I2C_LOCATION I2C_ROUTE_LOCATION_LOC0

// NAND Flash
#define NAND_POWER_PORT gpioPortB
#define NAND_POWER_PIN 15
#define NAND_READY_PORT gpioPortD
#define NAND_READY_PIN 15
#define NAND_CE_PORT gpioPortD
#define NAND_CE_PIN 14
#define NAND_WP_PORT gpioPortD
#define NAND_WP_PIN 13
#define NAND_ALE_BIT 24
#define NAND_CLE_BIT 25

#define EBI_ALE_PORT gpioPortC
#define EBI_ALE_PIN 1
#define EBI_CLE_PORT gpioPortC
#define EBI_CLE_PIN 2
#define EBI_WE_PORT gpioPortF
#define EBI_WE_PIN 8
#define EBI_RE_PORT gpioPortF
#define EBI_RE_PIN 9

#define EBI_DATA_PORT gpioPortE
#define EBI_DATA_PIN0 8

#endif