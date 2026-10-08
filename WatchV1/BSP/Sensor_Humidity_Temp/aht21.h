#ifndef __AHT21_H__
#define __AHT21_H__



typedef enum{
    AHT21_OK = 0,
    AHT21_ERR_NACK,     /* 从机没应答：器件不在 / 线断 / 总线被拽住 */
    AHT21_ERR_BUSY,     /* 测量还没完成（状态位 bit7 = 1） */
    AHT21_ERR_PARAM,    /* 传进来的指针是 NULL */
} aht21_err_t;

aht21_err_t aht21_init(void);                                  /* 上电等待 + 在线检查（写地址看 ACK）   */
aht21_err_t aht21_read_temp_humi(float *temp,float *humi);     /* 读一次温湿度 → ℃ / %RH（五步内部完成）*/


#endif
