/*
 * hsfw.c - functions for work with wheels
 *
 * Copyright 2016 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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

#include <string.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>

#include "cmdlnopts.h"
#include "usefull_macros.h"
#include "hsfw.h"
#include "hidmanage.h"

// wheel Id & filter position given (if given by name - check names stored)
static int wheel_id = -1      // wheel ID given or found by name
	,filter_pos = -1          // filter position given or found by name
	,max_pos = -1             // max filter position allowed
	,wheel_fd = -1            // file descriptor of active wheel
	,HW_found = 0             // amount of wheels found
;

static wheel_descr *wheel_chosen = NULL; // pointer to chosen wheel

static wheel_descr *wheels = NULL; // array with descriptors of found devs

char *get_filter_name(wheel_descr *wheel, int pos);
void list_hw(int show);
void rename_hw();
int move_wheel();
void set_cur_wheel(int idx);
int writereg(int fd, uint8_t *buf, int l);
int readreg(int fd, uint8_t *buf, int reg, int l);
void check_and_clear_err(int fd);
void go_home(int fd);
void list_props(_U_ int verblevl, wheel_descr *wheel);
int poll_regstatus(int fd, int msg);

/**
 * set variables pointing current wheel to wheels[idx]
 */
void set_cur_wheel(int idx){
	wheel_chosen = &wheels[idx];
	wheel_id = wheel_chosen->ID;
	wheel_fd = wheel_chosen->fd;
	max_pos = wheel_chosen->maxpos;
}

/**
 * Check command line arguments, find filters/wheels by name, find max positions & so on
 */
void check_args(){
	// here we fill value of wheel_id if no given and present only one turret
	list_hw(listNms);  // also exit if no HW found
	if(listNms) return;
	int i;
	if(G.wheelID || G.filterId){
		char wID = (G.wheelID) ? *G.wheelID : *G.filterId;
		if((wID < 'A' || wID > POS_B_END) || (G.wheelID && strlen(G.wheelID) != 1)){
			/// "Идентификатор колеса должен быть буквой от \"A\" до \"H\"!"
			ERRX(_("Wheel ID should be a letter from \"A\" to \"H\"!"));
		}
		wheel_id = wID;
		DBG("wheel given by id: %c", wheel_id);
		for(i = 0; i < HW_found; ++i){
			if(wheels[i].ID == wheel_id){
				/// "Обнаружено более одного колеса с идентификатором '%c'!"
				if(wheel_fd > 0) ERRX(_("More than one wheel with ID '%c' found!"), wheel_id);
				set_cur_wheel(i);
			}
		}
	}
	char oldid = wheel_id;
	if(G.wheelName && (!reName || (G.filterPos || G.filterName))){ // find wheel by name given
		if(G.wheelID && !reName){
			/// "Заданы и идентификатор, и имя колеса; попробуйте что-то одно!"
			ERRX(_("You give both wheel ID and wheel name, try something one!"));
		}
		for(i = 0; i < HW_found; ++i){
			if(strcmp(wheels[i].name, G.wheelName) == 0){
				set_cur_wheel(i);
				break;
			}
		}
		if(reName) wheel_id = oldid;
	}
	void setWid(){
		if(oldid > 0) wheel_id = oldid;
		if(!G.wheelID){
			G.wheelID = calloc(2, 1);
			*G.wheelID = wheel_id;
		}
	}
	if(G.serial){ // HW given by its serial
		for(i = 0; i < HW_found; ++i){
			if(strcmp(wheels[i].serial, G.serial) == 0){
				set_cur_wheel(i);
				if(reName) setWid();
				break;
			}
		}
		if(i == HW_found) wheel_id = 0; // make an error message later
	}
	// if there's only one turret, fill wheel_id
	if(HW_found == 1 && (wheel_id < 0 || (wheel_fd < 0 && reName))){
		set_cur_wheel(0);
		if(reName) setWid();
	}
	if((wheel_fd < 0 || !wheel_chosen) && !G.filterName){
		/// "Заданное колесо не обнаружено!"
		ERRX(_("Given wheel not found!"));
	}
	if(showpos || setdef) return;
	if(G.filterId){ // filter given by its id like "B3"
		char *fid = G.filterId;
		/// "Идентификатор фильтра состоит из буквы (колесо) и цифры (позиция)"
		if(strlen(G.filterId) != 2 || fid[1] > '9' || fid[1] < '0')
			/// "Идентификатор фильтра - буква (колесо) и число (позиция)"
			ERRX(_("Filter ID is letter (wheel) and number (position)"));
		filter_pos = fid[1] - '0';
	}else if(G.filterPos){ // filter given by numerical position
		filter_pos = G.filterPos;
	}else if(G.filterName){ // filter given by name - search it
		int search_f(int N){
			int i, m = wheels[N].maxpos;
			for(i = 1; i <= m; ++i){
				DBG("Search filter %s in pos %d (%s)", G.filterName, i, get_filter_name(&wheels[N], i));
				if(strcmp(G.filterName, get_filter_name(&wheels[N], i)) == 0){
					filter_pos = i;
					if(!wheel_chosen){
						set_cur_wheel(N);
					}
					break;
				}
			}
			if(i > m) return 1;
			return 0;
		}
		int not_found = 1;
		if(wheel_chosen) not_found = search_f(wheel_id);
		else for(i = 0; i < HW_found && not_found; ++i){
			not_found = search_f(i);
		}
		if(not_found){
			/// "Фильтр %s не обнаружен"
			ERRX(_("Filter %s not found!"), G.filterName);
		}
	}else{
		if(!gohome) showpos = 1; // no action given - just show position
		return;
	}
	if(reName) max_pos = get_max_pos(wheel_id);
	if(filter_pos < 1 || filter_pos > max_pos){
		/// "Позиция фильтра должна быть числом от 1 до %d!"
		ERRX(_("Filter position should be a number from 1 to %d!"), max_pos);
	}
	DBG("filter given by position: %d", filter_pos);
}

/**
 * write buf of length l to register & check answer
 * return 0 if all OK
 */
int writereg(int fd, uint8_t *buf, int l){
	uint8_t reg = buf[0];
	//#if 0//
	#ifdef EBUG
	int i;
	printf("Write reg %d:", reg);
	for(i = 0; i < l; ++i) printf(" %02hhx", buf[i]);
	printf("\n");
	#endif
	if(ioctl(fd, HIDIOCSFEATURE(l), buf) < 0 || buf[0] != reg){
		/// "Ошибка отправки данных"
		WARNX(_("Error sending data"));
		return 1;
	}
	return 0;
}

/**
 * read register to buf & check answer
 * return 0 if all OK
 */
int readreg(int fd, uint8_t *buf, int reg, int l){
	memset(buf, 0, l);
	buf[0] = reg;
	if(ioctl(fd, HIDIOCGFEATURE(l), buf) < 0 || buf[0] != reg){
		/// "Ошибка чтения данных"
		WARNX(_("Error reading data"));
		return 1;
	}
	//#if 0//
	#ifdef EBUG
	int i;
	printf("Read reg %d:", reg);
	for(i = 0; i < l; ++i) printf(" %02hhx", buf[i]);
	printf("\n");
	#endif
	return 0;
}

/**
 * Check error state and clear it if need
 */
void check_and_clear_err(int fd){
	int i, stat = 1;
	uint8_t buf[REG_STATUS_LEN];
	for(i = 0; i < 10 && stat; ++i){
		stat = readreg(fd, buf, REG_STATUS, REG_STATUS_LEN);
		if(stat) usleep(100000);
	}
	/// "Ошибка, количество попыток истекло"
	if(i == 10) ERRX(_("Error, tries amount exceed"));
	if(buf[1] != 0xff){
		if(buf[5]){
			/// "Ошибка No %d, сбрасываю"
			//red(_("Error No %d, clear it"), buf[5]);
			stat = 1;
			for(i = 0; i < 10 && stat; ++i){
				memset(buf, 0, sizeof(buf));
				buf[0] = REG_CLERR;
				stat = writereg(fd, buf, REG_CLERR_LEN);
				usleep(100000);
				if(!stat) stat = readreg(fd, buf, REG_STATUS, REG_STATUS_LEN);
				if(!stat && buf[5]) stat = 1;
			}
		}
		readreg(fd, buf, REG_STATUS, REG_STATUS_LEN);
		if(buf[1] != 0xff){
			/// "Турель не инициализирована, движение в \"дом\""
			red(_("Turret isn't initialized, move home..."));
			go_home(fd);
			readreg(fd, buf, REG_STATUS, REG_STATUS_LEN);
		}
	}
}

/**
 * poll status register until moving stops
 * @param fd  - turret file descriptor
 * @param msg - ==1 to show message
 * @return current position
 */
int poll_regstatus(int fd, int msg){
	uint8_t buf[REG_STATUS_LEN];
	int i, stat = 1;
	/// "Ожидание окончания движения"
	if(msg) printf(_("Wait for end of moving "));
	for(i = 0; i < 300 && stat; ++i){
		stat = readreg(fd, buf, REG_STATUS, REG_STATUS_LEN);
		if(!stat){
			if(buf[2] == 0xff || buf[3] == 0xff) stat = 1;
		}
		if(buf[5]){
			if(msg) printf("\n");
			/// "Произошла ошибка, повторите запуск"
			ERRX(_("Error ocured, repeat again"));
		}
		usleep(50000);
		if(msg) printf("."); fflush(stdout);
	}
	if(msg) printf("\n");
	return buf[4];
}

/**
 * return statically allocated buffer with given filter name
 * @param wheel - given wheel file structure
 * @param pos   - filter position (starts from 1)
 */
char *get_filter_name(wheel_descr *wheel, int pos){
	static uint8_t buf[REG_NAME_LEN];
	int fd = wheel->fd;
	if(fd < 0) return NULL;
	if(pos < 1 || pos > wheel->maxpos){
		/// "Заданная позиция вне диапазона 1..%d"
		WARNX(_("Given position out of range 1..%d"), wheel->maxpos);
		return NULL;
	}
	memset(buf, 0, sizeof(buf));
	buf[0] = REG_NAME;
	buf[1] = FILTER_NAME;
	buf[2] = wheel->ID;
	buf[3] = pos;
	if(writereg(fd, buf, REG_NAME_LEN)) return NULL;
	if(readreg(fd, buf, REG_NAME, REG_NAME_LEN)) return NULL;
	if(buf[2]) return NULL;
	if(buf[6]){
		char *x = strchr((char*)&buf[6], ' ');
		if(x) *x = 0;
		return (char*) &buf[6];
	}
	else return NULL;
}

/**
 * list properties of wheels & fill remain fields of struct wheel_descr
 */
void list_props(_U_ int verblevl, wheel_descr *wheel){
	uint8_t buf[REG_NAME_LEN+1];
	int fd = wheel->fd;
	if(fd < 0){
		/// "Не могу открыть устройство"
		WARNX(_("Can't open device"));
		return;
	}
	check_and_clear_err(fd);
	// get status of wheel
	if(readreg(fd, buf, REG_INFO, REG_INFO_LEN)) return;
	wheel->ID = buf[5];
	wheel->maxpos = buf[4];
	DBG("Wheel with id '%c' and maxpos %d", wheel->ID, wheel->maxpos);
	char *getwname(int id){
		memset(buf, 0, sizeof(buf));
		buf[0] = REG_NAME;
		buf[1] = WHEEL_NAME;
		buf[2] = id;
		if(writereg(fd, buf, REG_NAME_LEN)) return NULL;
		if(readreg(fd, buf, REG_NAME, REG_NAME_LEN)) return NULL;
		if(buf[6]){
			char *x = strchr((char*)&buf[6], ' ');
			if(x) *x = 0;
			return (char*)&buf[6];
		}
		else return NULL;
	}
	if(verblevl == LIST_PRES || !verblevl){ // list only presented devices or not list
		char *nm;
		/// "\nСвойства подключенного колеса\n"
		if(verblevl) green(_("\nConnected wheel properties\n"));
		if(verblevl) printf("Wheel ID '%c'", wheel->ID);
		nm = getwname(wheel->ID);
		if(nm){
			strncpy(wheel->name, nm, 9);
			if(verblevl) printf(", name '%s'", wheel->name);
		}
		if(wheel->serial && verblevl){
			printf(", serial '%s'", wheel->serial);
		}
		if(verblevl) printf(", %d filters:\n", wheel->maxpos);
		else return;
		int i, m = wheel->maxpos + 1;
		// now get filter names
		for(i = 1; i < m; ++i){
			nm = get_filter_name(wheel, i);
			printf("\t%d", i);
			if(nm) printf(": '%s'", nm);
			printf("\n");
		}
		if(verblevl){
			printf("current position: %d\n", poll_regstatus(wheel->fd, 0));
		}
	}
	if(verblevl != LIST_ALL) return;
	// now list all things stored in memory of turret driver
	int w;
	/// "\nВсе записи EEPROM\n"
	green(_("\nAll records from EEPROM\n"));
	wheel_descr wl;
	wl.fd = fd;
	if(wheel->serial) printf("Turret with serial '%s'\n", wheel->serial);
	for(w = 'A'; w < 'I'; ++w){
		char *nm = getwname(w);
		int f;
		printf("Wheel ID '%c'", w);
		wl.maxpos = get_max_pos(w);
		if(nm) printf(", name '%s'", nm);
		printf(", %d filters:\n", wl.maxpos );
		wl.ID = w;
		for(f = 1; f <= wl.maxpos; ++f){
			nm = get_filter_name(&wl, f);
			if(!nm){
				check_and_clear_err(fd);
				memset(buf, 0, sizeof(buf));
				buf[0] = REG_NAME;
				writereg(fd, buf, REG_NAME_LEN);
				readreg(fd, buf, REG_NAME, REG_NAME_LEN);
				break;
			}
			printf("\t%d: '%s'\n", f, nm);
		}
	}
}

/**
 * Check hardware present
 * if 'show' != 0 show names found
 */
void list_hw(int show){
	int i;
	if(show) DBG("show");
	HW_found = find_wheels(&wheels);
	DBG("Found %d dev[s]", HW_found);
	if(HW_found == 0){
		/// "Турели не обнаружены"
		ERRX(_("No turrets found"));
	}
	for(i = 0; i < HW_found; ++i)
		if(wheels[i].fd > 0) break;
	if(i == HW_found){
		/// "Обнаружено %d турелей, но ни к одной нет прав доступа"
		ERRX(_("Found %d turrets but have no access rights to any"), HW_found);
	}
	// read other wheel properties & list if show!=0
	for(i = 0; i < HW_found; ++i){
		list_props(show, &wheels[i]);
	}
}

/**
 * Rename wheels or filters
 */
void rename_hw(){
	FNAME();
	check_and_clear_err(wheel_fd);
	char *newname = NULL;
	size_t L = 0;
	uint8_t buf[REG_NAME_LEN];
	void checknm(char *nm){
		newname = nm;
		/// "Название не должно превышать восьми символов"
		if((L = strlen(nm)) > 8) ERRX(_("Name should be not longer than 8 symbols"));
	}
	memset(buf, 0, sizeof(buf));
	uint8_t cmd = 0;
	// now check what user wants to rename
	if(setdef){
		DBG("Reset names to default");
		cmd = RESTORE_DEFVALS;
	}else if(G.filterName && (G.filterId || G.filterPos)){ // user wants to rename filter
		checknm(G.filterName);
		DBG("Rename filter %d to %s", filter_pos, newname);
		cmd = RENAME_FILTER;
	}else if(G.wheelName && (G.wheelID || G.filterId)){ // rename wheel
		checknm(G.wheelName);
		DBG("Rename wheel '%c' to %s", wheel_id, newname);
		cmd = RENAME_WHEEL;
	}
	if(!cmd){
		/// "Чтобы переименовать, необходимо указать новое название фильтра/колеса и его позицию/идентификатор!"
		ERRX(_("You should give new filter/wheel name and its POS/ID to rename!"));
	}

	int i, stat = 1;
	for(i = 0; i < 10 && stat; ++i){
		buf[0] = REG_NAME;
		buf[1] = cmd;
		if(cmd != RESTORE_DEFVALS){
			buf[2] = wheel_id;
			memcpy(&buf[4], newname, L);
		}
		if(cmd == RENAME_FILTER) buf[3] = filter_pos;
		if((stat = writereg(wheel_fd, buf, REG_NAME_LEN))) continue;
		if((stat = readreg(wheel_fd, buf, REG_NAME, REG_NAME_LEN))) continue;
		if(buf[2]){ // err not empty
			DBG("ER: %d", buf[2]);
			check_and_clear_err(wheel_fd);
			stat = 1; continue;
		}
		if(memcmp(&buf[6], newname, L)){
			DBG("Names not match!");
			stat = 1;
			continue;
		}
		break;
	}
	/// "Не удалось переименовать"
	if(i == 10) ERRX(_("Can't rename"));
	/// "Переименовано удачно!"
	green(_("Succesfully renamed!\n"));
}

/**
 * move wheel home
 */
void go_home(int fd){
	static int Nruns = 0;
	/// "Зацикливание, попробуйте еще раз"
	if(++Nruns > 10) ERRX(_("Cycling detected, try again"));
	DBG("Wheel go home");
	poll_regstatus(fd, 0); // wait for last moving
	uint8_t buf[REG_HOME_LEN];
	int i, stat = 1;
	for(i = 0; i < 10 && stat; ++i){
		memset(buf, 0, REG_HOME_LEN);
		buf[0] = REG_HOME;
		stat = writereg(fd, buf, REG_HOME_LEN);
		if(stat){usleep(100000); continue;}
		if((stat = readreg(fd, buf, REG_HOME, REG_HOME_LEN))) continue;
		if(buf[1] != 0xff){
			stat = 1; continue;
		}else{
			readreg(fd, buf, REG_HOME, REG_HOME_LEN);
			break;
		}
	}
	if(i == 10) exit(1);
	// now poll REG_STATUS
	poll_regstatus(fd, 1);
	check_and_clear_err(fd);
}

/**
 * move given wheel
 * @return 0 if all OK
 */
int move_wheel(){
	DBG("Move wheel %c to pos %d", wheel_id, filter_pos);
	if(wheel_fd < 0) return 1;
	if(filter_pos == poll_regstatus(wheel_fd, 0)){
		/// "Уже в заданной позиции"
		WARNX(_("Already at position"));
		return 0;
	}
	uint8_t buf[REG_GOTO_LEN];
	int i, stat = 1;
	for(i = 0; i < 10 && stat; ++i){
		DBG("i=%d",i);
		memset(buf, 0, REG_GOTO_LEN);
		buf[0] = REG_GOTO;
		buf[1] = filter_pos;
		stat = writereg(wheel_fd, buf, REG_GOTO_LEN);
		usleep(100000);
		if(stat) continue;
		if((stat = readreg(wheel_fd, buf, REG_GOTO, REG_GOTO_LEN))) continue;
		if(buf[1] != 0xff){
			stat = 1; continue;
		}else{
			readreg(wheel_fd, buf, REG_GOTO, REG_HOME_LEN);
			break;
		}
	}
	if(i == 10) return 1;
	poll_regstatus(wheel_fd, 1);
	check_and_clear_err(wheel_fd);
	return 0;
}

/**
 * process arguments given
 * @return 0 if all OK
 */
int process_args(){
	FNAME();
	if(wheel_id < 0) return 1;
	if(showpos){
		printf("%d\n", poll_regstatus(wheel_fd, 0));
		return 0;
	}
	if(gohome){
		go_home(wheel_fd);
		return 0;
	}
	if(reName || setdef){
		rename_hw();
		return 0;
	}
	if(filter_pos < 0) return 1;
	return move_wheel();
}

// check max position allowed for given filter id
int get_max_pos(char id){
	if(id >= 'A' && id <= POS_A_END) return ABS_MAX_POS_A;
	else if(id > POS_A_END && id <= POS_B_END) return ABS_MAX_POS_B;
	return 0;
}
