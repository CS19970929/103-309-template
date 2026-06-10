#include "main.h"
#include "PowerUi.h"
#include "LedSnapshot.h"

static volatile UINT8 s_power_on_confirmed = 0;
static volatile UINT8 s_request_power_on = 0;
static volatile UINT8 s_request_shutdown = 0;

void PowerUi_ConfirmPowerOn(void)
{
    s_power_on_confirmed = 1;
    s_request_power_on = 1;
    sys_time.power_on = true;
}

void PowerUi_RequestShutdown(void)
{
    s_power_on_confirmed = 0;
    s_request_shutdown = 1;
    sys_time.power_on = false;
}

UINT8 PowerUi_IsPowerOnConfirmed(void)
{
    return s_power_on_confirmed ? 1 : 0;
}

void PowerUi_ApplyInitialMosForce(void)
{
    if (s_power_on_confirmed) {
        Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG = FORCE_KEEP_MODE;
        Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
    } else {
        Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG = FORCE_CLOSE_MODE;
        Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_CLOSE_MODE;
    }
}

void PowerUi_ProcessRequests(void)
{
    if (s_request_shutdown) {
        s_request_shutdown = 0;
        s_request_power_on = 0;
        s_power_on_confirmed = 0;
        sys_time.power_on = false;

        LedSnapshot_SaveRuntime();
        SH367309_DriverMos_Ctrl(GPIO_CHG, 0);
        SH367309_DriverMos_Ctrl(GPIO_DSG, 0);
        LedBar_OutputOff();
        entersleep(DEEP_MODE);
        return;
    }

    if (s_request_power_on) {
        s_request_power_on = 0;
        s_power_on_confirmed = 1;
        sys_time.power_on = true;
        Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG = FORCE_KEEP_MODE;
        Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_KEEP_MODE;
    }
}
