// This file is a part of Neo6502-Kermit.
// See LICENSE for the licensing details.

// neoio.c
// I/O routines of Neo6502 Kermit
// Author: Kenji Rikitake
// Based on unixio.c:
// Author: Frank da Cruz.

// Style: C99. It's 2025.

// This routines open Neo6502 UEXT UART port
// for Kermit I/O.
// Baud rate will be configurable later.
// Parity is fixed to 8N1 only.
// The file I/O is performed over Neo6502 Filesystem.

// Functions defined:
// (See unixio.c for the UNIX example and explanation)
// (prototypes in kermit.h)
// UART/device I/O:
// int readpkt()
// int tx_data()
// int inchk()
// TODO: implement UART baudrate config function
// File I/O:
// int openfile()
// ULONG fileinfo() (no timestamp)
// int readfile()
// int writefile()
// int closefile()

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <neo/api.h>

#include "cdefs.h"
#include "debug.h"
#include "kermit.h"

// File I/O buffers
UCHAR o_buf[OBUFLEN + 8];
UCHAR i_buf[IBUFLEN + 8];

// File I/O channel IDs
#define CHANNEL_INPUT_FILE (1)
#define CHANNEL_OUTPUT_FILE (2)

// Prototypes in main.c
// TODO: merge prototype to a new header
void doexit(int status);

// Debugging functions
// Output written simultaneoutly to
// Neo Console and file "KDEBUG.LOG"

#ifdef DEBUG

#define CHANNEL_DEBUG_OUTPUT (3)
#define DBUFLEN (256)
#define DEBUG_FILE "KDEBUG.LOG"

static int xdebug = 0; /* Debugging on/off */
static uint8_t dchannel = CHANNEL_DEBUG_OUTPUT;

static char dbuf[DBUFLEN];

void debugout(char *str) {
  (void)neo_file_write(dchannel, str, strlen(str));
  if (neo_api_error() != API_ERROR_NONE) {
    puts("debugout: neo_file_write error");
    doexit(FAILURE);
  }
  while (*str != '\0') {
    putchar((int)*str);
    str++;
  }
  // Force-set Neo console foreground color to green
  putchar(0x82);
  // Force-set Neo console background color to black
  putchar(0x98);
}

void dodebug(int fc, UCHAR *label, UCHAR *sval, long nval) {
  if (fc != DB_OPN && !xdebug) {
    return;
  }
  if (!label) {
    label = (UCHAR *)"";
  }
  switch (fc) { /* Function code */
  case DB_OPN:  /* Open debug log */
    xdebug = 1;
    neo_file_open(dchannel, (const char *)DEBUG_FILE,
                  3); // truncate and read-write
    if (neo_api_error() != API_ERROR_NONE) {
      puts("dodebug: neo_file_open error");
      doexit(FAILURE);
    }
    snprintf(dbuf, DBUFLEN, "DEBUG LOG OPEN\n");
    debugout(dbuf);
    return;
  case DB_MSG: /* Write a message */
    snprintf(dbuf, DBUFLEN, "%s\n", label);
    debugout(dbuf);
    return;
  case DB_CHR: /* Write label and character */
    snprintf(dbuf, DBUFLEN, "%s=[%c]\n", label, (char)nval);
    debugout(dbuf);
    return;
  // TODO: for DB_PKT, we need to output a more decent string
  case DB_PKT: /* Log a packet */
               /* (fill in later, fall thru for now...) */
  case DB_LOG: /* Write label and string or number */
    if (sval) {
      snprintf(dbuf, DBUFLEN, "%s[%s]\n", label, sval);
      debugout(dbuf);
    } else {
      snprintf(dbuf, DBUFLEN, "%s=%ld\n", label, nval);
      debugout(dbuf);
    }
    return;
  case DB_CLS: /* Close debug log */
    snprintf(dbuf, DBUFLEN, "DEBUG LOG CLOSE\n");
    debugout(dbuf);
    xdebug = 0;
    neo_file_close(dchannel);
    return;
  }
}
#endif /* DEBUG */

// UART section

#define SERIAL_PROTOCOL_8N1 (0)
#define SERIAL_TRANSFER_BAUD_RATE (9600)

// Initialize UART device.

void devinit(void) {
  neo_uext_uart_configure(SERIAL_TRANSFER_BAUD_RATE, SERIAL_PROTOCOL_8N1);
  debug(DB_LOG, "Serial port speed", 0, (long)SERIAL_TRANSFER_BAUD_RATE);
}

// Read a Kermit packet from UART
// Call with:
//    k   - Kermit struct pointer
//    p   - pointer to read buffer
//    len - length of read buffer
//
// When reading a packet, this function looks for start of Kermit packet
// (k->r_soh), then reads everything between it and the end of the packet
// (k->r_eom) into the indicated buffer.  Returns the number of bytes read,
// or:
//    0   - timeout or other possibly correctable error;
//   -1   - fatal error, such as loss of connection, or no buffer to read into.
//
// Timeout not implemented in this sample.
// Maximum packet length to receive: k->r_maxlen

int readpkt(struct k_data *k, UCHAR *p, int len) {
  int x, n, max;
  short flag;
  UCHAR c;
#ifdef DEBUG
  UCHAR *p2;
#endif /* DEBUG */

#ifdef F_CTRLC
  short ccn;
  ccn = 0;
#endif /* F_CTRLC */

  if (!p) { /* Device not open or no buffer */
    debug(DB_MSG, "readpkt FAIL", 0, 0);
    return (-1);
  }

  flag = 0;
  n = 0;

#ifdef DEBUG
  p2 = p;
#endif /* DEBUG */

  while (1) {
    // Busy-wait required for UART receiving
    while (!neo_uext_uart_available()) {
      // Maybe you can do something here
    }
    x = neo_uext_uart_read();
    c = (k->parity) ? x & 0x7f : x & 0xff; /* Strip parity */

#ifdef F_CTRLC
    /* In remote mode only: three consecutive ^C's to quit */
    if (k->remote && c == (UCHAR)3) {
      if (++ccn > 2) {
        debug(DB_MSG, "readpkt ^C^C^C", 0, 0);
        return (-1);
      }
    } else {
      ccn = 0;
    }
#endif /* F_CTRLC */

    if (!flag && c != k->r_soh) { /* No start of packet yet */
      continue;                   /* so discard these bytes. */
    }
    if (c == k->r_soh) {      /* Start of packet */
      flag = 1;               /* Remember */
      continue;               /* But discard. */
    } else if (c == k->r_eom  /* Packet terminator */
               || c == '\012' /* 1.3: For HyperTerminal */
    ) {
#ifdef DEBUG
      *p = NUL; /* Terminate for printing */
      debug(DB_PKT, "RPKT", p2, n);
#endif /* DEBUG */
      return (n);
    } else {                   /* Contents of packet */
      if (n++ > k->r_maxlen) { /* Check length */
        // Too long packet is not correctable
        debug(DB_MSG, "readpkt packet too long", 0, 0);
        return (-1);
      } else {
        *p++ = x & 0xff;
      }
    }
  }
  debug(DB_MSG, "READPKT FAIL (end)", 0, 0);
  return (-1);
}

// Writes n bytes of data to UART.
// Call with:
//   k = pointer to Kermit struct.
//   p = pointer to data to transmit.
//   n = length.
// Returns:
//   X_OK on success.
//   (Unable to detect write error here)

int tx_data(struct k_data *k, UCHAR *p, int n) {
  neo_uext_uart_block_write(0, p, n);
  debug(DB_MSG, "tx_data write", 0, n);
  return (X_OK); /* Success */
}

// Check if input waiting
//
// Check if input is waiting to be read, needed for sliding windows.
// If your platform does not provide a way to
// look at the device input buffer without blocking and without actually
// reading from it, make this routine return -1.  On success, returns the
// numbers of characters waiting to be read, i.e. that can be safely read
// without blocking.

int inchk(struct k_data *k) {
  // TODO: is this implementation sufficient?
  return neo_uext_uart_available() ? 1 : 0;
}

// File I/O section

// Static variables

static uint8_t ichannel = CHANNEL_INPUT_FILE;
static uint8_t ochannel = CHANNEL_OUTPUT_FILE;

// Open output file
//  Call with:
//    Pointer to filename.
//    Size in bytes.
//    Creation date in format yyyymmdd hh:mm:ss, e.g. 19950208 14:00:00
//    Mode: 1 = read, 2 = create.
//  Returns:
//    X_OK on success.
//    X_ERROR on failure, including rejection based on name, size, or date.

int openfile(struct k_data *k, UCHAR *s, int mode) {
  uint8_t error;

  switch (mode) {
  case 1:                                        /* Read */
    neo_file_open(ichannel, (const char *)s, 0); // read-only
    if ((error = neo_api_error()) != API_ERROR_NONE) {
      debug(DB_LOG, "openfile: neo_file_open read error", s, 0);
      debug(DB_LOG, "error code", 0, error);
      return (X_ERROR);
    }
    k->s_first = 1;        /* Set up for getkpt */
    k->zinbuf[0] = '\0';   /* Initialize buffer */
    k->zinptr = k->zinbuf; /* Set up buffer pointer */
    k->zincnt = 0;         /* and count */
    debug(DB_LOG, "openfile read ok", s, 0);
    printf("openfile read %s\n", s);
    return (X_OK);

  case 2:                                        /* Write (create) */
    neo_file_open(ochannel, (const char *)s, 3); // truncate and read-write
    if ((error = neo_api_error()) != API_ERROR_NONE) {
      debug(DB_LOG, "openfile: neo_file_open truncate error", s, 0);
      debug(DB_LOG, "error code", 0, error);
      return (X_ERROR);
    }
    neo_file_close(ochannel);                    // close the file first
    neo_file_open(ochannel, (const char *)s, 1); // re-open for write-only
    if ((error = neo_api_error()) != API_ERROR_NONE) {
      debug(DB_LOG, "openfile: neo_file_open write error", s, 0);
      debug(DB_LOG, "error code", 0, error);
      return (X_ERROR);
    }
    debug(DB_LOG, "openfile write ok", s, 0);
    printf("openfile write %s\n", s);
    return (X_OK);

  default:
    return (X_ERROR);
  }
}

// Get info about existing file.

// Call with:
//   Pointer to filename
//   Pointer to buffer for date-time string (unused for Neo6502)
//   Length of date-time string buffer (must be at least 18 bytes)
//   Pointer to int file type = always 1
//      0: (Not implemented) Prevailing type is text.
//      1: (Always) Prevailing type is binary.
//   Transfer mode (0 = auto, 1 = manual) = always 1
//      0: (Not implemented)
//         Figure out whether file is text or binary and return type.
//      1: (Always) (nonzero) Don't try to figure out file type.
// Returns:
//   X_ERROR on failure.
//   0L or greater on success == file length.
//   Date can't be determined; first byte of buffer is set to NUL.
//   Type set to 0 (text) or 1 (binary) if mode == 0.

ULONG
fileinfo(struct k_data *k, UCHAR *filename, UCHAR *buf, int buflen, short *type,
         short mode) {
  neo_file_stat_t stat;
  uint8_t error;

  if (!buf) {
    return (X_ERROR);
  }
  buf[0] = '\0'; // No datetime info
  if (buflen < 18) {
    return (X_ERROR);
  }
  neo_file_stat((const char *)filename, &stat);
  if ((error = neo_api_error()) != API_ERROR_NONE) {
    debug(DB_LOG, "fileinfo: neo_file_stat error", filename, 0);
    debug(DB_LOG, "error code", 0, error);
    return (X_ERROR);
  }
  *type = 1; // File type is always binary regardless of mode
  return ((ULONG)(stat.size));
}

// Read data from a file

int readfile(struct k_data *k) {
  uint8_t error;

  if (!k->zinptr) {
#ifdef DEBUG
    printf("readfile ZINPTR NOT SET\n");
#endif /* DEBUG */
    return (X_ERROR);
  }
  if (k->zincnt < 1) { /* Nothing in buffer - must refill */
    if (k->binary) {   /* Binary - just read raw buffers */
      k->dummy = 0;
      k->zincnt = neo_file_read(ichannel, k->zinbuf, k->zinlen);
      if ((error = neo_api_error()) != API_ERROR_NONE) {
        debug(DB_LOG, "readfile: binary neo_file_read error, code", 0, error);
        return (X_ERROR);
      }
      debug(DB_LOG, "readfile binary ok zincnt", 0, k->zincnt);

    } else {    /* Text mode needs LF/CRLF handling */
      UCHAR ch; // character buffer
      int c;    /* Current character */

      for (k->zincnt = 0; (k->zincnt < (k->zinlen - 2)); (k->zincnt)++) {
        if (neo_file_eof(ichannel)) {
          break;
        }
        (void)neo_file_read(ichannel, &ch, 1);
        if ((error = neo_api_error()) != API_ERROR_NONE) {
          debug(DB_LOG, "readfile: text neo_file_read error", 0, error);
          return (X_ERROR);
        }
        c = (int)ch;
        if (c == '\n') {                   /* Have newline? */
          k->zinbuf[(k->zincnt)++] = '\r'; /* Insert CR */
        }
        k->zinbuf[k->zincnt] = c;
      }
#ifdef DEBUG
      k->zinbuf[k->zincnt] = '\0';
      debug(DB_LOG, "readfile text ok zincnt", 0, k->zincnt);
#endif /* DEBUG */
    }
    k->zinbuf[k->zincnt] = '\0'; /* Terminate. */
    if (k->zincnt == 0) {        /* Check for EOF */
      return (-1);
    }
    k->zinptr = k->zinbuf; /* Not EOF - reset pointer */
  }
  (k->zincnt)--; /* Return first byte. */

  debug(DB_LOG, "readfile exit zincnt", 0, k->zincnt);
  debug(DB_LOG, "readfile exit zinptr", 0, k->zinptr);
  return (*(k->zinptr)++ & 0xff);
}

// Write data to file
//
// Call with:
//   Kermit struct
//   String pointer
//   Length
// Returns:
//   X_OK on success
//   X_ERROR on failure, such as i/o error, space used up, etc

int writefile(struct k_data *k, UCHAR *s, int n) {
  int rc;
  uint8_t error;
  rc = X_OK;

  debug(DB_LOG, "writefile binary", 0, k->binary);

  if (k->binary) { /* Binary mode, just write it */
    if (neo_file_write(ochannel, s, n) != n) {
      error = neo_api_error();
      debug(DB_LOG, "writefile: binary neo_file_write error, code", 0, error);
      rc = X_ERROR;
    }
  } else { /* Text mode, skip CRs */
    UCHAR *p, *q;
    int i;
    q = s;

    while (1) {
      for (p = q, i = 0; ((*p) && (*p != (UCHAR)13)); p++, i++)
        ;
      if (i > 0) {
        if (neo_file_write(ochannel, q, i) != i) {
          error = neo_api_error();
          debug(DB_LOG, "writefile: text neo_file_write error, code", 0, error);
          rc = X_ERROR;
        }
      }
      if (!*p) {
        break;
      }
      q = p + 1;
    }
  }
  return (rc);
}

// Close output file

//  Mode = 1 for input file, mode = 2 or 3 for output file.
//
//  For output files, the character c is the character (if any) from the Z
//  packet data field.  If it is D, it means the file transfer was canceled
//  in midstream by the sender, and the file is therefore incomplete.  This
//  routine should check for that and decide what to do.  It should be
//  harmless to call this routine for a file that that is not open.

int closefile(struct k_data *k, UCHAR c, int mode) {
  int rc = X_OK; /* Return code */
  uint8_t error;

  switch (mode) {
  case 1: /* Closing input file */
    debug(DB_LOG, "closefile (input)", k->filename, 0);
    printf("closefile (input) %s\n", k->filename);
    neo_file_close(ichannel);
    break;
  case 2: /* Closing output file */
  case 3:
    debug(DB_LOG, "closefile (output) name", k->filename, 0);
    debug(DB_LOG, "closefile (output) keep", 0, k->ikeep);
    printf("closefile (output) %s\n", k->filename);
    neo_file_close(ochannel);
    if ((k->ikeep == 0) && /* Don't keep incomplete files */
        (c == 'D')) {      /* This file was incomplete */
      if (k->filename) {
        debug(DB_LOG, "deleting incomplete", k->filename, 0);
        printf("closefile (delete incomplete) %s\n", k->filename);
        neo_file_delete((const char *)k->filename); /* Delete it. */
        if ((error = neo_api_error()) != API_ERROR_NONE) {
          debug(DB_LOG, "closefile: neo_file_delete error", k->filename, 0);
          debug(DB_LOG, "error code", 0, error);
          rc = X_ERROR;
        }
      }
    }
    break;
  default:
    rc = X_ERROR;
  }
  return (rc);
}
