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

/* ---- 字模参数（按最宽的字符定：'M'/'W' 会顶满 8 列，所以必须留 1 格缝） ---- */
#define LCD_FONT_W     8     /* 字模宽（像素） */
#define LCD_FONT_H     16    /* 字模高（像素） */
#define LCD_FONT_ADV   9     /* 步进 = 宽 + 1 格缝 */

void LCD_Init(void);
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_FillScreen(uint16_t color);
void LCD_Backlight(uint8_t on);
void LCD_DrawGlyph(uint16_t x, uint16_t y, const uint8_t *glyph, uint16_t fg, uint16_t bg);
uint16_t LCD_DrawString(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg);
/* 区域块写：把 RGB565 像素原样灌进矩形（LVGL flush 回调用）
   len 必须 ≥ (x1-x0+1)*(y1-y0+1)*2，不足则整片不画 */
void LCD_BlitArea(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  const uint8_t *pixels, uint32_t len);
#endif /* __LCD_H */
