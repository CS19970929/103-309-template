"""Build an isolated engineering firmware; never flashes hardware."""
from pathlib import Path
import os, shutil, subprocess, tempfile, sys, xml.etree.ElementTree as ET
sys.stdout.reconfigure(errors="replace")
root=Path(__file__).resolve().parents[1]
project=root/'103 + 309/Project/Users/BMS_SH3673520.uvprojx'
temp_root=Path(os.environ['LOCALAPPDATA'])/'CodexTemp'
temp_root.mkdir(parents=True,exist_ok=True)
work=Path(tempfile.mkdtemp(prefix='flash-endurance-',dir=temp_root))
tree=ET.parse(project)
scatter=project.parent/'Objects/FD_Release.sct'
if 'LR_IROM1 0x08004800 0x00009800' not in scatter.read_text():
    raise SystemExit('Unexpected App scatter boundary; refuse engineering build')
for node in tree.findall('.//FilePath'):
    node.text=str((project.parent/node.text).resolve())
for node in tree.findall('.//IncludePath'):
    if node.text: node.text=';'.join(str((project.parent/x).resolve()) for x in node.text.split(';') if x)
for node in tree.findall('.//Cads/VariousControls/Define'): node.text=(node.text or '')+',FLASH_ENDURANCE_TEST_ENABLE=1'
for node in tree.findall('.//Cads/Optim'): node.text='4' # ARMCC O3, isolated from user target options.
for node in tree.findall('.//ScatterFile'):
    if node.text: node.text=str((project.parent/node.text).resolve())
for node in tree.findall('.//OutputDirectory'): node.text=str(work/'Objects')+os.sep
for node in tree.findall('.//ListingPath'): node.text=str(work/'Listings')+os.sep
(work/'Objects').mkdir(); (work/'Listings').mkdir()
target=work/'Endurance.uvprojx'; tree.write(target,encoding='utf-8',xml_declaration=True)
log=work/'build.log'
proc=subprocess.run([r'C:\Keil_v5\UV4\UV4.exe','-r',str(target),'-t','FD_Release','-o',str(log)],creationflags=subprocess.CREATE_NO_WINDOW)
print(log.read_text(encoding='gbk',errors='replace') if log.exists() else 'No build log')
if proc.returncode not in (0,1): raise SystemExit('Keil build failed: '+str(log))
source=work/'Objects/FD_Release.bin'
if not source.exists() or not 0 < source.stat().st_size <= 38912: raise SystemExit('Missing or oversized App; no artifacts published: '+str(work))
dest=project.parent/'Objects_Endurance'; dest.mkdir(exist_ok=True)
for suffix in ['bin','hex','axf']:
    item=work/('Objects/FD_Release.'+suffix)
    if item.exists(): shutil.copy2(item,dest/('FD_Endurance.'+suffix))
shutil.copy2(log,dest/'build.log')
print('App=0x08004800; bytes='+str(source.stat().st_size)+'; remaining='+str(38912-source.stat().st_size))
print('Engineering firmware only: '+str(dest))
