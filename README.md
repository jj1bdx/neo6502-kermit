# Neo6502-Kermit

## Objectives

* File transfer only
* No terminal emulation
* Based on E-Kermit v1.8

## Kermit communication channel

* Neo6502 UEXT UART
* Default speed: 230400bps
* 8-bit, no parity

## How to configure

TBD

## Requirement

* [LLVM-MOS SDK](https://github.com/llvm-mos/llvm-mos-sdk/)

## Current status

* [x] Fix basic compilation errors
* [x] Add Neo6502 I/O functions to neoio.c
* [x] Basic invocation test on emulator
* [] Actual testing over baremetal UART
* [] Program enhancement on file transfer
* [] Debug logging on file

## License

Revised 3-Clause BSD License. See also [Revised 3-Clause BSD License for Columbia University Kermit Software](https://kermitproject.org/cu-bsd-license.html), which is consistent with the COPYING file of this repository.

