#!/bin/sh

### TODO: These should be automatically identified
touchpad='PS/2 Synaptics TouchPad'
touchdev='SYNAPTICS Synaptics Touch Digitizer V04'

# Global variables
CURRENT='normal'
GRAV=$(echo 7.0 / $(head -n1 /sys/bus/iio/devices/iio:device*/in_accel_scale) | bc)

rotatetouch() {
	# For some reason, xrandr causes the touch device to disappear briefly.
	# This loop waits for it to come back.
	while ! xinput --list | grep -q "$touchdev"; do :; done
	xinput set-float-prop "$touchdev" 'Coordinate Transformation Matrix' $*
}

rotatewacom() {
	### FIXME: the byte range (38-40) will probably break at some point
	for dev in $(xsetwacom --list devices | cut -b 38-40); do
		xsetwacom set "$dev" rotate $1
	done
}

check_orientation() {
	XRAW=$(head -n1 /sys/bus/iio/devices/iio:device*/in_accel_x_raw)
	YRAW=$(head -n1 /sys/bus/iio/devices/iio:device*/in_accel_y_raw)

	if [ $CURRENT != 'normal' ] && [ $YRAW -le -$GRAV ]; then
		CURRENT='normal'
		xrandr -o normal
		rotatetouch 1 0 0 0 1 0 0 0 1
		rotatewacom none
		### FIXME: There should be a separate script to enable/disable touchpad on folding events
		xinput enable "$touchpad"
	fi

	if [ $CURRENT != 'inverse' ] && [ $YRAW -ge $GRAV ]; then
		CURRENT='inverse'
		xrandr -o inverted
		rotatetouch -1 0 1 0 -1 1 0 0 1
		rotatewacom half
		xinput disable "$touchpad"
	fi

	if [ $CURRENT != 'left' ] && [ $XRAW -ge $GRAV ]; then
		CURRENT='left'
		xrandr -o left
		rotatetouch 0 -1 1 1 0 0 0 0 1
		rotatewacom ccw
		xinput disable "$touchpad"
	fi

	if [ $CURRENT != 'right' ] && [ $XRAW -le -$GRAV ]; then
		CURRENT='right'
		xrandr -o right
		rotatetouch 0 1 0 -1 0 1 0 0 1
		rotatewacom cw
		xinput disable "$touchpad"
	fi
}

while true; do
	check_orientation
	sleep 1
done
