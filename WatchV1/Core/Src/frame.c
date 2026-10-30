#include "frame.h"
#include "SEGGER_RTT.h"

#define FRAME_HEAD0     0xAAu
#define FRAME_HEAD1     0x55u
#define FRAME_MAX_DATA  32u          /* 单帧数据区上限 */
#define FRAME_TIMEOUT_MS  4         /*最大静默时间*/

/* 对账用的五个计数器：g_ok 数的是"帧"，其余四个数的是"字节"——口径不同，看日志时别混 */
static uint32_t g_ok       = 0;   /* 整帧通过校验并已交付的帧数（不含被丢弃的） */
static uint32_t g_junk     = 0;   /* 帧头对不上、或超时，丢掉的字节数（**含 g_timeout**） */
static uint32_t g_verfail  = 0;   /* CRC 不符、判为假帧头/坏帧，丢 1 字节的次数 */
static uint32_t g_oversize = 0;   /* LEN 超过上限、判整段不可信，丢 1 字节的次数 */
static uint32_t g_timeout  = 0;   /* 断流超时丢字节的次数（g_junk 的子集，别再叠加） */

static uint16_t g_last_avail   = 0;     /*上次环里的字节数*/
static uint32_t g_idle_since   = 0;     /*从哪一刻起就一直没进展*/
static uint8_t g_timer_armed  = 0;     /*基准有没有建立，0为没开始计时，1为已经计时*/


//等待与超时判断
static frame_err_t frame_wait_or_timeout(uint32_t now_ms)
{
    uint16_t avail = uart_rx_avail();       /*先获取当前环里字节数*/

    if(avail == 0)      /* 环里没增加 */
    {
        g_timer_armed = 1;
        g_idle_since = now_ms;
        g_last_avail = 0;
        return FRAME_WAIT;
    }

    if(!g_timer_armed)      /* 首次调用只记基准，不判超时（否则 now - 0 直接判死，第一帧必丢） */
    {
        g_timer_armed = 1;
        g_last_avail  = avail;
        g_idle_since  = now_ms;
        return FRAME_WAIT;
    }

    if(avail > g_last_avail)    /* 环里有进展，重置计时 */
    {
        g_last_avail = avail;
        g_idle_since = now_ms;    /* 有进展，重置计时 */
        return FRAME_WAIT;
    }

    if((uint32_t)(now_ms - g_idle_since) >= FRAME_TIMEOUT_MS)    /*静默够久：丢 1 字节，记账，重新计时*/
    {
        uart_rx_drop(1);
        g_junk ++;  g_timeout ++;
        g_last_avail = uart_rx_avail();     /* drop 后 avail 变小，必须同步，否则下轮误判 */
        g_idle_since = now_ms;
        return FRAME_PROGRESS;
    }

    return FRAME_WAIT;    
}


// CRC16校验函数:多项式 0x1021,初值0xFFFF,不反转输入输出,不异或输出
uint16_t frame_crc16(const uint8_t *byte, uint16_t len)
{
    uint16_t crc = 0xFFFF;      /* 初值 */

    for(uint16_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)byte[i] << 8);    /* 异或输入数据 */

        for(uint8_t bit = 0; bit < 8; bit ++)
        {
            if((crc & 0x8000) != 0)
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            else
                crc <<= 1;
        }
    }
    return crc;
}

//帧解析主流程
frame_err_t frame_poll(uint32_t now_ms, uint8_t *out, uint8_t *out_len)
{
    if ((NULL == out) || (NULL == out_len))  return FRAME_WAIT;  /* 空指针判断 */

    if (uart_rx_avail() < 3)    return FRAME_WAIT;      /* 帧头2 + 长度1 都读不满 */
                                                        /* 可能开头是帧头，所以FRAME_WAIT */
    uint8_t b0 = 0, b1 = 0, len = 0;    /* 初始化数据 */

    //帧头
    if (!uart_rx_at(0, &b0) || !uart_rx_at(1, &b1))  return frame_wait_or_timeout(now_ms);;    /* 读帧头 */
    if(b0 != FRAME_HEAD0 || b1 != FRAME_HEAD1)          /* 帧头不对，丢掉 */
    {
        uart_rx_drop(1);
        g_junk ++;
        return FRAME_PROGRESS;
    }

    //长度
    if(!uart_rx_at(2,&len))  return frame_wait_or_timeout(now_ms);      /* 读长度 */
    if(len > FRAME_MAX_DATA)                            /* 长度不对，丢掉 */
    {
        uart_rx_drop(1);
        g_oversize ++;
        return FRAME_PROGRESS;
    }

    //代入CRC进行校验
    uint8_t need = 3 + len + 2;    /* 总长度:帧头2 + 长度1 + 数据len + CRC2 */   
    if(uart_rx_avail() < need)   return frame_wait_or_timeout(now_ms);;      /* 数据没收齐，等下次唤醒 */

    uint8_t tmp[1 + FRAME_MAX_DATA]; 
    for (uint16_t i = 0; i < (uint16_t)(1 + len); i++)  /*装填tmp，长度位和数据位*/
    {
        uart_rx_at((uint16_t)(i + 2),&tmp[i]);    /*起点为i+2，跳过帧头*/  
    }

    uint16_t crc_calc = frame_crc16(tmp,(uint16_t)(1 + len));  /*计算CRC*/
    uint8_t crc_low = 0, crc_high = 0;
    uart_rx_at((uint16_t)(need - 2), &crc_low);    /*读CRC低位*/
    uart_rx_at((uint16_t)(need - 1), &crc_high);   /*读CRC高位*/
    uint16_t crc_rx = (uint16_t)((crc_high << 8) | crc_low);   /*组装CRC*/

    if(crc_calc != crc_rx)      /*比对*/
    {
        uart_rx_drop(1); g_verfail ++; return FRAME_PROGRESS;
    }
    else
    {
        for (uint16_t i = 0; i < len; i++)  /* 拷贝数据到输出 */
        {
            uart_rx_at((uint16_t)(i + 3), &out[i]);    /*起点为i+3，跳过帧头和长度位;<len,跳过帧尾*/  
        }
        uart_rx_drop(need);    /* 丢掉整帧 */
        *out_len = len;
        g_ok ++;
        return FRAME_OK;
    }
}

uint32_t frame_ok(void)       { return g_ok; }
uint32_t frame_junk(void)     { return g_junk; }
uint32_t frame_verfail(void)  { return g_verfail; }
uint32_t frame_oversize(void) { return g_oversize; }
uint32_t frame_timeout(void)  { return g_timeout; }

void frame_print_counters(void)
{
    SEGGER_RTT_printf(0, "[F] ok=%u junk=%u verfail=%u oversize=%u timeout=%u avail=%u\r\n",
                      (unsigned)g_ok,       (unsigned)g_junk,
                      (unsigned)g_verfail,  (unsigned)g_oversize,
                      (unsigned)g_timeout,  (unsigned)uart_rx_avail());
}
