/*
 * json.c - simple JSON parser
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

#include "usefull_macros.h"
#include "json.h"

/**
 * don't understand hex and octal numbers
 * don't understand mixed arrays
 */

static json_object *json_collect(char *data);

static char *skipspaces(char *data){
	while(*data){
		char ch = *data;
		switch (ch){
			case '\r': case '\n': case '\t': case ' ':
				++data;
			break;
			default:
				goto ret;
		}
	}
ret:
	return data;
}

static json_object *json_ini(){
	json_object *ret = MALLOC(json_object, 1);
	ret->objs = MALLOC(json_pair, JSON_BLKSZ);
	ret->max_len = JSON_BLKSZ; ret->len = 0;
	return ret;
}

void json_free_obj(json_object **obj){
	FREE((*obj)->objs);
	FREE((*obj)->original_data);
	FREE(*obj);
}

/**
 * find end of compound object & set last brace to zero
 * @return first symbol after object's end
 */
static char *find_obj_end(char *data){
	int opening = 0;
	while(*data && opening != -1){
		switch(*data++){
			case '{':
				++opening;
			break;
			case '}':
				--opening;
			break;
			default: break;
		}
	}
	if(opening != -1) return NULL;
	data[-1] = 0;
	return data;
}

/**
 * count objects in array
 * @return first symbol after array's end
 */
static char *count_objs(json_pair *pair){
	int a_closing = 0, o_opening = 0, commas = 0, objects = 0, valstart = 1, values = 0; // counts ']' & '{', ',' & objects
	char *data = pair->value;
	if(!data) return NULL;
	while(*data && a_closing != 1){
		switch(*data++){
			case '{':
				++o_opening; valstart = 0;
			break;
			case '}':
				if(--o_opening == 0) ++objects;
			break;
			case '[':
				--a_closing;
			break;
			case ']':
				++a_closing;
			break;
			case ',':
				if(o_opening == 0){
					++commas; // count commas separating objects
					valstart = 1;
				}
			break;
			case '\t': case '\n': case '\r': case ' ':
			break;
			default:
				if(valstart) ++values;
				valstart = 0;
			break;
		}
	}
	if(a_closing != 1) return NULL;
	//DBG("find array with %d objects & %d values, commas: %d", objects, values, commas);
	if((objects && commas < objects-1) || (values && commas < values-1)) return NULL; // delimeter omit
	if(objects && values) return NULL; // mixed array
	pair->len = objects + values;
	data[-1] = 0;
	pair->type = objects ? json_type_obj_array : json_type_data_array;
	return data;
}

/**
 * skip '.', numbers, signs & '[e|E]'
 * return first non-number
 */
static char *skipnumbers(char *data){
	while(*data){
		char ch = *data;
		if(ch < '0' || ch > '9'){
			if(ch != '.' && ch != 'e' && ch != 'E' && ch != '-' && ch !='+')
				break;
		}
		++data;
	}
	return data;
}

/**
 * read and add object from string
 */
int json_add_object(json_object *obj, char **data){
	//FNAME();
	if(!obj || !data || !*data || !**data) return 0;
	char *str = skipspaces(*data);
	json_pair pair;
	memset(&pair, 0, sizeof(pair));
	if(*str == '}') return 0; // no objects
	if(*str != '"') return -1; // err - not an object's name
	char *eptr = strchr(++str, '"');
	if(!eptr) return -1;
	*eptr = 0;
	pair.name = str;
	str = eptr + 1;
	str = skipspaces(str);
	if(*str != ':') return -1; // no delimeter
	str = skipspaces(str+1);
	if(*str == '"'){ // string
		pair.type = json_type_string;
		pair.value = ++str;
		eptr = strchr(str, '"');
		if(!eptr) return -1;
		*eptr = 0;
		str = eptr + 1;
	}else if(*str == '{'){ // compound object
		pair.type = json_type_object;
		pair.value = skipspaces(++str);
		str = find_obj_end(pair.value);
	}else if(*str == '['){ // array
		pair.value = skipspaces(++str);
		str = count_objs(&pair);
	}else{ // number ?
		pair.type = json_type_number;
		pair.value = str;
		str = skipnumbers(str);
		if(pair.value == str) return -1; // not a number
	}
	// skip comma & spaces, but leave '}' & add object
	if(!str) return -1;
	str = skipspaces(str);
	//DBG("char: %c", *str);
	int meetcomma = 0;
	if(*str == ','){
		*str++ = 0;
		meetcomma = 1;
		str = skipspaces(str);
	}
	if(*str == '}') --str;
	else if(!meetcomma && *str) return -1;
	*data = str;
	// add pair
	if(obj->len == obj->max_len){ // no space left - realloc
		obj->max_len += JSON_BLKSZ;
		obj->objs = realloc(obj->objs, sizeof(json_pair)*obj->max_len);
		if(!obj->objs) return -1;
	}
	memcpy(&(obj->objs[obj->len]), &pair, sizeof(json_pair));
	++obj->len;
/*
	#ifdef EBUG
	fprintf(stderr, "pair %zd, nm: %s, val: %s, type: %d", obj->len, pair.name, pair.value, pair.type);
	if(pair.len) fprintf(stderr, "; array length: %zd", pair.len);
	fprintf(stderr, "\n");
	#endif
	*/
	return 1;
}


static json_object *json_collect(char *data){
	//FNAME();
	json_object *ret = json_ini();
	ret->original_data = strdup(data);
	data = skipspaces(ret->original_data);
	int r;
	while((r = json_add_object(ret, &data)) > 0);
	if(r < 0) goto errjson;
	return ret;
errjson:
	json_free_obj(&ret);
	return NULL;
}

/**
 * get global object
 */
json_object *json_tokener_parse(char *data){
	data = skipspaces(data);
	if(*data != '{') return NULL;
	data = strdup(data+1);
	if(!data) return NULL;
	if(!find_obj_end(data)){
		free(data);
		return NULL;
	}
	json_object *jobj = json_collect(data);
	free(data);
	return jobj;
}

/**
 * return double value of number pair
 */
double json_pair_get_number(json_pair *pair){
	if(pair->type != json_type_number) return 0.;
	char *endptr;
	return strtod(pair->value, &endptr);
}
/**
 * return string value of string pair
 */
char *json_pair_get_string(json_pair *pair){
	if(pair->type != json_type_string) return NULL;
	return pair->value;
}
/**
 * create object from compound pair
 */
json_object *json_pair_get_object(json_pair *pair){
	if(pair->type != json_type_object) return NULL;
	return json_collect(pair->value);
}

/**
 * find pair with name @name
 */
json_pair *json_object_get_pair(json_object *obj, char *name){
	//DBG("search pair named %s", name);
	if(!obj || !name) return NULL;
	json_pair *pairs = obj->objs;
	size_t i, L = obj->len;
	for(i = 0; i < L; ++i){
		if(strcmp(name, pairs[i].name) == 0){
			//DBG("Find, val = %s", pairs[i].value);
			return &pairs[i];
		}
	}
	return NULL;
}

/**
 * return new object with index idx from array
 */
json_object *json_array_get_object(json_pair *pair, int idx){
	//DBG("get %d object from array type %d, len %zd", idx, pair->type, pair->len);
	if(pair->type != json_type_obj_array) return NULL;
	if(pair->len < 1 || idx > pair->len) return NULL;
	int opening = 0, curidx = 0;
	char *data = pair->value, *start = NULL;
	json_object *obj = NULL;
	while(*data && curidx <= idx){
		switch(*data++){
			case '{':
				if(++opening == 1 && curidx == idx) start = data;
			break;
			case '}':
				if(--opening == 0){
					++curidx;
					if(start){ // found
						data[-1] = 0;
						obj = json_collect(start);
						//DBG("found data with idx %d, val: %s", idx, start);
						data[-1] = '}';
					}
				}
			break;
			default: break;
		}
	}
	if(!start || opening || !obj->original_data){
		json_free_obj(&obj);
		return NULL;
	}
	return obj;
}

/**
 * return allocated memory - must be free'd
 * @return - string with data
 */
char *json_array_get_data(json_pair *pair, int idx){
	if(pair->type != json_type_data_array) return NULL;
	if(pair->len < 1 || idx > pair->len) return NULL;
	char *data = pair->value, *eptr, *val = NULL;
	int curidx = 0;
	while(*data && curidx <= idx){
		data = skipspaces(data);
		char ch = *data;
		if(!ch) return NULL;
		if(ch != ','){
			if(curidx == idx){
				if(ch == '"'){ // string
					eptr = strchr(++data, '"');
					if(!eptr) return NULL;
					*eptr = 0;
					val = strdup(data);
					*eptr = '"';
					return val;
				}else{ // number
					eptr = skipnumbers(data);
					if(!eptr || eptr == data) return NULL;
					char oldval = *eptr;
					*eptr = 0; val = strdup(data);
					*eptr = oldval;
					return val;
				}
			}else data = strchr(data, ',');
		}else{
			do{
				data = skipspaces(data+1);
			}while(*data == ',');
			++curidx;
		}
	}
	return NULL;
}
