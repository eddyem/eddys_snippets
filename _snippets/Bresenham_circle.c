#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *drawCircle(int R, int geom){
	if(R > 200 || R < 1) return NULL;
	int S, i, Y = 2 * R + 2;
	if(geom)
		S = Y * (R + 1);
	else
		S = Y * (Y - 1);
	char *buf = malloc(S+1);
	if(!buf) return NULL;
	memset(buf, ' ', S);
	buf[S] = 0;
	for(i = Y-1; i < S; i+=Y) buf[i] = '\n';
	inline void DrawPixel(int x, int y){
		if(geom){
			if(y%2==0) buf[Y * y/2 + x] = '*';
		}else{
			buf[Y * y + x] = '*';
		}
	}
	// Bresenham's circle algorithm
	int x = R;
	int y = 0;
	int radiusError = 1-x;
	while(x >= y){
		DrawPixel(x + R,   y + R);
		DrawPixel(y + R,   x + R);
		DrawPixel(-x + R,  y + R);
		DrawPixel(-y + R,  x + R);
		DrawPixel(-x + R, -y + R);
		DrawPixel(-y + R, -x + R);
		DrawPixel(x + R,  -y + R);
		DrawPixel(y + R,  -x + R);
		y++;
		if (radiusError < 0){
			radiusError += 2 * y + 1;
		}else{
			x--;
			radiusError += 2 * (y - x) + 1;
		}
	}
	return buf;
}

int main(int argc, char **argv){
	int i, R;
	char *buf;
	for(i = 1; i < argc; i++){
		if(!(buf = drawCircle(R = atoi(argv[i]), 1))){
			printf("Wrong parameter %s\n", argv[i]);
			continue;
		}
		printf("\nCircle with R = %d:\n%s\n", R, buf);
		free(buf); buf = NULL;
	}
	return 0;
}
