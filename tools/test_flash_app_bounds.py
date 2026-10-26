"""Test app flashing bounds using synthetic .dat inputs and dry-run only."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parent = Path(os.environ['LOCALAPPDATA']) / 'CodexTemp/103-309-copy'
parent.mkdir(parents=True, exist_ok=True)
output = Path(tempfile.mkdtemp(prefix='flash-bounds-', dir=parent))
fixture = output / 'synthetic.dat'
for length, address, accepted in ((0,'0x08004800',False), (1,'0x08004800',True),
                                  (0x17800,'0x08004800',True), (0x17801,'0x08004800',False),
                                  (1,'0x08000000',False)):
    fixture.write_bytes(b'\xff' * length)
    result = subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',
        str(ROOT / 'tools/soc_flash_app_safe.ps1'), '-Bin', str(fixture), '-Address', address,
        '-Programmer', str(Path(os.environ['SystemRoot']) / 'System32/cmd.exe')],
        capture_output=True, text=True, errors='replace')
    assert (result.returncode==0)==accepted, result.stdout+result.stderr
    if accepted:
        assert 'Dry run only' in result.stdout
print('PASS app flashing: empty/oversized/wrong-address inputs rejected; exact App limit accepted; dry-run only, no firmware images or hardware writes')
print(f'Host evidence only; files: {output}')
