#include <stdio.h>
#include <string.h>
#include <usb.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <errno.h>
#include <stdlib.h>

#define ERR(...)  fprintf(stderr, __VA_ARGS__)
#define DBG(...)  

int main(int argc, char **argv){
	int pid, vid;
	int fd, rc;
	char buf[256], *d = NULL, *f = NULL, *eptr;
	struct usb_bus *bus;
	struct usb_device *dev;
	int found = 0;
	if(argc != 2 || !(d = strchr(argv[1], ':'))){
		printf("Usage: %s vid:pid\n", argv[0]);
		return 1;
	}
	vid = (int)strtol(argv[1], &eptr, 16);
	pid = (int)strtol(d+1, &eptr, 16);
	printf("pid: %x, vid: %x\n", pid, vid);
	d = NULL;
	usb_init();
	usb_find_busses();
	usb_find_devices();
	for(bus = usb_busses; bus && !found; bus = bus->next) {
		for(dev = bus->devices; dev && !found; dev = dev->next) {
			if (dev->descriptor.idVendor == vid && dev->descriptor.idProduct == pid){
				found = 1;
				d = bus->dirname;
				f = dev->filename;
			}
		}
	}
	if(!found){
		// "Устройство не найдено"
		ERR("Device not found");
		return 1;
	}
	snprintf(buf, 255, "/dev/bus/usb/%s/%s", d,f);
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		// "Не могу открыть файл устройства %s: %s"
		ERR("Can't open device file %s: %s", buf, strerror(errno));
		return 1;
	}
	printf("Resetting USB device %s", buf);
	rc = ioctl(fd, USBDEVFS_RESET, 0);
	if (rc < 0) {
		// "Не могу вызывать ioctl"
		perror("Error in ioctl");
		return 1;
	}
	close(fd);
	return 0;
}

