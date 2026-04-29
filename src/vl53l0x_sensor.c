#include "vl53l0x_sensor.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define VL53L0X_INIT_RETRIES 3
#define VL53L0X_READ_RETRIES 2
#define VL53L0X_RETRY_DELAY_MS 200

static VL53L0X_Dev_t s_dev = {0};
static bool s_initialized = false;

static esp_err_t vl53l0x_startup(VL53L0X_Dev_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (dev->xshut_gpio_num >= 0) {
        esp_err_t hard_reset_ret = vl53l0x_idf_hard_reset(dev, 2, 5);
        if (hard_reset_ret != ESP_OK) {
            return hard_reset_ret;
        }
    }

    VL53L0X_Error status = VL53L0X_DataInit(dev);
    if (status != VL53L0X_ERROR_NONE) {
        return ESP_FAIL;
    }

    status = VL53L0X_StaticInit(dev);
    if (status != VL53L0X_ERROR_NONE) {
        return ESP_FAIL;
    }

    uint8_t vhv_settings = 0;
    uint8_t phase_cal = 0;
    status = VL53L0X_PerformRefCalibration(dev, &vhv_settings, &phase_cal);
    if (status != VL53L0X_ERROR_NONE) {
        return ESP_FAIL;
    }

    uint32_t ref_spad_count = 0;
    uint8_t is_aperture_spads = 0;
    status = VL53L0X_PerformRefSpadManagement(dev, &ref_spad_count, &is_aperture_spads);
    if (status != VL53L0X_ERROR_NONE) {
        return ESP_FAIL;
    }

    status = VL53L0X_SetDeviceMode(dev, VL53L0X_DEVICEMODE_SINGLE_RANGING);
    if (status != VL53L0X_ERROR_NONE) {
        return ESP_FAIL;
    }

    if (dev->gpio1_gpio_num >= 0) {
        status = VL53L0X_SetGpioConfig(
            dev,
            0,
            VL53L0X_DEVICEMODE_SINGLE_RANGING,
            VL53L0X_GPIOFUNCTIONALITY_NEW_MEASURE_READY,
            VL53L0X_INTERRUPTPOLARITY_LOW);
        if (status != VL53L0X_ERROR_NONE) {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t vl53l0x_sensor_init(const vl53l0x_idf_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_FAIL;
    for (int attempt = 1; attempt <= VL53L0X_INIT_RETRIES; attempt++) {
        vl53l0x_sensor_deinit();

        ret = vl53l0x_idf_init(&s_dev, cfg);
        if (ret != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(VL53L0X_RETRY_DELAY_MS));
            continue;
        }

        ret = vl53l0x_startup(&s_dev);
        if (ret != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(VL53L0X_RETRY_DELAY_MS));
            continue;
        }

        s_initialized = true;
        return ESP_OK;
    }

    vl53l0x_sensor_deinit();
    return ret;
}

esp_err_t vl53l0x_sensor_deinit(void)
{
    if (s_dev.i2c_dev_handle) {
        vl53l0x_idf_deinit(&s_dev);
        memset(&s_dev, 0, sizeof(s_dev));
    }

    s_initialized = false;
    return ESP_OK;
}

esp_err_t vl53l0x_sensor_read_mm(uint16_t *height_mm)
{
    if (height_mm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    for (int attempt = 0; attempt < VL53L0X_READ_RETRIES; attempt++) {
        VL53L0X_RangingMeasurementData_t measurement = {0};
        VL53L0X_Error status = VL53L0X_PerformSingleRangingMeasurement(&s_dev, &measurement);
        if (status == VL53L0X_ERROR_NONE) {
            *height_mm = measurement.RangeMilliMeter;
            return ESP_OK;
        }
    }

    return ESP_FAIL;
}

bool vl53l0x_sensor_is_initialized(void)
{
    return s_initialized;
}
