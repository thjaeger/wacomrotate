/*
 * Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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
int force = 0;
const char *device_name[] = { "stylus", "touch" };
int device_n = 1;

void rotate() {
	int i;
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
	for (i = 0; i < device_n; i++) {
		snprintf(buf, sizeof(buf), "xsetwacom set %s rotate %s", device_name[i], r_name);
		if (system(buf) == -1)
			fprintf(stderr, "Error: system() failed\n");
	}
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
	printf("  -s, --subpixel         Also adjust the orientation of subpixel smoothing\n");
	printf("  -f, --force            Rotate wacom input even if Tablet PC is not detected\n");
	printf("  -t, --touch            Also issue xsetwacom command for a 'touch' device");
	printf("  -h, --help             Display this help and exit\n");
}

void parse_opts(int argc, char *argv[]) {
	static struct option long_opts[] = {
		{"help",0,0,'h'},
		{"subpixel",0,0,'s'},
		{"force",0,0,'f'},
		{"touch",0,0,'t'},
		{0,0,0,0}
	};

	char opt;
	while ((opt = getopt_long(argc, argv, "shft", long_opts, 0)) != -1) {
		switch (opt) {
			case 's':
				subpixel = 1;
				break;
			case 'f':
				force = 1;
				break;
			case 't':
				device_n = 2;
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

	if (!force && !check_wacom()) {
		fprintf(stderr, "Not a Tablet PC, exiting... (use -f to overwrite)\n");
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
