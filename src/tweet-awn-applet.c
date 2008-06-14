/* tweet-awn-applet.c: Main Awn applet code
 *
 * This file is part of Tweet.
 * Copyright (C) 2008  Mark Lee  <avant-wn@lazymalevolence.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include <clutter/clutter.h>

#include <libawn/awn-applet-dialog.h>
#include <libawn/awn-applet-simple.h>

#include "tweet-vbox.h"

typedef struct
{
  AwnAppletSimple *applet;
  GtkWidget       *vbox;
  GtkWidget       *dialog;
/*AwnConfigClient *config;*/
  GtkWidget       *menu;
  gboolean         dialog_shown;
} TweetAwnApplet;

static gboolean
tweet_applet_menu_do_refresh (GtkMenuItem *item,
                              TweetVBox   *vbox)
{
  if (vbox->refresh_id)
  {
    g_source_remove (vbox->refresh_id);
    vbox->refresh_id = 0;
  }

  tweet_vbox_refresh (vbox);

  vbox->refresh_id =
    g_timeout_add_seconds (tweet_config_get_refresh_time (tweet_vbox_get_config (vbox)),
                           (GSourceFunc)tweet_vbox_refresh_timeout,
                           vbox);
  return TRUE;
}

static gboolean
tweet_applet_menu_show_about (GtkMenuItem *item)
{
  g_message ("TODO: ADD ABOUT DIALOG");
  return TRUE;
}

static gboolean
tweet_applet_onclick (GtkWidget      *applet,
                      GdkEventButton *event,
                      TweetAwnApplet *tweet)
{
  switch (event->button)
  {
    case 1: /* {{{ (left mouse button) */
      /* show/hide dialog */
      if (!tweet->dialog) 
      {
        GtkWidget *vbox;

        tweet->dialog = awn_applet_dialog_new (AWN_APPLET (tweet->applet));

        gtk_window_set_default_size (GTK_WINDOW (tweet->dialog), TWEET_VBOX_WIDTH, 600);

        tweet->vbox = vbox = tweet_vbox_new ();
        TWEET_VBOX (vbox)->mode = TWEET_MODE_RECENT;
        gtk_widget_set_size_request (vbox, TWEET_VBOX_WIDTH, 600);
        gtk_container_add (GTK_CONTAINER (tweet->dialog), vbox);
        gtk_widget_show (vbox);

        gtk_widget_realize (tweet_vbox_get_canvas (TWEET_VBOX (vbox)));
      }
      
      if (tweet->dialog_shown)
      {
        gtk_widget_hide (tweet->dialog);
      }
      else /* !tweet->dialog_shown */
      {
        gtk_widget_show_all (tweet->dialog);
      }
      tweet->dialog_shown = !tweet->dialog_shown;
      break; /* }}} */
    case 2: /* {{{ (middle mouse button) */
      /* nothing for now */
      break; /* }}} */
    case 3: /* {{{ (right mouse button) */
      /* show menu */
      if (!tweet->menu)
      {
        GtkWidget *item;
        tweet->menu = awn_applet_create_default_menu (AWN_APPLET (tweet->applet));
        gtk_menu_set_screen (GTK_MENU (tweet->menu), NULL);
        /* reload */
        item = gtk_image_menu_item_new_from_stock (GTK_STOCK_REFRESH, NULL);
        gtk_widget_show (item);
        g_signal_connect (G_OBJECT (item), "activate",
                          G_CALLBACK (tweet_applet_menu_do_refresh),
                          TWEET_VBOX (tweet->vbox));
        gtk_menu_shell_append (GTK_MENU_SHELL (tweet->menu), item);
        /* about */
        item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);
        gtk_widget_show (item);
        g_signal_connect (G_OBJECT (item), "activate",
                          G_CALLBACK (tweet_applet_menu_show_about),
                          NULL);
        gtk_menu_shell_append (GTK_MENU_SHELL (tweet->menu), item);
      }
      gtk_menu_popup (GTK_MENU (tweet->menu),
                      NULL, NULL, NULL,NULL,
                      event->button, event->time);
      break; /* }}} */
  }

  return TRUE;
}

AwnApplet *
awn_applet_factory_initp (const gchar *uid, gint orient, gint height)
{
  GtkIconTheme *theme;
  TweetAwnApplet *tweet;
  int dummy_argc = 0;
  char **dummy_argv = { NULL };

  clutter_init (&dummy_argc, &dummy_argv);

  theme = gtk_icon_theme_get_default ();

  gtk_icon_theme_append_search_path (theme,
                                     PKGDATADIR G_DIR_SEPARATOR_S "icons");

  tweet = g_malloc (sizeof (TweetAwnApplet));

  tweet->applet = AWN_APPLET_SIMPLE (awn_applet_simple_new (uid, orient, height));
  awn_applet_simple_set_temp_icon (tweet->applet,
                                   gtk_icon_theme_load_icon (theme,
                                                             "document-send",
                                                             height, 0, NULL));
  g_signal_connect (G_OBJECT (tweet->applet), "button-press-event",
                    G_CALLBACK (tweet_applet_onclick), tweet);

  tweet->dialog = NULL;
  tweet->vbox = NULL;
  tweet->menu = NULL;
  tweet->dialog_shown = FALSE;

  return AWN_APPLET (tweet->applet);
}
