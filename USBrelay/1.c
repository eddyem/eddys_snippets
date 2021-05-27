// SPDX-License-Identifier: GPL-2.0
/*
 * Hidraw Userspace Example
 *
 * Copyright (c) 2010 Alan Ott <alan@signal11.us>
 * Copyright (c) 2010 Signal 11 Software
 *
 * The code may be used by anyone for any purpose,
 * and can serve as a starting point for developing
 * applications using hidraw.
 */

/*
 * Copyright 2020 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <usefull_macros.h>

// chinese USB-HID relay management
// works only with one relay (the first found or pointed)

#define RELAY_VENDOR_ID       0x16c0
#define RELAY_PRODUCT_ID      0x05df
#define RELAY_VENDOR_ID_STR   "16c0"
#define RELAY_PRODUCT_ID_STR  "05df"

char *device = NULL; // device name
int help = 0; // call help
int quiet = 0; // no info messages
int **set = NULL; // turn ON given relays
int **reset = NULL; // turn OFF given relays
int fd = 0;  // device file descriptor

static myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&device),      _("serial device name")},
    {"set",     MULT_PAR,   NULL,   's',    arg_int,    APTR(&set),         _("turn ON relays with given number[s]")},
    {"reset",   MULT_PAR,   NULL,   'r',    arg_int,    APTR(&reset),       _("turn OFF relays with given number[s]")},
    {"quiet",   NO_ARGS,    NULL,   'q',    arg_int,    APTR(&quiet),       _("don't show info messages")},
   end_option
};

static void parse_args(int argc, char **argv){
    int i;
    char helpstring[1024];
    snprintf(helpstring, 1024, "Usage: %%s [args]\n\n\tWhere args are:\n");
    change_helpstring(helpstring);
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        WARNX("Wrong parameters:");
        for (i = 0; i < argc; i++)
            fprintf(stderr, "\t%s\n", argv[i]);
        showhelp(-1, cmdlnopts);
    }
}

#define CMD_ON      0xff
#define CMD_OFF     0xfd
#define CMD_SETSER  0xfa

/**
 * @brief relay_cmd - turn on/off given relay
 * @param cmd: CMD_ON/CMD_OFF
 * @param relaynmbr - 1..8
 */
static void relay_cmd(int cmd, int relaynmbr){
	char buf[9] = {0};
	// buf[0] = 0x00; /* Report Number */
	buf[1] = cmd; // ON - ff, OFF - fd; SET_SERIAL - 0xfa
	buf[2] = relaynmbr; // relay number or [2]..[6] - new serial
	int res = ioctl(fd, HIDIOCSFEATURE(9), buf);
	if(res < 0)
		ERR("HIDIOCSFEATURE");
	else if(res != 9) ERRX("Wrong return value: %d instead of 9", res);
}

#define INFO(...)   do{if(!quiet){green(__VA_ARGS__); printf("\n");}}while(0)

static int get_relay_fd(){
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    // Create the udev object
    udev = udev_new();
    int fd = -1;
    if(!udev){
        WARN("udev_new()");
        return -1;
    }
    // Create a list of the devices in the 'hidraw' subsystem.
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "hidraw");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    // Check out each device found
    udev_list_entry_foreach(dev_list_entry, devices){
        const char *path;
        struct udev_device *dev;
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);
        const char *devpath = udev_device_get_devnode(dev);
        DBG("Device Node Path: %s", devpath);
        dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
        if(!dev){
            udev_device_unref(dev);
            continue;
        }
        const char *vid, *pid;
        vid = udev_device_get_sysattr_value(dev,"idVendor");
        pid = udev_device_get_sysattr_value(dev, "idProduct");
        DBG("  VID/PID: %s/%s", vid, pid);
        if(strcmp(vid, RELAY_VENDOR_ID_STR) == 0 && strcmp(pid, RELAY_PRODUCT_ID_STR) == 0){
            fd = open(devpath, O_RDWR|O_NONBLOCK);
            DBG("get fd: %d", fd);
            if(fd < 0){
                udev_device_unref(dev);
                continue;
            }
            DBG("%s  %s",
                    udev_device_get_sysattr_value(dev,"manufacturer"),
                    udev_device_get_sysattr_value(dev,"product"));
            udev_device_unref(dev);
            break;
        }
    }
    // Free the enumerator object
    udev_enumerate_unref(enumerate);
    return fd;
}

int main(int argc, char **argv){
	int res;
	char buf[256];
	struct hidraw_report_descriptor rpt_desc;
	struct hidraw_devinfo info;

	initial_setup();
	parse_args(argc, argv);
	if(!device){
		fd = get_relay_fd();
	}else  fd = open(device, O_RDWR|O_NONBLOCK);
	if(fd < 0){
		WARNX("Unable to open device");
		return 1;
	}

	memset(&rpt_desc, 0x0, sizeof(rpt_desc));
	memset(&info, 0x0, sizeof(info));
	memset(buf, 0x0, sizeof(buf));
	/* Get Raw Name */
	res = ioctl(fd, HIDIOCGRAWNAME(256), buf);
	if(res < 0)
		ERR("HIDIOCGRAWNAME");
	else
		INFO("Raw Name: %s\n", buf);
	int L = strlen(buf), wrong = 0;
	char nr = '0';
	if(L < 9) wrong = 1;
	else{
		if(strncmp(buf+L-9, "USBRelay", 8)) wrong = 1;
		nr = buf[L-1];
		if(nr < '0' || nr > '8') wrong = 1;
	}
	/* Get Raw Info */
	res = ioctl(fd, HIDIOCGRAWINFO, &info);
	if(res < 0){
		ERR("HIDIOCGRAWINFO");
	}else{
		if(info.vendor != RELAY_VENDOR_ID || info.product != RELAY_PRODUCT_ID) wrong = 1;
	}
	if(wrong){
		ERRX("Wrong relay raw name: %s (VID=0x%04X, PID=0x%04X)", buf, info.vendor, info.product);
	}
	int Nrelays = nr - '0';
	INFO("N relays = %d", Nrelays);

	if(set || reset){
		if(set) do{
			int On = **set;
			if(On < 0 || On > Nrelays) WARNX("Wrong relay number: %d", On);
			else{
				INFO("SET: %d\n", On);
				relay_cmd(CMD_ON, On);
			}
			++set;
		}while(*set);
		if(reset) do{
			int Off = **reset;
			if(Off < 0 || Off > Nrelays) WARNX("Wrong relay number: %d", Off);
			else{
				INFO("RESET: %d\n", Off);
				relay_cmd(CMD_OFF, Off);
			}
			++reset;
		}while(*reset);
	}

	/* Get Feature */
	buf[0] = 0x01; /* Report Number */
	memset(buf+1, 0, 8);
	res = ioctl(fd, HIDIOCGFEATURE(9), buf);
	if(res < 0){
		ERR("HIDIOCGFEATURE");
	}else{
		if(res != 9) ERRX("Bad answer");
		// report:
		// bytes 0..4 - serial (e.g. HURTM)
		// byte 7 - state (each bit of state ==1 - ON, ==0 - off)
		buf[5] = 0;
		INFO("Relay serial: %s", buf);
		for(int i = 0; i < Nrelays; ++i){
			INFO("Relay%d=%d", i, buf[7]&(1<<i)?1:0);
		}
	}

	close(fd);
	return 0;
}



