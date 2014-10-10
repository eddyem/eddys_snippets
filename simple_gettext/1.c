#include <stdio.h>
#include <libintl.h>
#include <locale.h>

#define _(String)				gettext(String)
#define gettext_noop(String)	String
#define N_(String)				gettext_noop(String)

int main(int argc, char **argv){
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	/*
	 * To run in GUI bullshit (like qt or gtk) you must do:
	 * bind_textdomain_codeset(PACKAGE, "UTF8");
	 */
	textdomain(GETTEXT_PACKAGE);
	/*
	 * In case when you need both GUI and CLI, you should
	 * do this:
	 * 		char*old = bind_textdomain_codeset (PACKAGE, "");
	 * before printf calling
	 * To restore GUI hrunicode do
	 * 		bind_textdomain_codeset (PACKAGE, old);
	 * at the end of printf's
	 */
	/// Переведено геттекстом\n
	printf(_("Translated by gettext\n"));
	/// Вполне работает в любой кодировке\n
	printf(_("This works fine in any charset\n"));
	return 0;
}
