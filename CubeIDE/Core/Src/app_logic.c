/**
  ******************************************************************************
  * @file    app_logic.c
  * @author  Twoje Nazwisko
  * @brief   Główna logika sterowania: Regulator, UART, LCD, PWM.
  * Realizuje wymagania sterowania w pętli zamkniętej.
  ******************************************************************************
  */

#include "app_logic.h"
#include "bmp2_config.h"
#include "lcd_i2c.h"
#include "stdio.h"
#include "string.h"
#include "main.h"
#include <stdlib.h>

/* --- Hardware Handles --- */
extern UART_HandleTypeDef huart3;
extern TIM_HandleTypeDef htim3;

/* --- Configuration Defines --- */
#define HEATER_TIM      &htim3
#define HEATER_CHANNEL  TIM_CHANNEL_1
#define PWM_MAX_ARR     1000
#define FAN_PORT        GPIOE
#define FAN_PIN         GPIO_PIN_8
#define RX_BUFFER_SIZE 32
#define SAMPLING_PERIOD_MS 100

/* --- Private Variables --- */
float filterAlpha = 0.1f;
float temp_filtered = 0.0f;
float press_filtered = 0.0f;
float target_temp = 25.0f;
float error=0;

/* --- Communication Buffers --- */
uint8_t rx_byte;
uint8_t rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_index = 0;
volatile uint8_t data_received_flag = 0;
static char uart_tx_buffer[64];
static char lcd_buffer[17];

/* --- Timing Variables (Non-blocking) --- */
uint32_t last_sampling_time = 0;
uint32_t last_button_time = 0;

/**
  * @brief  Inicjalizacja modułu aplikacji, LCD, PWM i czujników.
  * @retval None
  */

void App_Init(void) {
    LCD_Init();
    LCD_SetCursor(0,0);
    LCD_SendString("Start systemu...");
    HAL_TIM_PWM_Start(HEATER_TIM, HEATER_CHANNEL);
        __HAL_TIM_SET_COMPARE(HEATER_TIM, HEATER_CHANNEL, 0); // Moc 0%
        HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_RESET);
    HAL_Delay(500);
    LCD_Clear();
    HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    if (BMP2_Init(&bmp2dev[0]) == BMP2_INTF_RET_SUCCESS) {
        LCD_SetCursor(0,0);
        LCD_SendString("Sensors OK");
    } else {
        LCD_SetCursor(0,0);
        LCD_SendString("Sensor Error");
    }

    HAL_Delay(1000);
    LCD_Clear();
    last_sampling_time = HAL_GetTick();
}

/**
  * @brief  Callback przerwania UART (Odbiór danych).
  * @param  huart: Wskaźnik do struktury UART
  * @retval None
  */
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

/**
  * @brief  Przetwarza komendy przychodzące z PC (format "SET:val").
  * @retval None
  */
void Process_Command() {
    if (strncmp((char*)rx_buffer, "SET:", 4) == 0) {
        float new_val = atof((char*)&rx_buffer[4]);
        if (new_val > 0 && new_val < 100) {
            target_temp = new_val;
        }
    }
}

/**
  * @brief  Obsługa przycisków fizycznych .
  * @retval None
  */
void Check_Buttons() {
    if (HAL_GetTick() - last_button_time < 200) return;
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_9) == GPIO_PIN_RESET) {
        target_temp += 0.5f;
        if(target_temp > 60.0f) target_temp = 60.0f;
        last_button_time = HAL_GetTick();
    }
    if (HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_13) == GPIO_PIN_RESET) {
        target_temp -= 0.5f;
        if(target_temp < 15.0f) target_temp = 15.0f;
        last_button_time = HAL_GetTick();
    }
}

/**
  * @brief  Algorytm sterowania klimatem (Grzałka PWM + Wentylator).
  * @note   Zastępuje domyślny regulator PID własną implementacją.
  * @retval None
  */
void Control_Climate() {
    error = target_temp - temp_filtered;
    int pwm_duty = 0;
    uint8_t is_cooling = 0;
    // 1. Logika GRZANIA (gdy jest za zimno)
    if (error > 0) {
        HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_RESET);
        is_cooling = 0;
        if (error >= 2.0f) {
            pwm_duty = PWM_MAX_ARR;
        } else {
            // Skalowanie: 2.0 stopnia błędu = 1000 PWM
            pwm_duty = (int)(error * (float)PWM_MAX_ARR / 2.0f);
        }
    }
    // 2. Logika CHŁODZENIA (gdy jest za ciepło)
    else {
        pwm_duty = 0; // Grzałka STOP
        if (error < -0.5f) {
            HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_SET);
            is_cooling = 1;
        }
        else if (error > -0.1f) {
            HAL_GPIO_WritePin(FAN_PORT, FAN_PIN, GPIO_PIN_RESET);
            is_cooling = 0;
        }
        else {
               if (HAL_GPIO_ReadPin(FAN_PORT, FAN_PIN) == GPIO_PIN_SET) {
                        is_cooling = 1;
               }
             }
    }
    __HAL_TIM_SET_COMPARE(HEATER_TIM, HEATER_CHANNEL, pwm_duty);

    if (pwm_duty > 0) {
    		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
    	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
        }
        else if (is_cooling) {
        	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
        	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
        }
        else {
        	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
        	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
        }
}

/**
  * @brief  Główna pętla procesu sterowania.
  * Wywoływana w main.c w pętli while(1).
  * @retval None
  */
void App_Process(void) {
	if (data_received_flag) {
	        Process_Command();
	        data_received_flag = 0;
	    }
	    Check_Buttons();
	    // Obsługa cykliczna (Stały okres próbkowania - Non-blocking)
	if (HAL_GetTick() - last_sampling_time >= SAMPLING_PERIOD_MS)
		{
		last_sampling_time = HAL_GetTick();
		double raw_temp = BMP2_ReadTemperature_degC(&bmp2dev[0]);
		if (temp_filtered == 0.0f) { temp_filtered = (float)raw_temp; }
		else {
				// Algorytm: Średnia = (Surowa * alpha) + (Poprzednia_Średnia * (1 - alpha))
				temp_filtered = ((float)raw_temp * filterAlpha) + (temp_filtered * (1.0f - filterAlpha));
			}

		Control_Climate();
		sprintf(lcd_buffer, "Akt: %.1f C", temp_filtered);
				LCD_SetCursor(0, 0);
				LCD_SendString(lcd_buffer);

		char status = ' ';
		if (temp_filtered < target_temp) status = '*';
		else if (temp_filtered > target_temp + 0.5f) status = 'F';

		sprintf(lcd_buffer, "Set: %.1f C %c", target_temp, status);
				LCD_SetCursor(1, 0);
				LCD_SendString(lcd_buffer);

		int len = sprintf(uart_tx_buffer, "%.2f;%.2f\n", temp_filtered, target_temp);
		HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, len, 100);
		}
}
