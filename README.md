This is a program to run in the background and automatically rotate your X
screen for you. It has been developed on a ThinkPad Yoga 12.

It monitors IIO events via ACPI to watch for the screen being folded back, at
which point rotation is enabled. When the computer is in laptop mode, rotation
doesn't occur. It also honors pressing the screen orientation lock button on
the side of the Yoga.

It currently has a hack calling xinput to rotate the touchscreen input to match
the screen orientation. A future release will do this operations directly from
the C code, removing the external dependencies on xinput.

This program requires libXrandr and libXi to compile to and run. On Debian,
you can install this with "sudo apt install -y libxrand-dev libxi-dev".

This is a very early version and is bound to have bugs. Bug reports are welcome
on GitHub.
