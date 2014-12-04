/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* always defined to indicate that i18n is enabled */
#define ENABLE_NLS 1

/* Gettext package */
#define GETTEXT_PACKAGE "gthumb"

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
#define HAVE_BIND_TEXTDOMAIN_CODESET 1

/* Define to 1 if you have the `dcgettext' function. */
#define HAVE_DCGETTEXT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if X11 support is included */
#define HAVE_GDKX 1

/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if your <locale.h> file defines LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Define to 1 to enable libgphoto2 support */
#define HAVE_LIBGPHOTO 1

/* Have libiptcdata */
/* #undef HAVE_LIBIPTCDATA */

/* Define to 1 if libjpeg support is included */
#define HAVE_LIBJPEG 1

/* Define to 1 to enable libopenraw support */
/* #undef HAVE_LIBOPENRAW */

/* Define to 1 if libtiff support is included */
/* #undef HAVE_LIBTIFF */

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if libjpeg supports progressive JPEG. */
#define HAVE_PROGRESSIVE_JPEG 1

/* Define to 1 if Xft/XRender support is included */
#define HAVE_RENDER 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if XF86VMODE Gamma is available */
/* #undef HAVE_XF86VMODE_GAMMA */

/* Define if XF86VMODE Gamma Ramp is available */
/* #undef HAVE_XF86VMODE_GAMMA_RAMP */

/* Have XTest */
/* #undef HAVE_XTEST */

/* Locale directory */
#define LOCALEDIR "/usr/local/share/locale"

/* Name of package */
#define PACKAGE "gthumb"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "http://bugzilla.gnome.org/enter_bug.cgi?product=gthumb"

/* Define to the full name of this package. */
#define PACKAGE_NAME "gthumb"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "gthumb 2.10.11"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "gthumb"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.10.11"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Build with Mac OS X menubar integration */
/* #undef USE_MACOSMENU */

/* Version number of package */
#define VERSION "2.10.11"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
#define YYTEXT_POINTER 1
