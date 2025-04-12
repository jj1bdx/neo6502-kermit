# How to use Neo6502-Kermit

## How to start

`load "kermit.neo"` runs the code.

## Commands

Neo6502-Kermit has only four commands. Hitting one of the following letters invokes the command: D for directory listing, R for receiving files, S for sending files, and Q for quitting.

### Quitting

Quitting the program puts you back to NeoBasic.

### Directory listing

The directory listing command simply invokes API File I/O function at Group 3, Function 1: *List Directory*, equivalent to `*. + Return` command on the NeoBasic prompt. This command shows all the files under the current directory.

### Receiving files

The program shows the prompt `Waiting to receive files...` and waits for the Kermit client program on the other end of the UART connection. Neo6502-Kermit receives all files sent from the Kermit client in the same session (also known as the transmission group). Canceling the single file or the entire transmission group terminates the session and puts Neo6502-Kermit into the command prompt.

Currently, there is no way to cancel receiving files in Neo6502-Kermit once the command is started. You can cancel the execution by sending three consecutive Control-Cs from the Kermit program on the other end; this will cause Neo6502-Kermit to quit and fall back into NeoBasic.

The following limitations apply:

* *Neo6502-Kermit deletes existing files with the same name in the current directory immediately after the receiving begins*.
* *All incompletely received files are deleted*.
* No Kermit Text protocol conversion is applied; all files are received in binary mode.

### Sending files

After entering the sending command, Neo6502-Kermit prompts for the filenames to send. You can choose one of the four possible choices:

* Entering the full filename and pressing Return to add a file to the sending list
* `> + Return` to finish entering the filenames and to prepare for sending files
* `. + Return` to show the directory listing (as in the Directory Listing command)
* Control-C (*without the Return key*) to cancel sending files and go back to the command prompt

The following limitations apply:

* The filenames are case-insensitive (due to Neo6502 FATfs capability), but are sent to the Kermit client on the other end as they are (without case conversion).
* No wildcards are supported, sorry.
* The maximum number of files for a sending session is 16.
* A duplication check is *not performed* in the sending file list.
* No Kermit Text protocol conversion is applied; all files are sent in binary mode.
* Only files from the current directory can be accessed.

[End of document]