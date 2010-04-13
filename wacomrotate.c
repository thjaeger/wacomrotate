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
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#define WACOM_PROP_ROTATION "Wacom Rotation"

Display *dpy;

Rotation r = RR_Rotate_0;
int wacom_rotation;

int subpixel = 0;
int verbosity = 0;

int opcode;

void rotate_device(int dev) {
	Atom prop, type;
	int format;
	unsigned char* data;
	unsigned long nitems, bytes_after;
	XDevice fake_dev;
	fake_dev.device_id = dev;

	prop = XInternAtom(dpy, WACOM_PROP_ROTATION, True);
	if (!prop)
		return;

	XGetDeviceProperty(dpy, &fake_dev, prop, 0, 1000, False, AnyPropertyType,
			&type, &format, &nitems, &bytes_after, &data);

	if (nitems == 0 || format != 8)
		return;

	*data = wacom_rotation;
	XChangeDeviceProperty(dpy, &fake_dev, prop, type, format,
			PropModeReplace, data, nitems);
}

void rotate() {
	Rotation old_r = r;
	char buf[256];
	char *order;
	int wacom_rotation;

	XRRRotations(dpy, DefaultScreen(dpy), &r);
	if (old_r == r)
		return;
	switch (r) {
		case RR_Rotate_0:
			wacom_rotation = 0;
			order = "rgb";
			break;
		case RR_Rotate_90:
			wacom_rotation = 2;
			order = "vrgb";
			break;
		case RR_Rotate_180:
			wacom_rotation = 3;
			order = "bgr";
			break;
		case RR_Rotate_270:
			wacom_rotation = 1;
			order = "vbgr";
			break;
		default:
			fprintf(stderr, "Warning: XRRRotations() returned garbage\n");
			return;
	}
	if (verbosity >= 1)
		printf("New orientation: %d\n", wacom_rotation);

	int n;
	XIDeviceInfo *info = XIQueryDevice(dpy, XIAllDevices, &n);
	for (int i = 0; i < n; i++)
		rotate_device(info[i].deviceid);
	XIFreeDeviceInfo(info);

	if (!subpixel)
		return;
	snprintf(buf, sizeof(buf), "gconftool -t string -s /desktop/gnome/font_rendering/rgba_order %s", order);
	if (system(buf) == -1)
		fprintf(stderr, "Error: system() failed\n");
}

void usage(const char *me) {
	printf("Usage: %s [OPTION]...\n\n", me);
	printf("Options:\n");
	printf("  -s, --subpixel         Also adjust the orientation of subpixel smoothing\n");
	printf("  -v, --verbose          Increase verbosity\n");
	printf("  -h, --help             Display this help and exit\n");
}

void parse_opts(int argc, char *argv[]) {
	static struct option long_opts[] = {
		{"subpixel",0,0,'s'},
		{"verbose",0,0,'v'},
		{"help",0,0,'h'},
		{0,0,0,0}
	};

	char opt;
	while ((opt = getopt_long(argc, argv, "svh", long_opts, 0)) != -1) {
		switch (opt) {
			case 's':
				subpixel = 1;
				break;
			case 'v':
				verbosity++;
				break;
			case 'h':
			default:
				usage(argv[0]);
				exit(opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
		}
	}
}

void hierarchy_changed(XIHierarchyEvent *event) {
	for (int i = 0; i < event->num_info; i++) {
		XIHierarchyInfo *info = event->info + i;
		if (info->flags & XISlaveAdded)
			rotate_device(info->deviceid);
	}
}

void init_wacom() {
	int major = 2, minor = 0, event, error;
	if (!XQueryExtension(dpy, "XInputExtension", &opcode, &event, &error) ||
			XIQueryVersion(dpy, &major, &minor) == BadRequest ||
			major < 2) {
		printf("Error: This version of wacomrotate needs an XInput 2.0-aware X server.\n");
		exit(EXIT_FAILURE);
	}

	XIEventMask hier_mask;
	unsigned char data[2] = { 0, 0 };
	hier_mask.deviceid = XIAllDevices;
	hier_mask.mask = data;
	hier_mask.mask_len = sizeof(data);
	XISetMask(hier_mask.mask, XI_HierarchyChanged);
	XISelectEvents(dpy, DefaultRootWindow(dpy), &hier_mask, 1);
}

int main(int argc, char *argv[]) {
	int randr_event, randr_error;

	parse_opts(argc, argv);

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "Couldn't open display %s.\n", XDisplayName(NULL));
		exit(EXIT_FAILURE);
	}

	if (!XRRQueryExtension(dpy, &randr_event, &randr_error)) {
		fprintf(stderr, "RandR not available.\n");
		exit(EXIT_FAILURE);
	}
	XRRSelectInput(dpy, DefaultRootWindow(dpy), RRScreenChangeNotifyMask);

	init_wacom();
	rotate();

	while (1) {
		XEvent ev;
		XNextEvent(dpy, &ev);
		if (ev.type == randr_event) {
			XRRUpdateConfiguration(&ev);
			rotate();
		} else if (ev.type == GenericEvent &&
				ev.xcookie.extension == opcode &&
				XGetEventData(dpy, &ev.xcookie)) {
			XIDeviceEvent *device_event = (XIDeviceEvent *)ev.xcookie.data;
			if (device_event->evtype == XI_HierarchyChanged)
				hierarchy_changed((XIHierarchyEvent *)device_event);
			XFreeEventData(dpy, &ev.xcookie);
		}
	}
}
