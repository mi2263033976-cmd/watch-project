/**
  ******************************************************************************
  * @file    i2c_scan.c
  * @brief   I2C 总线扫描器：逐个探测 0x01~0x7F，结果走 RTT
  * @note    ★ 本项目日志约定：统一用 SEGGER_RTT_printf(0, ...)
  *            - 不占串口，串口(PA9/PA10)留给 CH340G / 上位机 / Bootloader
  *            - 不依赖 C 库，中断里也能安全用
  *          探测原理：发 START + 地址，看从机有没有回 ACK（不读任何数据）
  ******************************************************************************
  */
#include "i2c_scan.h"
#include "i2c.h"
#include "SEGGER_RTT.h"

void I2C_Scan(void)
{
    uint8_t addr;
    uint8_t found = 0U;

    SEGGER_RTT_printf(0, "I2C scan.\r\n");

    for (addr = 1U; addr < 128U; addr++) {
        /* 第 2 个参数是 8 位地址：7 位地址 << 1 */
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2U, 20U) == HAL_OK) {
            SEGGER_RTT_printf(0, "  found: 0x%02X\r\n", addr);
            found++;
        }
    }

    SEGGER_RTT_printf(0, "I2C scan done: %d device(s)\r\n", found);
}
