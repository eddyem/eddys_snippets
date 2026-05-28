//Copyright (c) 2011 ashelly.myopenid.com under <http://www.opensource.org/licenses/mit-license>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

#define OMP_NUM_THREADS 4
#define Stringify(x) #x
#define OMP_FOR(x) _Pragma(Stringify(omp parallel for x))

//Customize for your data Item type
typedef int Item;
#define ItemLess(a,b) ((a)<(b))
#define ItemMean(a,b) (((a)+(b))/2)

typedef struct Mediator_t{
	Item* data; // circular queue of values
	int* pos;   // index into `heap` for each value
	int* heap;  // max/median/min heap holding indexes into `data`.
	int N;      // allocated size.
	int idx;    // position in circular queue
	int ct;     // count of items in queue
} Mediator;

/*--- Helper Functions ---*/

#define minCt(m) (((m)->ct-1)/2) //count of items in minheap
#define maxCt(m) (((m)->ct)/2) //count of items in maxheap

//returns 1 if heap[i] < heap[j]
inline int mmless(Mediator* m, int i, int j){
	return ItemLess(m->data[m->heap[i]],m->data[m->heap[j]]);
}

//swaps items i&j in heap, maintains indexes
inline int mmexchange(Mediator* m, int i, int j){
	int t = m->heap[i];
	m->heap[i] = m->heap[j];
	m->heap[j] = t;
	m->pos[m->heap[i]] = i;
	m->pos[m->heap[j]] = j;
	return 1;
}

//swaps items i&j if i<j; returns true if swapped
inline int mmCmpExch(Mediator* m, int i, int j){
	return (mmless(m,i,j) && mmexchange(m,i,j));
}

//maintains minheap property for all items below i/2.
void minSortDown(Mediator* m, int i){
	for(; i <= minCt(m); i*=2){
		if(i>1 && i < minCt(m) && mmless(m, i+1, i)) ++i;
		if(!mmCmpExch(m,i,i/2)) break;
	}
}

//maintains maxheap property for all items below i/2. (negative indexes)
void maxSortDown(Mediator* m, int i){
	for(; i >= -maxCt(m); i*=2){
		if(i<-1 && i > -maxCt(m) && mmless(m, i, i-1)) --i;
	if(!mmCmpExch(m,i/2,i)) break;
	}
}

//maintains minheap property for all items above i, including median
//returns true if median changed
int minSortUp(Mediator* m, int i){
	while (i > 0 && mmCmpExch(m, i, i/2)) i /= 2;
	return (i == 0);
}

//maintains maxheap property for all items above i, including median
//returns true if median changed
int maxSortUp(Mediator* m, int i){
	while (i < 0 && mmCmpExch(m, i/2, i)) i /= 2;
	return (i == 0);
}

/*--- Public Interface ---*/


//creates new Mediator: to calculate `nItems` running median.
//mallocs single block of memory, caller must free.
Mediator* MediatorNew(int nItems){
	int size = sizeof(Mediator) + nItems*(sizeof(Item)+sizeof(int)*2);
	Mediator* m = malloc(size);
	m->data = (Item*)(m + 1);
	m->pos = (int*) (m->data + nItems);
	m->heap = m->pos + nItems + (nItems / 2); //points to middle of storage.
	m->N = nItems;
	m->ct = m->idx = 0;
	while (nItems--){ //set up initial heap fill pattern: median,max,min,max,...
		m->pos[nItems] = ((nItems+1)/2) * ((nItems&1)? -1 : 1);
		m->heap[m->pos[nItems]] = nItems;
	}
	return m;
}


//Inserts item, maintains median in O(lg nItems)
void MediatorInsert(Mediator* m, Item v){
	int isNew=(m->ct<m->N);
	int p = m->pos[m->idx];
	Item old = m->data[m->idx];
	m->data[m->idx]=v;
	m->idx = (m->idx+1) % m->N;
	m->ct+=isNew;
	if(p>0){ //new item is in minHeap
		if (!isNew && ItemLess(old,v)) minSortDown(m,p*2);
		else if (minSortUp(m,p)) maxSortDown(m,-1);
	}else if (p<0){ //new item is in maxheap
		if (!isNew && ItemLess(v,old)) maxSortDown(m,p*2);
		else if (maxSortUp(m,p)) minSortDown(m, 1);
	}else{ //new item is at median
		if (maxCt(m)) maxSortDown(m,-1);
		if (minCt(m)) minSortDown(m, 1);
	}
}

//returns median item (or average of 2 when item count is even)
Item MediatorMedian(Mediator* m, Item *minval, Item *maxval){
	Item v= m->data[m->heap[0]];
	if ((m->ct&1) == 0) v = ItemMean(v,m->data[m->heap[-1]]);
	Item min, max;
	if(minval){
		min = v;
		int i;
		for(i = -maxCt(m); i < 0; ++i){
			int v = m->data[m->heap[i]];
			if(v < min) min = v;
		}
		*minval = min;
	}
	if(maxval){
		max = v;
		int i;
		for(i = 0; i <= minCt(m); ++i){
			int v = m->data[m->heap[i]];
			if(v > max) max = v;
		}
		*maxval = v;
	}
	return v;
}


/*--- Test Code ---*/
#include <stdio.h>
void PrintMaxHeap(Mediator* m){
	int i;
	if(maxCt(m))
	printf("Max: %3d",m->data[m->heap[-1]]);
	for (i=2;i<=maxCt(m);++i){
		printf("|%3d ",m->data[m->heap[-i]]);
		if(++i<=maxCt(m)) printf("%3d",m->data[m->heap[-i]]);
	}
	printf("\n");
}
void PrintMinHeap(Mediator* m){
	int i;
	if(minCt(m))
	printf("Min: %3d",m->data[m->heap[1]]);
	for (i=2;i<=minCt(m);++i){
		printf("|%3d ",m->data[m->heap[i]]);
		if(++i<=minCt(m)) printf("%3d",m->data[m->heap[i]]);
	}
	printf("\n");
}

void ShowTree(Mediator* m){
	PrintMaxHeap(m);
	printf("Mid: %3d\n",m->data[m->heap[0]]);
	PrintMinHeap(m);
	printf("\n");
}

double dtime(){
	double t;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	t = tv.tv_sec + ((double)tv.tv_usec)/1e6;
	return t;
}

#define SZ (4000)
int main(int argc, char* argv[])
{
	int siz = SZ * SZ;
	int i, *v = malloc(siz * sizeof(int)), *o = malloc(siz*sizeof(int)),
		*small = malloc(siz * sizeof(int)), *large = malloc(siz * sizeof(int));

for (i=0; i < siz; ++i){
	int s = (rand() & 0xf) + 0xf00, d = rand() & 0xffff;
	if(d > 0xefff) s+= 0xf000;
	else if(d < 0xfff) s = 0;
	v[i] = s;
}
double t0;
void printmas(int *arr){
	int  x, y, fst = SZ/2-5, lst = SZ/2+4;
	for(y = fst; y < lst; ++y){
		for(x = fst; x < lst; ++x)
			printf("%5d ", arr[x + y*SZ]);
		printf("\n");
	}
}
void printels(int hsz){
	int fulls = hsz*2+1;
	printf("median %dx%d for %dx%d image: %g seconds\n", fulls, fulls, SZ, SZ, dtime()-t0);
	printf("mid 10 values\n");
	printf("ori:\n");
	printmas(v);
	printf("\nmed:\n");
	printmas(o);
	printf("\nmin:\n");
	printmas(small);
	printf("\nmax:\n");
	printmas(large);
	printf("\n\n");
}

int hs;
for(hs = 1; hs < 5; ++hs){
	t0 = dtime();
	int blksz = hs*2+1, fullsz = blksz * blksz;
	OMP_FOR(shared(o, v, small, large))
	for(int x = hs; x < SZ - hs; ++x){
		int xx, yy, xm = x+hs+1, y;
		Mediator* m = MediatorNew(fullsz);
		// initial fill
		for(yy = 0; yy < blksz - 1; ++yy) for(xx = x-hs; xx < xm; ++xx)  MediatorInsert(m, v[xx+yy*SZ]);
		for(y = hs; y < SZ - hs; ++y){
			for(xx = x-hs; xx < xm; ++xx) MediatorInsert(m, v[xx+(y+hs)*SZ]);
			int curpos = x+y*SZ;
			o[curpos] = MediatorMedian(m, &small[curpos], &large[curpos]);
			//ShowTree(m);
			//if(y == 2) return 0;
		}
		free(m);
	}
	printels(hs);
}
free(o); free(v);
	free(small); free(large);
}
