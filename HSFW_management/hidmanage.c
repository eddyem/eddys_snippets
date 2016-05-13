/*
 * hidmanage.c - manage HID devices
 *
 * Copyright 2016 Edward V. Emelianoff <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <libudev.h>
#include "usefull_macros.h"
#include "hidmanage.h"

/**
 * Find turrets present
 * @param wheels (o) - if not NULL - list of wheels found (like "ABC")
 * return amount of devices found
 *
 * WARNING! If there's more than one turret with wheels having same name
 *    access by wheel ID could lead undefined behaviour!
 */
int find_wheels(wheel_descr **wheels){
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	wheel_descr *founded = NULL;
	// Create the udev object
	udev = udev_new();
	int N = 0;
	if(!udev){
		ERR("Can't create udev");
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
			WARNX("Unable to find parent usb device for %s", devpath);
			udev_device_unref(dev);
			continue;
		}
		const char *vid, *pid;
		vid = udev_device_get_sysattr_value(dev,"idVendor");
		pid = udev_device_get_sysattr_value(dev, "idProduct");
		DBG("  VID/PID: %s/%s", vid, pid);
		if(strcmp(vid, W_VID) == 0 && strcmp(pid, W_PID) == 0){
			++N;
			if(!founded){
				founded = MALLOC(wheel_descr, 1);
			}else{
				founded = realloc(founded, sizeof(wheel_descr)*N);
				if(!founded) ERR("realloc");
			}
			wheel_descr *curdev = &founded[N-1];
			int fd = open(devpath, O_RDWR|O_NONBLOCK);
			if(fd < 0){
				/// "Не могу открыть %s"
				WARN(_("Can't open %s"), devpath);
				curdev->fd = -1;
			}else
				curdev->fd = fd;
			DBG("%s  %s",
					udev_device_get_sysattr_value(dev,"manufacturer"),
					udev_device_get_sysattr_value(dev,"product"));
			curdev->serial = strdup(udev_device_get_sysattr_value(dev, "serial"));
			DBG("serial: %s\n", curdev->serial);
		}
		udev_device_unref(dev);
	}
	// Free the enumerator object
	udev_enumerate_unref(enumerate);
	if(wheels){
		*wheels = founded;
	}else
		free(founded);
	return N;
}
