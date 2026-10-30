#include "frame.h"
#include "SEGGER_RTT.h"

#define FRAME_HEAD0     0xAAu
#define FRAME_HEAD1     0x55u
#define FRAME_MAX_DATA  32u          /* 单帧数据区上限 */

static uint32_t g_ok       = 0;
static uint32_t g_junk     = 0;
static uint32_t g_verfail  = 0;
static uint32_t g_oversize = 0;
static uint32_t g_timeout  = 0;

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

frame_err_t frame_poll(uint32_t now_ms, uint8_t *out, uint8_t *out_len)
{
    if ((NULL == out) || (NULL == out_len)) return FRAME_WAIT;  /* 空指针判断 */

    if (uart_rx_avail() < 3)    return FRAME_WAIT;      /* 帧头2 + 长度1 都读不满 */
                       
    uint8_t b0 = 0, b1 = 0, len = 0;    /* 初始化数据 */

    //帧头
    if (!uart_rx_at(0, &b0) || !uart_rx_at(1, &b1))     return FRAME_WAIT;    /* 读帧头 */
    if(b0 != FRAME_HEAD0 || b1 != FRAME_HEAD1)          /* 帧头不对，丢掉 */
    {
        uart_rx_drop(1);
        g_junk ++;
        return FRAME_PROGRESS;
    }

    //长度
    if(!uart_rx_at(2,&len))      return FRAME_WAIT;      /* 读长度 */
    if(len > FRAME_MAX_DATA)                            /* 长度不对，丢掉 */
    {
        uart_rx_drop(1);
        g_oversize ++;
        return FRAME_PROGRESS;
    }

    //代入CRC进行校验
    uint8_t need = 3 + len + 2;    /* 总长度:帧头2 + 长度1 + 数据len + CRC2 */   
    if(uart_rx_avail() < need)      return FRAME_WAIT;      /* 数据没收齐，等下次唤醒 */

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

    return FRAME_WAIT;  /* 理论上不会到这里 */

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
