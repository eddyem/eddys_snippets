/*
 * json.h
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

#pragma once
#ifndef __JSON_H__
#define __JSON_H__

enum json_type{
	json_type_object,     // compound object
	json_type_obj_array,  // array of objects
	json_type_data_array, // array of data
	json_type_number,     // number
	json_type_string      // string
};

// JSON pair (name - value):
typedef struct{
	char *name;
	char *value;
	enum json_type type;
	size_t len;  // amount of objects in array
}json_pair;


typedef struct{
	size_t len;     // amount of pairs
	size_t max_len; // max amount of pairs
	char *original_data;  // contains string with data
	json_pair *objs;// objects themself
}json_object;

#define JSON_BLKSZ (128)

// JSON object


json_object *json_tokener_parse(char *data);
void json_free_obj(json_object **obj);
char *json_pair_get_string(json_pair *pair);
double json_pair_get_number(json_pair *pair);
json_object *json_pair_get_object(json_pair *pair);
json_pair *json_object_get_pair(json_object *obj, char *name);
json_object *json_array_get_object(json_pair *pair, int idx);
char *json_array_get_data(json_pair *pair, int idx);

#endif // __JSON_H__
