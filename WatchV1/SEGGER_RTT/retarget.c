/**
  ******************************************************************************
  * @file    retarget.c
  * @brief   printf 重定向到 SEGGER RTT（Keil MDK + MicroLIB）
  *
  * 为什么需要这个文件：
  *   MicroLIB 的 printf 最终出口是 fputc。库自带的 fputc 是 "semihosting 版"
  *   ——靠调试器停机应答，脱机运行会卡死在 BKPT。这里用 RTT 版的 fputc 覆盖它，
  *   printf 就变成往 RTT 通道 0 写（不占串口、不阻塞、不需要调试器停在断点）。
  *
  * 注意：MicroLIB 的 printf 不支持 %f（浮点）。要打印 25.3 得整数拆分：
  *       SEGGER_RTT_printf(0, "%d.%d", v/10, v%10);
  ******************************************************************************
  */
#include <stdio.h>
#include "SEGGER_RTT.h"

int fputc(int ch, FILE *f)
{
  (void)f;                          /* 只用通道 0，不区分 FILE* */
  SEGGER_RTT_PutChar(0, (char)ch);
  return ch;
}
