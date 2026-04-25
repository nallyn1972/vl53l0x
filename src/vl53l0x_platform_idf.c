#include "vl53l0x_platform.h"

#include <string.h>

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define VL53L0X_IDF_MAX_DEVICES 8

#ifndef CONFIG_VL53L0X_I2C_TIMEOUT_MS
#define CONFIG_VL53L0X_I2C_TIMEOUT_MS 100
#endif

#ifndef CONFIG_VL53L0X_POLLING_DELAY_MS
#define CONFIG_VL53L0X_POLLING_DELAY_MS 2
#endif

typedef struct {
    uint8_t used;
    uint8_t addr_7bit;
    i2c_master_dev_handle_t handle;
} vl53l0x_idf_device_slot_t;

static vl53l0x_idf_device_slot_t s_devices[VL53L0X_IDF_MAX_DEVICES];
static SemaphoreHandle_t s_registry_lock;
static const char *TAG = "vl53l0x_idf";

static SemaphoreHandle_t registry_lock_get(void)
{
    if (s_registry_lock == NULL) {
        s_registry_lock = xSemaphoreCreateMutex();
    }
    return s_registry_lock;
}

static uint8_t normalize_addr_7bit(uint8_t addr)
{
    if (addr > 0x7F) {
        return (uint8_t)(addr >> 1);
    }
    return addr;
}

static i2c_master_dev_handle_t find_device_by_addr(uint8_t addr)
{
    const uint8_t addr7 = normalize_addr_7bit(addr);
    SemaphoreHandle_t lock = registry_lock_get();

    if (lock == NULL) {
        return NULL;
    }

    i2c_master_dev_handle_t out = NULL;
    if (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < VL53L0X_IDF_MAX_DEVICES; i++) {
            if (s_devices[i].used && s_devices[i].addr_7bit == addr7) {
                out = s_devices[i].handle;
                break;
            }
        }
        xSemaphoreGive(lock);
    }

    return out;
}

int32_t vl53l0x_idf_register_device(uint8_t address, i2c_master_dev_handle_t handle)
{
    SemaphoreHandle_t lock = registry_lock_get();
    const uint8_t addr7 = normalize_addr_7bit(address);

    if (lock == NULL || handle == NULL) {
        return 1;
    }

    if (xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
        return 1;
    }

    int32_t status = 1;

    for (int i = 0; i < VL53L0X_IDF_MAX_DEVICES; i++) {
        if (s_devices[i].used && s_devices[i].addr_7bit == addr7) {
            s_devices[i].handle = handle;
            status = 0;
            break;
        }
    }

    if (status != 0) {
        for (int i = 0; i < VL53L0X_IDF_MAX_DEVICES; i++) {
            if (!s_devices[i].used) {
                s_devices[i].used = 1;
                s_devices[i].addr_7bit = addr7;
                s_devices[i].handle = handle;
                status = 0;
                break;
            }
        }
    }

    xSemaphoreGive(lock);
    return status;
}

void vl53l0x_idf_unregister_device(uint8_t address)
{
    SemaphoreHandle_t lock = registry_lock_get();
    const uint8_t addr7 = normalize_addr_7bit(address);

    if (lock == NULL) {
        return;
    }

    if (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < VL53L0X_IDF_MAX_DEVICES; i++) {
            if (s_devices[i].used && s_devices[i].addr_7bit == addr7) {
                memset(&s_devices[i], 0, sizeof(s_devices[i]));
                break;
            }
        }
        xSemaphoreGive(lock);
    }
}

static int32_t esp_err_to_status(esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C transaction failed: %s", esp_err_to_name(err));
    }
    return (err == ESP_OK) ? 0 : 1;
}

int32_t VL53L0X_comms_initialise(uint8_t comms_type, uint16_t comms_speed_khz)
{
    (void)comms_type;
    (void)comms_speed_khz;
    return 0;
}

int32_t VL53L0X_comms_close(void)
{
    return 0;
}

int32_t VL53L0X_cycle_power(void)
{
    return 1;
}

int32_t VL53L0X_write_multi(uint8_t address, uint8_t index, uint8_t *pdata, int32_t count)
{
    if (pdata == NULL || count < 0 || count > COMMS_BUFFER_SIZE) {
        return 1;
    }

    i2c_master_dev_handle_t dev = find_device_by_addr(address);
    if (dev == NULL) {
        return 1;
    }

    uint8_t tx_buf[COMMS_BUFFER_SIZE + 1];
    tx_buf[0] = index;
    memcpy(&tx_buf[1], pdata, (size_t)count);

    return esp_err_to_status(i2c_master_transmit(dev, tx_buf, (size_t)count + 1U, CONFIG_VL53L0X_I2C_TIMEOUT_MS));
}

int32_t VL53L0X_read_multi(uint8_t address, uint8_t index, uint8_t *pdata, int32_t count)
{
    if (pdata == NULL || count < 0 || count > COMMS_BUFFER_SIZE) {
        return 1;
    }

    i2c_master_dev_handle_t dev = find_device_by_addr(address);
    if (dev == NULL) {
        return 1;
    }

    uint8_t reg = index;
    return esp_err_to_status(i2c_master_transmit_receive(dev, &reg, 1, pdata, (size_t)count, CONFIG_VL53L0X_I2C_TIMEOUT_MS));
}

int32_t VL53L0X_write_byte(uint8_t address, uint8_t index, uint8_t data)
{
    return VL53L0X_write_multi(address, index, &data, 1);
}

int32_t VL53L0X_write_word(uint8_t address, uint8_t index, uint16_t data)
{
    uint8_t buf[2] = {(uint8_t)(data >> 8), (uint8_t)(data & 0xFF)};
    return VL53L0X_write_multi(address, index, buf, 2);
}

int32_t VL53L0X_write_dword(uint8_t address, uint8_t index, uint32_t data)
{
    uint8_t buf[4] = {
        (uint8_t)(data >> 24),
        (uint8_t)(data >> 16),
        (uint8_t)(data >> 8),
        (uint8_t)data,
    };
    return VL53L0X_write_multi(address, index, buf, 4);
}

int32_t VL53L0X_read_byte(uint8_t address, uint8_t index, uint8_t *pdata)
{
    return VL53L0X_read_multi(address, index, pdata, 1);
}

int32_t VL53L0X_read_word(uint8_t address, uint8_t index, uint16_t *pdata)
{
    uint8_t buf[2];
    int32_t status = VL53L0X_read_multi(address, index, buf, 2);
    if (status == 0 && pdata != NULL) {
        *pdata = ((uint16_t)buf[0] << 8) | buf[1];
    }
    return status;
}

int32_t VL53L0X_read_dword(uint8_t address, uint8_t index, uint32_t *pdata)
{
    uint8_t buf[4];
    int32_t status = VL53L0X_read_multi(address, index, buf, 4);
    if (status == 0 && pdata != NULL) {
        *pdata = ((uint32_t)buf[0] << 24) |
                 ((uint32_t)buf[1] << 16) |
                 ((uint32_t)buf[2] << 8) |
                 (uint32_t)buf[3];
    }
    return status;
}

int32_t VL53L0X_platform_wait_us(int32_t wait_us)
{
    if (wait_us > 0) {
        esp_rom_delay_us((uint32_t)wait_us);
    }
    return 0;
}

int32_t VL53L0X_wait_ms(int32_t wait_ms)
{
    if (wait_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
    return 0;
}

int32_t VL53L0X_set_gpio(uint8_t level)
{
    (void)level;
    return 0;
}

int32_t VL53L0X_get_gpio(uint8_t *plevel)
{
    if (plevel != NULL) {
        *plevel = 0;
    }
    return 0;
}

int32_t VL53L0X_release_gpio(void)
{
    return 0;
}

int32_t VL53L0X_get_timer_frequency(int32_t *ptimer_freq_hz)
{
    if (ptimer_freq_hz == NULL) {
        return 1;
    }

    *ptimer_freq_hz = 1000000;
    return 0;
}

int32_t VL53L0X_get_timer_value(int32_t *ptimer_count)
{
    if (ptimer_count == NULL) {
        return 1;
    }

    *ptimer_count = (int32_t)esp_timer_get_time();
    return 0;
}

VL53L0X_Error VL53L0X_LockSequenceAccess(VL53L0X_DEV Dev)
{
    if (Dev != NULL && Dev->io_lock != NULL) {
        xSemaphoreTake((SemaphoreHandle_t)Dev->io_lock, portMAX_DELAY);
    }
    return VL53L0X_ERROR_NONE;
}

VL53L0X_Error VL53L0X_UnlockSequenceAccess(VL53L0X_DEV Dev)
{
    if (Dev != NULL && Dev->io_lock != NULL) {
        xSemaphoreGive((SemaphoreHandle_t)Dev->io_lock);
    }
    return VL53L0X_ERROR_NONE;
}

VL53L0X_Error VL53L0X_WriteMulti(VL53L0X_DEV Dev, uint8_t index, uint8_t *pdata, uint32_t count)
{
    if (Dev == NULL) {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    return (VL53L0X_write_multi((uint8_t)(Dev->I2cDevAddr >> 1), index, pdata, (int32_t)count) == 0) ?
        VL53L0X_ERROR_NONE : VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_ReadMulti(VL53L0X_DEV Dev, uint8_t index, uint8_t *pdata, uint32_t count)
{
    if (Dev == NULL) {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    return (VL53L0X_read_multi((uint8_t)(Dev->I2cDevAddr >> 1), index, pdata, (int32_t)count) == 0) ?
        VL53L0X_ERROR_NONE : VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_WrByte(VL53L0X_DEV Dev, uint8_t index, uint8_t data)
{
    if (Dev == NULL) {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    return (VL53L0X_write_byte((uint8_t)(Dev->I2cDevAddr >> 1), index, data) == 0) ?
        VL53L0X_ERROR_NONE : VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_WrWord(VL53L0X_DEV Dev, uint8_t index, uint16_t data)
{
    if (Dev == NULL) {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    return (VL53L0X_write_word((uint8_t)(Dev->I2cDevAddr >> 1), index, data) == 0) ?
        VL53L0X_ERROR_NONE : VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_WrDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t data)
{
    if (Dev == NULL) {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    return (VL53L0X_write_dword((uint8_t)(Dev->I2cDevAddr >> 1), index, data) == 0) ?
        VL53L0X_ERROR_NONE : VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_RdByte(VL53L0X_DEV Dev, uint8_t index, uint8_t *data)
{
    if (Dev == NULL) {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    return (VL53L0X_read_byte((uint8_t)(Dev->I2cDevAddr >> 1), index, data) == 0) ?
        VL53L0X_ERROR_NONE : VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_RdWord(VL53L0X_DEV Dev, uint8_t index, uint16_t *data)
{
    if (Dev == NULL) {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    return (VL53L0X_read_word((uint8_t)(Dev->I2cDevAddr >> 1), index, data) == 0) ?
        VL53L0X_ERROR_NONE : VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_RdDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t *data)
{
    if (Dev == NULL) {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    return (VL53L0X_read_dword((uint8_t)(Dev->I2cDevAddr >> 1), index, data) == 0) ?
        VL53L0X_ERROR_NONE : VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_UpdateByte(VL53L0X_DEV Dev, uint8_t index, uint8_t AndData, uint8_t OrData)
{
    uint8_t data = 0;
    VL53L0X_Error status = VL53L0X_RdByte(Dev, index, &data);
    if (status == VL53L0X_ERROR_NONE) {
        data = (uint8_t)((data & AndData) | OrData);
        status = VL53L0X_WrByte(Dev, index, data);
    }
    return status;
}

VL53L0X_Error VL53L0X_PollingDelay(VL53L0X_DEV Dev)
{
    (void)Dev;
    (void)VL53L0X_wait_ms(CONFIG_VL53L0X_POLLING_DELAY_MS);
    return VL53L0X_ERROR_NONE;
}
