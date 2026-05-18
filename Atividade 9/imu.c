#include "imu.h"

#include "driver/i2c.h"

#define MPU6050_ADDR 0x68

#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

void i2c_master_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ};

    i2c_param_config(I2C_MASTER_NUM, &conf);

    i2c_driver_install(I2C_MASTER_NUM,
                       conf.mode,
                       0,
                       0,
                       0);
}

void mpu6050_init()
{
    uint8_t data[2];

    data[0] = 0x6B;
    data[1] = 0x00;

    i2c_master_write_to_device(
        I2C_MASTER_NUM,
        MPU6050_ADDR,
        data,
        2,
        1000 / portTICK_PERIOD_MS);
}

void mpu6050_read_accel(float *ax, float *ay, float *az)
{
    uint8_t reg = 0x3B;
    uint8_t data[6];

    i2c_master_write_read_device(
        I2C_MASTER_NUM,
        MPU6050_ADDR,
        &reg,
        1,
        data,
        6,
        1000 / portTICK_PERIOD_MS);

    int16_t raw_ax = (data[0] << 8) | data[1];
    int16_t raw_ay = (data[2] << 8) | data[3];
    int16_t raw_az = (data[4] << 8) | data[5];

    *ax = raw_ax / 16384.0;
    *ay = raw_ay / 16384.0;
    *az = raw_az / 16384.0;
}