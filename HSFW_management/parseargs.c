/*
 * parseargs.c - parsing command line arguments & print help
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

#include <stdio.h>	// printf
#include <getopt.h>	// getopt_long
#include <stdlib.h>	// calloc, exit, strtoll
#include <assert.h>	// assert
#include <string.h> // strdup, strchr, strlen
#include <strings.h>// strcasecmp
#include <limits.h> // INT_MAX & so on
#include <libintl.h>// gettext
#include <ctype.h>	// isalpha
#include "parseargs.h"
#include "usefull_macros.h"

char *helpstring = "%s\n";

/**
 * Change standard help header
 * MAY consist ONE "%s" for progname
 * @param str (i) - new format
 */
void change_helpstring(char *s){
	int pcount = 0, scount = 0;
	char *str = s;
	// check `helpstring` and set it to default in case of error
	for(; pcount < 2; str += 2){
		if(!(str = strchr(str, '%'))) break;
		if(str[1] != '%') pcount++; // increment '%' counter if it isn't "%%"
		else{
			str += 2; // pass next '%'
			continue;
		}
		if(str[1] == 's') scount++; // increment "%s" counter
	};
	if(pcount > 1 || pcount != scount){ // amount of pcount and/or scount wrong
		/// "������������ ������ ������ ������"
		ERRX(_("Wrong helpstring!"));
	}
	helpstring = s;
}

/**
 * Carefull atoll/atoi
 * @param num (o) - returning value (or NULL if you wish only check number) - allocated by user
 * @param str (i) - string with number must not be NULL
 * @param t   (i) - T_INT for integer or T_LLONG for long long (if argtype would be wided, may add more)
 * @return TRUE if conversion sone without errors, FALSE otherwise
 */
static bool myatoll(void *num, char *str, argtype t){
	long long tmp, *llptr;
	int *iptr;
	char *endptr;
	assert(str);
	assert(num);
	tmp = strtoll(str, &endptr, 0);
	if(endptr == str || *str == '\0' || *endptr != '\0')
		return FALSE;
	switch(t){
		case arg_longlong:
			llptr = (long long*) num;
			*llptr = tmp;
		break;
		case arg_int:
		default:
			if(tmp < INT_MIN || tmp > INT_MAX){
				/// "����� ��� ����������� ���������"
				WARNX(_("Integer out of range"));
				return FALSE;
			}
			iptr = (int*)num;
			*iptr = (int)tmp;
	}
	return TRUE;
}

// the same as myatoll but for double
// There's no NAN & INF checking here (what if they would be needed?)
static bool myatod(void *num, const char *str, argtype t){
	double tmp, *dptr;
	float *fptr;
	char *endptr;
	assert(str);
	tmp = strtod(str, &endptr);
	if(endptr == str || *str == '\0' || *endptr != '\0')
		return FALSE;
	switch(t){
		case arg_double:
			dptr = (double *) num;
			*dptr = tmp;
		break;
		case arg_float:
		default:
			fptr = (float *) num;
			*fptr = (float)tmp;
		break;
	}
	return TRUE;
}

/**
 * Get index of current option in array options
 * @param opt (i) - returning val of getopt_long
 * @param options (i) - array of options
 * @return index in array
 */
static int get_optind(int opt, myoption *options){
	int oind;
	myoption *opts = options;
	assert(opts);
	for(oind = 0; opts->name && opts->val != opt; oind++, opts++);
	if(!opts->name || opts->val != opt) // no such parameter
	showhelp(-1, options);
	return oind;
}

/**
 * reallocate new value in array of multiple repeating arguments
 * @arg paptr - address of pointer to array (**void)
 * @arg type - its type (for realloc)
 * @return pointer to new (next) value
 */
void *get_aptr(void *paptr, argtype type){
	int i = 1;
	void **aptr = *((void***)paptr);
	if(aptr){ // there's something in array
		void **p = aptr;
		while(*p++) ++i;
	}
	size_t sz = 0;
	switch(type){
		default:
		case arg_none:
			/// "�� ���� ������������ ��������� ���������� ��� ����������!"
			ERRX("Can't use multiple args with arg_none!");
		break;
		case arg_int:
			sz = sizeof(int);
		break;
		case arg_longlong:
			sz = sizeof(long long);
		break;
		case arg_double:
			sz = sizeof(double);
		break;
		case arg_float:
			sz = sizeof(float);
		break;
		case arg_string:
			sz = 0;
		break;
	/*	case arg_function:
			sz = sizeof(argfn *);
		break;*/
	}
	aptr = realloc(aptr, (i + 1) * sizeof(void*));
	*((void***)paptr) = aptr;
	aptr[i] = NULL;
	if(sz){
		aptr[i - 1] = malloc(sz);
	}else
		aptr[i - 1] = &aptr[i - 1];
	return aptr[i - 1];
}


/**
 * Parse command line arguments
 * ! If arg is string, then value will be strdup'ed!
 *
 * @param argc (io) - address of argc of main(), return value of argc stay after `getopt`
 * @param argv (io) - address of argv of main(), return pointer to argv stay after `getopt`
 * BE CAREFUL! if you wanna use full argc & argv, save their original values before
 * 		calling this function
 * @param options (i) - array of `myoption` for arguments parcing
 *
 * @exit: in case of error this function show help & make `exit(-1)`
  */
void parseargs(int *argc, char ***argv, myoption *options){
	char *short_options, *soptr;
	struct option *long_options, *loptr;
	size_t optsize, i;
	myoption *opts = options;
	// check whether there is at least one options
	assert(opts);
	assert(opts[0].name);
	// first we count how much values are in opts
	for(optsize = 0; opts->name; optsize++, opts++);
	// now we can allocate memory
	short_options = calloc(optsize * 3 + 1, 1); // multiply by three for '::' in case of args in opts
	long_options = calloc(optsize + 1, sizeof(struct option));
	opts = options; loptr = long_options; soptr = short_options;
	// in debug mode check the parameters are not repeated
#ifdef EBUG
	char **longlist = MALLOC(char*, optsize);
	char *shortlist = MALLOC(char, optsize);
#endif
	// fill short/long parameters and make a simple checking
	for(i = 0; i < optsize; i++, loptr++, opts++){
		// check
		assert(opts->name); // check name
#ifdef EBUG
		longlist[i] = strdup(opts->name);
#endif
		if(opts->has_arg){
			assert(opts->type != arg_none); // check error with arg type
			assert(opts->argptr);  // check pointer
		}
		if(opts->type != arg_none) // if there is a flag without arg, check its pointer
			assert(opts->argptr);
		// fill long_options
		// don't do memcmp: what if there would be different alignment?
		loptr->name		= opts->name;
		loptr->has_arg	= (opts->has_arg < MULT_PAR) ? opts->has_arg : 1;
		loptr->flag		= opts->flag;
		loptr->val		= opts->val;
		// fill short options if they are:
		if(!opts->flag){
#ifdef EBUG
			shortlist[i] = (char) opts->val;
#endif
			*soptr++ = opts->val;
			if(loptr->has_arg) // add ':' if option has required argument
				*soptr++ = ':';
			if(loptr->has_arg == 2) // add '::' if option has optional argument
				*soptr++ = ':';
		}
	}
	// sort all lists & check for repeating
#ifdef EBUG
	int cmpstringp(const void *p1, const void *p2){
		return strcmp(* (char * const *) p1, * (char * const *) p2);
	}
	int cmpcharp(const void *p1, const void *p2){
		return (int)(*(char * const)p1 - *(char *const)p2);
	}
	qsort(longlist, optsize, sizeof(char *), cmpstringp);
	qsort(shortlist,optsize, sizeof(char), cmpcharp);
	char *prevl = longlist[0], prevshrt = shortlist[0];
	for(i = 1; i < optsize; ++i){
		if(longlist[i]){
			if(prevl){
				if(strcmp(prevl, longlist[i]) == 0) ERRX("double long arguments: --%s", prevl);
			}
			prevl = longlist[i];
		}
		if(shortlist[i]){
			if(prevshrt){
				if(prevshrt == shortlist[i]) ERRX("double short arguments: -%c", prevshrt);
			}
			prevshrt = shortlist[i];
		}
	}
#endif
	// now we have both long_options & short_options and can parse `getopt_long`
	while(1){
		int opt;
		int oindex = 0, optind = 0; // oindex - number of option in argv, optind - number in options[]
		if((opt = getopt_long(*argc, *argv, short_options, long_options, &oindex)) == -1) break;
		if(opt == '?'){
			opt = optopt;
			optind = get_optind(opt, options);
			if(options[optind].has_arg == NEED_ARG || options[optind].has_arg == MULT_PAR)
				showhelp(optind, options); // need argument
		}
		else{
			if(opt == 0 || oindex > 0) optind = oindex;
			else optind = get_optind(opt, options);
		}
		opts = &options[optind];
		if(opt == 0 && opts->has_arg == NO_ARGS) continue; // only long option changing integer flag
		// now check option
		if(opts->has_arg == NEED_ARG || opts->has_arg == MULT_PAR)
			if(!optarg) showhelp(optind, options); // need argument
		void *aptr;
		if(opts->has_arg == MULT_PAR){
			aptr = get_aptr(opts->argptr, opts->type);
		}else
			aptr = opts->argptr;
		bool result = TRUE;
		// even if there is no argument, but argptr != NULL, think that optarg = "1"
		if(!optarg) optarg = "1";
		switch(opts->type){
			default:
			case arg_none:
				if(opts->argptr) *((int*)aptr) += 1; // increment value
			break;
			case arg_int:
				result = myatoll(aptr, optarg, arg_int);
			break;
			case arg_longlong:
				result = myatoll(aptr, optarg, arg_longlong);
			break;
			case arg_double:
				result = myatod(aptr, optarg, arg_double);
			break;
			case arg_float:
				result = myatod(aptr, optarg, arg_float);
			break;
			case arg_string:
				result = (*((void**)aptr) = (void*)strdup(optarg));
			break;
			case arg_function:
				result = ((argfn)aptr)(optarg);
			break;
		}
		if(!result){
			showhelp(optind, options);
		}
	}
	*argc -= optind;
	*argv += optind;
}

/**
 * compare function for qsort
 * first - sort by short options; second - sort arguments without sort opts (by long options)
 */
static int argsort(const void *a1, const void *a2){
	const myoption *o1 = (myoption*)a1, *o2 = (myoption*)a2;
	const char *l1 = o1->name, *l2 = o2->name;
	int s1 = o1->val, s2 = o2->val;
	int *f1 = o1->flag, *f2 = o2->flag;
	// check if both options has short arg
	if(f1 == NULL && f2 == NULL){ // both have short arg
		return (s1 - s2);
	}else if(f1 != NULL && f2 != NULL){ // both don't have short arg - sort by long
		return strcmp(l1, l2);
	}else{ // only one have short arg -- return it
		if(f2) return -1; // a1 have short - it is 'lesser'
		else return 1;
	}
}

/**
 * Show help information based on myoption->help values
 * @param oindex (i)  - if non-negative, show only help by myoption[oindex].help
 * @param options (i) - array of `myoption`
 *
 * @exit:  run `exit(-1)` !!!
 */
void showhelp(int oindex, myoption *options){
	int max_opt_len = 0; // max len of options substring - for right indentation
	const int bufsz = 255;
	char buf[bufsz+1];
	myoption *opts = options;
	assert(opts);
	assert(opts[0].name); // check whether there is at least one options
	if(oindex > -1){ // print only one message
		opts = &options[oindex];
		printf("  ");
		if(!opts->flag && isalpha(opts->val)) printf("-%c, ", opts->val);
		printf("--%s", opts->name);
		if(opts->has_arg == 1) printf("=arg");
		else if(opts->has_arg == 2) printf("[=arg]");
		printf("  %s\n", _(opts->help));
		exit(-1);
	}
	// header, by default is just "progname\n"
	printf("\n");
	if(strstr(helpstring, "%s")) // print progname
		printf(helpstring, __progname);
	else // only text
		printf("%s", helpstring);
	printf("\n");
	// count max_opt_len
	do{
		int L = strlen(opts->name);
		if(max_opt_len < L) max_opt_len = L;
	}while((++opts)->name);
	max_opt_len += 14; // format: '-S , --long[=arg]' - get addition 13 symbols
	opts = options;
	// count amount of options
	int N; for(N = 0; opts->name; ++N, ++opts);
	if(N == 0) exit(-2);
	// Now print all help (sorted)
	opts = options;
	qsort(opts, N, sizeof(myoption), argsort);
	do{
		int p = sprintf(buf, "  "); // a little indent
		if(!opts->flag) // .val is short argument
			p += snprintf(buf+p, bufsz-p, "-%c, ", opts->val);
		p += snprintf(buf+p, bufsz-p, "--%s", opts->name);
		if(opts->has_arg == 1) // required argument
			p += snprintf(buf+p, bufsz-p, "=arg");
		else if(opts->has_arg == 2) // optional argument
			p += snprintf(buf+p, bufsz-p, "[=arg]");
		assert(p < max_opt_len); // there would be magic if p >= max_opt_len
		printf("%-*s%s\n", max_opt_len+1, buf, _(opts->help)); // write options & at least 2 spaces after
		++opts;
	}while(--N);
	printf("\n\n");
	exit(-1);
}

/**
 * get suboptions from parameter string
 * @param str - parameter string
 * @param opt - pointer to suboptions structure
 * @return TRUE if all OK
 */
bool get_suboption(char *str, mysuboption *opt){
	int findsubopt(char *par, mysuboption *so){
		int idx = 0;
		if(!par) return -1;
		while(so[idx].name){
			if(strcasecmp(par, so[idx].name) == 0) return idx;
			++idx;
		}
		return -1; // badarg
	}
	bool opt_setarg(mysuboption *so, int idx, char *val){
		mysuboption *soptr = &so[idx];
		bool result = FALSE;
		void *aptr = soptr->argptr;
		switch(soptr->type){
			default:
			case arg_none:
				if(soptr->argptr) *((int*)aptr) += 1; // increment value
				result = TRUE;
			break;
			case arg_int:
				result = myatoll(aptr, val, arg_int);
			break;
			case arg_longlong:
				result = myatoll(aptr, val, arg_longlong);
			break;
			case arg_double:
				result = myatod(aptr, val, arg_double);
			break;
			case arg_float:
				result = myatod(aptr, val, arg_float);
			break;
			case arg_string:
				result = (*((void**)aptr) = (void*)strdup(val));
			break;
			case arg_function:
				result = ((argfn)aptr)(val);
			break;
		}
		return result;
	}
	char *tok;
	bool ret = FALSE;
	char *tmpbuf;
	tok = strtok_r(str, ":,", &tmpbuf);
	do{
		char *val = strchr(tok, '=');
		int noarg = 0;
		if(val == NULL){ // no args
			val = "1";
			noarg = 1;
		}else{
			*val++ = '\0';
			if(!*val || *val == ':' || *val == ','){ // no argument - delimeter after =
				val = "1"; noarg = 1;
			}
		}
		int idx = findsubopt(tok, opt);
		if(idx < 0){
			/// "������������ ��������: %s"
			WARNX(_("Wrong parameter: %s"), tok);
			goto returning;
		}
		if(noarg && opt[idx].has_arg == NEED_ARG){
			/// "%s: ��������� ��������!"
			WARNX(_("%s: argument needed!"), tok);
			goto returning;
		}
		if(!opt_setarg(opt, idx, val)){
			/// "������������ �������� \"%s\" ��������� \"%s\""
			WARNX(_("Wrong argument \"%s\" of parameter \"%s\""), val, tok);
			goto returning;
		}
	}while((tok = strtok_r(NULL, ":,", &tmpbuf)));
	ret = TRUE;
returning:
	return ret;
}
