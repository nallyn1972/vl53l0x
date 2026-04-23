# VL53L0X ESP-IDF Component

Reusable ESP-IDF component for ST's VL53L0X API using the native `i2c_master` driver (`driver/i2c_master.h`).

## Highlights

- ESP-IDF native I2C master integration (`driver/i2c_master.h`)
- ST VL53L0X API core preserved under `st_api/`
- Optional hardware control pins:
  - `xshut_gpio_num` for hardware reset/shutdown
  - `gpio1_gpio_num` for measurement-ready interrupt wiring
- Helper APIs:
  - `vl53l0x_idf_init()` / `vl53l0x_idf_deinit()`
  - `vl53l0x_idf_set_xshut()`
  - `vl53l0x_idf_hard_reset()`

## ESP-IDF Support

- Supported: 5.5.4 and 6.0.0
- Intended compatibility range in code: >=5.5.0 and <7.0.0

If a future ESP-IDF major release changes I2C APIs, this component fails fast at compile time with a clear version error.

## CI Compatibility Matrix

GitHub Actions workflow: [.github/workflows/idf-matrix-ci.yml](.github/workflows/idf-matrix-ci.yml)

- Builds with ESP-IDF `v5.5.4`
- Builds with ESP-IDF `v6.0.0`
- Compiles the ESP32 test application against this component

## What is included

- Unmodified ST API core sources under `st_api/core/`
- ESP-IDF platform layer in `src/vl53l0x_platform_idf.c`
- Helper init/deinit wrapper in `src/vl53l0x_idf.c`
- Public helper header in `include/vl53l0x_idf.h`
- ESP32 example app under `examples/esp32_test_app`

## Quick use in your app

```c
#include "vl53l0x_api.h"
#include "vl53l0x_idf.h"

VL53L0X_Dev_t sensor;
vl53l0x_idf_config_t cfg = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .scl_speed_hz = 400000,
    .i2c_address_7bit = 0x29,
    .enable_internal_pullup = 1,
    .xshut_gpio_num = -1,
    .gpio1_gpio_num = -1,
    .existing_bus = NULL,
};

ESP_ERROR_CHECK(vl53l0x_idf_init(&sensor, &cfg));
ESP_ERROR_CHECK(VL53L0X_DataInit(&sensor) == VL53L0X_ERROR_NONE ? ESP_OK : ESP_FAIL);
```

To use hardware reset via XSHUT:

```c
ESP_ERROR_CHECK(vl53l0x_idf_hard_reset(&sensor, 2, 5));
```

To use GPIO1 interrupt-driven ranging, configure the sensor GPIO function as
`VL53L0X_GPIOFUNCTIONALITY_NEW_MEASURE_READY`, attach an ISR to
`cfg.gpio1_gpio_num`, then read data after interrupt and clear interrupt mask.

See the test app implementation for a full flow:
- `vl53l0x_idf_test_app/main/main.c` in this workspace

## Config

Component menuconfig entries:

- `VL53L0X_POLLING_DELAY_MS`
- `VL53L0X_I2C_TIMEOUT_MS`

## Publishing as a dedicated GitHub repo

1. Create a new repo, for example `vl53l0x-idf`.
2. Copy this folder's contents into that repo root.
3. Update `idf_component.yml` with your final repo URL and version tags.
4. Push `main` and tag releases (example `v0.1.0`) so ESP Component Manager can fetch pinned versions.

Example git bootstrap:

```bash
git init
git add .
git commit -m "Initial VL53L0X ESP-IDF component"
git branch -M main
git remote add origin https://github.com/<your-org>/vl53l0x-idf.git
git push -u origin main
git tag v0.1.0
git push origin v0.1.0
```

## Consume from another project via Component Manager

In your project's `main/idf_component.yml`:

```yaml
dependencies:
  your-org/vl53l0x-idf:
    version: "~0.1.0"
```

If you want to consume directly from Git while iterating:

```yaml
dependencies:
  vl53l0x-idf:
    git: "https://github.com/your-org/vl53l0x-idf.git"
    path: "."
```
