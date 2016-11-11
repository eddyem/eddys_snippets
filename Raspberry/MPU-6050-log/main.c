#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <stdint.h>
#include <math.h>
#include <sys/time.h>

#include "MPU6050.h"

int fd, ctr=0;
uint8_t addrs[6] = {ACCEL_XOUT_H,ACCEL_YOUT_H,ACCEL_ZOUT_H, GYRO_XOUT_H,GYRO_YOUT_H,GYRO_ZOUT_H};
const double scales[6] = {19.6/32768., 19.6/32768., 19.6/32768., 250/32768., 250/32768., 250/32768.};
// threshold based on an accelerometer statistics for 30sec; gyro isn't mean
//const double thres[6] = {0.6, 0.6, 0.9, 5., 5., 5.};
const double thres[6] = {0.2, 0.2, 0.3, 1., 1., 1.};

#define RD8(reg)   wiringPiI2CReadReg8(fd, reg)
#define RD16(reg)  (RD8(reg)<<8|RD8(reg+1))
#define WR8(r,d)   do{wiringPiI2CWriteReg8(fd, r,d);}while(0)
#define WR16(r,d)  do{WR8(r,(d>>8)&0xff); WR8(r+1, d&0xff);}while(0)

double get_fta(int8_t at){
	if(!at) return 0.;
	return 4096.*0.34*pow((0.92/0.34), ((double)at-1.)/30.);
}

double get_ftg(int8_t gt){
	if(!gt) return 0.;
	return 25.*131.*pow(1.046, (double)gt - 1.);
}

double dtime(){
    double t;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    t = tv.tv_sec + ((double)tv.tv_usec)/1e6;
    return t;
}

void reset_brd(){
	fprintf(stderr, "RESET!\n");
    WR8(PWR_MGMT_1, 0x80); // device reset
    sleep(1);
	WR16(PWR_MGMT_1, 0);
	WR8(CONFIG, 3); // 3 == 44/42Hz, 6 == 5Hz
	WR8(SMPLRT_DIV, 9); // 1kHz / 10 = 100Hz
    WR8(GYRO_CONFIG, 0); WR8(ACCEL_CONFIG, 0); // turn off self-test, max precision
	usleep(100000);
}

void print_ag(){
	static int16_t oldvals[6];
	int16_t dat[6];
	static double doldvals[6] = {-1000.,-1000.,-1000.,-1000.,-1000.,-1000.};
	double vals[6];
	int i, ctr = 0;
	double t0 = dtime();
	while(!(RD8(INT_STATUS) & 1) && dtime() - t0 < 1.); // wait next data portion
	if(dtime() - t0 > 1.){
		reset_brd();
		return;
	}
	for(i = 0; i < 6; ++i){
		dat[i] = RD16(addrs[i]);
		if(oldvals[i] == dat[i]) ++ctr;
		else oldvals[i] = dat[i];
	}
	if(ctr == 6){ // hang
		reset_brd();
		return;
	}
	ctr = 0;
	for(i = 0; i < 6; ++i){
		vals[i] = ((double)dat[i]) * scales[i];
		if(fabs(vals[i] - doldvals[i]) > thres[i]){
			++ctr; doldvals[i] = vals[i];
		}
	}
	if(ctr){ // there was changes
		int16_t t = RD16(TEMP_OUT_H);
		printf("%.3f\t", dtime());
		for(i = 0; i < 6; ++i) printf("%.1f\t", vals[i]);
		printf("%.0f\n", t/340.+36.53);
	}
}

// get statistics for 30 seconds
void get_stat(){
	double t0 = dtime(), scds = 1.;
	double mean[7]={0.}, max[7], min[7], s2[7]={0.}, d, N = 0.;
	int i;
	int16_t r;
	for(i = 0; i < 6; ++i){ // fill initial values
		r = RD16(addrs[i]);
		max[i] = min[i] = r * scales[i];
	}
	r = RD16(TEMP_OUT_H);
	max[6] = min[6] = r/340.+36.53;
	printf("Wait for 30 seconds, please\n");
	do{
		double t1 = dtime();
		while(!(RD8(INT_STATUS) & 1) && dtime() - t1 < 1.); // wait next data portion
		if(dtime() - t1 > 1.){
			reset_brd();
			printf("ERROR!!! System hangs!\n");
			return;
		}
		for(i = 0; i < 6; ++i){
			r = RD16(addrs[i]);
			d = r * scales[i];
			mean[i] += d;
			s2[i] += d*d;
			if(max[i] < d) max[i] = d;
			if(min[i] > d) min[i] = d;
		}
		r = RD16(TEMP_OUT_H);
		d = r/340.+36.53;
		mean[6] += d; s2[6] += d*d;
		if(max[6] < d) max[6] = d;
		if(min[6] > d) min[6] = d;

		N += 1.;
		if(dtime() - t0 > scds){
			printf("\r%.0f ", scds);
			scds += 1.;
		}
	}while(dtime() - t0 < 30.);
	printf("\nAX, AY, AZ, GX, GY, GZ, T. [ min, max, mean, max-min, std ]\n");
	for(i = 0; i < 7; ++i){
		double m = mean[i]/N;
		printf("[ %.1f, %.1f, %.1f, %.1f, %.4f ] ", min[i], max[i], m, max[i]-min[i], sqrt(s2[i]/N - m*m));
	}
	printf("\n\n");
}

int main(int argc, char **argv){
//	int i;
    fd = wiringPiI2CSetup(MPU6050_I2C_ADDRESS);
    setbuf(stdout, NULL);
    if (fd == -1){
    	printf("I2C error\n");
        return 1;
    }
    int answ = RD8(WHO_AM_I_MPU6050);
    if(answ != MPU6050_I2C_ADDRESS){
    	printf("No MPU6050 detected\n");
    	return 2;
    }
    reset_brd();
//    printf("got first 32 regs:\n");
//    for(i = 0; i < 32; ++i) printf("R[%x]=%u ", i, RD8(i));
    (void) argv;
    if(argc == 2) get_stat();
    else{
    	printf("Unix time\tAX\tAY\tAZ\tGX\tGY\tGZ\tT\n");
	    while(1) print_ag();
    }
    return 0;
}
