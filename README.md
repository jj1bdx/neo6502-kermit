# Neo6502-Kermit

* Kermit protocol program for OLIMEX Neo6502 board
* Based on E-Kermit v1.8

## Alpha phase yet

### Version 0.0.5

While the protocol engine in kermit.c seems to be working OK,
I haven't made decisions on how this code should work.

## Objectives

* File transfer only
* No terminal emulation

## Kermit communication channel

* Neo6502 UEXT UART
* Default speed: 9600 bps
* 8-bit, no parity
* Tested with a Mac running USB serial device

### Communication performance

Using 8 simulated sliding windows with 256-byte packets, minimal prefixing:

* With `-DDEBUG`: ~300 bytes/sec (including the console and KDEBUG.LOG file outputs)
* With `-NODEBUG` (no debug code): ~910 bytes/sec (almost full speed)

## How to configure

* Details: TBD
* see `ckermit-config.txt` for C-Kermit configuration example

## Note well: do not set file type other than binary (no conversion)

In Kermit Protocol, there is no practical way to disable or prevent text file CR/LF conversion from the recipient side, if the sender side wants to do so.

For C-Kermit, you need to run all of the following commands to avoid doing text file conversion:

```text
# This is required to avoid file type conversion
set file type binary
set file scan off
set file patterns off
set file text-patterns
```

Also, the format of "Text Type File" on Neo6502 is not clearly defined, although in the console code it seems to be CR-terminated, as shown in LLVM-MOS SDK.

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
* [ ] Implement file *sending* functions
* [ ] Communication performance improvement

## License

Revised 3-Clause BSD License. See LICENSE.

See also [Revised 3-Clause BSD License for Columbia University Kermit Software](https://kermitproject.org/cu-bsd-license.html).

## Authors

* E-Kermit: Frank da Cruz
* Neo6502 configuration and I/O functions: Kenji Rikitake
