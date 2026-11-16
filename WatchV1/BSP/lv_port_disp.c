/**
  ******************************************************************************
  * @file    lv_port_disp.c
  * @brief   LVGL 显示移植层
  *
  *  职责只有一件事：LVGL 说「这块区域我画好了，像素在这里」，
  *  我们就把它原样搬到屏上，然后告诉 LVGL「送完了」。
  *
  *  为什么能这么简单：底下已经有 LCD_BlitArea()（矩形 + 像素一次性搬完），
  *  这个回调不需要知道 ST7789、不需要知道 SPI、也不需要认识字符。
  ******************************************************************************
  */
#include "lv_port_disp.h"
#include "lcd.h"

/* 显示缓冲（静态，放 ZI 段）。
   LV_COLOR_16_SWAP = 1 时，LVGL 已经把每个像素按「高字节在前」排好，
   所以这里可以直接按字节数组发给屏，不用逐像素拆。 */
static lv_color_t disp_buf[LCD_WIDTH * LV_PORT_BUF_LINES];

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (uint32_t)(area->x2 - area->x1) + 1U;
    uint32_t h = (uint32_t)(area->y2 - area->y1) + 1U;
    uint32_t len = w * h * 2U;      /* RGB565：每像素 2 字节 */

    LCD_BlitArea((uint16_t)area->x1, (uint16_t)area->y1,
                 (uint16_t)area->x2, (uint16_t)area->y2,
                 (const uint8_t *)color_p, len);

    /* ★ 必须调用：告诉 LVGL「这块送完了，可以给我下一块」。
       漏了这一句，LVGL 会一直等，屏上只出第一块就不动了。 */
    lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init(void)
{
    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t      disp_drv;

    /* 单缓冲：buf1 有效、buf2 = NULL。够了——瓶颈在 SPI，不在双缓冲。 */
    lv_disp_draw_buf_init(&draw_buf, disp_buf, NULL, LCD_WIDTH * LV_PORT_BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = LCD_WIDTH;      /* 240 */
    disp_drv.ver_res  = LCD_HEIGHT;     /* 280 */
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;

    lv_disp_drv_register(&disp_drv);
}
