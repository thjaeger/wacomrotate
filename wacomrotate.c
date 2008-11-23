#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

Display *dpy;
Rotation r = 0;
int subpixel = 0;

void rotate() {
	Rotation old_r = r;
	char buf[256];
	char *r_name, *order;

	XRRRotations(dpy, DefaultScreen(dpy), &r);
	if (old_r == r)
		return;
	switch (r) {
		case RR_Rotate_0:
			r_name = "none";
			order = "rgb";
			break;
		case RR_Rotate_90:
			r_name = "ccw";
			order = "vrgb";
			break;
		case RR_Rotate_180:
			r_name = "half";
			order = "bgr";
			break;
		case RR_Rotate_270:
			r_name = "cw";
			order = "vbgr";
			break;
		default:
			return;
	}
	snprintf(buf, sizeof(buf), "xsetwacom set stylus rotate %s", r_name);
	if (system(buf) == -1)
		fprintf(stderr, "Error: system() failed\n");
	if (!subpixel)
		return;
	snprintf(buf, sizeof(buf), "gconftool -t string -s /desktop/gnome/font_rendering/rgba_order %s", order);
	if (system(buf) == -1)
		fprintf(stderr, "Error: system() failed\n");
}

int check_wacom() {
	char buf[10];
	int n = readlink("/dev/input/wacom", buf, 9);
	if (n < 0)
		return 0;
	buf[n] = '\0';
	return !strcmp("/dev/ttyS", buf);
}

void usage(const char *me) {
	printf("Usage: %s [OPTION]...\n\n", me);
	printf("Options:\n");
	printf("  -s, --subpixel         Directory for config files\n");
	printf("  -h, --help             Display this help and exit\n");
}

void parse_opts(int argc, char *argv[]) {
	static struct option long_opts[] = {
		{"help",0,0,'h'},
		{"subpixel",0,0,'s'},
		{0,0,0,0}
	};

	char opt;
	while ((opt = getopt_long(argc, argv, "sh", long_opts, 0)) != -1) {
		switch (opt) {
			case 's':
				subpixel = 1;
				break;
			case 'h':
			default:
				usage(argv[0]);
				exit(opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
		}
	}
}

int main(int argc, char *argv[]) {
	XEvent ev;
	int event_basep;
	int error_basep;

	parse_opts(argc, argv);

	if (!check_wacom()) {
		fprintf(stderr, "Not a Tablet PC, exiting...\n");
		exit(EXIT_SUCCESS);
	}

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "Couldn't open display %s.\n", XDisplayName(NULL));
		exit(EXIT_FAILURE);
	}

	if (!XRRQueryExtension(dpy, &event_basep, &error_basep)) {
		fprintf(stderr, "RandR not available.\n");
		exit(EXIT_FAILURE);
	}

	XRRSelectInput(dpy, DefaultRootWindow(dpy), RRScreenChangeNotifyMask);

	while (1) {
		XNextEvent(dpy, &ev);
		if (ev.type == event_basep) {
			XRRUpdateConfiguration(&ev);
			rotate();
		}
	}
}
