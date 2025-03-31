// This file is a part of Neo6502-Kermit.
// See LICENSE for the licensing details.

// Neo6502-Kermit main program
// By Kenji Rikitake
// Based on E-Kermit 1.8 main.c
// Author: Frank da Cruz, the Kermit Project, Columbia University, New York.

#define NEO6502_KERMIT_VERSION "v0.0.5"

#include "cdefs.h"  // Data types for all modules
#include "debug.h"  // Debugging
#include "kermit.h" // Kermit symbols and data structures

// Neo6502 I/O
#include <kernel.h>
#include <neo/api.h>

#include <stdio.h>

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

char **xargv;                 /* Global pointer to arg vector */
UCHAR **cmlist = (UCHAR **)0; /* Pointer to file list */
char *xname = "kermit.neo";   /* Default program name */

int action = A_RECV; /* Send or Receive */
int xmode = 0;       /* File-transfer mode (manual) */
int ftype = 1;       /* Global file type 0=text 1=binary (always binary)*/
int keep = 0;        /* Keep incompletely received files */
int parity = 0;      /* Parity */
#ifdef F_CRC
int check = 3; /* Block check */
#else
int check = 1;
#endif          /* F_CRC */
int remote = 1; /* 1 = Remote, 0 = Local */

// Load BASIC and restart
// TODO: should be declared with noreturn flag, but how?
__attribute__((leaf)) void load_basic_and_restart(void) {
  KSendMessageSync(API_GROUP_SYSTEM, API_FN_BASIC);
  __attribute__((leaf)) asm volatile("jmp ($0000)" : : : "p");
}

void doexit(int status) {
  // Close all files
  neo_file_close((uint8_t)0xff);
#ifdef DEBUG
  // Close debug log
  debug(DB_CLS, "", 0, 0);
#endif // DEBUG
  printf("doexit status=%d\n", status);
  puts("Restart into NeoBasic");
  // Force system reset
  load_basic_and_restart();
}

void start_banner(void) {
  neo_console_clear_screen();
  printf("This is Neo6502-Kermit %s\n", NEO6502_KERMIT_VERSION);
}

int main(int argc, char **argv) {
  int status, rx_len, i, x;
  char c;
  UCHAR *inbuf;
  short r_slot;
  bool running = true;

  parity = P_PARITY; /* Set this to desired parity */
  status = X_OK;     /* Initial kermit status */

  // Code starts here

#ifdef DEBUG
  debug(DB_OPN, "DEBUG enabled", 0, 0);
#endif // DEBUG

  start_banner();
  devinit();

  // Parameters for this run

  k.xfermode = xmode;               /* Text/binary automatic/manual  */
  k.remote = remote;                /* Remote vs local */
  k.binary = 1;                     /* 0 = text, 1 = binary */
  k.parity = parity;                /* Communications parity */
  k.bct = (check == 5) ? 3 : check; /* Block check type */
  k.ikeep = keep;                   /* Keep incompletely received files */
  k.filelist = cmlist;              /* List of files to send (if any) */
  k.cancel = 0;                     /* Not canceled yet */

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

  // Force Type 3 Block Check (16-bit CRC) on all packets, or not
  k.bctf = (check == 5) ? 1 : 0;

  // Toplevel loop for send/receive multiple files
  while (running) {

    // Initialize Kermit protocol
    status = kermit(K_INIT, &k, 0, 0, "", &r);
#ifdef DEBUG
    debug(DB_LOG, "init status:", 0, status);
    debug(DB_LOG, "E-Kermit version:", k.version, 0);
#endif /* DEBUG */
    if (status == X_ERROR) {
      doexit(FAILURE);
    }

    // Sending files start here
    if (action == A_SEND) {
      status = kermit(K_SEND, &k, 0, 0, "", &r);
    }

    // Now we read a packet ourselves and call Kermit with it.  Normally, Kermit
    // would read its own packets, but in the embedded context, the device must
    // be free to do other things while waiting for a packet to arrive.  So the
    // real control program might dispatch to other types of tasks, of which
    // Kermit is only one.  But in order to read a packet into Kermit's internal
    // buffer, we have to ask for a buffer address and slot number. To interrupt
    // a transfer in progress, set k.cancel to I_FILE to interrupt only the
    // current file, or to I_GROUP to cancel the current file and all remaining
    // files.  To cancel the whole operation in such a way that the both Kermits
    // return an error status, call Kermit with K_ERROR.

    while (status != X_DONE) {

      // Here we block waiting for a packet to come in (unless readpkt times
      // out). Another possibility would be to call inchk() to see if any bytes
      // are waiting to be read, and if not, go do something else for a while,
      // then come back here and check again.

      inbuf = getrslot(&k, &r_slot);       /* Allocate a window slot */
      rx_len = k.rxd(&k, inbuf, P_PKTLEN); /* Try to read a packet */
      debug(DB_PKT, "main packet", &(k.ipktbuf[0][r_slot]), rx_len);

      // For simplicity, kermit() ACKs the packet immediately after verifying it
      // was received correctly.  If, afterwards, the control program fails to
      // handle the data correctly (e.g. can't open file, can't write data,
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
        // name, date, size, and bytes transferred so far.  These can be used in
        // a file-transfer progress display, log, etc.
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
    puts("Press ^C to abort, other to continue");
    c = getchar();
    if (c == 0x03) {
      running = false;
    }
  }
  doexit(SUCCESS);
}
