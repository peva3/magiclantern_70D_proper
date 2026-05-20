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
PRINT "RAM probe at 0x40000000:"
v0 = MEM(&H40000000)
PRINT "MEM(0x40000000) = ", v0
v4 = MEM(&H40000004)
PRINT "MEM(0x40000004) = ", v4

' --- Test 3: attempt file creation via FIO eventprocs ---
' These may fail if eventprocs require args that CBasic can't pass
PRINT "Attempting file creation..."
call("FIO_CreateFile")
call("FIO_RemoveFile")

' --- Test 4: identify camera ---
' Try known eventproc names from firmware
PRINT "Camera identification..."
call("GetCameraModel")
call("GetFirmwareVersion")

' --- Script complete ---
PRINT "=== EXTEND.M DONE ==="
PRINT "If nothing appeared on screen, that's NORMAL."
PRINT "Check SD card for new files or connect serial cable."
PRINT "Script executed at boot time."
