// Tweak FTDI EEPROM file

#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <err.h>
#include <sys/mman.h>

int debug = 0;
unsigned char *buf = NULL;

inline unsigned char
getbyte (int n)
{
  return buf[n];
}

void
setbyte (int n, int v, const char *desc)
{
  unsigned char was = getbyte (n);
  if (debug)
    fprintf (stderr, "0x%02X   %s", was, desc ? : "");
  if (v < 0)
    {
      if (debug)
	fprintf (stderr, " not set\n");
      return;
    }
  if (v == was)
    {
      if (debug)
	fprintf (stderr, " unchanged\n");
      return;
    }
  if (debug)
    fprintf (stderr, " changed to 0x%02X\n", v);
  buf[n] = v;
}

inline unsigned short
getword (int n)
{
  return (buf[n * 2 + 1] << 8) | (buf[n * 2]);
}

void
setword (int n, int v, const char *desc)
{
  unsigned short was = getword (n);
  if (debug)
    fprintf (stderr, "0x%04X %s", was, desc ? : "");
  if (v < 0)
    {
      if (debug)
	fprintf (stderr, " not set\n");
      return;
    }
  if (v == was)
    {
      if (debug)
	fprintf (stderr, " unchanged\n");
      return;
    }
  if (debug)
    fprintf (stderr, " changed to 0x%04X\n", v);
  buf[n * 2 + 1] = (v >> 8);
  buf[n * 2] = v;
}

void
checksum (void)
{				// Calculate and store checksum
  unsigned short c = 0xAAAA;
  for (int a = 0; a < 0x12; a++)
    {
      c ^= getword (a);
      c = (c << 1) | (c >> 15);
    }
  for (int a = 0x40; a < 0x7F; a++)
    {
      c ^= getword (a);
      c = (c << 1) | (c >> 15);
    }
  setword (0x7F, c, "Checksum");
}

int
main (int argc, const char *argv[])
{
  int cbus0 = -1, cbus1 = -1, cbus2 = -1, cbus3 = -1, cbus4 = -1, cbus5 = -1, cbus6 = -1;
  int vid = -1, pid = -1;
  const char *efile = NULL;
  {				// POPT
    poptContext optCon;		// context for parsing command-line options
    const struct poptOption optionsTable[] = {
      {"file", 'f', POPT_ARG_STRING, &efile, 0, "EEPROM file", "filename"},
      {"vid", 'V', POPT_ARG_INT, &vid, 0, "Vendor ID", "N"},
      {"pid", 'P', POPT_ARG_INT, &pid, 0, "Product ID", "N"},
      {"cbus0", '0', POPT_ARG_INT, &cbus0, 0, "CBUS0 mapping", "N"},
      {"cbus1", '1', POPT_ARG_INT, &cbus1, 0, "CBUS1 mapping", "N"},
      {"cbus2", '2', POPT_ARG_INT, &cbus2, 0, "CBUS2 mapping", "N"},
      {"cbus3", '3', POPT_ARG_INT, &cbus3, 0, "CBUS3 mapping", "N"},
      {"cbus4", '4', POPT_ARG_INT, &cbus4, 0, "CBUS4 mapping", "N"},
      {"cbus5", '5', POPT_ARG_INT, &cbus5, 0, "CBUS5 mapping", "N"},
      {"cbus6", '6', POPT_ARG_INT, &cbus6, 0, "CBUS6 mapping", "N"},
      {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug"},
      POPT_AUTOHELP {}
    };

    optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
    poptSetOtherOptionHelp (optCon, "[filename]");

    int c;
    if ((c = poptGetNextOpt (optCon)) < -1)
      errx (1, "%s: %s\n", poptBadOption (optCon, POPT_BADOPTION_NOALIAS), poptStrerror (c));

    if (poptPeekArg (optCon) && !efile)
      efile = poptGetArg (optCon);

    if (poptPeekArg (optCon))
      {
	poptPrintUsage (optCon, stderr, 0);
	return -1;
      }
    poptFreeContext (optCon);
  }

  int f = open (efile, O_RDWR);
  if (f < 0)
    err (1, "Cannot open %s", efile);
  struct stat s;
  if (fstat (f, &s) < 0)
    err (1, "Cannot stat %s", efile);
  size_t len = s.st_size;
  buf = mmap (NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, f, 0);
  setword (0x01, vid, "VID");
  setword (0x02, pid, "PID");
  setbyte (0x1A, cbus0, "CBUS0");
  setbyte (0x1B, cbus1, "CBUS1");
  setbyte (0x1C, cbus2, "CBUS2");
  setbyte (0x1D, cbus3, "CBUS3");
  setbyte (0x1E, cbus4, "CBUS4");
  setbyte (0x1F, cbus5, "CBUS5");
  setbyte (0x20, cbus6, "CBUS6");

  checksum ();
  munmap (buf, len);
  return 0;
}
