"""Compile/link the active BMS sources in user temp, without HEX/BIN or hooks."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=['FD_Release', 'FD_Debug'], default='FD_Release')
    parser.add_argument('--uv4', default=r'C:\Keil_v5\UV4\UV4.exe')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    project = root / '103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx'
    tree = ET.parse(project)
    targets = tree.getroot().find('Targets')
    for target in list(targets):
        if target.findtext('TargetName') != args.target:
            targets.remove(target)
    target = targets.find('Target')
    temp_root = Path(os.environ['LOCALAPPDATA']) / 'CodexTemp/103-309-copy'
    temp_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='no-bin-', dir=temp_root))

    def absolute(value):
        return str((project.parent / value).resolve())

    # Only the disposable project is edited; use the worktree sources directly.
    for node in target.iter('FilePath'):
        node.text = absolute(node.text)
    for node in target.iter('IncludePath'):
        if node.text:
            node.text = ';'.join(absolute(item) for item in node.text.split(';') if item)
    common = target.find('TargetOption/TargetCommonOption')
    common.find('OutputDirectory').text = str(output / 'Objects') + '\\'
    common.find('ListingPath').text = str(output / 'Listings') + '\\'
    common.find('CreateHexFile').text = '0'
    common.find('BrowseInformation').text = '0'
    for hooks in ('BeforeCompile', 'BeforeMake', 'AfterMake'):
        for node in common.find(hooks):
            if node.tag.startswith('RunUserProg'):
                node.text = '0'
            elif node.tag.startswith('UserProg') and node.tag.endswith('Name'):
                node.text = ''
    # Honor the checked-in App layout instead of the inconsistent Device ROM size.
    scatter = project.parent / 'Objects/FD_Release.sct'
    linker = target.find('TargetOption/TargetArmAds/LDads')
    linker.find('umfTarg').text = '0'
    linker.find('ScatterFile').text = str(scatter)
    (output / 'Objects').mkdir()
    (output / 'Listings').mkdir()
    isolated = output / project.name
    tree.write(isolated, encoding='utf-8', xml_declaration=True)
    log = output / 'build.log'
    result = subprocess.run([args.uv4, '-b', str(isolated), '-t', args.target, '-j0', '-o', str(log)],
                            cwd=output, check=False)
    text = log.read_bytes().decode('utf-8', errors='replace') if log.exists() else ''
    print(text)
    print(f'Validation files: {output}')
    images = list(output.rglob('*.bin')) + list(output.rglob('*.hex'))
    if images:
        raise SystemExit('Unexpected image output: ' + str(images))
    if result.returncode != 0 or not list(output.rglob('*.axf')):
        raise SystemExit(result.returncode or 1)


if __name__ == '__main__':
    main()
