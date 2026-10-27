' Canon EOS 90D — CBasic RAM Enumeration Script (v2)
' 
' CRITICAL: CBasic PRINT output goes to the **serial/UART console** only,
' NOT the camera LCD screen. You will NOT see anything on the camera display.
'
' To see output you need one of:
'   1. Serial cable on the camera UART pins (115200 baud)
'   2. Check the SD card after boot for new files (EXTEND.LOG, etc.)
'   3. Look for a brief camera LED flash or boot delay as sign of execution
'
' Deployment:
'   1. SD card MUST be FAT32 (EXFAT does NOT support SCRIPT flag!)
'   2. python3 tools/card-flags/edit_card_flags.py /dev/sdX1 --script --bootdisk --eos-develop
'   3. Copy this file as "extend.m" to SD card root
'   4. Insert card, power on camera, wait 10 seconds, power off
'   5. Examine SD card on computer for any new files

PRINT "=== CBasic 90D v2 ==="

' --- Test 1: system info ---
PRINT "Testing firmware calls..."
call("dumpf")

' --- Test 2: RAM probe ---
PRINT "RAM probe:"
PRINT MEM(&H40000000)
PRINT MEM(&H40000004)

' --- Test 3: attempt file creation via FIO eventprocs ---
' These will very likely fail: CBasic can call an eventproc but cannot pass it
' arguments, so FIO_CreateFile gets a garbage path. Kept as a resolution probe
' - what matters is whether the name is found at all.
PRINT "Attempting FIO eventproc resolution..."
call("FIO_CreateFile")
call("FIO_RemoveFile")

' --- Test 4: identify camera ---
' GetCameraModel and GetFirmwareVersion are NOT eventproc names - they do not
' resolve through call() on this family of bodies, so they were removed rather
' than left in as guaranteed no-ops. Camera identity comes from the ROM probe
' above and from the build the script was written for.
PRINT "Camera identification skipped (no eventproc for it)"

' --- Script complete ---
PRINT "=== EXTEND.M DONE ==="
PRINT "If nothing appeared on screen, that's NORMAL."
PRINT "Check SD card for new files or connect serial cable."
PRINT "Script executed at boot time."
