m4_define([tweet_major_version], [0])
m4_define([tweet_minor_version], [3])
m4_define([tweet_micro_version], [0])

m4_define([tweet_version],
          [tweet_major_version.tweet_minor_version.tweet_micro_version])

m4_define([tweet_interface_age], [0])
m4_define([tweet_binary_age],
          [m4_eval(100 * tweet_minor_version + tweet_micro_version)])

m4_define([lt_current],
          [m4_eval(100 * tweet_minor_version + tweet_micro_version - tweet_interface_age)])
m4_define([lt_revision], [tweet_interface_age])
m4_define([lt_age],
          [m4_eval(tweet_binary_age - tweet_interface_age)])

m4_define([glib_req_version], [2.16])
m4_define([json_glib_req_version], [0.6.0])
m4_define([clutter_req_version], [0.6.2])
m4_define([gtk_req_version], [2.12])
m4_define([clutter_gtk_req_version], [0.6.1])
m4_define([clutter_cairo_req_version], [0.6.2])

AC_PREREQ([2.59])
AC_INIT([tweet], [tweet_version], [], [tweet])

AM_INIT_AUTOMAKE([1.10])
AM_CONFIG_HEADER([config.h])

GNOME_COMMON_INIT
GNOME_COMPILE_WARNINGS
GNOME_DEBUG_CHECK

IT_PROG_INTLTOOL([0.35.0])

AM_DISABLE_STATIC
AM_PATH_GLIB_2_0
AM_PROG_CC_C_O
AM_PROG_LIBTOOL

AC_HEADER_STDC
AC_CHECK_HEADERS([unistd.h])
AC_C_CONST
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_CHECK_FUNCS([strptime localtime_r])

TWEET_MAJOR_VERSION=tweet_major_version
TWEET_MINOR_VERSION=tweet_minor_version
TWEET_MICRO_VERSION=tweet_micro_version
TWEET_VERSION=tweet_version
AC_SUBST(TWEET_MAJOR_VERSION)
AC_SUBST(TWEET_MICRO_VERSION)
AC_SUBST(TWEET_MINOR_VERSION)
AC_SUBST(TWEET_VERSION)

dnl tweet checks
PKG_CHECK_MODULES(TWEET,
                  twitter-glib-1.0 dnl
                  gobject-2.0 >= glib_req_version dnl
                  json-glib-1.0 >= json_glib_req_version dnl
                  clutter-0.6 >= clutter_req_version dnl
                  gtk+-2.0 >= gtk_req_version dnl
                  clutter-gtk-0.6 >= clutter_gtk_req_version dnl
                  clutter-cairo-0.6 >= clutter_cairo_req_version)
AC_SUBST(TWEET_CFLAGS)
AC_SUBST(TWEET_LIBS)

AC_ARG_WITH([networkmanager],
            AC_HELP_STRING([--with-networkmanager=[yes|no|auto]],
                           [build with NetworkManager support (defaults to auto-detect)]),,
            with_networkmanager=auto)

if test "x$with_networkmanager" != "xno"; then
  PKG_CHECK_MODULES(NMGLIB, libnm_glib, [has_nm_glib=yes],[
    if test "x$with_networkmanager" = "xyes"; then
      AC_MSG_ERROR([libnm_glib (NetworkManager GLib library) is required for building.])
    else
      has_nm_glib=no
    fi
  ])
  AC_SUBST(NMGLIB_CFLAGS)
  AC_SUBST(NMGLIB_LIBS)
fi

if test "x$has_nm_glib" = "xyes"; then
  AC_DEFINE([HAVE_NM_GLIB], [1], [Define to 1 if we have NM-GLib support])
fi

AC_ARG_ENABLE([awn-applet],
              AC_HELP_STRING([--enable-awn-applet],
                             [build the Avant Window Navigator applet]),,
              enable_awn_applet=no)

if test "x$enable_awn_applet" = "xyes"; then
  PKG_CHECK_MODULES(AWN, [awn >= 0.3.1])
  AC_SUBST(AWN_CFLAGS)
  AC_SUBST(AWN_LIBS)
fi
AM_CONDITIONAL([BUILD_AWN_APPLET], test "x$enable_awn_applet" = "xyes")

AC_ARG_WITH([gnome-keyring],
            AC_HELP_STRING([--with-gnome-keyring],
                           [store the username/password via Gnome Keyring]),,
            with_gnome_keyring=no)

if test "x$with_gnome_keyring" = "xyes"; then
  PKG_CHECK_MODULES(GNOME_KEYRING, [gnome-keyring-1 >= 2.21.3])
  AC_SUBST(GNOME_KEYRING_CFLAGS)
  AC_SUBST(GNOME_KEYRING_LIBS)
  AC_DEFINE([USE_GNOME_KEYRING], 1, [Defined if Gnome Keyring support was enabled])
fi

AC_ARG_ENABLE([spellcheck],
              AC_HELP_STRING([--enable-spellcheck],
                             [Enable spellcheck support]),,
              enable_spellcheck=no)

if test "x$enable_spellcheck" = "xyes"; then
  PKG_CHECK_MODULES(LIBSEXY, [libsexy])
  AC_SUBST(LIBSEXY_CFLAGS)
  AC_SUBST(LIBSEXY_LIBS)
  AC_DEFINE([HAVE_SPELLCHECK], [1], [Define to 1 if we have libsexy support])
fi

dnl = Enable debug level ===================================================

m4_define([debug_default],
          m4_if(m4_eval(tweet_minor_version % 2), [1], [yes], [minimum]))

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                             [turn on debugging @<:@default=debug_default@:>@]),
,
              enable_debug=debug_default)

if test "x$enable_debug" = "xyes"; then
  test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
  TWEET_DEBUG_CFLAGS="-DTWEET_ENABLE_DEBUG"
else
  if test "x$enable_debug" = "xno"; then
    TWEET_DEBUG_CFLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
  else # minimum
    TWEET_DEBUG_CFLAGS="-DTWEET_ENABLE_DEBUG -DG_DISABLE_CAST_CHECKS"
  fi
fi

AC_SUBST(TWEET_DEBUG_CFLAGS)

dnl = Enable strict compiler flags =========================================

# use strict compiler flags only on development releases
#m4_define([maintainer_flags_default],
#          m4_if(m4_eval(tweet_minor_version % 2), [1], [yes], [no]))
m4_define([maintainer_flags_default], [no])
AC_ARG_ENABLE([maintainer-flags],
              AC_HELP_STRING([--enable-maintainer-flags=@<:@no/yes@:>@],
                             [Use strict compiler flags @<:@default=maintainer_flags_default@:>@]),,
              enable_maintainer_flags=maintainer_flags_default)

if test "x$enable_maintainer_flags" = "xyes"; then
  CPPFLAGS="$CPPFLAGS -g -Wall -Wshadow -Wcast-align -Wno-uninitialized -Werror"
else
  CPPFLAGS="$CPPFLAGS -g -Wall"
fi

dnl Internationalization
GETTEXT_PACKAGE=tweet
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], "$GETTEXT_PACKAGE", [Define the gettext package to use])
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT

dnl gnome-doc-utils stuff
GNOME_DOC_INIT

dnl miscellaneous
AS_AC_EXPAND(LIBDIR, "$libdir")

AC_CONFIG_FILES([
        Makefile
        copy-and-paste/Makefile
        copy-and-paste/tidy/Makefile
        src/Makefile
        po/Makefile.in
        data/Makefile
        data/icons/Makefile
        data/icons/16x16/Makefile
        data/icons/16x16/apps/Makefile
        data/icons/22x22/Makefile
        data/icons/22x22/apps/Makefile
        data/icons/22x22/status/Makefile
        data/icons/32x32/Makefile
        data/icons/32x32/actions/Makefile
        data/icons/32x32/apps/Makefile
        data/icons/32x32/status/Makefile
        data/icons/scalable/Makefile
        data/icons/scalable/actions/Makefile
        data/icons/scalable/apps/Makefile
        data/icons/scalable/status/Makefile
        data/tweet-applet.desktop.in
])

AC_OUTPUT

echo ""
echo "                  Tweet: $VERSION"
echo ""
echo "                 Prefix: ${prefix}"
echo "            Debug level: ${enable_debug}"
echo "         Compiler flags: ${CPPFLAGS}"
echo " NetworkManager support: $has_nm_glib"
echo "  Gnome Keyring support: $with_gnome_keyring"
echo ""
