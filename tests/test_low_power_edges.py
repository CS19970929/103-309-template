"""Execute production quiet/partial-frame/dirty-bit helpers with fault injection."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from test_current_calibration import function_source, msvc_environment
from test_sleep import clean, read

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"

COMMON = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "low_power.h"
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
volatile LowPowerDiag g_low_power_diag;
static uint32_t now_ms, rtc_seconds, irq_mask;
static uint32_t __get_PRIMASK(void) { return irq_mask; }
static void __disable_irq(void) { irq_mask=1U; }
static void __set_PRIMASK(uint32_t value) { irq_mask=value; }
static uint32_t BoardLowPower_Lock(void) { uint32_t value=irq_mask; irq_mask=1U; return value; }
static void BoardLowPower_Unlock(uint32_t value) { irq_mask=value; }
static uint32_t System_GetUptimeMs(void) { return now_ms; }
static uint32_t RTC_ReadCounterCoherent(void) { return rtc_seconds; }
static void LowPower_RecordFault(uint32_t faults) { g_low_power_diag.fault_mask|=faults; }
'''

QUIET_STUBS = r'''
#define BOARD_COMM_QUIET_MS 5U
#define BOARD_PREPARE_TIMEOUT_MS 100U
#define GPIO_INT_WK_CMNT 0U
#define PIN_INT_WK_CMNT 1U
#define EXTI_Line12 (1UL << 12)
static struct { uint32_t PR; } exti;
#define EXTI (&exti)
static uint32_t s_prepare_started_ms, s_quiet_started_ms, s_prepare_started_rtc;
static uint8_t s_comm_was_active, active;
static uint8_t GPIO_ReadInputDataBit(uint32_t gpio,uint32_t pin) { (void)gpio; (void)pin; return active; }
static void EXTI_ClearITPendingBit(uint32_t bits) { exti.PR&=~bits; }
'''

QUIET_TESTS = r'''
int main(void) {
    assert(BoardLowPower_WaitQuiet()==0U); now_ms=5U;
    assert(BoardLowPower_WaitQuiet()==1U && !irq_mask);
    now_ms=6U; exti.PR=EXTI_Line12;
    assert(BoardLowPower_WaitQuiet()==0U && g_low_power_diag.quarantine_edges==1U);
    now_ms=10U; assert(BoardLowPower_WaitQuiet()==0U);
    now_ms=11U; assert(BoardLowPower_WaitQuiet()==1U);
    active=1U; now_ms=12U; assert(BoardLowPower_WaitQuiet()==0U);
    active=0U; now_ms=13U; assert(BoardLowPower_WaitQuiet()==0U);
    now_ms=17U; assert(BoardLowPower_WaitQuiet()==0U);
    now_ms=18U; assert(BoardLowPower_WaitQuiet()==1U);
    now_ms=100U; assert(BoardLowPower_WaitQuiet()==2U);
    now_ms=0U; rtc_seconds=2U; assert(BoardLowPower_WaitQuiet()==2U);
    assert(g_low_power_diag.fault_mask&LP_FAULT_NO_TIMEBASE);
    rtc_seconds=0U; s_prepare_started_ms=s_quiet_started_ms=UINT32_MAX-2U;
    now_ms=2U; assert(BoardLowPower_WaitQuiet()==1U && !irq_mask);
    puts("PASS: PB12 edges/high level restart quiet window; timeout, missing tick and tick wrap");
    return 0;
}
'''

RX_STUBS = r'''
#define SCI_RX_PARTIAL_TIMEOUT_MS 100U
#define RS485_STA_IDLE 0U
static volatile uint32_t g_sci_partial_timeouts;
struct RS485MSG { volatile UINT8 ptr_no, csr; UINT8 u16Buffer[256]; };
'''

RX_TESTS = r'''
int main(void) {
    struct RS485MSG message={0}; volatile uint32_t last=0U;
    message.ptr_no=1U; now_ms=99U; Sci_ExpirePartialRx(&message,&last);
    assert(message.ptr_no==1U); now_ms=100U; Sci_ExpirePartialRx(&message,&last);
    assert(!message.ptr_no && g_sci_partial_timeouts==1U && !irq_mask);
    message.ptr_no=8U; message.csr=1U; Sci_ExpirePartialRx(&message,&last);
    assert(message.ptr_no==8U); /* complete frame must not be discarded */
    message.csr=0U; last=UINT32_MAX-49U; now_ms=49U;
    Sci_ExpirePartialRx(&message,&last); assert(message.ptr_no==8U);
    now_ms=50U; irq_mask=1U; Sci_ExpirePartialRx(&message,&last);
    assert(!message.ptr_no && irq_mask==1U);
    puts("PASS: partial RX expiry, complete-frame preservation, wrap and prior IRQ mask");
    return 0;
}
'''

EEPROM_STUBS = r'''
static UINT8 s_eeprom_write_failed, fail_write;
static UINT32 s_eeprom_failure_ms;
static UINT16 last_address,last_value;
static UINT8 WriteEEPROM_Word_NoZone(UINT16 address,UINT16 value) {
    last_address=address; last_value=value; return fail_write;
}
static UINT16 ReadObjectWord(const void *object,UINT16 offset) {
    UINT16 value; memcpy(&value,(const uint8_t*)object+offset,sizeof(value)); return value;
}
'''

EEPROM_TESTS = r'''
int main(void) {
    uint16_t addresses[32], values[32]; uint32_t flags=UINT32_MAX;
    for(uint16_t i=0;i<32U;i++) { addresses[i]=130U+2U*i; values[i]=1000U+i; }
    fail_write=1U; EEPROM_WriteFlaggedField(&flags,32U,addresses,values,0U);
    assert(flags==UINT32_MAX && s_eeprom_write_failed && g_low_power_diag.storage_write_failures==1U);
    assert(g_low_power_diag.storage_last_address==130U && g_low_power_diag.storage_last_value==1000U);
    fail_write=0U;
    for(uint16_t i=0;i<32U;i++) {
        EEPROM_WriteFlaggedField(&flags,32U,addresses,values,0U);
        assert(last_address==130U+2U*i && last_value==1000U+i);
    }
    assert(flags==0U && !s_eeprom_write_failed);
    flags=1UL<<12; EEPROM_WriteFlaggedField(&flags,12U,addresses,values,0U);
    assert(flags==(1UL<<12) && (g_low_power_diag.fault_mask&LP_FAULT_STORAGE_RANGE));
    puts("PASS: failed writes retain dirty bits; all 32 indices and invalid-mask guard");
    return 0;
}
'''


def main():
    base = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/103-309-template/edge-tests"
    base.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="run-", dir=base))
    programs = {
        "quiet": COMMON + QUIET_STUBS + function_source(read("board_low_power.c"), "BoardLowPower_WaitQuiet") + QUIET_TESTS,
        "rx": COMMON + RX_STUBS + function_source(read("Sci_Upper.c"), "Sci_ExpirePartialRx") + RX_TESTS,
        "eeprom": COMMON + EEPROM_STUBS + "\n".join(function_source(read("EEPROM.c"), name) for name in
                   ("EEPROM_TryPendingWord", "EEPROM_PendingRangeFault", "EEPROM_WriteFlaggedField")) + EEPROM_TESTS,
    }
    env = msvc_environment()
    for name, program in programs.items():
        source = folder / f"{name}.c"
        source.write_text(clean(program), encoding="ascii")
        result = subprocess.run([shutil.which("cl.exe", path=env["PATH"]), "/nologo", "/TC", "/W3", "/O2",
                                 f"/I{SOURCE}", str(source), f"/Fe{folder / (name + '.exe')}"],
                                env=env, cwd=folder, capture_output=True)
        if result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode("gbk", errors="replace"))
        subprocess.run([folder / f"{name}.exe"], check=True, timeout=10)
    print(f"Temporary outputs: {folder}")


if __name__ == "__main__":
    main()
