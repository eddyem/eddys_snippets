/*
 * cclabling_1.h - inner part of functions cclabel4_1 and cclabel8_1
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
	size_t N = 0, // current label
		Ntot = 0, // number of found objects
		Nmax = W*H/4; // max number of labels
	size_t *labels = char2st(I, W, H, W_0);
	int w = W - 1;
printf("time for char2st: %gs\n", dtime()-t0);
	int y;
	size_t *assoc = Malloc(Nmax, sizeof(size_t)); // allocate memory for "remark" array
	inline void remark(size_t old, size_t *newv){
		size_t new = *newv;
		if(old == new || assoc[old] == new || assoc[new] == old){
			DBG("(%zd)\n", new);
			return;
		}
		DBG("[cur: %zx, tot: %zd], %zx -> %zx ", N, Ntot, old, new);
		if(!assoc[old]){ // try to remark non-marked value
			assoc[old] = new;
			DBG("\n");
			return;
		}
		// value was remarked -> we have to remark "new" to assoc[old]
		// and decrement all marks, that are greater than "new"
		size_t ao = assoc[old];
		DBG("(remarked to %zx) ", ao);
		DBG(" {old: %zd, new: %zd, a[o]=%zd, a[n]=%zd} ", old, new, assoc[old], assoc[new]);
		// now we must check assoc[old]: if it is < new -> change *newv, else remark assoc[old]
		if(ao < new)
			*newv = ao;
		else{
			size_t x = ao; ao = new; new = x; // swap values
		}
		int m = (old > new) ? old : new;
		int xx = m + W / 2;
		if(xx > Nmax) xx = Nmax;
		DBG(" [[xx=%d]] ", xx);
		OMP_FOR()
		for(int i = 1; i <= xx; i++){
			size_t m = assoc[i];
			if(m < new) continue;
			if(m == new){
				assoc[i] = ao;
				DBG("remark %x (%zd) to %zx ", i, m, ao);
			}else{
				assoc[i]--;
				DBG("decr %x: %zx, ", i, assoc[i]);
			}
		}
		DBG("\n");
		Ntot--;
	}
	t0 = dtime();
	size_t *ptr = labels;
	for(y = 0; y < H; y++){  // FIXME!!!! throw out that fucking "if" checking coordinates!!!
		bool found = false;
		size_t curmark = 0; // mark of pixel to the left
		for(int x = 0; x < W; x++, ptr++){
			size_t curval = *ptr;
			if(!curval){ found = false; continue;}
			size_t *U = (y) ? &ptr[-W] : NULL;
			size_t upmark = 0; // mark from line above
			if(!found){ // new pixel, check neighbours above
				found = true;
				// now check neighbours in upper string:
				if(U){
					//#ifdef LABEL_8
					if(x && U[-1]){ // there is something in upper left corner -> use its value
						upmark = U[-1];
					}else // check point above only if there's nothing in left up
					//#endif
						if(U[0]) upmark = U[0];
					//#ifdef LABEL_8
					if(x < w && U[1]){ // there's something in upper right
						if(upmark){ // to the left of it was pixels
							remark(U[1], &upmark);
						}else
							upmark = U[1];
					}
					//#endif
				}
				if(!upmark){ // there's nothing above - set current pixel to incremented counter
					#ifdef LABEL_8  // check, whether pixel is not single
					size_t *D = (y < w) ? &ptr[W] : NULL;
					if(  !(x && ((D && D[-1]) || ptr[-1]))   // no left neighbours
					  && !(x < w && ((D && D[1]) || ptr[1])) // no right neighbours
					  && !(D && D[0])){ // no neighbour down
						*ptr = 0; // erase this hermit!
						continue;
					}
					#endif
					upmark = ++N;
					Ntot++;
					assoc[upmark] = upmark; // refresh "assoc"
				}
				*ptr = upmark;
				curmark = upmark;
				//remark(curval, &upmark);
			}else{ // there was something to the left -> we must chek only U[1]
				if(U){
					if(x < w && U[1]){ // there's something in upper right
						remark(U[1], &curmark);
					}
				}
				*ptr = curmark;
			}
		}
	}
printf("time for step 2: %gs, found %zd objects\n", dtime()-t0, Ntot);
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

t0 = dtime();
	// Step 2: rename markers
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
