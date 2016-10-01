#define _XOPEN_SOURCE 700
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <X11/extensions/Xrandr.h>

#define ACPID_SOCK_PATH "/var/run/acpid.socket"

/* once to go into tablet mode, twice to come out */
#define ACPI_ROTATE	"ibm/hotkey LEN0068:00 00000080 000060c0\n"

/* once on keydown, once on keyup */
#define ACPI_ROTATELOCK	"ibm/hotkey LEN0068:00 00000080 00006020\n"

#define FIXME_HARDCODED_DEV_NUMBER 4
#define TOUCH_DEVICE	"SYNAPTICS Synaptics Touch Digitizer V04"

#define GRAVITY_CUTOFF	(7.0)
#define DEV_PATH	"/sys/bus/iio/devices/iio:device%d"
#define SCALE_FILE	"in_accel_scale"
#define X_RAW_FILE	"in_accel_x_raw"
#define Y_RAW_FILE	"in_accel_y_raw"

int tabletmode = 0;
int rotatelock = 0;
enum rotation { NORMAL, INVERSE, LEFT, RIGHT };

void rotatescreen(enum rotation r)
{
	Rotation xr[] = { RR_Rotate_0, RR_Rotate_180, RR_Rotate_90, RR_Rotate_270 };
	static Display *dpy = NULL;
	if (dpy == NULL) {
		dpy = XOpenDisplay(NULL);
	}
	static Window root = 0;
	if (root == 0) {
		root = RootWindow(dpy, DefaultScreen(dpy));
	}
	XRRScreenConfiguration *xsc = XRRGetScreenInfo(dpy, root);
	XRRSetScreenConfig(dpy, xsc, root, 0, xr[r], CurrentTime);
}

void rotatetouch(enum rotation r)
{
	char *matrix[] = {
		"1 0 0 0 1 0 0 0 1",
		"-1 0 1 0 -1 1 0 0 1",
		"0 -1 1 1 0 0 0 0 1",
		"0 1 0 -1 0 1 0 0 1",
	};
	char cmd[256];
	sprintf(cmd, "xinput set-float-prop '%s' 'Coordinate Transformation Matrix' %s", TOUCH_DEVICE, matrix[r]);
	system(cmd);
}

enum rotation setrotation(enum rotation r)
{
	static enum rotation prev = NORMAL;
	if (r == prev) {
		return r;
	}

	rotatescreen(r);
	sleep(1);
	rotatetouch(r);

	prev = r;
	return r;
}

int openacpi(const char *path)
{
	struct sockaddr_un sun = {0};
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		return -1;
	}

	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, path ? path : ACPID_SOCK_PATH);
	if (connect(sock, (const struct sockaddr*)&sun, sizeof(sun)) == -1) {
		close(sock);
		return -1;
	}

	return sock;
}

void checkacpi(int acpi)
{
	static int nrotate = 0;
	static int nlock = 0;

	char buf[BUFSIZ];
	ssize_t n = read(acpi, buf, sizeof(buf));
	if (n <= 0) {
		return;
	}

	if (!strcmp(buf, ACPI_ROTATE)) {
		nrotate++;
		if (nrotate == 1) {
			tabletmode = 1;
			/* disable touchpad */
			/* disable trackpoint */
		} else if (nrotate == 3) {
			/* enable touchpad */
			/* enable trackpoint */
			setrotation(NORMAL);
			tabletmode = 0;
			nrotate = 0;
		}
	} else if (!strcmp(buf, ACPI_ROTATELOCK)) {
		nlock++;
		if (nlock == 2) {
			nlock = 0;
			rotatelock = ! rotatelock;
		}
	}
}

FILE *opendevfile(const char *file)
{
	static char devdir[sizeof(DEV_PATH) + 5] = { '\0' };
	char *path = NULL;
	if (devdir[0] == '\0') {
		sprintf(devdir, DEV_PATH, FIXME_HARDCODED_DEV_NUMBER);
	}
	path = malloc(strlen(devdir) + strlen(file) + 4);
	sprintf(path, "%s/%s", devdir, file);
	FILE *dev = fopen(path, "r");
	free(path);
	return dev;
}


double getgravity(double cutoff)
{
	FILE *scale = opendevfile(SCALE_FILE);

	if (scale == NULL) {
		return cutoff;
	}

	double scalefactor;
	fscanf(scale, "%lf", &scalefactor);
	fclose(scale);

	return cutoff / scalefactor;
}

double getraw(const char *file)
{
	double value = 0.0;
	FILE *raw = opendevfile(file);
	if (raw) {
		fscanf(raw, "%lg", &value);
		fclose(raw);
	}
	return value;
}

int main(void)
{
	double gravity = getgravity(GRAVITY_CUTOFF);

	int acpi = openacpi(NULL);
	if (acpi == -1) {
		tabletmode = 1;
		rotatelock = 0;
	}

	for (;;) {
		int nfds = acpi + 1;
		fd_set fds;
		FD_ZERO(&fds);
		struct timeval to = {0};
		to.tv_usec = 5000;
		int forever = 0;

		if (acpi != -1) {
			FD_SET(acpi, &fds);
			if (tabletmode == 0 || rotatelock == 1) {
				forever = 1;
			}
		}

		select(nfds, &fds, NULL, NULL, forever ? NULL : &to);

		if (acpi != -1 && FD_ISSET(acpi, &fds)) {
			checkacpi(acpi);
		}

		if (tabletmode == 1 && rotatelock == 0) {
			double x = getraw(X_RAW_FILE);
			double y = getraw(Y_RAW_FILE);
			if (y <= -gravity) {
				setrotation(NORMAL);
			} else if (y >= gravity) {
				setrotation(INVERSE);
			} else if (x >= gravity) {
				setrotation(LEFT);
			} else if (x <= -gravity) {
				setrotation(RIGHT);
			}
		}
	}

	return 1;
}
