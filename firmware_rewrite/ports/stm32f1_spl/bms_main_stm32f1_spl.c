#include "bms_firmware.h"
#include "bms_port_stm32f1_spl.h"

#define BMS_STM32F1_MAIN_TICK_MS 100u

static bms_storage_t s_storage;
static bms_app_t s_app;
static bms_platform_ops_t s_ops;

int main(void)
{
    bms_config_t config;

    bms_stm32f1_platform_init();
    s_ops = bms_stm32f1_platform_ops();
    config = bms_config_default();

    bms_storage_init(&s_storage);
    bms_app_init(&s_app, &config, &s_storage);

    for (;;) {
        if (bms_stm32f1_wait_tick(BMS_STM32F1_MAIN_TICK_MS)) {
            (void)bms_firmware_run_once(&s_app, &s_ops, BMS_STM32F1_MAIN_TICK_MS);
            bms_stm32f1_poll(&s_app);
        }
    }
}
