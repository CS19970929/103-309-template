"""Exercise production RTC IO setup twice with an armed key EXTI model.

Software regression only: GPIO analog mode disconnects the digital input and
can latch a falling key edge. Physical wake validation is separate.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from test_current_calibration import function_source, msvc_environment
from test_sleep import clean, read

STUBS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define GPIOA 0
#define GPIOB 1
#define GPIOC 2
#define GPIOD 3
#define GPIOE 4
#define GPIO_Pin_All 0xffffu
#define GPIO_Pin_0 (1u << 0)
#define GPIO_Pin_2 (1u << 2)
#define GPIO_Pin_4 (1u << 4)
#define GPIO_Pin_9 (1u << 9)
#define GPIO_Pin_12 (1u << 12)
#define GPIO_Pin_13 (1u << 13)
#define GPIO_Pin_14 (1u << 14)
#define PIN_INT_WK_MCU GPIO_Pin_0
#define PIN_KEY1 GPIO_Pin_9
#define PIN_INT_WK_CMNT GPIO_Pin_12
#define PIN_AFE1_CTL GPIO_Pin_14
#define GPIO_Mode_AIN 0
#define GPIO_Mode_Out_PP 1
#define GPIO_Mode_IN_FLOATING 2
#define GPIO_Speed_2MHz 2
#define RCC_APB2Periph_GPIOA 1
#define RCC_APB2Periph_GPIOB 2
#define RCC_APB2Periph_GPIOC 4
#define RCC_APB2Periph_GPIOD 8
#define RCC_APB2Periph_GPIOE 16
#define ENABLE 1
#define ADC1 1
typedef struct { unsigned GPIO_Pin, GPIO_Mode, GPIO_Speed; } GPIO_InitTypeDef;
static unsigned modes[5][16], key_level=1, key_pending, external_pending;
static unsigned driver, power_ctr, power_stb;
#define MCUO_DRV_CMNT driver
#define MCUO_PWSV_CTR power_ctr
#define MCUO_PWSV_STB power_stb
static void RCC_APB2PeriphClockCmd(int p,int e) { (void)p; (void)e; }
static void ADC_DeInit(int a) { (void)a; }
static void __delay_ms(int ms) { (void)ms; }
static void GPIO_Init(unsigned port, GPIO_InitTypeDef *init) {
    unsigned pin;
    for (pin=0; pin<16; ++pin) {
        if (!(init->GPIO_Pin & (1u << pin))) continue;
        modes[port][pin]=init->GPIO_Mode;
        if (port==GPIOA && pin==9 && init->GPIO_Mode==GPIO_Mode_AIN && key_level) {
            key_level=0; key_pending=1;
        }
    }
}
'''

TESTS = r'''
int main(void) {
    unsigned n;
    modes[GPIOA][0]=modes[GPIOA][9]=modes[GPIOB][12]=GPIO_Mode_IN_FLOATING;
    modes[GPIOB][14]=GPIO_Mode_Out_PP;
    external_pending=1; /* an already latched real event must survive IO setup */
    for (n=0; n<2; ++n) {
        IOstatus_RTCMode();
        assert(!key_pending && key_level);
        assert(modes[GPIOA][0]==GPIO_Mode_IN_FLOATING);
        assert(modes[GPIOA][9]==GPIO_Mode_IN_FLOATING);
        assert(modes[GPIOB][12]==GPIO_Mode_IN_FLOATING);
        assert(modes[GPIOB][14]==GPIO_Mode_Out_PP);
        assert(external_pending);
        assert(modes[GPIOA][1]==GPIO_Mode_AIN);
    }
    puts("PASS: repeated RTC GPIO setup preserves wake inputs and CTLC without a false key edge");
    return 0;
}
'''


def main():
    base = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/103-309-template/rtc-gpio-tests"
    base.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="run-", dir=base))
    source = folder / "rtc_gpio.c"
    source.write_text(clean(STUBS + function_source(read("conf/conf.c"), "IOstatus_RTCMode") + TESTS),
                      encoding="ascii")
    env = msvc_environment()
    result = subprocess.run([shutil.which("cl.exe", path=env["PATH"]), "/nologo", "/TC", "/W3", "/O2",
                             str(source), f"/Fe{folder / 'rtc_gpio.exe'}", f"/Fo{folder / 'rtc_gpio.obj'}"],
                            env=env, cwd=folder, capture_output=True)
    log = (result.stdout + result.stderr).decode("gbk", errors="replace")
    (folder / "build.log").write_text(log, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(log)
    subprocess.run([folder / "rtc_gpio.exe"], check=True, timeout=10)
    print(f"Temporary outputs: {folder}")


if __name__ == "__main__":
    main()
