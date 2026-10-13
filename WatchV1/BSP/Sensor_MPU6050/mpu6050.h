/* mpu6050.h —— MPU6050 器件层（Driver）对外契约
 * 分层：main.c(app) → mpu6050_handler → 【本层】 → soft_i2c(时序)
 * 2026-10-11 建（W5）· 内容待写
 */
#ifndef __MPU6050_H__
#define __MPU6050_H__

#include <stdint.h>

typedef enum {
    MPU6050_OK = 0,
    MPU6050_ERR_NACK,     
    MPU6050_ERR_ID,
    MPU6050_ERR_BUSY,     
    MPU6050_ERR_PARAM,    
} mpu6050_err_t;

typedef struct {
    int16_t ax, ay, az;
    int16_t temp;
    int16_t gx, gy, gz;
} mpu6050_raw_t;

typedef struct {
    float ax_g, ay_g, az_g;
    float temp_c;
    float gx_dps, gy_dps, gz_dps;
} mpu6050_conv_t;

typedef struct{
    mpu6050_raw_t raw;
    mpu6050_conv_t conv;
} mpu6050_data_t;

mpu6050_err_t mpu6050_init(void);
mpu6050_err_t mpu6050_read(mpu6050_data_t *out);

#endif /* __MPU6050_H__ */
