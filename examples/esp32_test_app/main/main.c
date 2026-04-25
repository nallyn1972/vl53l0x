#include <inttypes.h>
#include <stdio.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "vl53l0x_api.h"
#include "vl53l0x_idf.h"

static const char *TAG = "vl53l0x_test";

static void log_pal_error(VL53L0X_Error status)
{
    char buf[VL53L0X_MAX_STRING_LENGTH] = {0};
    VL53L0X_GetPalErrorString(status, buf);
    ESP_LOGE(TAG, "PAL status=%d (%s)", status, buf);
}

static esp_err_t vl53l0x_startup(VL53L0X_Dev_t *dev)
{
    VL53L0X_Error status = VL53L0X_DataInit(dev);
    if (status != VL53L0X_ERROR_NONE) {
        log_pal_error(status);
        return ESP_FAIL;
    }

    status = VL53L0X_StaticInit(dev);
    if (status != VL53L0X_ERROR_NONE) {
        log_pal_error(status);
        return ESP_FAIL;
    }

    uint8_t vhv_settings = 0;
    uint8_t phase_cal = 0;
    status = VL53L0X_PerformRefCalibration(dev, &vhv_settings, &phase_cal);
    if (status != VL53L0X_ERROR_NONE) {
        log_pal_error(status);
        return ESP_FAIL;
    }

    uint32_t ref_spad_count = 0;
    uint8_t is_aperture_spads = 0;
    status = VL53L0X_PerformRefSpadManagement(dev, &ref_spad_count, &is_aperture_spads);
    if (status != VL53L0X_ERROR_NONE) {
        log_pal_error(status);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Ref SPAD count=%" PRIu32 ", aperture=%u", ref_spad_count, is_aperture_spads);

    status = VL53L0X_SetDeviceMode(dev, VL53L0X_DEVICEMODE_SINGLE_RANGING);
    if (status != VL53L0X_ERROR_NONE) {
        log_pal_error(status);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void app_main(void)
{
    VL53L0X_Dev_t sensor;

    vl53l0x_idf_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = (gpio_num_t)CONFIG_VL53L0X_TEST_I2C_SDA,
        .scl_io_num = (gpio_num_t)CONFIG_VL53L0X_TEST_I2C_SCL,
        .scl_speed_hz = 400000,
        .i2c_address_7bit = 0x29,
        .enable_internal_pullup = 1,
        .xshut_gpio_num = (gpio_num_t)CONFIG_VL53L0X_TEST_XSHUT_GPIO,
        .gpio1_gpio_num = (gpio_num_t)CONFIG_VL53L0X_TEST_GPIO1_GPIO,
        .existing_bus = NULL,
    };

    ESP_ERROR_CHECK(vl53l0x_idf_init(&sensor, &cfg));
    ESP_ERROR_CHECK(vl53l0x_startup(&sensor));

    while (1) {
        VL53L0X_RangingMeasurementData_t measurement;
        VL53L0X_Error status = VL53L0X_PerformSingleRangingMeasurement(&sensor, &measurement);

        if (status == VL53L0X_ERROR_NONE) {
            ESP_LOGI(TAG,
                     "range_mm=%u signal=%u ambient=%u status=%u",
                     measurement.RangeMilliMeter,
                     measurement.SignalRateRtnMegaCps,
                     measurement.AmbientRateRtnMegaCps,
                     measurement.RangeStatus);
        } else {
            log_pal_error(status);
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_VL53L0X_TEST_PERIOD_MS));
    }
}
