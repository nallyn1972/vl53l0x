#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "vl53l0x_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int i2c_port;
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
    uint32_t scl_speed_hz;
    uint8_t i2c_address_7bit;
    uint8_t enable_internal_pullup;
    gpio_num_t xshut_gpio_num;
    gpio_num_t gpio1_gpio_num;
    i2c_master_bus_handle_t existing_bus;
} vl53l0x_idf_config_t;

esp_err_t vl53l0x_idf_init(VL53L0X_Dev_t *dev, const vl53l0x_idf_config_t *cfg);
esp_err_t vl53l0x_idf_deinit(VL53L0X_Dev_t *dev);
esp_err_t vl53l0x_idf_set_xshut(VL53L0X_Dev_t *dev, bool enabled);
esp_err_t vl53l0x_idf_hard_reset(VL53L0X_Dev_t *dev, uint32_t low_ms, uint32_t boot_ms);

#ifdef __cplusplus
}
#endif
