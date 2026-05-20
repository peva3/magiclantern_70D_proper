/** \file
 * Entry points into the firmware image for Canon EOS 90D 1.1.1.
 *
 * WARNING: All addresses below are PLACEHOLDERS requiring firmware RE.
 * These values are copied from 850D.100 and WILL be wrong — they must be
 * updated once ROM dumps are analyzed in Ghidra.
 *
 * Finding these addresses is Phase 1 of the port:
 * 1. Dump ROM0 (0xE0000000, 16MB code) and ROM1 (0xF0000000, 16MB assets)
 * 2. Load into Ghidra (ARM:LE:32:v7, Thumb mode)
 * 3. Search for known patterns from 850D cross-reference
 * 4. Update each NSTUB with the correct address
 */

#include <stub.h>

/** Startup **/
NSTUB(0xE0040000, firmware_entry)
//NSTUB(0xE0041BA8, cstart)          // TBD
//NSTUB(0xE00454CC, init_task)        // TBD

/** Task functions **/
//NSTUB(0x????, CreateTask)           // TBD
//NSTUB(0x????, msleep)              // TBD
//NSTUB(0x????, task_create)         // TBD

/** Memory **/
//NSTUB(0x????, malloc)              // TBD
//NSTUB(0x????, free)               // TBD
//NSTUB(0x????, fio_malloc)         // TBD
//NSTUB(0x????, fio_free)           // TBD

/** File I/O **/
//NSTUB(0x????, FIO_OpenFile)       // TBD
//NSTUB(0x????, FIO_CloseFile)      // TBD
//NSTUB(0x????, FIO_ReadFile)       // TBD
//NSTUB(0x????, FIO_WriteFile)      // TBD

/** Properties **/
//NSTUB(0x????, prop_request_change) // TBD
//NSTUB(0x????, prop_register_slave) // TBD

/** GUI **/
//NSTUB(0x????, gui_main_task)      // TBD
//NSTUB(0x????, bmp_vram)           // TBD
//NSTUB(0x????, bmp_printf)         // TBD

/** EDMAC **/
//NSTUB(0x????, ConnectReadEDMAC)   // TBD
//NSTUB(0x????, ConnectWriteEDMAC)  // TBD
//NSTUB(0x????, StartEDMAC)         // TBD
//NSTUB(0x????, StopEDMAC)          // TBD

/** Lens **/
//NSTUB(0x????, SetLensFocalLength) // TBD
//NSTUB(0x????, LvLensDrive)       // TBD

/** Timers **/
//NSTUB(0x????, SetHPTimerAfterNow) // TBD
//NSTUB(0x????, SetTimerAfter)      // TBD

/** Semaphores **/
//NSTUB(0x????, create_named_semaphore) // TBD
//NSTUB(0x????, take_semaphore)    // TBD
//NSTUB(0x????, give_semaphore)    // TBD

/** Display **/
//NSTUB(0x????, disp_check)         // TBD
//NSTUB(0x????, LCD_Palette)       // TBD (D8 may not have this)

/** Audio **/
//NSTUB(0x????, SoundDevActiveIn)  // TBD
//NSTUB(0x????, SetSamplingRate)   // TBD
