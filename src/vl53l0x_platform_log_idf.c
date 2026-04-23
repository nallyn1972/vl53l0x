#include "vl53l0x_platform_log.h"

#ifdef VL53L0X_LOG_ENABLE

#include <stdarg.h>

#include "esp_log.h"

uint32_t _trace_level = TRACE_LEVEL_WARNING;
static uint32_t s_trace_modules = TRACE_MODULE_ALL;
static uint32_t s_trace_functions = TRACE_FUNCTION_ALL;

int32_t VL53L0X_trace_config(char *filename, uint32_t modules, uint32_t level, uint32_t functions)
{
    (void)filename;
    s_trace_modules = modules;
    s_trace_functions = functions;
    _trace_level = level;
    return 0;
}

void trace_print_module_function(uint32_t module, uint32_t level, uint32_t function, const char *format, ...)
{
    if (((level <= _trace_level) && ((module & s_trace_modules) > 0U)) ||
        ((function & s_trace_functions) > 0U)) {
        va_list args;
        va_start(args, format);
        esp_log_writev(ESP_LOG_INFO, "VL53L0X", format, args);
        va_end(args);
    }
}

#endif
