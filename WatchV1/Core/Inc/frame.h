#ifndef FRAME_H
#define FRAME_H

#include "usart.h"

typedef enum {
    FRAME_WAIT = 0,     /* 帧没收齐，读指针没动，下次再来 */
    FRAME_PROGRESS,     /* 已丢弃 1 字节，可立刻再调 */
    FRAME_OK            /* 收到整帧，数据在 out */  
} frame_err_t;

uint16_t frame_crc16(const uint8_t *byte, uint16_t len);                      /* 算 CRC-16/CCITT-FALSE */

frame_err_t frame_poll(uint32_t now_ms, uint8_t *out, uint8_t *out_len); /* 解析一步：等 / 丢 1 字节 / 交整帧 */
uint32_t frame_ok(void);          /* 通过的整帧数 */
uint32_t frame_junk(void);        /* 丢弃的字节数（垃圾 + 超时） */
uint32_t frame_verfail(void);     /* 校验失败次数 */
uint32_t frame_oversize(void);    /* LEN 超上限次数 */
uint32_t frame_timeout(void);     /* 超时丢弃的字节数（含在 junk 里） */

void frame_print_counters(void);  /* 打印统计信息到 RTT */

#endif // FRAME_H
