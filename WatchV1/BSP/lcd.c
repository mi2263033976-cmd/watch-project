/**
  ******************************************************************************
  * @file    lcd.c
  * @brief   1.69" P169H002 (ST7789, 240x280, SPI) 最小驱动 —— W1 探路版
  * @note    ★ 本文件与教学资源 Bsp/Disp/ST7789 的写法保持一致：
  *          - COLMOD = 0x55（16bit RGB565）
  *          - MADCTL = 0xC0（MX|MY|RGB）
  *          - 每次传输都包 CS（拉低→发→拉高），不常低
  *          - 偏移 X=0 / Y=20（对应 240x280 + ROTATION 0）
  *          - SPI Mode 3（CPOL=High, CPHA=2Edge）
  ******************************************************************************
  */
#include "lcd.h"
#include "spi.h"        /* CubeMX 生成：hspi1 */

/* ============ ① 引脚操作宏（对应 CubeMX 里的 User Label）============ */
#define CS_L()    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,  LCD_CS_Pin,  GPIO_PIN_RESET)
#define CS_H()    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,  LCD_CS_Pin,  GPIO_PIN_SET)
#define DC_CMD()  HAL_GPIO_WritePin(LCD_DC_GPIO_Port,  LCD_DC_Pin,  GPIO_PIN_RESET)  /* DC=0 命令 */
#define DC_DATA() HAL_GPIO_WritePin(LCD_DC_GPIO_Port,  LCD_DC_Pin,  GPIO_PIN_SET)    /* DC=1 数据 */
#define RST_L()   HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET)
#define RST_H()   HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET)

/* ============ ② 发命令 / 发数据（各自独立包 CS，照教学资源）============ */
static void LCD_WriteCmd(uint8_t cmd)
{
    CS_L();
    DC_CMD();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    CS_H();
}

static void LCD_WriteData(const uint8_t *buf, uint16_t len)
{
    CS_L();
    DC_DATA();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, 1000);
    CS_H();
}

/* ============ ③ 设显存窗口（CASET + RASET + RAMWR，一个 CS 窗口内）============
   ⚠️ 必须加偏移：本屏 240x280，ST7789 显存 240x320，行要 +20 */
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t p[4];

    x0 = (uint16_t)(x0 + LCD_X_OFFSET);  x1 = (uint16_t)(x1 + LCD_X_OFFSET);
    y0 = (uint16_t)(y0 + LCD_Y_OFFSET);  y1 = (uint16_t)(y1 + LCD_Y_OFFSET);

    CS_L();
    DC_CMD();
    p[0] = 0x2A; HAL_SPI_Transmit(&hspi1, p, 1, 100);          /* CASET */
    DC_DATA();
    p[0] = (uint8_t)(x0 >> 8); p[1] = (uint8_t)x0;
    p[2] = (uint8_t)(x1 >> 8); p[3] = (uint8_t)x1;
    HAL_SPI_Transmit(&hspi1, p, 4, 1000);

    DC_CMD();
    p[0] = 0x2B; HAL_SPI_Transmit(&hspi1, p, 1, 100);          /* RASET */
    DC_DATA();
    p[0] = (uint8_t)(y0 >> 8); p[1] = (uint8_t)y0;
    p[2] = (uint8_t)(y1 >> 8); p[3] = (uint8_t)y1;
    HAL_SPI_Transmit(&hspi1, p, 4, 1000);

    DC_CMD();
    p[0] = 0x2C; HAL_SPI_Transmit(&hspi1, p, 1, 100);          /* RAMWR */
    CS_H();
}

/* ============ ④ 初始化（顺序/数值完全照教学资源 st7789_init）============ */
void LCD_Init(void)
{
    uint8_t d1[1];
    uint8_t d2[2];
    uint8_t d5[5];
    uint8_t d14[14];

    /* --- 硬件复位：低10ms → 高后等20ms（照教学资源）--- */
    RST_H(); HAL_Delay(10);
    RST_L(); HAL_Delay(10);
    RST_H(); HAL_Delay(20);

    d1[0] = 0x55; LCD_WriteCmd(0x3A); LCD_WriteData(d1, 1);   /* COLMOD: 16bit RGB565 */

    d5[0]=0x0C; d5[1]=0x0C; d5[2]=0x00; d5[3]=0x33; d5[4]=0x33;
    LCD_WriteCmd(0xB2); LCD_WriteData(d5, 5);                 /* PORCTRL */

    d1[0] = 0xC0; LCD_WriteCmd(0x36); LCD_WriteData(d1, 1);   /* MADCTL: MX|MY|RGB */

    d1[0]=0x35; LCD_WriteCmd(0xB7); LCD_WriteData(d1, 1);     /* GCTRL   */
    d1[0]=0x19; LCD_WriteCmd(0xBB); LCD_WriteData(d1, 1);     /* VCOMS   */
    d1[0]=0x2C; LCD_WriteCmd(0xC0); LCD_WriteData(d1, 1);     /* LCMCTRL */
    d1[0]=0x01; LCD_WriteCmd(0xC2); LCD_WriteData(d1, 1);     /* VDVVRHEN */
    d1[0]=0x12; LCD_WriteCmd(0xC3); LCD_WriteData(d1, 1);     /* VRHS    */
    d1[0]=0x20; LCD_WriteCmd(0xC4); LCD_WriteData(d1, 1);     /* VDVS    */
    d1[0]=0x0F; LCD_WriteCmd(0xC6); LCD_WriteData(d1, 1);     /* FRCTRL2 */
    d2[0]=0xA4; d2[1]=0xA1; LCD_WriteCmd(0xD0); LCD_WriteData(d2, 2);  /* PWCTRL1 */

    d14[0]=0xD0; d14[1]=0x04; d14[2]=0x0D; d14[3]=0x11; d14[4]=0x13;
    d14[5]=0x2B; d14[6]=0x3F; d14[7]=0x54; d14[8]=0x4C; d14[9]=0x18;
    d14[10]=0x0D; d14[11]=0x0B; d14[12]=0x1F; d14[13]=0x23;
    LCD_WriteCmd(0xE0); LCD_WriteData(d14, 14);               /* PVGAMCTRL */

    d14[0]=0xD0; d14[1]=0x04; d14[2]=0x0C; d14[3]=0x11; d14[4]=0x13;
    d14[5]=0x2C; d14[6]=0x3F; d14[7]=0x44; d14[8]=0x51; d14[9]=0x2F;
    d14[10]=0x1F; d14[11]=0x1F; d14[12]=0x20; d14[13]=0x23;
    LCD_WriteCmd(0xE1); LCD_WriteData(d14, 14);               /* NVGAMCTRL */

    LCD_WriteCmd(0x21);                                       /* INVON  反显 */
    LCD_WriteCmd(0x11);                                       /* SLPOUT 退出睡眠 */
    LCD_WriteCmd(0x13);                                       /* NORON  常规显示 */
    LCD_WriteCmd(0x29);                                       /* DISPON 开显示 */
    HAL_Delay(50);
}

/* ============ ⑤ 全屏填色 ============ */
void LCD_FillScreen(uint16_t color)
{
    static uint8_t line[LCD_WIDTH * 2];   /* 一行像素（RGB565 = 2 字节/像素）*/
    uint16_t i, y;

    for (i = 0U; i < LCD_WIDTH; i++) {
        line[i * 2U]      = (uint8_t)(color >> 8);
        line[i * 2U + 1U] = (uint8_t)(color & 0xFFU);
    }

    LCD_SetWindow(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    CS_L();
    DC_DATA();
    for (y = 0U; y < LCD_HEIGHT; y++) {
        HAL_SPI_Transmit(&hspi1, line, (uint16_t)sizeof(line), 1000);
    }
    CS_H();
}
