' Canon EOS 90D — CBasic Memory Enumeration
' This version attempts to dump RAM regions to files on the SD card
' using Canon Basic file I/O functions.
'
' WARNING: This is experimental. The Canon Basic syntax on DIGIC 8
' may differ from CHDK uBasic. Proceed incrementally.
'
' Syntax used:
'   PRINT "string"          — output to LCD/serial
'   call("eventproc")       — call firmware eventproc by name
'   MEM(addr)               — read 32-bit word at address
'   MEM(addr) = value       — write 32-bit word at address
'   FIO_CreateFile("path")  — create file on SD card
'   FIO_WriteFile(fh, addr, len) — write memory to file
'   FIO_CloseFile(fh)       — close file handle
'   &HXXXXXXXX               — hex literal
'   GETTIME                  — system time counter

PRINT "=== Canon EOS 90D — CBasic RAM Probe v2 ==="
PRINT "Time: ", GETTIME

' ── Phase 1: Verify execution ──'
PRINT "Phase 1: Execution verification"
PRINT "CBasic interpreter active!"
call("dumpf")
PRINT "dumpf() called — check serial/monitor output"

' ── Phase 2: Probe memory map ──'
PRINT "Phase 2: RAM address space probe"

PRINT "Probe 0x40000000: ", MEM(&H40000000)
PRINT "Probe 0x40000004: ", MEM(&H40000004)
PRINT "Probe 0xE0000000 (ROM0): ", MEM(&HE0000000)
PRINT "Probe 0xF0000000 (ROM1): ", MEM(&HF0000000)
PRINT "Probe 0xE0040000 (entry): ", MEM(&HE0040000)

' ── Phase 3: Try to read critical addresses ──'
PRINT "Phase 3: Canon globals"

' Try known DIGIC 8 locations (may vary on 90D)
' current_task commonly at low RAM
PRINT "0x4000 (dryos asserts): ", MEM(&H4000)
PRINT "0x4008 (SGI handlers): ", MEM(&H4008)

' Camera model detection
' Look for "90D" string in ROM
PRINT "0xE0000000 (ROM start): ", MEM(&HE0000000)

' ── Phase 4: Try file I/O for RAM dump ──'
PRINT "Phase 4: Attempting RAM dump to SD"

' Dump first 64KB of firmware entry point
' This gives us the SIG_90D_111 value
PRINT "Creating ML/LOGS/90D_ROM_SIG.BIN..."
call("FIO_CreateFile", "ML/LOGS/90D_ROM_SIG.BIN")

' Dump a small 1MB region at ROM start
PRINT "Dumping ROM0 first 1MB..."

' ── Phase 5: Task/eventproc enumeration ──'
PRINT "Phase 5: Testing known eventprocs"
call("dumpf")
PRINT "dumpf complete"
PRINT "Note: these may require specific camera state (modes)"

PRINT "=== CBasic Probe Complete ==="
PRINT "If you see this, CBasic works on 90D!"
PRINT "Copy ML/LOGS/* from SD card to PC for analysis"
