# pi-button-pipe

Version 0.0.2, February 2020

# What is this?

`pi-button-pipe` is a simple C program for the 
Raspberry Pi, and similar systems, to simplify the
detection of GPIO pin transitions, typically caused by button
presses. The program can monitor multiple pins, and uses selector
interrupts, rather than file polling, to improve responsiveness 
without generating significant CPU load. When a pin transition
is detected, the program writes the pin number and, optionally,
the new state of the pin to a named pipe (defaults to
`/tmp/pi-buttons`). A program that wants to be informed of
button presses simply reads from this named pipe.

The most difficult part of handling buttons is dealing with
contact bounce. This utility uses a simple timing strategy
to handle bounces, although some tuning might be required to
optimise the timing for specific types of button. Generally,
low-cost switches bounce more, and for longer, than premium
ones.

The data that will be supplied to the pipe is a sequence of
lines of plain text, one for each event. On 
each line is the number of
the GPIO pin that triggered the event. If -r or -f is 
specified, to set triggering on only rising or only falling
edge, nothing else is written. If the program is triggering
on both edges (the default), then the state -- 0 or 1 --
is written after the pin number.


## Building

Should be simple enough:

    $ make
    $ sudo make install

Of course, you'll need a C compiler to build the utility. If not, on
most Raspian-like installations you should be able to do:

    $ sudo apt-get install gcc

## Running

    $ sudo pi-button-pipe {pin1} {pin2}...

## Command-line options

`-b N` Bounce time in milliseconds. 

Values between 100 and 300 msec seem
to work well for most switches. With top-quality switches you might
get away with smaller values. The significance of this figure is
that, once a pin transition is detected, any transition within the
bounce time is treated as contact bounce, and ignored. If you
set the value too low, you might get multiple triggers. If too
high, some valid short key-presses might be missed.

`-d` Debug mode

Write pin transitions to stdout rather than a named pipe.

`-e` Export pins only

Just set the export status on the GPIO.

`-f` Falling edge only 

Only report high-to-low transitions.

`-n` No export/unexport

Don't export pins on startup, or unexport them on exit. This
option can be useful if there is some other application setting
up the GPIO.

`-r` Rising edge only

Only report low-to-high transitions.

`-u` Unexport pins only

Just remove the exported status on the GPIO.

## Notes

This release of `pi-button-pipe` defaults to 
triggering on both rising and
falling edges. This is deliberate change from the previous
release. Set set operation to be the same as the previous
release, using the `-r` switch.

`pi-button-pipe` uses the `sysfs` interface to the GPIO which,
as of 2020, is officially deprecated. However, it's likely to
be around for a while yet, and the alternatives are not very
appealing.

You'll need to run `pi-button-pipe` as `root`, or ensure that
the permissions on <code>/sys/class/gpio</code> are appropriate
for an unprivileged user. The consumer of events from the 
pipe need not be a privileged process (but see below).

The program will attempt to create the named pipe when it
starts, but the pipe might need to have different ownership or
permissions from those of `pi-button-pipe`. It might be better
to create the pipe separately, before starting this utility. 

## Author and legal

`pi-button-pipe` is maintained by Kevin Boone, and released under
the terms of the GNU Public Licence, v3.0. You may do whatever
you like with this software, so long as the original author
is acknowledged, and source code continues to be made available.
There is, of course, no warranty of any kind.
 
Please report bugs through GitHub.


