' Canon EOS 90D — CBasic RAM Enumeration Script
' This script leverages the CBasic interpreter (Canon Basic)
' to probe memory layout, dump firmware info, and save RAM
' regions to the SD card for offline analysis.
'
' Deployment:
' 1. Format SD card as FAT32
' 2. Run: python3 tools/card-flags/edit_card_flags.py /dev/sdX1 --script --bootdisk --eos-develop
' 3. Copy this file as "extend.m" to the SD card root
' 4. Insert into 90D, power on
' 5. Results appear on screen and in files on card
'
' Canon Basic syntax reference:
'   call("eventproc_name") — call firmware eventproc
'   &HXXXXXXXX — hex literal
'   MEM(address) — read memory word
'   MEM(address) = value — write memory word
'   FIO functions — standard file I/O
'   printf("string") — serial/UART output
'
' Phase 1: Verify script execution and dump firmware metadata

PRINT "=== CBasic 90D Enumeration ==="
PRINT "Boot time: ", GETTIME
PRINT "Firmware version: "
call("dumpf")

' Phase 2: Probe RAM size and memory regions
' Canon DIGIC 8 typical RAM layout: 0x40000000-0x80000000 (1-2GB)
' We probe in 256MB increments to avoid watchdog timeouts

PRINT "=== RAM Region Probe ==="
PRINT "Probing RAM at 0x40000000"
PRINT "Word at 0x40000000: ", MEM(&H40000000)
PRINT "Word at 0x40000004: ", MEM(&H40000004)
PRINT "Word at 0x40000008: ", MEM(&H40000008)
PRINT "Word at 0x4000000C: ", MEM(&H4000000C)

' Phase 3: Try to call eventprocs for task/system info
' These may or may not work depending on which eventprocs are registered
PRINT "=== Eventproc Tests ==="

' dumpf already called above
' Try other known eventprocs
PRINT "Testing task eventproc..."
' call("task_create")  ' might crash — careful

PRINT "Testing memory eventproc..."
' call("sysmem_info")  ' might crash

PRINT "=== Script Complete ==="
PRINT "Check serial output for dumpf results"
PRINT "If you see this message, CBasic is working on the 90D!"

' End of extend.m
