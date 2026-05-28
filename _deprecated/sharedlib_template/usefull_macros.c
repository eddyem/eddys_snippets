#include <stdio.h>

#if defined GETTEXT
#include <libintl.h>
#define _(String)               gettext(String)
#define gettext_noop(String)    String
#define N_(String)              gettext_noop(String)
#else
#define _(String)               (String)
#define N_(String)              (String)
#endif


void helloworld(){
	/// Шалом, мир!!!\n
	printf(_("Hello, World!!!\n"));
}
