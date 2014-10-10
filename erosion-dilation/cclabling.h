/*
 * cclabling.h - inner part of functions cclabel4 and cclabel8
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
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

double t0 = dtime();
	CCbox *ret = NULL;
	size_t N _U_ = 0, Ncur = 0;
	size_t *labels = char2st(I, W, H, W_0);
	int y;
#ifdef EBUG
for(y = 0; y < H; y++){
	size_t *ptr = &labels[y*W];
	for(int x = 0; x < W; x++, ptr++){
		if(*ptr) printf("%02zx", *ptr);
		else printf("  ");
	}
	printf("\n");
}
FREE(labels); return ret;
#endif
printf("time for char2st: %gs\n", dtime()-t0);
t0 = dtime();
	// Step 1: mark neighbours by strings
	size_t *ptr = labels;
	for(y = 0; y < H; y++){
		bool found = false;
		for(int x = 0; x < W; x++, ptr++){
			if(!*ptr){ found = false; continue;}
			if(!found){
				found = true;
				++Ncur;
			}
			*ptr = Ncur;
		}
	}
printf("\n\ntime for step1: %gs (Ncur=%zd)\n", dtime()-t0, Ncur);
t0 = dtime();
	DBG("Initial mark\n");
	#ifdef EBUG
for(y = 0; y < H; y++){
	size_t *ptr = &labels[y*W];
	for(int x = 0; x < W; x++, ptr++){
		if(*ptr) printf("%02zx", *ptr);
		else printf("  ");
	}
	printf("\n");
}
	#endif

	// Step 2: fill associative array with remarking
	DBG("remarking\n");
	N = Ncur+1; // size of markers array (starting from 1)
	size_t i, *assoc = Malloc(N, sizeof(size_t));
	for(i = 0; i < N; i++) assoc[i] = i; // primary initialisation
	// now we should scan image again to rebuild enumeration
	// THIS PROCESS AVOID PARALLELISATION???
	Ncur = 0;
	size_t h = H-1
	#ifdef LABEL_8
		,w = W-1;
	#endif
	;
	inline void remark(size_t old, size_t *newv){
		size_t new = *newv;
		if(assoc[old] == new){
			DBG("(%zd)\n", new);
			return;
		}
		DBG("[%zx], %zx -> %zx ", Ncur, old, new);
		if(assoc[old] == old){ // remark non-remarked value
			assoc[old] = new;
			DBG("\n");
			return;
		}
		// value was remarked -> we have to remark "new" to assoc[old]
		// and decrement all marks, that are greater than "new"
		size_t ao = assoc[old];
		DBG("(remarked to %zx) ", assoc[old]);
		// now we must check assoc[old]: if it is < new -> change *newv, else remark assoc[old]
		if(ao < new)
			*newv = ao;
		else{
			size_t x = ao; ao = new; new = x; // swap values
		}
		int xx = old + W / 2;
		if(xx > N) xx = N;
		OMP_FOR()
		for(int i = 1; i <= xx; i++){
			size_t m = assoc[i];
			if(m < new) continue;
			if(m == new){
				assoc[i] = ao;
				DBG("remark %x (%zd) to %zx ", i, m, ao);
			}else if(m <= Ncur){
				assoc[i]--;
				DBG("decr %x: %zx, ", i, assoc[i]);
			}
		}
		DBG("\n");
		Ncur--;
	}
	for(y = 0; y < H; y++){  // FIXME!!!! throw out that fucking "if" checking coordinates!!!
		size_t *ptr = &labels[y*W];
		bool found = false;
		for(int x = 0; x < W; x++, ptr++){
			size_t curval = *ptr;
			if(!curval){ found = false; continue;}
			if(found) continue; // we go through remarked pixels
			found = true;
			DBG("curval: %zx ", curval);
			// now check neighbours in upper and lower string:
			size_t *U = (y) ? &ptr[-W] : NULL;
			size_t *D = (y < h) ? &ptr[W] : NULL;
			size_t upmark = 0; // mark from line above
			if(U){
				#ifdef LABEL_8
				if(x && U[-1]){ // there is something in upper left corner -> make remark by its value
					upmark = assoc[U[-1]];
				}else // check point above only if there's nothing in left up
				#endif
					if(U[0]) upmark = assoc[U[0]];
				#ifdef LABEL_8
				if(x < w) if(U[1]){ // there's something in upper right
					if(upmark){ // to the left of it was pixels
						remark(U[1], &upmark);
					}else
						upmark = assoc[U[1]];
				}
				#endif
			}
			if(!upmark){ // there's nothing above - set current pixel to incremented counter
				#ifdef LABEL_8  // check, whether pixel is not single
				if(  !(x && ((D && D[-1]) || ptr[-1]))   // no left neighbours
				  && !(x < w && ((D && D[1]) || ptr[1])) // no right neighbours
				  && !(D && D[0])){ // no neighbour down
					*ptr = 0; // erase this hermit!
					continue;
				}
				#endif
				upmark = ++Ncur;
			}

			// now remark DL and DR corners (bottom pixel will be checked on next line)
			if(D){
				#ifdef LABEL_8
				if(x && D[-1]){
					remark(D[-1], &upmark);
				}
				if(x < w && D[1]){
					remark(D[1], &upmark);
				}
				#else
				if(D[0]) remark(D[0], &upmark);
				#endif
			}
			 // remark current
			remark(curval, &upmark);
		}
	}
printf("time for step 2: %gs, found %zd objects\n", dtime()-t0, Ncur);
t0 = dtime();
	// Step 3: rename markers
	DBG("rename markers\n");
	// first correct complex assotiations in assoc
	OMP_FOR()
	for(y = 0; y < H; y++){
		size_t *ptr = &labels[y*W];
		for(int x = 0; x < W; x++, ptr++){
			size_t p = *ptr;
			if(!p){continue;}
			*ptr = assoc[p];
		}
	}
printf("time for step 3: %gs\n", dtime()-t0);
#ifdef EBUG
printf("\n\n");
for(y = 0; y < H; y++){
	size_t *ptr = &labels[y*W];
	for(int x = 0; x < W; x++, ptr++){
		if(*ptr) printf("%02zx", *ptr);
		else printf("  ");
	}
	printf("\n");
}
#endif
	FREE(assoc);
	FREE(labels);
