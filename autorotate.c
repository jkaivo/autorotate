#define _XOPEN_SOURCE 700
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ACPID_SOCK_PATH "/var/run/acpid.socket"

/* once to go into tablet mode, twice to come out */
#define ACPI_ROTATE	"ibm/hotkey LEN0068:00 00000080 000060c0\n"

/* once on keydown, once on keyup */
#define ACPI_ROTATELOCK	"ibm/hotkey LEN0068:00 00000080 00006020\n"

#define FIXME_HARDCODED_DEV_NUMBER 4

#define GRAVITY_CUTOFF	(7.0)
#define DEV_PATH	"/sys/bus/iio/devices/iio:device%d"
#define SCALE_FILE	"in_accel_scale"
#define X_RAW_FILE	"in_accel_x_raw"
#define Y_RAW_FILE	"in_accel_y_raw"

int tabletmode = 0;
int rotatelock = 0;
enum { NORMAL, LEFT, INVERTED, RIGHT } rotation = NORMAL;

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
			printf("Entering tablet mode\n");
			tabletmode = 1;
		} else if (nrotate == 3) {
			printf("Leaving tablet mode\n");
			tabletmode = 0;
			nrotate = 0;
		}
	} else if (!strcmp(buf, ACPI_ROTATELOCK)) {
		nlock++;
		if (nlock == 2) {
			nlock = 0;
			rotatelock = ! rotatelock;
			printf("Set rotate lock to %d\n", rotatelock);
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

	printf("Grativational cutoff is %lf\n", gravity);
	int acpi = openacpi(NULL);
	if (acpi == -1) {
		tabletmode = 1;
		rotatelock = 0;
		rotation = NORMAL;
	}

	for (;;) {
		int nfds = 0;
		fd_set fds = acpi + 1;
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
			printf("X: %lg; Y: %lg\n", x, y);
		} else {
			printf("Not rotating because tabletmode is %d and rotatelock is %d\n", tabletmode, rotatelock);
		}
	}

	return 1;
}
