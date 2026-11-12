"""Pure coulomb-counting reference for the simulation UI.

Firmware acceptance runs the actual SOC.c through run_soc_host_c_test.py.
The voltage curve below generates stimulus only; it never corrects model SOC.
"""
from dataclasses import dataclass
from pathlib import Path
import re
import sys
sys.dont_write_bytecode = True

PERIOD_MS = 200
TICKS_PER_SECOND = 5
FULL_SECONDS = 0  # Compatibility duration for the legacy scenario name.
DEFAULT_SOC = 60
try:
    text = (Path(__file__).resolve().parents[1] / '103 + 309/Project/Source/DataDeal.h').read_text(encoding='utf-8')
    CAP_A10 = int(re.search(r'#define\s+BMS_CAPCITY\s+(\d+)', text).group(1))
except (OSError, AttributeError):
    CAP_A10 = 245
CAP_FACTORY_AS10 = CAP_A10 * 3600

OCV_TABLE = [
    (4160, 100), (4100, 95), (4050, 90), (3995, 85), (3935, 80),
    (3880, 75), (3835, 70), (3795, 65), (3760, 60), (3725, 55),
    (3695, 50), (3670, 45), (3645, 40), (3615, 35), (3585, 30),
    (3555, 25), (3525, 20), (3480, 15), (3400, 10), (3250, 5),
    (3000, 0),
]

@dataclass
class Snapshot:
    valid: bool = True
    soc: int = DEFAULT_SOC
    cap_now: int = 0
    cap_full: int = 0
    cycle_x100: int = 300
    cycle_acc_as10: int = 0

@dataclass
class SocModel:
    soc: int = DEFAULT_SOC
    cycle_x100: int = 300
    cycle_acc_as10: int = 0
    cap_factory: int = CAP_FACTORY_AS10
    cap_full: int = CAP_FACTORY_AS10
    cap_now: int = 0
    remainder_mams: int = 0

    @property
    def soh(self):
        return max(80, 100 - self.cycle_x100 // 10000)

    @classmethod
    def from_snapshot(cls, snapshot):
        model = cls()
        if snapshot.valid:
            model.cycle_x100 = snapshot.cycle_x100
            model.cycle_acc_as10 = snapshot.cycle_acc_as10 % (model.cap_full // 100)
            if snapshot.cap_full and 0 <= snapshot.cap_now <= snapshot.cap_full:
                model.cap_now = snapshot.cap_now * model.cap_full // snapshot.cap_full
                model.soc = model.soc_from_cap()
            else:
                model.set_soc(snapshot.soc)
        else:
            model.set_soc(DEFAULT_SOC)
        return model

    def soc_from_cap(self):
        return (self.cap_now * 100 + self.cap_full // 2) // self.cap_full

    def set_soc(self, soc):
        self.soc = max(0, min(100, soc))
        self.cap_now = self.soc * self.cap_full // 100
        self.remainder_mams = 0

    def tick(self, vmax=3600, vmin=3600, ichg=0, idsg=0):
        total = (ichg - idsg) * 100 * PERIOD_MS + self.remainder_mams
        delta = (abs(total) // 100000) * (-1 if total < 0 else 1)
        self.remainder_mams = total - delta * 100000
        if delta < 0:
            self.cycle_acc_as10 += -delta
            steps, self.cycle_acc_as10 = divmod(self.cycle_acc_as10, self.cap_full // 100)
            self.cycle_x100 = min(0xFFFFFFFF, self.cycle_x100 + steps)
        self.cap_now = max(0, min(self.cap_full, self.cap_now + delta))
        if delta and self.cap_now in (0, self.cap_full):
            self.remainder_mams = 0
        self.soc = self.soc_from_cap()

def voltage_from_soc(soc):
    soc = max(0, min(100, soc))
    for (v1, s1), (v2, s2) in zip(OCV_TABLE, OCV_TABLE[1:]):
        if s1 >= soc >= s2:
            if s1 == s2:
                return v1
            return int((v1 * (soc - s2) + v2 * (s1 - soc)) / (s1 - s2))
    return OCV_TABLE[0][0] if soc >= OCV_TABLE[0][1] else OCV_TABLE[-1][0]


def run_seconds(model, seconds, **kwargs):
    for _ in range(seconds * TICKS_PER_SECOND):
        model.tick(**kwargs)

def main():
    from run_soc_host_c_test import run
    run()

if __name__ == '__main__':
    main()
