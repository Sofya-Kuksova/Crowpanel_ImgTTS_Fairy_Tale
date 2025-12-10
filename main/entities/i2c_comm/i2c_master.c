#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "driver/i2c.h"
#include "esp_log.h"

#include "i2c_master.h"

#define I2C_MASTER_SDA_IO  GPIO_NUM_15
#define I2C_MASTER_SCL_IO  GPIO_NUM_16
#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000
#define HIMAX_ADDON_ADDR     0x24

int i2c_master_init()
{
    i2c_port_t i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode             = I2C_MODE_MASTER;
    conf.sda_io_num       = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
    conf.scl_io_num       = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags        = 0;

    ESP_ERROR_CHECK(i2c_param_config(i2c_master_port, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0));
    return 0;
}

int i2c_master_release()
{
    ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));
    return 0;
}

int i2c_master_send(void* data, size_t len, size_t xTicksToWait)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (! cmd) {
        return -1;
    }
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (HIMAX_ADDON_ADDR << 1) | I2C_MASTER_WRITE, 1));
    ESP_ERROR_CHECK(i2c_master_write(cmd, data, len, 0));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, xTicksToWait);
    i2c_cmd_link_delete(cmd);
    return ret;
}

int i2c_master_recv(void* data, size_t len, size_t xTicksToWait)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (! cmd) {
        return -1;
    }
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (HIMAX_ADDON_ADDR << 1) | I2C_MASTER_READ, 1));
    ESP_ERROR_CHECK(i2c_master_read(cmd, data, len, 0));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, xTicksToWait);
    i2c_cmd_link_delete(cmd);
    return ret;
}

int i2c_master_scan()
{
    int i;
    esp_err_t espRc;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("00:         ");
    for (i = 3; i < 0x78; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MODE_SLAVE, 1 /* expect ack */);
        i2c_master_stop(cmd);

        espRc = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(10));
        if (i % 16 == 0) {
            printf("\n%.2x:", i);
        }
        if (espRc == 0) {
            printf(" %.2x", i);
        } else {
            printf(" --");
        }
        // ESP_LOGI(tag, "i=%d, rc=%d (0x%x)", i, espRc, espRc);
        i2c_cmd_link_delete(cmd);
    }
    printf("\n");
    return 0;
}

int i2c_master_dev_available()
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (HIMAX_ADDON_ADDR << 1) | I2C_MODE_SLAVE, 1 /* expect ack */);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK;
}

#ifdef __cplusplus
}
#endif // __cplusplus
