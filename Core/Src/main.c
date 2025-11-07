/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "software_timer.h"
#include "led_7seg.h"
#include "button.h"
#include "lcd.h"
#include "picture.h"
#include "ds3231.h"
#include "uart.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void system_init();
void test_LedDebug();
void test_Uart();
void display_time_on_lcd();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_SPI1_Init();
  MX_FSMC_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  system_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  uint8_t rx_char;

  while (1)
  {
    while (!flag_timer2)
      ;
    flag_timer2 = 0;

    button_Scan();
    test_LedDebug();
    ds3231_ReadTime();
    test_Uart();

    // Hiển thị thời gian trên LCD (chỉ khi không trong chế độ cập nhật)
    if (time_update_state == TIME_UPDATE_IDLE)
    {
      display_time_on_lcd();
    }

    // --- Xử lý UART (Bài 1 và Bài 2) ---
    // Xử lý chế độ cập nhật thời gian liên tục
    if (time_update_state != TIME_UPDATE_IDLE &&
        time_update_state != TIME_UPDATE_COMPLETE &&
        time_update_state != TIME_UPDATE_ERROR)
    {
      // FIX: Xử lý liên tục cho cả bài 1 và bài 2 (không cần uart_data_ready)
      // Timeout cần được kiểm tra liên tục, không chỉ khi có dữ liệu
      uart_ProcessTimeUpdate();
    }
    else if (uart_data_ready)
    {
      uart_data_ready = 0;
      // Chế độ echo bình thường
      while (uart_read_byte(&rx_char))
      {
        HAL_UART_Transmit(&huart1, &rx_char, 1, 10); // Echo lại
      }
    }
  }
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void system_init()
{
  HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, 0);
  HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, 0);
  HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, 0);
  timer_init();
  led7_init();
  button_init();
  lcd_init();
  uart_init_rs232();
  setTimer2(50);
}

uint16_t count_led_debug = 0;

void test_LedDebug()
{
  count_led_debug = (count_led_debug + 1) % 20;
  if (count_led_debug == 0)
  {
    HAL_GPIO_TogglePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin);
  }
}

void test_button()
{
  for (int i = 0; i < 16; i++)
  {
    if (button_count[i] == 1)
    {
      led7_SetDigit(i / 10, 2, 0);
      led7_SetDigit(i % 10, 3, 0);
    }
  }
}

void test_Uart()
{
  // Button 12: Gửi thời gian qua UART (Bài 1)
  if (button_count[12] == 1)
  {
    uart_Rs232SendNum(ds3231_hours);
    uart_Rs232SendString(":");
    uart_Rs232SendNum(ds3231_min);
    uart_Rs232SendString(":");
    uart_Rs232SendNum(ds3231_sec);
    uart_Rs232SendString("\n");
  }

  // Button 13: Cập nhật thời gian - Bài 1 (không có timeout/retry)
  if (button_count[13] == 1)
  {
    uart_StartTimeUpdate();
  }

  // Button 14: Cập nhật thời gian - Bài 2 (có timeout và retry)
  if (button_count[14] == 1)
  {
    uart_StartTimeUpdateEx();
  }
}

void display_time_on_lcd()
{
  static uint8_t prev_sec = 0xFF; // Để kiểm tra xem có cần cập nhật LCD không

  if (ds3231_sec != prev_sec)
  { // Chỉ cập nhật LCD khi giây thay đổi
    prev_sec = ds3231_sec;

    char time_str[20];

    // Xóa toàn bộ màn hình
    lcd_Fill(0, 0, 240, 320, BLACK);

    // Hiển thị tiêu đề
    lcd_ShowStr(50, 80, (uint8_t *)"DIGITAL CLOCK", WHITE, BLACK, 24, 0);

    // Hiển thị thời gian HH:MM:SS (căn giữa màn hình)
    sprintf(time_str, "%02d:%02d:%02d", ds3231_hours, ds3231_min, ds3231_sec);
    lcd_ShowStr(40, 140, (uint8_t *)time_str, YELLOW, BLACK, 32, 0);
  }
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
