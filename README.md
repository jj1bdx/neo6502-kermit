# Neo6502-Kermit

## Alpha phase yet

### Version 0.0.5

While the protocol engine in kermit.c seems to be working OK,
I haven't made decisions on how this code should work.

## Objectives

* File transfer only
* No terminal emulation
* Based on E-Kermit v1.8

## Kermit communication channel

* Neo6502 UEXT UART
* Default speed: 9600 bps
* 8-bit, no parity
* Tested with a Mac running USB serial device

## How to configure

* Details: TBD
* see `ckermit-config.txt` for C-Kermit configuration example
* Note well: do not set file type other than binary (no conversion)

## Requirement

* [LLVM-MOS SDK](https://github.com/llvm-mos/llvm-mos-sdk/)

## Current status

* [x] Fix basic compilation errors
* [x] Add Neo6502 I/O functions to neoio.c
* [x] Complete invocation test on emulator
* [x] Perform actual communication testing over Neo6502 UEXT UART
* [x] Make debug log available on file (KDEBUG.LOG)
* [x] Test basic file receiving functions
* [x] Describe *non-capability* of distinguishing file types (text/binary)
* [ ] Error handling of filesystem open/read/write operations
* [ ] Fix specification of the code (User interface)
* [ ] Communication performance improvement

## License

Revised 3-Clause BSD License. See also [Revised 3-Clause BSD License for Columbia University Kermit Software](https://kermitproject.org/cu-bsd-license.html), which is consistent with the COPYING file of this repository.
## Authors

* E-Kermit: Frank da Cruz
* Neo6502 configuration and I/O functions: Kenji Rikitake
