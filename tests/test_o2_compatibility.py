"""Source guards for the ARMCC O2 migration.

Run: python -B tests/test_o2_compatibility.py
This test reads source files only and creates no firmware artifact.
"""

from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"
PROJECT = ROOT / "103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx"


def read_source(name):
    data = (SOURCE / name).read_bytes()
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return data.decode("gbk")


def require(text, fragment, label):
    if fragment not in text:
        raise AssertionError(f"missing {label}: {fragment}")


def reject(text, fragment, label):
    if fragment in text:
        raise AssertionError(f"obsolete {label} remains: {fragment}")


def main():
    cads = ET.parse(PROJECT).find("./Targets/Target/TargetOption/TargetArmAds/Cads")
    assert cads.findtext("Optim") == "3", "Keil Level 2 must be explicit"
    assert cads.findtext("oTime") == "0", "migration keeps Ospace selected"

    pub = read_source("PubFunc.c")
    afe_bus = read_source("I2C_AFE1.c")
    system = read_source("System_Init.c")
    system_h = read_source("System_Init.h")
    soc = read_source("SOC.c")
    sleep = read_source("rtc_sleep.c")
    sci_h = read_source("Sci_Upper.h")
    sci = read_source("Sci_Upper.c")
    can = read_source("Can_HDX.c")
    data = read_source("DataDeal.c")
    afe_data = read_source("SH367309_DataDeal.c")

    require(pub, "__delay_ms(delaycnt);", "SysTick millisecond delay")
    require(pub, "__delay_us(10U);", "SysTick 10-us delay")
    require(afe_bus, "__delay_us(4U);", "SysTick AFE bus delay")
    reject(pub, "for (j = 0; j < 560; j++)", "empty millisecond delay loop")
    reject(afe_bus, "for (i = 0; i < 13; i++)", "empty AFE delay loop")

    require(system, "volatile UINT8 g_u810msClockCnt", "ISR 10-ms counter visibility")
    require(system, "volatile UINT8 gu8_200msAccClock_Flag", "SOC event visibility")
    require(system_h, "UINT8 TakePendingFlag(volatile UINT8 *flag);", "atomic event helper")
    require(soc, "TakePendingFlag(&gu8_200msAccClock_Flag)", "atomic SOC event consumption")
    require(sleep, "TakePendingFlag(&gu8_1000msAccClock_Flag)", "atomic RTC event consumption")
    require(sci_h, "volatile UINT8\tptr_no", "USART ISR parser index visibility")
    require(sci_h, "volatile UINT8\tcsr", "USART ISR state visibility")
    for port in range(1, 4):
        require(sci, f"volatile UINT16 gu16_CommuErrCnt_SCI{port}",
                f"USART{port} ISR error-counter visibility")

    require(can, "static volatile UINT32 s_received_std_id", "CAN ISR mailbox")
    reject(can, "CanRxMsg RxMessage;", "shared non-volatile CAN frame")
    require(data, "memset(&Registers_AFE1, 0, sizeof(Registers_AFE1));", "defined register reset")
    reject(afe_data, "UINT16 *P = (UINT16 *)&AFE_Parameters_RS485_Struction", "aliased AFE traversal")

    for name in ("EEPROM.c", "Sci_Upper.c", "System_Monitor.c"):
        text = read_source(name)
        reject(text, "*(&PRT_E2ROMParas.", f"cross-member protection traversal in {name}")
        reject(text, "*(&OtherElement.", f"cross-member parameter traversal in {name}")
        reject(text, "*(&Heat_Cool_Element.", f"cross-member temperature traversal in {name}")

    print("PASS: explicit O2 setting and O2-sensitive source guards")


if __name__ == "__main__":
    main()
