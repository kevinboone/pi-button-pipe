/*======================================================================
  pi_button_pipe
  A simple program to capture GPIO button presses and make them
  available to other programs via a named pipe. This program can
  monitor multiple pins, and takes care of contact debouncing both
  on press and release

  Pipe filename defaults to /tmp/pi-buttons, and should ideally
  exist before running the program. The program will create it, but
  unless we can be sure that whatever consumes from the pipe does not
  start until the pipe is established, the consumer may fail.
  Moreover, the pipe might need to have different ownership or
  permissions for this program.

  The data that will be read from the pipe is a number of
  lines, one for each event. On each line is the number of
  the GPIO pin that triggered the event. If -r or -f is 
  specified, to set triggering on only rising or only falling
  edge, nothing else is written. If the program is triggering
  on both edges (the default), then the state -- 0 or 1 --
  is written after the pin number.

  See http://kevinboone.me/pi-button-pipe.html  for details

  (c)2014 Kevin Boone. Distributed under the terms of the GPL, v2.0
======================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

// TUNEABLE PARAMETERS

#define DEFAULT_PIPE_FILENAME "/tmp/pi-buttons"

// CLOCK_ERROR_SECONDS is how long the elapsed time can be
//   between two events, before we conclude that the system
//   clock has been adjusted, and we need to reset the internal
//   timer
// The value needs to be large enough that minor adjustments caused
//   by NTP sync don't cause events to be skipped, but not so large
//   that we don't detect problems. In practce, in a Pi without
//   real-time clock, when NTP sets the date, it will result in
//   a time discrepancy of at least 30 years, which is easy to
//   detect
#define SEC_PER_YEAR 31536000
#define CLOCK_ERROR_SECONDS SEC_PER_YEAR

// Maximum number of pins that can be monitored
#define MAX_PINS 20

// Default time between interrupts below which we will treat the
//  interrupt as a switch bounce. Can be changed on the
//  command line
#define BOUNCE_MSEC 300 

// END TUNEABLE PARAMETERS

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE 
#define FALSE 0
#endif

#ifndef BOOL
typedef int BOOL;
#endif

#define abs(x) ((x) > 0 ? (x) : (-x))

// Note -- the following variables must be global, as they
//  are used by the quit signal handler
// Number of pins specified on command line
int npins = 0;
// Array of pin numbers specified on the command line
int pins[MAX_PINS];
// Whether to avoid exporting/unexporting pins
int no_export = FALSE;

#define EDGE_RISING 0x01
#define EDGE_FALLING 0x02

int edge = EDGE_RISING | EDGE_FALLING;
int bounce_time = BOUNCE_MSEC;

/*======================================================================
  write_to_file
  Helper function for writing a text string to a file
======================================================================*/
void write_to_file (const char *file, const char *text)
  {
  FILE *f = fopen (file, "w");
  if (f)
    {
    fprintf (f, text);
    fclose (f);
    }
  else
    {
    fprintf (stderr, "Can't write to %s: %s\n", file, 
      strerror (errno));
    exit (-1);
    }
  }

/*======================================================================
  unexport_pins
======================================================================*/
void unexport_pins (void)
  {
  int i;
  for (i = 0; i < npins; i++)
    {
    int pin = pins[i];
    char s[50];
    snprintf (s, sizeof(s), "%d", pin); 
    write_to_file ("/sys/class/gpio/unexport", s);  
    }
  }

/*======================================================================
  quit_signal 
  In response to a quit, or interrupt, we must unexport any pins that
  we exported
======================================================================*/
void quit_signal (int dummy)
  {
  if (!no_export)
    unexport_pins();
  exit (0);
  }

/*======================================================================
  show_version
======================================================================*/
void show_version (FILE *f, const char *argv0)
  {
  fprintf (f, "%s version " VERSION "\n", argv0); 
  fprintf (f, "GPIO button watcher for Raspberry Pi\n");
  fprintf (f, "Copyright (c)2014-2020 Kevin Boone\n");
  fprintf (f, "Distributed under the terms of the GPL, v3.0\n");
  }

/*======================================================================
  show_short_usage 
======================================================================*/
void show_short_usage (FILE *f, const char *argv0)
  {
  fprintf (f, "Usage: %s [-dehunv] pin# pin#...\n", argv0);
  }

/*======================================================================
  show_long_usage 
======================================================================*/
void show_long_usage (FILE *f, const char *argv0)
  {
  fprintf (f, "Usage: %s [-dehunv] pin# pin#...\n", argv0);
  fprintf (f, "  -b N         : bounce time, in millseconds\n");
  fprintf (f, "  -d           : debug mode -- output to console, not pipe\n");
  fprintf (f, "  -e           : export pins only\n");
  fprintf (f, "  -f           : falling edge only\n");
  fprintf (f, "  -h           : show this message\n");
  fprintf (f, "  -n           : no export/unexport\n");
  fprintf (f, "  -r           : rising edge only\n");
  fprintf (f, "  -u           : unexport pins only\n");
  fprintf (f, "  -v           : show version\n");
  fprintf (f, "For more information, "
       "see http://kevinboone.net/pi-button-pipe.html\n");  
  }

/*======================================================================
  get_pin_state 
  Read the state of the pin from the gpio 'value' psuedo file. 
  In principle this function can return -1 if the data read is in
  the wrong format but, in practice, it always seems to read exactly
  two bytes, of which the first is the digit 0 or 1, and the second
  is the EOL. It seems that the read() call will never block (which is,
  I suppose, to be expected) 
======================================================================*/
int get_pin_state (int pin)
  {
  char s[50];
  char buff[3];
  snprintf (s, sizeof(s), "/sys/class/gpio/gpio%d/value", pin); 
  int fd = open (s, O_RDONLY);
  int rc = read (fd, buff, sizeof(buff));
  close (fd);
  if (rc == 2) return (buff[0] - '0');
  return -1;
  }


/*======================================================================
  export_pins
======================================================================*/
void export_pins (void)
  {
  int i;
  for (i = 0; i < npins; i++)
    {
    int pin = pins[i];
    char s[50];
    snprintf (s, sizeof(s), "%d", pin); 
    write_to_file ("/sys/class/gpio/export", s);  
    snprintf (s, sizeof(s), "/sys/class/gpio/gpio%d/direction", pin); 
    write_to_file (s, "in");  
    snprintf (s, sizeof(s), "/sys/class/gpio/gpio%d/edge", pin); 
    // With most switches it hardly matters what we set the
    // 'edge' value to, since all transitions will generate
    // multiple rising and falling edges
    write_to_file (s, "both");  
    snprintf (s, sizeof(s), "/sys/class/gpio/gpio%d/value", pin); 
    }
  }

/*======================================================================
  main 
======================================================================*/
int main(int argc, char **argv)
  {
  int i, c; 
  int export_only = 0, unexport_only = 0, debug = 0;
  int ticks[MAX_PINS]; // Time of last button press
  char pipe_filename[200];
  struct pollfd fdset[MAX_PINS];
  struct pollfd fdset_base[MAX_PINS];

  strcpy (pipe_filename, DEFAULT_PIPE_FILENAME);

  while ((c = getopt (argc, argv, "dehnuvrfb:")) !=-1)
    {
    switch (c)
      {
      case 'h':
        show_long_usage (stdout, argv[0]);
        exit (-1);
      case 'b':
        bounce_time = atoi (optarg);
        break;
      case 'd':
        debug = 1;
        break;
      case 'e':
        export_only = 1;
        break;
      case 'n':
        no_export = 1;
        break;
      case 'u':
        unexport_only = 1;
        break;
      case 'r':
        edge = EDGE_RISING;
        break;
      case 'f':
        edge = EDGE_FALLING;
        break;
      case 'v':
        show_version (stdout, argv[0]);
        exit(-1);
      default:
        show_short_usage (stderr, argv[0]);
        exit(-1);
      }
    }

  int nargs = argc - optind;
  if (nargs < 1)
    {
    show_short_usage (stderr, argv[0]);
    exit (-1);
    }

  if (nargs > MAX_PINS)
    {
    fprintf (stderr, "%s: too many pins specified\n", argv[0]);
    exit (-1);
    }

  for (i = optind; i < argc; i++)
    {
    pins[i - optind] = atoi(argv[i]);
    } 

  npins = nargs;

  time_t start = time(NULL);

  memset (ticks, 0, sizeof(ticks)); 
  memset (fdset_base, 0, sizeof(fdset)); 

  if (unexport_only)
    {
    unexport_pins();
    exit (0);
    }

  if (export_only)
    {
    export_pins();
    exit (0);
    }

  if (!no_export)
    export_pins();

  for (i = 0; i < npins; i++)
    {
    int pin = pins[i];
    char s[50];
    snprintf (s, sizeof(s), "%d", pin); 
    snprintf (s, sizeof(s), "/sys/class/gpio/gpio%d/value", pin); 
    int gpio_fd = open (s, O_RDONLY|O_NONBLOCK);
    if (gpio_fd < 0)
      {
      fprintf (stderr, "Can't open GPIO device %s\n", s);
      exit(-1);
      }
    fdset_base[i].fd = gpio_fd;  
    fdset_base[i].events = POLLPRI;
    }

  signal (SIGQUIT, quit_signal);
  signal (SIGTERM, quit_signal);
  signal (SIGHUP, quit_signal);
  signal (SIGINT, quit_signal);
  // Handle SIGPIPE, because we want to clean up if the consumer closes
  //  its end of the pipe before this program exits (which is likely)
  signal (SIGPIPE, quit_signal);

  FILE *pipe = NULL;
  if (!debug)
    {
    mkfifo (pipe_filename, 0777);
    pipe = fopen (pipe_filename, "w");
    if (!pipe)
      {
      fprintf (stderr, "Can't open pipe %s for writing\n", pipe_filename);
      exit (-1);
      }
    }

  for (;;)
    {
    memcpy (&fdset, &fdset_base, sizeof (fdset));
    poll (fdset, npins, 3000);
  
    for (i = 0; i < npins; i++)
      {  
      if (fdset[i].revents & POLLPRI)
        {
        // For each pin, check for interrupt events
        int pin = pins[i];
        char buff[50];
	// In practice, I've never seen more than two bytes
	//   delivered per interrupt, however many
	//   switch bounces there are
        read (fdset[i].fd, buff, sizeof(buff));

	// If the discrepancy between start and now is too
	//   great, assume that the clock has been fiddled
	//   with
	// If you have a real-time-clock, this test can probably
	//   be removed
        if (abs (time (NULL) - start) > CLOCK_ERROR_SECONDS)
          {
          start = time (NULL);
          }
	else
          {
	  // Work how long it was in msec since the last
	  //   switch event
          struct timeval tv;
          gettimeofday (&tv, NULL);
          int msec = tv.tv_usec / 1000;
          int total_msec = (tv.tv_sec  - start) * 1000.0 + msec;
          //printf ("tick %d %d %d\n", pins[i], total_msec, state);
	  // The test for total > 1000 is to prevent spurious events
	  //   when the program first starts up
          if (total_msec - ticks[i] > bounce_time && total_msec > 1000)
            {
            // We need a small delay here. Even though the last interrupt
	    //   received should have been for the desired edge, in practice
	    //   it seems that we need to wait a little while for the 
	    //   sysfs state to settle. I am not sure whether the figure
	    //   I have chosen is universally applicable, or whether it
	    //   needs to be tweaked.
            usleep (2000);
            int state = get_pin_state (pin);
	    if ((state == 0 && (edge & EDGE_FALLING))
                 || (state == 1 && (edge & EDGE_RISING)))
              {
              if (pipe)
                {
                if (edge == (EDGE_FALLING | EDGE_RISING)) 
                  fprintf (pipe, "%d %d\n", pin, state);
		else
                  fprintf (pipe, "%d\n", pin);
                fflush (pipe);
                }
              else
                {
                if (edge == (EDGE_FALLING | EDGE_RISING)) 
                  printf ("%d %d\n", pin, state);
		else
                  printf ("%d\n", pin);
                }
              }
            ticks[i] = total_msec;
            }
	  }
        }
      }
    }

  // In reality, we never get here. The only way to stop the program is
  //  with a signal, in which case tidying up is dealt with by the
  //  signal handler
  for (i = 0; i < npins; i++)
    {
    close (fdset[i].fd);
    }
 
  if (pipe)
    fclose (pipe);

  if (!no_export)
    unexport_pins();

  return 0;
  }

