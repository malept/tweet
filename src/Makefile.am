NULL =

INCLUDES = \
	-DPREFIX=\""$(prefix)"\"				\
	-DLIBDIR=\""$(libdir)"\"				\
	-DDATADIR=\""$(datadir)"\"				\
	-DPKGDATADIR=\""$(datadir)/tweet"\"			\
	-DUIDATADIR=\""$(datadir)/gnome-2.0/ui"\"		\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DG_DISABLE_DEPRECATED					\
	-DGDK_DISABLE_DEPRECATED				\
	-DGDK_PIXBUF_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED				\
	$(NULL)

common_cflags = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/copy-and-paste \
	$(TWEET_CFLAGS) \
	$(NMGLIB_CFLAGS) \
	$(GNOME_KEYRING_CFLAGS) \
	$(LIBSEXY_CFLAGS) \
	$(NULL)

common_ldadd = \
	$(top_builddir)/copy-and-paste/tidy/libtidy.la \
	$(TWEET_LIBS) \
	$(NMGLIB_LIBS) \
	$(GNOME_KEYRING_LIBS) \
	$(LIBSEXY_LIBS) \
	$(NULL)

if BUILD_AWN_APPLET
tweet_applet = tweet-applet.la
endif

noinst_LTLIBRARIES = libtweet-ui.la
bin_PROGRAMS = tweet

awndir = $(libdir)/awn/applets/tweet
awn_LTLIBRARIES = $(tweet_applet)

tweet_h = \
	tweet-animation.h \
	tweet-app.h \
	tweet-auth-dialog.h \
	tweet-canvas.h \
	tweet-config.h \
	tweet-hot-actor.h \
	tweet-interval.h \
	tweet-overlay.h \
	tweet-preferences.h \
	tweet-spinner.h \
	tweet-status-cell.h \
	tweet-status-column.h \
	tweet-status-info.h \
	tweet-status-model.h \
	tweet-status-renderer.h \
	tweet-status-view.h \
	tweet-url-label.h \
	tweet-utils.h \
	tweet-vbox.h \
	tweet-window.h \
	$(NULL)

tweet-enum-types.h: stamp-tweet-enum-types.h
	@true
stamp-tweet-enum-types.h: $(tweet_h) Makefile
	( cd $(top_srcdir)/src && \
	  $(GLIB_MKENUMS) \
		--template $(top_srcdir)/src/tweet-enum-types.h.in \
	  $(tweet_h) ) > xgen-ceth \
	&& (cmp xgen-ceth tweet-enum-types.h || cp xgen-ceth tweet-enum-types.h) \
	&& rm -f xgen-ceth \
	&& echo timestamp > $(@F)

tweet-enum-types.c: tweet-enum-types.h
	( cd $(top_srcdir)/src && \
	  $(GLIB_MKENUMS) \
		--template $(top_srcdir)/src/tweet-enum-types.c.in \
	  $(tweet_h) ) > xgen-cetc \
	&& cp xgen-cetc tweet-enum-types.c \
	&& rm -f xgen-cetc

BUILT_SOURCES = tweet-enum-types.h tweet-enum-types.c

libtweet_ui_la_SOURCES = \
	tweet-animation.c \
	tweet-animation.h \
	tweet-app.c \
	tweet-app.h \
	tweet-auth-dialog.c \
	tweet-auth-dialog.h \
	tweet-canvas.c \
	tweet-canvas.h \
	tweet-config.c \
	tweet-config.h \
	tweet-enum-types.c \
	tweet-enum-types.h \
	tweet-hot-actor.c \
	tweet-hot-actor.h \
	tweet-interval.c \
	tweet-interval.h \
	tweet-overlay.c \
	tweet-overlay.h \
	tweet-preferences.c \
	tweet-preferences.h \
	tweet-spinner.c \
	tweet-spinner.h \
	tweet-status-cell.c \
	tweet-status-cell.h \
	tweet-status-column.c \
	tweet-status-column.h \
	tweet-status-info.c \
	tweet-status-info.h \
	tweet-status-model.c \
	tweet-status-model.h \
	tweet-status-renderer.c \
	tweet-status-renderer.h \
	tweet-status-view.c \
	tweet-status-view.h \
	tweet-url-label.c \
	tweet-url-label.h \
	tweet-utils.c \
	tweet-utils.h \
	tweet-vbox.c \
	tweet-vbox.h \
	tweet-window.c \
	tweet-window.h \
	$(NULL)
libtweet_ui_la_CFLAGS = $(common_cflags)
libtweet_ui_la_LIBADD = $(common_ldadd)

tweet_SOURCES = tweet-main.c
tweet_CFLAGS = $(common_cflags)
tweet_LDADD = $(common_ldadd) libtweet-ui.la

tweet_applet_la_SOURCES = tweet-awn-applet.c
tweet_applet_la_CFLAGS = $(common_cflags) $(AWN_CFLAGS)
tweet_applet_la_LIBADD = $(AWN_LIBS) libtweet-ui.la
tweet_applet_la_LDFLAGS = -module -avoid-version

CLEANFILES = stamp-tweet-enum-types.h

DISTCLEANFILES = tweet-enum-types.h tweet-enum-types.c

EXTRA_DIST = tweet-enum-types.c.in tweet-enum-types.h.in
