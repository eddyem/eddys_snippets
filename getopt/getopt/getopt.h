#ifndef __USAGE_H__
#define __USAGE_H__

#include "takepic.h"
#include <getopt.h>
#include <stdarg.h>

/*
 * here are global variables and global data structures definition, like
 * int val;
 * enum{reset = 0, set};
 */

extern char *__progname;
void usage(char *fmt, ...);
void parse_args(int argc, char **argv);

#endif // __USAGE_H__
