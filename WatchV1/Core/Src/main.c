/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "SEGGER_RTT.h"
#include <stdio.h>
#include "lcd.h"
#include "i2c_scan.h"

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
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  MX_SPI1_Init();
  MX_I2C1_Init();
  i2c_gpio_init();
  /* USER CODE BEGIN 2 */
  SEGGER_RTT_Init();                       /* 可选：第一次调用会自动初始化 */
  SEGGER_RTT_printf(0, "--- boot ---\r\n");     /* ← 加这句 */
  HAL_Delay(200);          /* ← 加这句：等 AHT21 上电就绪（手册要求 ≥100ms）*/
//  I2C_Scan();
//  uint8_t val = 0xA5;
//  uint8_t rd  = 0x00;
//  HAL_I2C_Mem_Write(&hi2c1, 0xA0, 0x00, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
//  HAL_Delay(10);
//  HAL_I2C_Mem_Read(&hi2c1, 0xA0, 0x00, I2C_MEMADD_SIZE_8BIT, &rd, 1, 100);

//  SEGGER_RTT_printf(0, "EEPROM write=0x%02X read=0x%02X -> %s\r\n",
//                  val, rd, (rd == val) ? "OK" : "FAIL");



//    /* ---- W1 块 2：点亮屏 ---- */
//  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);              /* ① 背光先亮起来 */
//  SEGGER_RTT_printf(0, "LCD init...\r\n");
//  LCD_Init();                                            /* ③ 屏初始化 */
//  SEGGER_RTT_printf(0, "LCD init done\r\n");
    uint8_t buf[6];

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    //呼叫AHT21
    i2c_start();
    i2c_write_byte(0x38 << 1 | 0);   /* 0x70 写地址 */
    i2c_wait_ack();                  /* 检查返回值！这次开始，每一步都该查 */
    HAL_Delay(10);
    i2c_write_byte(0xAC);   i2c_wait_ack();
    i2c_write_byte(0x33);   i2c_wait_ack();
    i2c_write_byte(0x00);   i2c_wait_ack();
    i2c_stop();
    HAL_Delay(80);                   /* 等测量完成 */

    //准备读温湿度数据
    i2c_start();
    i2c_write_byte(0x38 << 1 | 1);   /* 0x71 读地址 */
    i2c_wait_ack();

    //读取状态字,若Bit[7]为0，表示测量完成
    for (uint8_t i = 0; i < 6; i++)
    {
        buf[i] = i2c_read_byte();
        if (i < 5)  i2c_send_ack();     /* 后面还有，继续发 */
        else        i2c_send_nack();    /* 第 6 个是最后一个（不读 CRC），喊停 */
    }
    //开始读温湿度数据
    if( 0x00 == (buf[0] & 0x80) )//Bit[7]为0，表示测量完成
    {
      uint32_t data_w = 0;
      uint32_t data_t = 0;
      data_w = ((uint32_t)buf[3] >> 4) + ((uint32_t)buf[2] << 4) + ((uint32_t)buf[1] << 12);
      //float wet_data = data_w * 100.0f / (1 << 20);

      data_t = ((uint32_t)(buf[3] & 0x0F) << 16) + ((uint32_t)buf[4] << 8) + ((uint32_t)buf[5]);
      //float temperature_data = ( data_t * 200.0f / (1 << 20) ) - 50 ;


      

      /* ---- 打印：MicroLIB 不支持 %f，所以放大 10 倍后用整数拆开 ---- */
      uint32_t wet_x10 = (data_w * 1000u) >> 20;                          /* 湿度 ×10 */
      int32_t  tem_x10 = (int32_t)((data_t * 2000u) >> 20) - 500;         /* 温度 ×10 = raw×200/2^20 − 50 */
      int32_t  t_abs   = (tem_x10 < 0) ? -tem_x10 : tem_x10;              /* 负数单独取符号 */

      SEGGER_RTT_printf(0, "H = %d.%d %%   T = %s%d.%d C\r\n",
                        wet_x10 / 10, wet_x10 % 10,
                        (tem_x10 < 0) ? "-" : "", t_abs / 10, t_abs % 10);

      SEGGER_RTT_printf(0, "raw: %02X %02X %02X %02X %02X %02X\r\n",
                        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
                       // HAL_Delay(2000);
    }
    i2c_stop();
  }
  /* USER CODE END 3 */
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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

#ifdef  USE_FULL_ASSERT
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
