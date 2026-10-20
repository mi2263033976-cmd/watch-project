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
#include "cmsis_os.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "SEGGER_RTT.h"
#include <stdio.h>
#include "lcd.h"
#include "aht21.h"
#include "soft_i2c.h"
#include "aht21_handler.h"
#include "mpu6050.h"
#include "mpu6050_handler.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MPU6050_ADDR7              0x68
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 打印一次温湿度：RTT 不支持 %f → ×10 拆整数（这是【显示层】的活儿）
 * tag：数据新鲜度标注 —— "" = 新鲜；" [stale]" = 用的是上次缓存 */
//static void print_temp_humi(float t, float h, const char *tag)
//{

//	int32_t h_x10 = (int32_t)(h * 10.0f);
//		int32_t t_x10 = (int32_t)(t * 10.0f);
//		int32_t t_abs = (t_x10 < 0) ? -t_x10 : t_x10;      /* 负数单独取符号 */
//	
//	SEGGER_RTT_printf(0, "H = %d.%d %%   T = %s%d.%d C%s\r\n",
//						h_x10 / 10, h_x10 % 10,
//							(t_x10 < 0) ? "-" : "", t_abs / 10, t_abs % 10,
//              tag);
//}
/* ── MPU6050 显示层（RTT 没有 %f，浮点要自己拆）────────────────── */

/* 把一个 float 按 ×10 打成 "名字=±整数.小数"
 *   套路同 W4：先取绝对值再拆，符号单独打（负数 % 在 C 里是负数） */
static void rtt_print_f10(const char *name, float v)
{
    int32_t x10 = (int32_t)(v * 10.0f);
    int32_t a   = (x10 < 0) ? -x10 : x10;

    SEGGER_RTT_printf(0, "%s=%s%d.%d ", name,
                      (x10 < 0) ? "-" : "", (int)(a / 10), (int)(a % 10));
}

/* 打印一次 6 轴 + 温度：raw 与换算值都打
 *   raw 用 %d 直接打 —— 它是核对"地址自增"和"量程配对"的原始证据
 * tag：数据新鲜度标注 —— "" = 新鲜；" [stale]" = 用的是上次缓存 */
static void print_mpu6050(const mpu6050_data_t *d, const char *tag)
{
    SEGGER_RTT_printf(0, "RAW  ax=%6d ay=%6d az=%6d temp=%6d gx=%6d gy=%6d gz=%6d\r\n",
                      d->raw.ax, d->raw.ay, d->raw.az, d->raw.temp,
                      d->raw.gx, d->raw.gy, d->raw.gz);

    SEGGER_RTT_printf(0, "CONV ");
    rtt_print_f10("ax",   d->conv.ax_g);
    rtt_print_f10("ay",   d->conv.ay_g);
    rtt_print_f10("az",   d->conv.az_g);
    rtt_print_f10("temp", d->conv.temp_c);
    rtt_print_f10("gx",   d->conv.gx_dps);
    rtt_print_f10("gy",   d->conv.gy_dps);
    rtt_print_f10("gz",   d->conv.gz_dps);
    SEGGER_RTT_printf(0, "%s\r\n", tag);
}

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
  /* USER CODE BEGIN 2 */
  i2c_gpio_init();         /* ⚠️ 必须留在 USER CODE 区：把 PB6/PB7 覆盖成开漏（放生成区会被 CubeMX 删掉） */

  SEGGER_RTT_Init();                            /* 先把日志起起来（下面才好打） */
  SEGGER_RTT_printf(0, "--- boot ---\r\n");

  mpu6050_handler_set_delay(HAL_Delay);      /* ← 把平台的延时接给 handler */

  /* MPU6050 上电初始化 —— 【开机只做一次】
   *   ⚠️ 它内部会复位器件 ⇒ 放进主循环 = 每秒把器件打回睡眠
   *   失败要打【错误码】：1=NACK（总线不通） 2=ID 不符（不是 6050） 3=BUSY（复位没等到） */
  mpu6050_err_t init_ret = mpu6050_init();
  if (init_ret != MPU6050_OK)
  {
    SEGGER_RTT_printf(0, "MPU6050 init fail = %d\r\n", init_ret);
  }

  mpu6050_data_t mpu;      /* 出参实例：栈上开一次，主循环里反复复用 */

  /* 打印限流用（放 while 外面 —— 放循环体里每次都"看起来"重置，骗读代码的人）*/
  static uint32_t last_print_ms = 0;

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

  

 
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
      /* 中断驱动：有新数据才读（标志由 HAL_GPIO_EXTI_Callback → notify() 置）
       *   没新数据      → H_NO_DATA 不读、不打，立刻下一圈
       *   正常          → H_OK      新鲜数据，照打
       *   连续失败 < 5  → H_STALE   给上次缓存，标注 [stale]
       *   连续失败 ≥ 5  → H_OFFLINE 旧数据已撤，明确报失联
       *   参数错        → H_PARAM   （本处不会触发） */
      // mpu6050_handler_status_t hs = mpu6050_handler_read_if_new(&mpu);

      // if (MPU6050_H_NO_DATA == hs)
      // {
      //   continue;                       /* ← 最常走的路：没新数据 */
      // }

      // /* 打印限流：中断 100Hz，不能每次打（否则 1 秒 100 行刷屏）
      //  *   ⚠️ 只拦"打印"，不拦"读" —— 若让主循环变慢，标志又会恒为 1 ⇒ 退回轮询 */
      // if ((HAL_GetTick() - last_print_ms) < 200)
      // {
      //   continue;
      // }
      // last_print_ms = HAL_GetTick();

      // switch (hs)
      // {
      //   case MPU6050_H_OK:
      //     print_mpu6050(&mpu, "");
      //     break;
      
      //   case MPU6050_H_STALE:
      //     print_mpu6050(&mpu, " [stale]");
      //     break;

      //   case MPU6050_H_OFFLINE:
      //     SEGGER_RTT_printf(0, "MPU6050 OFFLINE\r\n");
      //     break;

      //   case MPU6050_H_PARAM:
      //     SEGGER_RTT_printf(0, "MPU6050 param error\r\n");
      //     break;

      //   default:
      //     break;
      // }

	    // hs = aht21_handler_read(&t, &h);

	    // switch (hs)
	    // {
	    // 	case AHT21_H_OK:
	    // 		/* 新鲜数据：照老样子打印 */
	    // 	print_temp_humi(t, h, "");          /* 新鲜数据 */
	    // 		break;

	    // 	case AHT21_H_STALE:
	    // 		/* 旧数据：数据照样打，后面加个标记 */
	    // 		/* 旧数据：照打，只多一个 [stale] 标注 */
	    // 	print_temp_humi(t, h, " [stale]");  /* 这次没读到 → 给的是上次缓存 */
	    // 		break;

	    // 	case AHT21_H_OFFLINE:
	    // 		SEGGER_RTT_printf(0, "AHT21 OFFLINE\r\n");
	    // 		break;

	    // 	case AHT21_H_PARAM:
	    // 		SEGGER_RTT_printf(0, "AHT21 param error\r\n");
	    // 		break;

	    // 	default:
	    // 		break;
	    // }
    
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
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_5)
    {
      mpu6050_handler_notify();
    }
}
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
