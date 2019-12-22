/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 1987 Digital Equipment Corporation
 *           2006 Paul Fox
 *           2019 Cy 'kkm' K'Nelson
 *
 * tty_resize: Read geometry from an ANSI-compatible TTY and sync the system
 *             using POSIX tty ioctl TIOCGWINSZ. You need it only if your TTY
 *             is connected to the host via a serial link; run it in the same
 *             tty after every possible terminal window resize. SSH handles
 *             resize transparently, you don't need it for SSH remoting.
 *
 *             This single file is a complete source of the utility.
 *
 *
 * Salvaged and upgraded for modern C by kkm in 2019 from:
 * http://web.archive.org/web/20081224152013/http://www.davehylands.com/gumstix-wiki/resize/resize.c
 * Thanks to the answer https://unix.stackexchange.com/a/19265
 */

/* This version of resize.c has been modified from the original, which
 * came with X11/xterm.  It no longer tries to emit shell commands for
 * setting LINES, COLUMNS, or TERMCAP.  It assumes an ANSI terminal,
 * and the availability of the TIOCGWINSZ ioctl.  The portability
 * ifdefs were also removed -- posix termios access is assumed.
 *      Paul Fox, June 2006
 *
 * original copyright messages preserved below...
 */

/*
 *      $Xorg: resize.c,v 1.3 2000/08/17 19:55:09 cpqbld Exp $
 */

/* $XFree86: xc/programs/xterm/resize.c,v 3.51 2001/10/09 21:52:40 alanh Exp $ */

/*
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital Equipment
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 *
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */


/* resize.c */

#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termio.h>
#include <termios.h>
#include <unistd.h>

#if __STDC_VERSION__ >= 201112L
#include <stdnoreturn.h>
#else
#define noreturn
#endif

#define ESC "\033"

#define TIMEOUT 10


static char *myname;

static char const csi_getsize[] = ESC "7"  ESC "[r" ESC "[999;999H" ESC "[6n";
static char const csi_getsize_reply[] = ESC "[%d;%dR";
static char const csi_restore[] = ESC "8";
static char const name_of_tty[] = "/dev/tty";

static struct termios tioorig;

static int tty;
static FILE *ttyfp;

static void onintr (int sig);
static void resize_timeout (int sig);
static void Usage (void);
static void readstring (FILE *fp, char *buf, const char *csi_str);

static char *
x_basename(char *name)
{
  char *cp;

  cp = strrchr(name, '/');
  return (cp ? cp + 1 : name);
}


/*
  Tells tty driver to reflect current screen size.
 */

int
main(int argc, char **argv)
{
  int rows, cols;
  struct termios tio;
  char buf[BUFSIZ];
  struct winsize ws;

  myname = x_basename(argv[0]);

  if (argc > 1) Usage();  /* Exits process. */

  if ((ttyfp = fopen(name_of_tty, "r+")) == NULL) {
    fprintf(stderr, "%s: can't open terminal %s\n", myname, name_of_tty);
    exit(1);
  }
  tty = fileno(ttyfp);

  tcgetattr(tty, &tioorig);
  tio = tioorig;
  tio.c_iflag &= ~ICRNL;
  tio.c_lflag &= ~(ICANON | ECHO);
  tio.c_cflag |= CS8;
  tio.c_cc[VMIN] = 6;
  tio.c_cc[VTIME] = 1;
  signal(SIGINT, onintr);
  signal(SIGQUIT, onintr);
  signal(SIGTERM, onintr);
  tcsetattr(tty, TCSADRAIN, &tio);

  write(tty, csi_getsize, strlen(csi_getsize));
  readstring(ttyfp, buf, csi_getsize_reply);   /* Exits on error. */
  if (sscanf(buf, csi_getsize_reply, &rows, &cols) != 2) {
    fprintf(stderr, "%s: Can't get rows and columns\r\n", myname);
    onintr(0);  /* Exits process. */
  }
  write(tty, csi_restore, strlen(csi_restore));

  if (ioctl(tty, TIOCGWINSZ, &ws) != -1) {
    /* we don't have any way of directly finding out
       the current height & width of the window in pixels. We try
       our best by computing the font height and width from the "old"
       struct winsize values, and multiplying by these ratios. */
    if (ws.ws_col != 0) ws.ws_xpixel = cols * (ws.ws_xpixel / ws.ws_col);
    if (ws.ws_row != 0) ws.ws_ypixel = rows * (ws.ws_ypixel / ws.ws_row);
    ws.ws_row = rows;
    ws.ws_col = cols;
    ioctl(tty, TIOCSWINSZ, &ws);
  }

  tcsetattr(tty, TCSADRAIN, &tioorig);
  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);

  exit(0);
}

static void
readstring(register FILE *fp, register char *buf, const char *csi_str)
{
  register int last, c;

  signal(SIGALRM, resize_timeout);
  alarm(TIMEOUT);
  if ((c = getc(fp)) == 0233) {   /* meta-escape, CSI */
    *buf++ = c = ESC[0];
    *buf++ = '[';
  } else {
    *buf++ = c;
  }
  if (c != *csi_str) {
    fprintf(stderr, "%s: unknown character, exiting.\r\n", myname);
    onintr(0);  /* Exits process. */
  }
  last = csi_str[strlen(csi_str) - 1];
  while((*buf++ = getc(fp)) != last) { /* nothing */ }
  alarm(0);
  *buf = 0;
}

noreturn static void
Usage(void)
{
  fprintf(stderr, "Usage: %s\n   sets size via ioctl\n", myname);
  exit(1);
}

noreturn static void
onintr(int sig)
{
  (void)sig;
  tcsetattr (tty, TCSADRAIN, &tioorig);
  exit(1);
}

noreturn static void
resize_timeout(int sig)
{
  fprintf(stderr, "\n%s: timeout occurred\r\n", myname);
  onintr(sig);  /* Exits process. */
}
