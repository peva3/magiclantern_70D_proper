' Canon EOS 90D - CBasic memory probe
'
' WHAT THIS DOES
'   Reads a handful of 32-bit words and prints them. Nothing is written to the
'   SD card: Canon Basic on DIGIC 8 gives us MEM() reads and call() by eventproc
'   name, but there is no CBasic statement that writes a memory range to a file.
'   (An earlier revision of this script announced an SD dump in its comments and
'   PRINT lines; it never performed one, because there was no code to do it.)
'
' WHERE THE OUTPUT GOES
'   CBasic PRINT writes to the serial/UART console, NOT the camera LCD. Expect
'   nothing on screen. Options:
'     1. Serial cable on the UART pins, 115200 baud
'     2. Look for a boot delay / LED blink as evidence the script ran
'     3. Use call("dumpf") and then read the DryOS log from the card
'
' DEPLOYMENT
'   1. Card must be FAT32 - exFAT does not support the SCRIPT flag
'   2. python3 tools/card-flags/edit_card_flags.py /dev/sdX1 --script --bootdisk --eos-develop
'   3. Copy as "extend.m" to the CARD ROOT
'   4. Insert, power on, wait ~10 s, power off, read the log from the card
'
' PATHS
'   Any file path passed to an eventproc needs an explicit drive prefix
'   ("A:/..."), otherwise it is resolved against whatever the current directory
'   happens to be at boot.

PRINT "=== Canon EOS 90D - CBasic probe ==="
PRINT "Output is on serial, not on the LCD."

' --- Phase 1: prove the interpreter is running ---
PRINT "Phase 1: execution check"
call("dumpf")
PRINT "dumpf returned - DryOS log should now hold log0000.log"

' --- Phase 2: RAM ---
PRINT "Phase 2: RAM words"
PRINT MEM(&H40000000)
PRINT MEM(&H40000004)
PRINT MEM(&H40000008)

' --- Phase 3: ROM ---
PRINT "Phase 3: ROM words"
PRINT MEM(&HE0000000)
PRINT MEM(&HF0000000)
PRINT MEM(&HE0040000)

' --- Phase 4: low memory / dryos ---
PRINT "Phase 4: DryOS low words"
PRINT MEM(&H4000)
PRINT MEM(&H4008)

' --- Phase 5: probe a few eventproc names ---
' call() returns a value if the name resolves; the return is what we are
' actually interested in, not the side effect. These three are documented as
' present on DIGIC 8 bodies. Add more names here as they are discovered - but
' do not guess, because a missing name is silently ignored.
PRINT "Phase 5: eventproc resolution"
call("dumpf")
call("GetBatteryLevel")

PRINT "=== Probe complete ==="
PRINT "Power off, then read log0000.log from the card root."
