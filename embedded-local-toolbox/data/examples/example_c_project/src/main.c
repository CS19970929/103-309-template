#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define PROJECT_CFG_BMS_ENABLE 1
#define PROJECT_CFG_SOC_ENABLE 1
#define MOS_CHG_ON()  do { g_mos_state |= 0x01; } while (0)
#define MOS_DSG_OFF() do { g_mos_state &= (uint8_t)~0x02; } while (0)

volatile uint32_t g_ms_tick;
uint8_t g_mos_state;
static uint16_t s_pack_voltage_mv;

void DelayMs(uint32_t ms);
void Flash_WriteParam(uint16_t addr, uint16_t value);

void USART1_IRQHandler(void)
{
    g_ms_tick++;
}

void CAN1_RX0_IRQHandler(void)
{
    /* TODO: decode CAN frame */
}

static void App_WaitReady(void)
{
    while ((g_ms_tick & 0x01U) == 0U) {
    }
}

static void App_SaveParam(void)
{
    Flash_WriteParam(0x2200, s_pack_voltage_mv);
}

static void App_CopyName(char *dst, const char *src)
{
    strcpy(dst, src);
    sprintf(dst, "BMS-%s", src);
}

void App_MosControl(uint8_t enable)
{
    if (enable) {
        MOS_CHG_ON();
    } else {
        MOS_DSG_OFF();
    }
}

int main(void)
{
    char name[16];
    memset(name, 0, sizeof(name));
    App_CopyName(name, "DEMO");
    App_WaitReady();
    DelayMs(10);
    App_SaveParam();
    return 0;
}
