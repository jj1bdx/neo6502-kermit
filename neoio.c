// neoio.c
// I/O routines of Neo6502 Kermit
// Author: Kenji Rikitake
// Copyright (C) 2025 by Kenji Rikitake.
// This file follows BSD 3-Clause License.
// 
// Based on unixio.c:
// Author: Frank da Cruz.
// Copyright (C) 1995, 2011.
// Trustees of Columbia University in the City of New York.
// All rights reserved.
// See kermit.c for license.

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
// File I/O:
// int openfile()
// ULONG fileinfo() (no timestamp)
// int readfile()
// int writefile()
// int closefile()

// NOTE: stdio is unavailable on Neo6502!

#include <neo/api.h>

#include "cdefs.h"
#include "debug.h"
#include "kermit.h"
#include "platform.h"

UCHAR o_buf[OBUFLEN + 8]; /* File output buffer */
UCHAR i_buf[IBUFLEN + 8]; /* File output buffer */

/* Debugging */
// TODO: debugging should be done on console
// Again: NO stdio

#ifdef DEBUG
static FILE *dp = (FILE *)0; /* Debug log */
static int xdebug = 0;       /* Debugging on/off */

void dodebug(int fc, UCHAR *label, UCHAR *sval, long nval) {

  if (fc != DB_OPN && !xdebug)
    return;
  if (!label)
    label = "";

  switch (fc) { /* Function code */
  case DB_OPN:  /* Open debug log */
    if (dp)
      fclose(dp);
    if (!*label)
      label = "debug.log";
    dp = fopen(label, "w");
    if (!dp) {
      dp = stderr;
    } else {
      setbuf(dp, (char *)0);
    }
    xdebug = 1;
    fprintf(dp, "DEBUG LOG OPEN\n");
    return;
  case DB_MSG: /* Write a message */
    if (dp)
      fprintf(dp, "%s\n", label);
    return;
  case DB_CHR: /* Write label and character */
    if (dp)
      fprintf(dp, "%s=[%c]\n", label, (char)nval);
    return;
  case DB_PKT: /* Log a packet */
               /* (fill in later, fall thru for now...) */
  case DB_LOG: /* Write label and string or number */
    if (sval && dp)
      fprintf(dp, "%s[%s]\n", label, sval);
    else
      fprintf(dp, "%s=%ld\n", label, nval);
    return;
  case DB_CLS: /* Close debug log */
    if (dp) {
      fclose(dp);
      dp = (FILE *)0;
    }
    xdebug = 0;
  }
}
#endif /* DEBUG */

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

int readpkt(struct k_data *k, UCHAR *p, int len, int fc) {
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

    if (!flag && c != k->r_soh) /* No start of packet yet */
      continue;                 /* so discard these bytes. */
    if (c == k->r_soh) {        /* Start of packet */
      flag = 1;                 /* Remember */
      continue;                 /* But discard. */
    } else if (c == k->r_eom    /* Packet terminator */
               || c == '\012'   /* 1.3: For HyperTerminal */
    ) {
#ifdef DEBUG
      *p = NUL; /* Terminate for printing */
      debug(DB_PKT, "RPKT", p2, n);
#endif /* DEBUG */
      return (n);
    } else {                 /* Contents of packet */
      if (n++ > k->r_maxlen) /* Check length */
        return (0);
      else
        *p++ = x & 0xff;
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
//   X_ERROR on failure to write - i/o error.

int tx_data(struct k_data *k, UCHAR *p, int n) {
  int x;
  int max;

  max = 10; /* Loop breaker */

  while (n > 0) { /* Keep trying till done */
    x = neo_uext_uart_block_write(0, p, n);
    debug(DB_MSG, "tx_data write", 0, x);
    if (x < 0 || --max < 1) /* Errors are fatal */
      return (X_ERROR);
    n -= x;
    p += x;
  }
  return (X_OK); /* Success */
}

/*  O P E N F I L E  --  Open output file  */
/*
  Call with:
    Pointer to filename.
    Size in bytes.
    Creation date in format yyyymmdd hh:mm:ss, e.g. 19950208 14:00:00
    Mode: 1 = read, 2 = create, 3 = append.
  Returns:
    X_OK on success.
    X_ERROR on failure, including rejection based on name, size, or date.
*/
int openfile(struct k_data *k, UCHAR *s, int mode) {

  switch (mode) {
  case 1: /* Read */
    if (!(ifile = fopen(s, "r"))) {
      debug(DB_LOG, "openfile read error", s, 0);
      return (X_ERROR);
    }
    k->s_first = 1;        /* Set up for getkpt */
    k->zinbuf[0] = '\0';   /* Initialize buffer */
    k->zinptr = k->zinbuf; /* Set up buffer pointer */
    k->zincnt = 0;         /* and count */
    debug(DB_LOG, "openfile read ok", s, 0);
    return (X_OK);

  case 2: /* Write (create) */
    ofile = creat(s, 0644);
    if (ofile < 0) {
      debug(DB_LOG, "openfile write error", s, 0);
      return (X_ERROR);
    }
    debug(DB_LOG, "openfile write ok", s, 0);
    return (X_OK);

#ifdef COMMENT
  case 3: /* Append (not used) */
    ofile = open(s, O_WRONLY | O_APPEND);
    if (ofile < 0) {
      debug(DB_LOG, "openfile append error", s, 0);
      return (X_ERROR);
    }
    debug(DB_LOG, "openfile append ok", s, 0);
    return (X_OK);
#endif /* COMMENT */

  default:
    return (X_ERROR);
  }
}

/*  F I L E I N F O  --  Get info about existing file  */
/*
  Call with:
    Pointer to filename
    Pointer to buffer for date-time string
    Length of date-time string buffer (must be at least 18 bytes)
    Pointer to int file type:
       0: Prevailing type is text.
       1: Prevailing type is binary.
    Transfer mode (0 = auto, 1 = manual):
       0: Figure out whether file is text or binary and return type.
       1: (nonzero) Don't try to figure out file type.
  Returns:
    X_ERROR on failure.
    0L or greater on success == file length.
    Date-time string set to yyyymmdd hh:mm:ss modtime of file.
    If date can't be determined, first byte of buffer is set to NUL.
    Type set to 0 (text) or 1 (binary) if mode == 0.
*/
#ifdef F_SCAN
#define SCANBUF 1024
#define SCANSIZ 49152
#endif /* F_SCAN */

ULONG
fileinfo(struct k_data *k, UCHAR *filename, UCHAR *buf, int buflen, short *type,
         short mode) {
  struct stat statbuf;
  struct tm *timestamp, *localtime();

#ifdef F_SCAN
  FILE *fp;            /* File scan pointer */
  char inbuf[SCANBUF]; /* and buffer */
#endif                 /* F_SCAN */

  if (!buf)
    return (X_ERROR);
  buf[0] = '\0';
  if (buflen < 18)
    return (X_ERROR);
  if (stat(filename, &statbuf) < 0)
    return (X_ERROR);
  timestamp = localtime(&(statbuf.st_mtime));
  sprintf(buf, "%04d%02d%02d %02d:%02d:%02d", timestamp->tm_year + 1900,
          timestamp->tm_mon + 1, timestamp->tm_mday, timestamp->tm_hour,
          timestamp->tm_min, timestamp->tm_sec);
#ifdef F_SCAN
  /*
    Here we determine if the file is text or binary if the transfer mode is
    not forced.  This is an extremely crude sample, which diagnoses any file
    that contains a control character other than HT, LF, FF, or CR as binary.
    A more thorough content analysis can be done that accounts for various
    character sets as well as various forms of Unicode (UTF-8, UTF-16, etc).
    Or the diagnosis could be based wholly or in part on the filename.
    etc etc.  Or the implementation could skip this entirely by not defining
    F_SCAN and/or by always calling this routine with type set to -1.
  */
  if (!mode) { /* File type determination requested */
    int isbinary = 1;
    fp = fopen(filename, "r"); /* Open the file for scanning */
    if (fp) {
      int n = 0, count = 0;
      char c, *p;

      debug(DB_LOG, "fileinfo scan ", filename, 0);

      isbinary = 0;
      while (count < SCANSIZ && !isbinary) { /* Scan this much */
        n = fread(inbuf, 1, SCANBUF, fp);
        if (n == EOF || n == 0)
          break;
        count += n;
        p = inbuf;
        while (n--) {
          c = *p++;
          if (c < 32 || c == 127) {
            if (c != 9 &&  /* Tab */
                c != 10 && /* LF */
                c != 12 && /* FF */
                c != 13) { /* CR */
              isbinary = 1;
              debug(DB_MSG, "fileinfo BINARY", 0, 0);
              break;
            }
          }
        }
      }
      fclose(fp);
      *type = isbinary;
    }
  }
#endif /* F_SCAN */

  return ((ULONG)(statbuf.st_size));
}

/*  R E A D F I L E  --  Read data from a file  */

int readfile(struct k_data *k) {
  if (!k->zinptr) {
#ifdef DEBUG
    fprintf(dp, "readfile ZINPTR NOT SET\n");
#endif /* DEBUG */
    return (X_ERROR);
  }
  if (k->zincnt < 1) { /* Nothing in buffer - must refill */
    if (k->binary) {   /* Binary - just read raw buffers */
      k->dummy = 0;
      k->zincnt = fread(k->zinbuf, 1, k->zinlen, ifile);
      debug(DB_LOG, "readfile binary ok zincnt", 0, k->zincnt);

    } else { /* Text mode needs LF/CRLF handling */
      int c; /* Current character */
      for (k->zincnt = 0; (k->zincnt < (k->zinlen - 2)); (k->zincnt)++) {
        if ((c = getc(ifile)) == EOF)
          break;
        if (c == '\n')                     /* Have newline? */
          k->zinbuf[(k->zincnt)++] = '\r'; /* Insert CR */
        k->zinbuf[k->zincnt] = c;
      }
#ifdef DEBUG
      k->zinbuf[k->zincnt] = '\0';
      debug(DB_LOG, "readfile text ok zincnt", 0, k->zincnt);
#endif /* DEBUG */
    }
    k->zinbuf[k->zincnt] = '\0'; /* Terminate. */
    if (k->zincnt == 0)          /* Check for EOF */
      return (-1);
    k->zinptr = k->zinbuf; /* Not EOF - reset pointer */
  }
  (k->zincnt)--; /* Return first byte. */

  debug(DB_LOG, "readfile exit zincnt", 0, k->zincnt);
  debug(DB_LOG, "readfile exit zinptr", 0, k->zinptr);
  return (*(k->zinptr)++ & 0xff);
}

/*  W R I T E F I L E  --  Write data to file  */
/*
  Call with:
    Kermit struct
    String pointer
    Length
  Returns:
    X_OK on success
    X_ERROR on failure, such as i/o error, space used up, etc
*/
int writefile(struct k_data *k, UCHAR *s, int n) {
  int rc;
  rc = X_OK;

  debug(DB_LOG, "writefile binary", 0, k->binary);

  if (k->binary) { /* Binary mode, just write it */
    if (write(ofile, s, n) != n)
      rc = X_ERROR;
  } else { /* Text mode, skip CRs */
    UCHAR *p, *q;
    int i;
    q = s;

    while (1) {
      for (p = q, i = 0; ((*p) && (*p != (UCHAR)13)); p++, i++)
        ;
      if (i > 0)
        if (write(ofile, q, i) != i)
          rc = X_ERROR;
      if (!*p)
        break;
      q = p + 1;
    }
  }
  return (rc);
}

/*  C L O S E F I L E  --  Close output file  */
/*
  Mode = 1 for input file, mode = 2 or 3 for output file.

  For output files, the character c is the character (if any) from the Z
  packet data field.  If it is D, it means the file transfer was canceled
  in midstream by the sender, and the file is therefore incomplete.  This
  routine should check for that and decide what to do.  It should be
  harmless to call this routine for a file that that is not open.
*/
int closefile(struct k_data *k, UCHAR c, int mode) {
  int rc = X_OK; /* Return code */

  switch (mode) {
  case 1:       /* Closing input file */
    if (!ifile) /* If not not open */
      break;    /* do nothing but succeed */
    debug(DB_LOG, "closefile (input)", k->filename, 0);
    if (fclose(ifile) < 0)
      rc = X_ERROR;
    break;
  case 2: /* Closing output file */
  case 3:
    if (ofile < 0) /* If not open */
      break;       /* do nothing but succeed */
    debug(DB_LOG, "closefile (output) name", k->filename, 0);
    debug(DB_LOG, "closefile (output) keep", 0, k->ikeep);
    if (close(ofile) < 0) { /* Try to close */
      rc = X_ERROR;
    } else if ((k->ikeep == 0) && /* Don't keep incomplete files */
               (c == 'D')) {      /* This file was incomplete */
      if (k->filename) {
        debug(DB_LOG, "deleting incomplete", k->filename, 0);
        unlink(k->filename); /* Delete it. */
      }
    }
    break;
  default:
    rc = X_ERROR;
  }
  return (rc);
}

#ifdef DEBUG
int xerror() {
  unsigned int x;
  extern int errorrate; /* Fix this - NO EXTERNS */
  if (!errorrate)
    return (0);
  x = rand() % 100; /* Fix this - NO C LIBRARY */
  debug(DB_LOG, "RANDOM", 0, x);
  debug(DB_LOG, "ERROR", 0, (x < errorrate));
  return (x < errorrate);
}
#endif /* DEBUG */
