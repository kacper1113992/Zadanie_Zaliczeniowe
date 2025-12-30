/*
 * lcd_i2c.h
 *
 *  Created on: Dec 30, 2025
 *      Author: kacpe
 */

#ifndef INC_LCD_I2C_H_
#define INC_LCD_I2C_H_

#include "stm32f7xx_hal.h"
#define LCD_ADDR (0x27 << 1)

void LCD_Init(void);
void LCD_SendString(char *str);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Clear(void);

#endif /* INC_LCD_I2C_H_ */
