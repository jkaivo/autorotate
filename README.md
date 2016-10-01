This is a program to run in the background and automatically rotate your X
screen for you. It has been developed on a ThinkPad Yoga 12.

It monitors IIO events via ACPI to watch for the screen being folded back, at
which point rotation is enabled. When the computer is in laptop mode, rotation
doesn't occur. It also honors pressing the screen orientation lock button on
the side of the Yoga.

It currently has hacks calling external programs to rotate the touchscreen
input and Wacom digitizer to match the orientation of the screen. A future
release will do these operations directly from the C code, removing the
external dependencies on xinput and xsetwacom.

This program requires libXrandr to compile to and run. On Debian, you can
install this with "sudo apt install -y libxrand-dev".

This is a very early version and is bound to have bugs. Bug reports are welcome
on GitHub.
