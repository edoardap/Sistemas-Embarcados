#ifndef IMU_H
#define IMU_H

void i2c_master_init();
void mpu6050_init();
void mpu6050_read_accel(float *ax, float *ay, float *az);

#endif