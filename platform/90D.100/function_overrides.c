/*
 * 90D function overrides for EDMAC/ENGIO when stubs are not found.
 * Most of these are copied from 850D and will need 90D-specific values.
 */
#include <dryos.h>
#include <dryos_rpc.h>
#include <consts.h>
#include <patch.h>

/* EDMAC — TBD */
int ConnectReadEDMAC(uint32_t channel, uint32_t connection, uint32_t flag) { return 0; }
int ConnectWriteEDMAC(uint32_t channel, uint32_t connection, uint32_t flag) { return 0; }
int StartEDMAC(uint32_t channel, uint32_t flag) { return 0; }
void StopEDMAC(uint32_t channel) {}
void SetEDMAC(uint32_t channel, uint32_t flag) {}
void AbortEDMAC(uint32_t channel) {}

/* ENGIO — TBD */
uint32_t shamem_read(uint32_t addr) { return 0; }
void shamem_write(uint32_t addr, uint32_t val) {}
void EngDrvOut(uint32_t reg, uint32_t value) {}
