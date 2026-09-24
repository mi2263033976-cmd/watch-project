/**
  ******************************************************************************
  * @file    lcd.h
  * @brief   1.69" ST7789 (240x280, SPI) 最小驱动 —— W1 探路版
  ******************************************************************************
  */
#ifndef __LCD_H
#define __LCD_H

#include "main.h"
#include <stdint.h>

/* ---- 屏幕尺寸与显存偏移 ---- */
#define LCD_WIDTH     240
#define LCD_HEIGHT    280
#define LCD_X_OFFSET  0
#define LCD_Y_OFFSET  20     /* 本屏 240x280，而 ST7789 显存是 240x320，行要 +20 */

/* ---- RGB565 常用色 ---- */
#define LCD_BLACK   0x0000
#define LCD_WHITE   0xFFFF
#define LCD_RED     0xF800
#define LCD_GREEN   0x07E0
#define LCD_BLUE    0x001F
#define LCD_YELLOW  0xFFE0
#define LCD_CYAN    0x07FF
#define LCD_MAGENTA 0xF81F

void LCD_Init(void);
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_FillScreen(uint16_t color);

#endif /* __LCD_H */
