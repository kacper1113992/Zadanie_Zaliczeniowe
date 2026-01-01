#include "app_logic.h"
#include "bmp2_config.h"
#include "lcd_i2c.h"
#include "stdio.h"
#include "string.h"

// Zmienne prywatne modułu
float filterAlpha = 0.1f;
float temp_filtered = 0.0f;
float press_filtered = 0.0f;
static char lcd_buffer[17];

void App_Init(void) {
    LCD_Init();
    LCD_SetCursor(0,0);
    LCD_SendString("Start systemu...");
    HAL_Delay(500);
    LCD_Clear();
    if (BMP2_Init(&bmp2dev[0]) == BMP2_INTF_RET_SUCCESS) {
        LCD_SetCursor(0,0);
        LCD_SendString("Sensors OK");
    } else {
        LCD_SetCursor(0,0);
        LCD_SendString("Sensor Error");
        while(1);
    }

    HAL_Delay(1000);
    LCD_Clear();
}

void App_Process(void) {

    double raw_temp = BMP2_ReadTemperature_degC(&bmp2dev[0]);
    double raw_press = BMP2_ReadPressure_hPa(&bmp2dev[0]);

    if (temp_filtered == 0.0f) {
        temp_filtered = (float)raw_temp;
        press_filtered = (float)raw_press;
    } else {
        // Algorytm: Średnia = (Surowa * alpha) + (Poprzednia_Średnia * (1 - alpha))
        temp_filtered = ((float)raw_temp * filterAlpha) + (temp_filtered * (1.0f - filterAlpha));
        press_filtered = ((float)raw_press * filterAlpha) + (press_filtered * (1.0f - filterAlpha));
    }

    sprintf(lcd_buffer, "Temp: %.2f C", temp_filtered);
    LCD_SetCursor(0, 0);
    LCD_SendString(lcd_buffer);
    sprintf(lcd_buffer, "Pres: %.1f hPa", press_filtered);
    LCD_SetCursor(1, 0);
    LCD_SendString(lcd_buffer);

    HAL_Delay(100);
}
