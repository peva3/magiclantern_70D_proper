WiFi / socket diagnostics
=========================

Read-only WiFi and socket diagnostics for the Canon 70D. Verifies the addresses of the RAM-loaded DryOS socket library and of the ROM1 PTPIP wrappers, then optionally opens one listening socket to prove the API is reachable. Run it from Debug, then WiFi / socket diagnostics. It does not run at boot.

Checks
------

- Address verification of the seven socket functions in RAM
- Existence check of the eight ROM1 PTPIP / socket-close wrappers
- Live probe: create, bind and listen on port 5555, a second
  socket attempting connect to 127.0.0.1:5555, then close
- Lists the eleven WiFi bootstrap names that are absent from the
  70D eventproc table and so cannot be invoked with call()

Notes
-----

The module is named wifitst and not wifi_test because module names must be at most 8 characters; the card is FAT and 8.3 truncation breaks TCC symbol lookup.

socket_close_caller needs socket_convertfd, which has no 70D NSTUB, so the ROM1 helper socket_close_if_valid is used instead.

0x7CFEC is the EVF state-object pointer slot, not a LiveView flag.

:Author: peva3
:License: GPL v2
:Summary: WiFi / socket diagnostics - address verification for the 70D
:Details: Read-only checks of the RAM-loaded socket library and the ROM1 PTPIP wrappers, plus one live listening socket probe.
