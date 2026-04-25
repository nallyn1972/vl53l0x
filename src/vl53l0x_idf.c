#include "vl53l0x_idf.h"

#include <stdbool.h>
#include <string.h>

#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define VL53L0X_COMMS_TYPE_I2C 1

#define VL53L0X_IDF_OK 0
#define VL53L0X_IDF_FAIL 1

#if !defined(ESP_IDF_VERSION) || \
    ((ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)) || \
     (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(7, 0, 0)))
#error "vl53l0x_idf_component supports ESP-IDF >=5.5.0 and <7.0.0"
#endif

int32_t vl53l0x_idf_register_device(uint8_t address, i2c_master_dev_handle_t handle);
void vl53l0x_idf_unregister_device(uint8_t address);

static bool is_valid_gpio(gpio_num_t gpio)
{
    return gpio >= 0;
}

esp_err_t vl53l0x_idf_init(VL53L0X_Dev_t *dev, const vl53l0x_idf_config_t *cfg)
{
    if (dev == NULL || cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));

    i2c_master_bus_handle_t bus_handle = cfg->existing_bus;
    bool owns_bus = false;

    if (bus_handle == NULL) {
        i2c_master_bus_config_t bus_cfg = {0};
        bus_cfg.i2c_port = cfg->i2c_port;
        bus_cfg.sda_io_num = cfg->sda_io_num;
        bus_cfg.scl_io_num = cfg->scl_io_num;
        bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_cfg.glitch_ignore_cnt = 7;
        bus_cfg.flags.enable_internal_pullup = cfg->enable_internal_pullup ? 1 : 0;

        esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus_handle);
        if (err != ESP_OK) {
            return err;
        }
        owns_bus = true;
    }

    i2c_device_config_t dev_cfg = {0};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = cfg->i2c_address_7bit;
    dev_cfg.scl_speed_hz = cfg->scl_speed_hz;

    i2c_master_dev_handle_t dev_handle = NULL;
    esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        if (owns_bus) {
            i2c_del_master_bus(bus_handle);
        }
        return err;
    }

    SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    if (lock == NULL) {
        i2c_master_bus_rm_device(dev_handle);
        if (owns_bus) {
            i2c_del_master_bus(bus_handle);
        }
        return ESP_ERR_NO_MEM;
    }

    if (is_valid_gpio(cfg->xshut_gpio_num)) {
        gpio_config_t out_cfg = {
            .pin_bit_mask = (1ULL << cfg->xshut_gpio_num),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        err = gpio_config(&out_cfg);
        if (err != ESP_OK) {
            vSemaphoreDelete(lock);
            i2c_master_bus_rm_device(dev_handle);
            if (owns_bus) {
                i2c_del_master_bus(bus_handle);
            }
            return err;
        }
        err = gpio_set_level(cfg->xshut_gpio_num, 1);
        if (err != ESP_OK) {
            vSemaphoreDelete(lock);
            i2c_master_bus_rm_device(dev_handle);
            if (owns_bus) {
                i2c_del_master_bus(bus_handle);
            }
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (is_valid_gpio(cfg->gpio1_gpio_num)) {
        gpio_config_t in_cfg = {
            .pin_bit_mask = (1ULL << cfg->gpio1_gpio_num),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        err = gpio_config(&in_cfg);
        if (err != ESP_OK) {
            vSemaphoreDelete(lock);
            i2c_master_bus_rm_device(dev_handle);
            if (owns_bus) {
                i2c_del_master_bus(bus_handle);
            }
            return err;
        }
    }

    dev->I2cDevAddr = (uint8_t)(cfg->i2c_address_7bit << 1);
    dev->comms_type = VL53L0X_COMMS_TYPE_I2C;
    dev->comms_speed_khz = (uint16_t)(cfg->scl_speed_hz / 1000U);
    dev->i2c_bus_handle = (void *)bus_handle;
    dev->i2c_dev_handle = (void *)dev_handle;
    dev->io_lock = (void *)lock;
    dev->xshut_gpio_num = (int)cfg->xshut_gpio_num;
    dev->gpio1_gpio_num = (int)cfg->gpio1_gpio_num;
    dev->owns_i2c_bus = owns_bus ? 1 : 0;

    if (vl53l0x_idf_register_device(cfg->i2c_address_7bit, dev_handle) != VL53L0X_IDF_OK) {
        vSemaphoreDelete(lock);
        i2c_master_bus_rm_device(dev_handle);
        if (owns_bus) {
            i2c_del_master_bus(bus_handle);
        }
        memset(dev, 0, sizeof(*dev));
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t vl53l0x_idf_deinit(VL53L0X_Dev_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t addr7 = (uint8_t)(dev->I2cDevAddr >> 1);
    vl53l0x_idf_unregister_device(addr7);

    if (dev->io_lock != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)dev->io_lock);
    }

    if (dev->i2c_dev_handle != NULL) {
        i2c_master_bus_rm_device((i2c_master_dev_handle_t)dev->i2c_dev_handle);
    }

    if (dev->owns_i2c_bus && dev->i2c_bus_handle != NULL) {
        i2c_del_master_bus((i2c_master_bus_handle_t)dev->i2c_bus_handle);
    }

    memset(dev, 0, sizeof(*dev));
    return ESP_OK;
}

esp_err_t vl53l0x_idf_set_xshut(VL53L0X_Dev_t *dev, bool enabled)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (dev->xshut_gpio_num < 0) {
        return ESP_ERR_INVALID_STATE;
    }

    return gpio_set_level((gpio_num_t)dev->xshut_gpio_num, enabled ? 1 : 0);
}

esp_err_t vl53l0x_idf_hard_reset(VL53L0X_Dev_t *dev, uint32_t low_ms, uint32_t boot_ms)
{
    esp_err_t err = vl53l0x_idf_set_xshut(dev, false);
    if (err != ESP_OK) {
        return err;
    }

    if (low_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(low_ms));
    }

    err = vl53l0x_idf_set_xshut(dev, true);
    if (err != ESP_OK) {
        return err;
    }

    if (boot_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(boot_ms));
    }

    return ESP_OK;
}
