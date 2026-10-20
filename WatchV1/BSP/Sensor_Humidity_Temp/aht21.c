#include "aht21.h"
#include "main.h"
#include "soft_i2c.h"
#include "FreeRTOS.h"
#include "task.h"

/* 初始化：等 AHT21 上电就绪，再发一次"写地址"，靠从机 ACK 确认器件在线 */
aht21_err_t aht21_init(void)
{
	vTaskDelay(pdMS_TO_TICKS(200)); /* 等 200ms，保证 AHT21 上电就绪 */
	/* 发一次 START + 写地址 0x38，确认它在不在 */
	if(i2c_ping(0x38) != 0)	{return AHT21_ERR_NACK;}   /* 探活：无应答即 NACK；STOP 已由 soft_i2c 发出 */
    return AHT21_OK;
}
/* 读一次温湿度（五步）：参数检查 → 触发测量 → 读 6 字节 → 状态位校验 → 拼接+换算
 * 出参：*temp = ℃，*humi = %RH（物理量归驱动，排版归上层）
 * 失败路径：总线由 soft_i2c 层释放（i2c_mem_checked 内部已 stop），本层只返回错误码 */
aht21_err_t aht21_read_temp_humi(float *temp,float *humi)
{
	uint8_t buf[6];
	const uint8_t cmd[2] = { 0x33, 0x00 };   /* 0xAC 的两字节参数（AHT21 无寄存器概念） */
	
	/* ① 参数检查：temp / humi 是空指针 → AHT21_ERR_PARAM */
	if( NULL == temp || NULL == humi )	{return AHT21_ERR_PARAM;}
	/* ② 触发测量：START → 0x70 → 0xAC 0x33 0x00 → STOP → 等 80ms */
	if (i2c_mem_write(0x38, 0xAC, cmd, 2) != 0) {return AHT21_ERR_NACK;}
    vTaskDelay(pdMS_TO_TICKS(80));  /* 等 80ms，保证测量完成 */             

	/* ③ 读 6 字节：START → 0x71 → 连读 6 个，第 6 个发 NACK → STOP */
	if(i2c_read_bytes(0x38, buf, 6) != 0)  {return AHT21_ERR_NACK;} /* 连读 6 个字节 */

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


