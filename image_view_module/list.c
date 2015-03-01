/*
 * simple_list.c - simple one-direction list
 *
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

#include "list.h"
#include "macros.h"

typedef struct list_{
	windowData *data;
	struct list_ *next;
	struct list_ *prev;
} WinList;

static WinList *root = NULL;

/**
 * add element v to list with root root (also this can be last element)
 * @param root (io) - pointer to root (or last element) of list or NULL
 * 						if *root == NULL, just created node will be placed there
 * 						IDs in list are sorted, so if root's ID isn't 0, it will be moved
 * @param w         - data inserted (with empty ID - identificator will be assigned here)
 * @return pointer to data inside created node or NULL
 */
windowData *addWindow(windowData *w){
	WinList *node, *last, *curr;
	int freeID = 1, curID;
	if((node = MALLOC(WinList, 1)) == 0)  return NULL; // allocation error
	node->data = w; // insert data
	if(root){ // there was root node - search last
		last = root;
		if(last->data->ID != 0){ // root element have non-zero ID
			node->next = last;
			root->prev = node;
			root = NULL;
		}else{ // root element have ID==0, search first free ID
			while((curr = last->next)){
				if((curID = curr->data->ID) > freeID) // we found a hole!
					break;
				else
					freeID = curID + 1;
				last = last->next;
			}
			last->next = node; // insert pointer to new node into last element in list
			node->prev = last; // don't forget a pointer to previous element
			node->next = curr; // next item after a hole or NULL if there isn't holes
			w->ID = freeID;
		}
	}
	if(!root){ // we need to change root to this element; (*root=NULL could be done upper)
		root = node;
		w->ID = 0;
	}
	DBG("added window with id = %d", w->ID);
	return w;
}

/**
 * search window with given inner identificator winID
 * @return pointer to window struct or NULL if not found
 */
WinList *searchWindowList(int winID){
	WinList *node = NULL, *next = root;
	int curID;
	if(!root){
		DBG("no root leaf");
		return NULL;
	}
	do{
		node = next;
		next = node->next;
		curID = node->data->ID;
	}while(curID < winID && next);
	if(curID != winID) return NULL;
	return node;
}
/**
 * the same as upper but for outern usage
 */
windowData *searchWindow(int winID){
	WinList *node = searchWindowList(winID);
	if(!node) return NULL;
	return node->data;
}

/**
 * search window with given OpenGL identificator GL_ID
 * @return pointer to window struct or NULL if not found
 */
windowData *searchWindow_byGLID(int GL_ID){
	WinList *node = NULL, *next = root;
	int curID;
	if(!root) return NULL;
	do{
		node = next;
		next = node->next;
		curID = node->data->GL_ID;
	}while(curID != GL_ID && next);
	if(curID != GL_ID) return NULL;
	return node->data;
}

/**
 * free() all data for node of list
 * !!! data for raw pixels (win->image) will be removed only if image->protected == 0
 * 			only for initialisation and free() by user !!!
 */
void WinList_freeNode(WinList **node){
	if(!node || !*node) return;
	WinList *cur = *node, *prev = cur->prev, *next = cur->next;
	windowData *win = cur->data;
	if(root == cur) root = next;
	FREE(win->title);
	if(win->image->protected == 0){
		FREE(win->image->rawdata);
		FREE(win->image);
	}
	pthread_mutex_destroy(&win->mutex);
	FREE(*node);
	if(prev)
		prev->next = next;
	if(next)
		next->prev = prev;
}

/**
 * remove window with ID winID
 * @return 0 in case of error !0 if OK
 */
int removeWindow(int winID){
	WinList *node = searchWindowList(winID);
	if(!node){
		DBG("Not found");
		return 0;
	}
	DBG("removing win ID=%d", winID);
	WinList_freeNode(&node);
	return 1;
}

/**
 * remove all nodes in list
 */
void freeWinList(){
	WinList *node = root, *next;
	if(!root) return;
	do{
		next = node->next;
		WinList_freeNode(&node);
		node = next;
	}while(node);
	root = NULL;
}

/**
 * run function for each window in list
 */
void forEachWindow(void (*fn)(int GL_ID)){
	WinList *node = root;
	if(!root) return;
	do{
		int id = node->data->GL_ID;
		if(id) fn(id);
		node = node->next;
	}while(node);
}

