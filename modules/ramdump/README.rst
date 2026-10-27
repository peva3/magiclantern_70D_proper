RAM + Serial Flash Dump
=======================

Offline reverse-engineering dump for the 70D.

Two address spaces, deliberately handled differently:

- RAM ``0x40000000``-``0x60000000`` is ordinary memory. Chunks that look
  erased are skipped, but every chunk is recorded in a ``.MAP`` sidecar so the
  file stays address-mappable.
- Serial flash is **not** memory mapped. It is read through the SPI stubs
  (``SF_CreateSerial`` / ``SF_readSerialFlash``), 16 MB.

Output files
------------

- ``ML/LOGS/RAM_DUMP.BIN`` - 512 MB of RAM, holes omitted
- ``ML/LOGS/RAM_DUMP.MAP`` - one byte per 256 KB chunk: ``1`` kept, ``0`` skipped
- ``ML/LOGS/SF_DUMP.BIN`` - 16 MB serial flash

To convert a file offset back to an address, count the preceding ``1`` bytes in
the map, multiply by 262144, and add ``0x40000000``.

Running it
----------

Menu: Debug -> Dump RAM + serial flash.

Or create ``ML/SETTINGS/RAMDUMP.RUN`` on the card; the flag is cleared as soon
as the dump starts so a failed run does not re-arm on the next boot.

Requires roughly 530 MB free on the card and at least 50% battery. The dump runs
in its own task and takes several minutes.

:Author: peva3
:License: GPL v2
:Summary: RAM and serial flash dumper for offline RE
:Details: Dumps 512MB of RAM with a chunk map plus 16MB of serial flash via the SPI stubs, for offline firmware reverse engineering.
