#!/usr/bin/env python3
"""Verify Canon boot flags on an SD card.

The flags are not single bytes -- they are NUL/space-terminated ASCII STRINGS
written into the boot sector (or, on exFAT, into the checksummed VBR):

    EOS_DEVELOP  11 bytes
    BOOTDISK      8 bytes
    SCRIPT        6 bytes

Testing only the first byte gives false positives: a normally formatted FAT16
card has the volume label "NO NAME    " at offset 43, whose first byte is 'N'
(0x4E, non-zero) and would be reported as "EOS_DEVELOP SET".

Usage:
    ./verify_flags.py /dev/sdX1
    ./verify_flags.py card.img

Exit status is 1 if EOS_DEVELOP or BOOTDISK is missing.
"""
import os
import sys

FAT16 = {'EOS_DEVELOP': 43, 'BOOTDISK': 64, 'SCRIPT': 496}
FAT32 = {'EOS_DEVELOP': 71, 'BOOTDISK': 92, 'SCRIPT': 496}
EXFAT = {'EOS_DEVELOP': 130, 'BOOTDISK': 122}

# Highest offset we ever read is 496 + 6 = 502.
READ_SIZE = 4096


def detect_fs(data):
    if data[3:3 + 8] == b'EXFAT   ':
        return 'EXFAT'
    if data[0x52:0x52 + 8] == b'FAT32   ':
        return 'FAT32'
    return 'FAT16'


def read_head(path):
    with open(path, 'rb') as f:
        return f.read(READ_SIZE)


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ('-h', '--help'):
        print(__doc__)
        return 0

    path = sys.argv[1]
    try:
        data = read_head(path)
    except OSError as e:
        print(f"Error: cannot read {path}: {e}", file=sys.stderr)
        return 2

    if len(data) < 512:
        print(f"Error: {path} is too small to contain a boot sector", file=sys.stderr)
        return 2

    fs = detect_fs(data)
    offsets = {'FAT16': FAT16, 'FAT32': FAT32, 'EXFAT': EXFAT}[fs]

    print(f"Filesystem: {fs}")
    try:
        print(f"Size: {os.path.getsize(path)} bytes")
    except OSError:
        pass
    print()

    missing = []
    for name, off in offsets.items():
        want = name.encode()
        got = data[off:off + len(want)]
        ok = got == want
        shown = got.decode('ascii', errors='replace')
        print(f"  {name:12s} @ {off:3d} (0x{off:03X}): "
              f"{'SET  ' if ok else 'not set'}  found={shown!r}")
        if not ok:
            missing.append(name)

    print()
    required = [n for n in ('EOS_DEVELOP', 'BOOTDISK') if n in offsets]
    if any(n in missing for n in required):
        print(f"Flags NOT set: {', '.join(n for n in required if n in missing)}")
        print("Run: python3 edit_card_flags.py /dev/sdX --eos-develop --bootdisk")
        print("(add --script too on DIGIC 8 and later bodies)")
        return 1

    print("Required flags present.")
    print("If the camera still did not load autoexec.bin, check that:")
    print("  - autoexec.bin is in the CARD ROOT (not in ML/)")
    print("  - the card is FAT16/FAT32/exFAT as the camera expects")
    print("  - the card is not locked and the camera firmware matches the build")
    return 0


if __name__ == '__main__':
    sys.exit(main())
