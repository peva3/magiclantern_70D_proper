#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Set Canon boot flags (EOS_DEVELOP, BOOTDISK, SCRIPT) on SD card — Windows version.

Works on Windows via `\\.\D:` physical volume access.
Also works on Linux/macOS via `/dev/sdX1` block devices.

Usage:
    python edit_card_flags_win.py D: --script --bootdisk --eos-develop
    python edit_card_flags_win.py D: --script    (SCRIPT flag only)

The SCRIPT flag enables Canon Basic scripting on DIGIC 8+ cameras,
needed for the 90D to execute extend.m on boot.

References:
    http://chdk.setepontos.com/index.php/topic,4214.0.html
    tools/card-flags/edit_card_flags.py (@kitor, 2025)
"""

import sys
import os
import struct
import argparse

# FAT16 boot sector offsets
FAT16_EOS_DEVELOP = 43
FAT16_BOOTDISK = 64
FAT16_SCRIPT = 496

# FAT32 boot sector offsets
FAT32_EOS_DEVELOP = 71
FAT32_BOOTDISK = 92
FAT32_SCRIPT = 496

def detect_fs(data):
    """Detect FAT type from boot sector bytes."""
    if data[54:62] == b'FAT16   ':
        return 'FAT16'
    if data[82:90] == b'FAT32   ':
        return 'FAT32'
    if data[3:11] == b'EXFAT   ':
        return 'EXFAT'
    return None

def open_volume(path):
    """Open a volume for raw read/write on Windows or Linux."""
    if sys.platform == 'win32':
        # Windows: \\.\D: for volume, \\.\PhysicalDriveN for disk
        if ':' in path and not path.startswith('\\\\.\\'):
            path = f'\\\\.\\{path}'
        return open(path, 'r+b')
    else:
        return open(path, 'r+b')

def set_flags(fh, offsets, args):
    """Write boot flags at the specified offsets."""
    fh.seek(0)
    data = bytearray(fh.read(512))

    if args.eos_develop:
        off = offsets['eos_develop']
        data[off:off+11] = b'EOS_DEVELOP'
        print(f'  Wrote EOS_DEVELOP at offset {off}')

    if args.bootdisk:
        off = offsets['bootdisk']
        data[off:off+8] = b'BOOTDISK'
        print(f'  Wrote BOOTDISK at offset {off}')

    if args.script and 'script' in offsets and offsets['script']:
        off = offsets['script']
        data[off:off+6] = b'SCRIPT'
        print(f'  Wrote SCRIPT at offset {off}')

    # Remove flags if --remove-unused-flags is set
    if args.remove_unused_flags:
        if offsets.get('eos_develop') and not args.eos_develop:
            data[offsets['eos_develop']:offsets['eos_develop']+11] = b'\0' * 11
        if offsets.get('bootdisk') and not args.bootdisk:
            data[offsets['bootdisk']:offsets['bootdisk']+8] = b'\0' * 8
        if offsets.get('script') and not args.script:
            data[offsets['script']:offsets['script']+6] = b'\0' * 6

    fh.seek(0)
    fh.write(data)
    fh.flush()

def main():
    parser = argparse.ArgumentParser(
        description='Set Canon boot flags on SD card — Windows/Linux compatible'
    )
    parser.add_argument('volume',
        help='Volume path (e.g. D: on Windows, /dev/sdX1 on Linux)')
    parser.add_argument('--eos-develop', action='store_true',
        help='Set EOS_DEVELOP flag')
    parser.add_argument('--bootdisk', action='store_true',
        help='Set BOOTDISK flag')
    parser.add_argument('--script', action='store_true',
        help='Set SCRIPT flag (required for CBasic on DIGIC 8+)')
    parser.add_argument('--remove-unused-flags', action='store_true',
        help='Clear flags not selected in this run')

    args = parser.parse_args()

    # Default: set all three flags
    if not any([args.eos_develop, args.bootdisk, args.script,
                args.remove_unused_flags]):
        print('No flags selected. Defaulting to: EOS_DEVELOP + BOOTDISK + SCRIPT')
        args.eos_develop = True
        args.bootdisk = True
        args.script = True

    try:
        fh = open_volume(args.volume)
    except PermissionError:
        print(f'ERROR: Cannot open {args.volume}. Run as Administrator.', file=sys.stderr)
        print('Right-click Command Prompt/PowerShell → Run as Administrator', file=sys.stderr)
        sys.exit(1)
    except FileNotFoundError:
        print(f'ERROR: Volume {args.volume} not found.', file=sys.stderr)
        print(f'On Windows, use drive letter (e.g. D: or E:).', file=sys.stderr)
        sys.exit(1)

    fh.seek(0)
    boot = fh.read(512)
    fstype = detect_fs(boot)

    if fstype == 'FAT16':
        print(f'Detected FAT16 on {args.volume}')
        offsets = {'eos_develop': FAT16_EOS_DEVELOP, 'bootdisk': FAT16_BOOTDISK, 'script': FAT16_SCRIPT}
    elif fstype == 'FAT32':
        print(f'Detected FAT32 on {args.volume}')
        offsets = {'eos_develop': FAT32_EOS_DEVELOP, 'bootdisk': FAT32_BOOTDISK, 'script': FAT32_SCRIPT}
    elif fstype == 'EXFAT':
        print(f'Detected EXFAT on {args.volume}')
        print('WARNING: SCRIPT flag not supported on EXFAT — use FAT32!', file=sys.stderr)
        if args.script:
            print('Skipping SCRIPT flag (EXFAT limitation)', file=sys.stderr)
            args.script = False
        offsets = {'eos_develop': 130, 'bootdisk': 122, 'script': None}
    else:
        print(f'ERROR: {args.volume} is not FAT16, FAT32, or EXFAT.', file=sys.stderr)
        print('Format your SD card in-camera first, then try again.', file=sys.stderr)
        fh.close()
        sys.exit(1)

    set_flags(fh, offsets, args)
    fh.close()

    print(f'\nFlags set successfully on {args.volume}!')
    print('Safe to eject the SD card.')

if __name__ == '__main__':
    main()
