// Tweak FTDI EEPROM

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
#include <libusb-1.0/libusb.h>
#include <libftdi1/ftdi.h>
#include <assert.h>

int             debug = 0;
#define	ELEN	0x800
unsigned char   buf[ELEN];
unsigned char   was[ELEN];

inline unsigned char
getbyte(unsigned int n)
{
   assert(n < ELEN);
   return buf[n];
}

void
setbyte(unsigned int n, int v, const char *desc)
{
   assert(n < ELEN);
   assert(v < 0x100);
   unsigned char   was = getbyte(n);
   if (debug)
      fprintf(stderr, "0x%02X   %s", was, desc ? : "");
   if (v < 0)
   {
      if (debug)
         fprintf(stderr, " not set\n");
      return;
   }
   if (v == was)
   {
      if (debug)
         fprintf(stderr, " unchanged\n");
      return;
   }
   if (debug)
      fprintf(stderr, " changed to 0x%02X\n", v);
   buf[n] = v;
}

inline unsigned short
getword(unsigned int n)
{
   assert(n < ELEN);
   return (buf[n * 2 + 1] << 8) | (buf[n * 2]);
}

void
setword(unsigned int n, int v, const char *desc)
{
   assert(n < ELEN);
   assert(v < 0x10000);
   unsigned short  was = getword(n);
   if (debug)
      fprintf(stderr, "0x%04X %s", was, desc ? : "");
   if (v < 0)
   {
      if (debug)
         fprintf(stderr, " not set\n");
      return;
   }
   if (v == was)
   {
      if (debug)
         fprintf(stderr, " unchanged\n");
      return;
   }
   if (debug)
      fprintf(stderr, " changed to 0x%04X\n", v);
   buf[n * 2 + 1] = (v >> 8);
   buf[n * 2] = v;
}

void
checksum(void)
{
   //Calculate and store checksum
   unsigned short  c = 0xAAAA;
   for (int a = 0; a < 0x12; a++)
   {
      c ^= getword(a);
      c = (c << 1) | (c >> 15);
   }
   for (int a = 0x40; a < 0x7F; a++)
   {
      c ^= getword(a);
      c = (c << 1) | (c >> 15);
   }
   setword(0x7F, c, "Checksum");
}

int
main(int argc, const char *argv[])
{
   int             cbus0 = -1,
                   cbus1 = -1,
                   cbus2 = -1,
                   cbus3 = -1,
                   cbus4 = -1,
                   cbus5 = -1,
                   cbus6 = -1;
   int             vid = -1,
                   pid = -1;
   int             vendor = 0x0403,
                   product = 0x6015,
                   index = 0;
   const char     *description = NULL;
   const char     *serial = NULL;
   /* Defaults for the FT320X */
   {
      poptContext     optCon;
      const struct poptOption optionsTable[] = {
         {"vendor", 'v', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &vendor, 0, "Vendor ID to find device", "N"},
         {"product", 'p', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &product, 0, "Product ID to find device", "N"},
         {"index", 'i', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &index, 0, "Index to find device", "N"},
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

      optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
      //poptSetOtherOptionHelp(optCon, "[filename]");

      int             c;
      if ((c = poptGetNextOpt(optCon)) < -1)
         errx(1, "%s: %s\n", poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(c));

      if (poptPeekArg(optCon))
      {
         poptPrintUsage(optCon, stderr, 0);
         return -1;
      }
      poptFreeContext(optCon);
   }

   struct ftdi_context *ftdi;

   memset(buf, 0, ELEN);
   if (!(ftdi = ftdi_new()))
      errx(1, "Failed FTDI init");

   if (ftdi_usb_open_desc_index(ftdi, vendor, product, description, serial, index) < 0)
      errx(1, "Cannot find device");
   if (ftdi_read_eeprom(ftdi))
      errx(1, "Cannot read EEPROM: %s", ftdi_get_error_string(ftdi));
   int             elen = 0;
   if (ftdi_get_eeprom_value(ftdi, CHIP_SIZE, &elen))
      if (ftdi_read_eeprom(ftdi))
         errx(1, "Cannot read EEPROM: %s", ftdi_get_error_string(ftdi));
   if (debug)
      fprintf(stderr, "EEPROM size 0x%04X\n", elen);
   if (elen > ELEN)
      errx(1, "EEPROM too big");
   if (ftdi_get_eeprom_buf(ftdi, buf, elen))
      errx(1, "Cannot read EEPROM: %s", ftdi_get_error_string(ftdi));
   memcpy(was, buf, ELEN);

   setword(0x01, vid, "VID");
   setword(0x02, pid, "PID");
   setbyte(0x1A, cbus0, "CBUS0");
   setbyte(0x1B, cbus1, "CBUS1");
   setbyte(0x1C, cbus2, "CBUS2");
   setbyte(0x1D, cbus3, "CBUS3");
   setbyte(0x1E, cbus4, "CBUS4");
   setbyte(0x1F, cbus5, "CBUS5");
   setbyte(0x20, cbus6, "CBUS6");

   checksum();

   for (int a = 0; a < ELEN; a += 2)
      if (was[a] != buf[a] || was[a + 1] != buf[a + 1])
         if (libusb_control_transfer(ftdi->usb_dev, FTDI_DEVICE_OUT_REQTYPE,
                                     SIO_WRITE_EEPROM_REQUEST, getword(a / 2), a / 2, NULL, 0,
                                     ftdi->usb_write_timeout))
            errx(1, "Write failed at 0x%04X: %s", a, ftdi_get_error_string(ftdi));


   //libusb_close(ftdi->usb_dev);
   ftdi_free(ftdi);

   return 0;
}
