#ifndef __RC522_CONFIG_H
#define __RC522_CONFIG_H

//##################RC522≈‰÷√##########

#define RC522_SPI_GPIO                       GPIOA
#define RC522_SPI_GPIO_ENABLED               RCC_APB2Periph_GPIOA
#define RC522_SPI_GPIO_PIN_CS                GPIO_Pin_4
#define RC522_SPI_GPIO_PIN_SCK               GPIO_Pin_5
#define RC522_SPI_GPIO_PIN_MISO              GPIO_Pin_6
#define RC522_SPI_GPIO_PIN_MOSI              GPIO_Pin_7

#define RC522_SPI_GPIO_RST                   GPIOB
#define RC522_SPI_GPIO_RST_ENABLED           RCC_APB2Periph_GPIOB
#define RC522_SPI_GPIO_PIN_RST               GPIO_Pin_0

#endif
