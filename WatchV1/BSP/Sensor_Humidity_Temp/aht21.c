#include "aht21.h"
#include "main.h"
#include "soft_i2c.h"

/* 写一个字节 + 检查应答（私有助手） */
static aht21_err_t aht21_write_checked(uint8_t byte)
{
	
    /* ① 把这个字节发出去（调用 i2c_write_byte）                      */
	
	i2c_write_byte(byte);   
    /* ② 等应答：非 0（NACK）→ 先 i2c_stop() 释放总线，再 return NACK  */
	if(0 != i2c_wait_ack())
	{
		i2c_stop();
		return AHT21_ERR_NACK;
	}
    /* ③ 应答正常 → return AHT21_OK                                   */
	else
	{
		return AHT21_OK;
	}
}


aht21_err_t aht21_init(void)
{
	HAL_Delay(200); 
	/* 发一次 START + 写地址 0x38，确认它在不在 */
	i2c_start();
	if (aht21_write_checked(0x38 << 1 | 0) != AHT21_OK) return AHT21_ERR_NACK;  
	i2c_stop();
    return AHT21_OK;
}
aht21_err_t aht21_read_temp_humi(float *temp,float *humi)
{
	uint8_t buf[6];
	
	/* ① 参数检查：temp / humi 是空指针 → AHT21_ERR_PARAM */
	if( NULL == temp || NULL == humi )	{return AHT21_ERR_PARAM;}
	/* ② 触发测量：START → 0x70 → 0xAC 0x33 0x00 → STOP → 等 80ms */
	i2c_start();
    if (aht21_write_checked(0x38 << 1 | 0) != AHT21_OK) return AHT21_ERR_NACK;                  
    HAL_Delay(10);
    if (aht21_write_checked(0xAC) != AHT21_OK) return AHT21_ERR_NACK;
    if (aht21_write_checked(0x33) != AHT21_OK) return AHT21_ERR_NACK;
    if (aht21_write_checked(0x00) != AHT21_OK) return AHT21_ERR_NACK;
    i2c_stop();
    HAL_Delay(80);                  

	/* ③ 读 6 字节：START → 0x71 → 连读 6 个，第 6 个发 NACK → STOP */
	i2c_start();
	if (aht21_write_checked(0x38 << 1 | 1) != AHT21_OK) return AHT21_ERR_NACK;   /* 读地址 */
	for (uint8_t i = 0; i < 6; i++)
	{
	    buf[i] = i2c_read_byte();
	    if (i < 5)  i2c_send_ack();     /* 还有 → 主机回 ACK */
	    else        i2c_send_nack();    /* 最后一个 → 主机喊停 */
	}
	i2c_stop();

	/* ④ 状态位校验：buf[0] 的 bit7 是 1 → AHT21_ERR_BUSY */
	if (buf[0] & 0x80) 
	{ return AHT21_ERR_BUSY; }

	/* ⑤ 拼接 + 换算：→ *temp / *humi，return AHT21_OK */
	else 
	{
		uint32_t data_w = 0;
		uint32_t data_t = 0;
		data_w = ((uint32_t)buf[3] >> 4) + ((uint32_t)buf[2] << 4) + ((uint32_t)buf[1] << 12);
		data_t = ((uint32_t)(buf[3] & 0x0F) << 16) + ((uint32_t)buf[4] << 8) + ((uint32_t)buf[5]);

		*humi = data_w * 100.0f / (1 << 20);
		*temp = ( data_t * 200.0f / (1 << 20) ) - 50 ; 

		return AHT21_OK;
	}
      
}


