#include "app_logic.h"
#include "bmp2_config.h"
#include "lcd_i2c.h"
#include "stdio.h"
#include "string.h"
#include "main.h"
#include <stdlib.h>

extern UART_HandleTypeDef huart3;

// Zmienne prywatne modułu
float filterAlpha = 0.1f;
float temp_filtered = 0.0f;
float press_filtered = 0.0f;
static char lcd_buffer[17];

float target_temp = 25.0f;
#define RX_BUFFER_SIZE 32
uint8_t rx_byte;
uint8_t rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_index = 0;
volatile uint8_t data_received_flag = 0;
uint32_t last_button_time = 0;
static char lcd_buffer[17];
static char uart_tx_buffer[64];

void App_Init(void) {
    LCD_Init();
    LCD_SetCursor(0,0);
    LCD_SendString("Start systemu...");
    HAL_Delay(500);
    LCD_Clear();
    HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
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

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        if (rx_byte == '\n') {
            rx_buffer[rx_index] = 0;
            data_received_flag = 1;
            rx_index = 0;
        }
        else {
            if (rx_index < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = rx_byte;
            }
        }
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }
}

void Process_Command() {
    if (strncmp((char*)rx_buffer, "SET:", 4) == 0) {
        float new_val = atof((char*)&rx_buffer[4]);
        if (new_val > 0 && new_val < 100) {
            target_temp = new_val;
        }
    }
}

void Check_Buttons() {
    if (HAL_GetTick() - last_button_time < 200) return;
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_9) == GPIO_PIN_RESET) {
        target_temp += 0.5f;
        if(target_temp > 35.0f) target_temp = 35.0f;
        last_button_time = HAL_GetTick();
    }
    if (HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_13) == GPIO_PIN_RESET) {
        target_temp -= 0.5f;
        if(target_temp < 15.0f) target_temp = 15.0f;
        last_button_time = HAL_GetTick();
    }
}

void App_Process(void) {
	if (data_received_flag) {
	        Process_Command();
	        data_received_flag = 0;
	    }
	    Check_Buttons();

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

    sprintf(lcd_buffer, "Akt: %.1f C", temp_filtered);
        LCD_SetCursor(0, 0);
        LCD_SendString(lcd_buffer);
    sprintf(lcd_buffer, "Set: %.1f C", target_temp);
        LCD_SetCursor(1, 0);
        LCD_SendString(lcd_buffer);


     int len = sprintf(uart_tx_buffer, "%.2f;%.2f\n", temp_filtered, target_temp);
            HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, len, 100);
    HAL_Delay(100);
}
