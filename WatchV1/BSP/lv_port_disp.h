/**
  ******************************************************************************
  * @file    lv_port_disp.h
  * @brief   LVGL 显示移植层 —— 把 LVGL 画好的像素送到 ST7789 屏上
  ******************************************************************************
  */
#ifndef __LV_PORT_DISP_H
#define __LV_PORT_DISP_H

#include "lvgl.h"

/* 显示缓冲：一行 = 屏宽 × 2 字节；LV_HOR_LINES 行 = 一块
   本屏 240×280，LOCAL_BUF_LINES = 20 ⇒ 240×20×2 = 9600 B
   行数越小越省 RAM，但 LVGL 要分更多次刷（每次都要重设窗口）。 */
#define LV_PORT_BUF_LINES   20U

/* 初始化并注册 LVGL 显示设备（要在 lv_init() 之后调用） */
void lv_port_disp_init(void);

#endif /* __LV_PORT_DISP_H */
