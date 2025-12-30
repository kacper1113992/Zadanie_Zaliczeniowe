/*
 * lcd_i2c.c
 *
 *  Created on: Dec 30, 2025
 *      Author: kacpe
 */
#include "lcd_i2c.h"

extern I2C_HandleTypeDef hi2c1;
void LCD_Write_Byte(uint8_t val) {
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, &val, 1, 100);
}

void LCD_Send_Cmd(uint8_t cmd) {
	uint8_t data_u, data_l;
	uint8_t data_t[4];
	data_u = (cmd & 0xf0);
	data_l = ((cmd << 4) & 0xf0);
	data_t[0] = data_u | 0x0C;  // en=1, rs=0
	data_t[1] = data_u | 0x08;  // en=0, rs=0
	data_t[2] = data_l | 0x0C;  // en=1, rs=0
	data_t[3] = data_l | 0x08;  // en=0, rs=0
	HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, (uint8_t *) data_t, 4, 100);
}

void LCD_Send_Data(uint8_t data) {
	uint8_t data_u, data_l;
	uint8_t data_t[4];
	data_u = (data & 0xf0);
	data_l = ((data << 4) & 0xf0);
	data_t[0] = data_u | 0x0D;  // en=1, rs=1
	data_t[1] = data_u | 0x09;  // en=0, rs=1
	data_t[2] = data_l | 0x0D;  // en=1, rs=1
	data_t[3] = data_l | 0x09;  // en=0, rs=1
	HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, (uint8_t *) data_t, 4, 100);
}

void LCD_Init(void) {
	HAL_Delay(50);
	LCD_Send_Cmd(0x30);
	HAL_Delay(5);
	LCD_Send_Cmd(0x30);
	HAL_Delay(1);
	LCD_Send_Cmd(0x30);
	HAL_Delay(10);
	LCD_Send_Cmd(0x20); // 4-bit mode
	HAL_Delay(10);

	LCD_Send_Cmd(0x28); // Function set: DL=0 (4-bit), N=1 (2 lines), F=0 (5x8)
	HAL_Delay(1);
	LCD_Send_Cmd(0x08); // Display on/off control: Display off, cursor off, blink off
	HAL_Delay(1);
	LCD_Send_Cmd(0x01); // Clear display
	HAL_Delay(1);
	LCD_Send_Cmd(0x06); // Entry mode set: I/D=1 (increment), S=0 (no shift)
	HAL_Delay(1);
	LCD_Send_Cmd(0x0C); // Display on/off control: Display on, cursor off, blink off
}

void LCD_SendString(char *str) {
	while (*str) LCD_Send_Data(*str++);
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t addr;
    if (row == 0) addr = 0x80 + col;
    else addr = 0xC0 + col;
    LCD_Send_Cmd(addr);
}

void LCD_Clear(void) {
	LCD_Send_Cmd(0x01); // Clear display
	HAL_Delay(2);
}

void LCD_ScrollLeft(void) {
    LCD_Send_Cmd(0x18);
}

void LCD_ScrollRight(void) {
    LCD_Send_Cmd(0x1C);
}

