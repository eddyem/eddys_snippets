#include <locale.h>
#include <libintl.h>
#include <usefull_macros.h>

int main(){
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    #if defined GETTEXT_PACKAGE && defined LOCALEDIR
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    textdomain(GETTEXT_PACKAGE);
    #endif

    helloworld();
    return 0;
}
