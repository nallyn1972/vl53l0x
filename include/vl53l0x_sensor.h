#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "vl53l0x_idf.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t vl53l0x_sensor_init(const vl53l0x_idf_config_t *cfg);
esp_err_t vl53l0x_sensor_deinit(void);
esp_err_t vl53l0x_sensor_read_mm(uint16_t *height_mm);
bool vl53l0x_sensor_is_initialized(void);

#ifdef __cplusplus
}
#endif
