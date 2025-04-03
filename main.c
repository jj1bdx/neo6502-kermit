// This file is a part of Neo6502-Kermit.
// See LICENSE for the licensing details.

// Neo6502-Kermit main program
// By Kenji Rikitake
// Based on E-Kermit 1.8 main.c
// Author: Frank da Cruz, the Kermit Project, Columbia University, New York.

// Note well: the stdio is assumed to be Neo6502 console device

// Macro definitions

#define NEO6502_KERMIT_VERSION "v0.1.1"
#define MAXSENDFILENUM (16)

#include "cdefs.h"  // Data types for all modules
#include "debug.h"  // Debugging
#include "kermit.h" // Kermit symbols and data structures

// Neo6502 I/O
#include <kernel.h>
#include <neo/api.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Prototypes of functions in neoio.c

void devinit(void);
int readpkt(struct k_data *, UCHAR *, int);
int tx_data(struct k_data *, UCHAR *, int);
int inchk(struct k_data *);

int openfile(struct k_data *, UCHAR *, int);
int writefile(struct k_data *, UCHAR *, int);
int readfile(struct k_data *);
int closefile(struct k_data *, UCHAR, int);
ULONG fileinfo(struct k_data *, UCHAR *, UCHAR *, int, short *, short);

// External data in neoio.c

extern UCHAR o_buf[];
extern UCHAR i_buf[];
extern int errno;

// Data global to this module

struct k_data k;     /* Kermit data structure */
struct k_response r; /* Kermit response structure */

// Sending filenames
UCHAR sendfilelist[MAXSENDFILENUM + 1][FN_MAX];

// Simple line input
// This only accept CTRL/H and CTRL/C
// for the control characters

#define LINELEN (50)
char linebuf[LINELEN];

int lineinput(void) {
  int c;
  int i;

  i = 0;
  linebuf[0] = '\0';
  do {
    c = getchar();
    if (isprint(c)) {
      putchar(c);
      linebuf[i] = (char)c;
      i++;
    } else if (c == 0x08) {
      // backspace
      putchar(c);
      if (i > 0) {
        linebuf[i] = '\0';
        i--;
      }
    } else if (c == '\n') {
      // line terminator, exit
      putchar(c);
      linebuf[i] = '\0';
      i++;
    } else if (c == 0x03) {
      // CTRL/C
      // Remove all input
      linebuf[0] = 0x03;
      linebuf[1] = 0;
      i = 1;
      // exit immediately
      c = '\n';
    }
    // If exceeding line width
    if (i == LINELEN) {
      // Rub out one letter
      putchar(0x08);
      linebuf[i] = '\0';
      i--;
    }
  } while (c != '\n');
  // Return strlen(linebuf)
  return i - 1;
}

// Load BASIC and restart
// TODO: should be declared with noreturn flag, but how?

__attribute__((leaf)) void load_basic_and_restart(void) {
  KSendMessageSync(API_GROUP_SYSTEM, API_FN_BASIC);
  __attribute__((leaf)) asm volatile("jmp ($0000)" : : : "p");
}

// Exit function for the program

void doexit(int status) {
#ifdef DEBUG
  // Close debug log
  debug(DB_CLS, "", 0, 0);
#endif // DEBUG
  // Close all files
  neo_file_close((uint8_t)0xff);
  printf("doexit status=%d\n", status);
  puts("Restart into NeoBasic");
  // Force system reset
  load_basic_and_restart();
}

// Output the banner for startup

void start_banner(void) {
  neo_console_clear_screen();
  printf("This is Neo6502-Kermit %s\n", NEO6502_KERMIT_VERSION);
}

// Main program

int main(int argc, char **argv) {
  int rx_len, i, x;
  char c;
  UCHAR *inbuf;
  short r_slot;
  // State of running main loop
  bool running = true;
  // Set this to desired parity
  int parity = P_PARITY;
  // Initial Kermit status
  int status = X_OK;
  int action = A_NONE;
  // Block check
#ifdef F_CRC
  int check = 3;
#else
  int check = 1;
#endif // F_CRC
  int sfnum = 0;

  // Code starts here

#ifdef DEBUG
  debug(DB_OPN, "DEBUG enabled", 0, 0);
#endif // DEBUG

  start_banner();
  devinit();

  // Parameters for this run

  // Manual mode transfer only
  k.xfermode = 0;
  // Remote mode
  k.remote = 1;
  // Binary mode transfer only
  k.binary = 1;
  // Set communications parity
  k.parity = parity;
  // Block check type
  k.bct = (check == 5) ? 3 : check;
  // Force Type 3 Block Check (16-bit CRC) on all packets, or not
  k.bctf = (check == 5) ? 1 : 0;
  // Do not keep incompletely received files
  k.ikeep = 0;
  // Not canceled yet
  k.cancel = 0;
  // List of files to send (if any)
  k.filelist = (UCHAR **)sendfilelist;

  //  Fill in the i/o pointers

  k.zinbuf = i_buf;    /* File input buffer */
  k.zinlen = IBUFLEN;  /* File input buffer length */
  k.zincnt = 0;        /* File input buffer position */
  k.obuf = o_buf;      /* File output buffer */
  k.obuflen = OBUFLEN; /* File output buffer length */
  k.obufpos = 0;       /* File output buffer position */

  // Fill in function pointers

  k.rxd = readpkt;      /* for reading packets */
  k.txd = tx_data;      /* for sending packets */
  k.ixd = inchk;        /* for checking connection */
  k.openf = openfile;   /* for opening files */
  k.finfo = fileinfo;   /* for getting file info */
  k.readf = readfile;   /* for reading files */
  k.writef = writefile; /* for writing to output file */
  k.closef = closefile; /* for closing files */
#ifdef DEBUG
  k.dbf = dodebug; /* for debugging */
#else
  k.dbf = 0;
#endif /* DEBUG */

  // Toplevel loop for send/receive multiple files
  while (running) {

    // Clear sendfile list
    for (i = 0; i <= MAXSENDFILENUM; i++) {
      sendfilelist[i][0] = '\0';
    }
    // Prompting user for actions
    int cmd;
    printf("S)end, R)eceive, show D)irectory, or Q)uit? ");
    c = getchar();
    cmd = toupper(c);
    // Echo back if alphabet
    if (isalpha(cmd)) {
      putchar(cmd);
    }
    putchar('\n');

    switch (cmd) {
    case 'S':
      puts("Sending files");
      action = A_NONE;
      bool entering = true;
      char name[FN_MAX];
      int count = 0;
      // TODO: is this channel ID OK?
      uint8_t tchannel = 1;
      uint8_t error;

      while (entering) {
        puts("Enter filename to send, '>' to finish,\n"
             "'.' to show directory, ^C to cancel");
        int len;
        len = lineinput();
        if (len > 0) {
          strncpy(name, linebuf, FN_MAX - 1);
          int cmd = name[0];
          if (cmd == '>') {
            // Finish entering file names
            // And continue to send
            entering = false;
            action = A_SEND;
          } else if (cmd == 0x03) {
            puts("Cancel sending files");
            // Cancel sending
            entering = false;
            action = A_NONE;
          } else if (cmd == '.') {
            // Show directory
            neo_file_list_directory();
          } else {
            // Test if file is readable
            neo_file_open(tchannel, (const char *)name, 0);
            if ((error = neo_api_error()) != API_ERROR_NONE) {
              printf("Unable to open file %s\n", name);
            } else {
              neo_file_close(tchannel);
              // File readable
              // TODO: Is strlcpy() not in LLVM-MOS library?
              strncpy((char *)sendfilelist[count], name, FN_MAX);
              count++;
              printf("Set File number %d to \"%s\"\n", count, name);
            }
          }
          // Exit if exceeding the MAXSENDFILENUM
          if (count >= MAXSENDFILENUM) {
            printf("Number of files sent reached to maximum %d",
                   MAXSENDFILENUM);
            puts("No more entering filenames");
            entering = false;
            action = A_SEND;
          }
        }
      }
      if (action != A_NONE) {
        printf("Press ^C or Q to cancel, others to continue:");
        c = getchar();
        if ((c == 0x03) || (toupper(c) == 'Q')) {
          // Do nothing
          puts("File sending canceled");
          action = A_NONE;
        } else {
          // List sending files
          printf("\nNumber of sending files: %d\n", count);
          if (count > 0) {
            int i;
            for (i = 0; i < count; i++) {
              printf("Sending file number %d: \"%s\"\n", i, sendfilelist[i]);
            }
            puts("Sending file begins, set receiving program ready");
            // Send files
            action = A_SEND;
          } else {
            puts("No file to send");
            action = A_NONE;
          }
        }
      }
      break;
    // Receive files
    case 'R':
      puts("Waiting to receive files...");
      action = A_RECV;
      break;
    // Show current directory listing
    case 'D':
      action = A_NONE;
      neo_file_list_directory();
      break;
    // Quit (Do nothing)
    // Also for CTRL/C
    case 'Q':
    case 0x03:
      action = A_NONE;
      running = false;
      break;
    default:
      puts("Command not understood");
      action = A_NONE;
      break;
    }

    // Activate Kermit only if action is not A_NONE
    if (action != A_NONE) {

      // Initialize Kermit protocol
      status = kermit(K_INIT, &k, 0, 0, "", &r);
#ifdef DEBUG
      debug(DB_LOG, "init status:", 0, status);
      debug(DB_LOG, "E-Kermit version:", k.version, 0);
#endif /* DEBUG */
      if (status == X_ERROR) {
        doexit(FAILURE);
      }

      // Sending files starts here
      if (action == A_SEND) {
        status = kermit(K_SEND, &k, 0, 0, "", &r);
      }

      // Now we read a packet ourselves and call Kermit with it.  Normally,
      // Kermit would read its own packets, but in the embedded context, the
      // device must be free to do other things while waiting for a packet to
      // arrive.  So the real control program might dispatch to other types of
      // tasks, of which Kermit is only one.  But in order to read a packet into
      // Kermit's internal buffer, we have to ask for a buffer address and slot
      // number. To interrupt a transfer in progress, set k.cancel to I_FILE to
      // interrupt only the current file, or to I_GROUP to cancel the current
      // file and all remaining files.  To cancel the whole operation in such a
      // way that the both Kermits return an error status, call Kermit with
      // K_ERROR.

      while (status != X_DONE) {

        // Here we block waiting for a packet to come in (unless readpkt times
        // out). Another possibility would be to call inchk() to see if any
        // bytes are waiting to be read, and if not, go do something else for a
        // while, then come back here and check again.

        // TODO: add CTRL/C acceptance code here

        inbuf = getrslot(&k, &r_slot);       /* Allocate a window slot */
        rx_len = k.rxd(&k, inbuf, P_PKTLEN); /* Try to read a packet */
        debug(DB_PKT, "main packet", &(k.ipktbuf[0][r_slot]), rx_len);

        // For simplicity, kermit() ACKs the packet immediately after verifying
        // it was received correctly.  If, afterwards, the control program fails
        // to handle the data correctly (e.g. can't open file, can't write data,
        // can't close file), then it tells Kermit to send an Error packet next
        // time through the loop.

        if (rx_len < 1) {        /* No data was read */
          freerslot(&k, r_slot); /* So free the window slot */
          if (rx_len < 0) {      /* If there was a fatal error */
            doexit(FAILURE);     /* give up */
          }
          // This would be another place to dispatch to another task
          // while waiting for a Kermit packet to show up.
        }

        // Handle the input

        status = kermit(K_RUN, &k, r_slot, rx_len, "", &r);
        switch (status) {
        case X_OK:
#ifdef DEBUG
          // This shows how, after each packet, you get the protocol state, file
          // name, date, size, and bytes transferred so far.  These can be used
          // in a file-transfer progress display, log, etc.
          debug(DB_LOG, "NAME",
                (UCHAR *)(r.filename != (UCHAR *)(0) ? (char *)r.filename
                                                     : "(NULL)"),
                0);
          debug(DB_LOG, "DATE",
                (UCHAR *)(r.filedate != (UCHAR *)(0) ? (char *)r.filedate
                                                     : "(NULL)"),
                0);
          debug(DB_LOG, "SIZE", 0, r.filesize);
          debug(DB_LOG, "STATE", 0, r.status);
          debug(DB_LOG, "SOFAR", 0, r.sofar);
#endif /* DEBUG */
          // Maybe do other brief tasks here...
          break; // Exit the switch statement and keep looping
        case X_DONE:
#ifdef DEBUG
          debug(DB_MSG, "Status X_DONE", 0, 0);
#endif // DEBUG
          puts("Kermit session completed");
          break; /* Finished */
        case X_ERROR:
          doexit(FAILURE); /* Failed */
          // NOTREACHABLE
        }
      }
    }
  }
  doexit(SUCCESS);
}
