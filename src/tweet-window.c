/* tweet-window.c: Main application window
 *
 * This file is part of Tweet.
 * Copyright (C) 2008  Emmanuele Bassi  <ebassi@gnome.org>
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

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>

#include <glib-object.h>

#include <gtk/gtk.h>

#include "tweet-preferences.h"
#include "tweet-vbox.h"
#include "tweet-window.h"

#define TWEET_WINDOW_GET_PRIVATE(obj)   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TWEET_TYPE_WINDOW, TweetWindowPrivate))

struct _TweetWindowPrivate
{
  GtkWidget *vbox;
  GtkWidget *menubar;

  GtkUIManager *manager;
  GtkActionGroup *action_group;
};

G_DEFINE_TYPE (TweetWindow, tweet_window, GTK_TYPE_WINDOW);

static void
tweet_window_dispose (GObject *gobject)
{
  TweetWindowPrivate *priv = TWEET_WINDOW (gobject)->priv;

  if (priv->manager)
    {
      g_object_unref (priv->manager);
      priv->manager = NULL;
    }

  if (priv->action_group)
    {
      g_object_unref (priv->action_group);
      priv->action_group = NULL;
    }

  G_OBJECT_CLASS (tweet_window_parent_class)->dispose (gobject);
}

static void
tweet_window_cmd_quit (GtkAction   *action,
                       TweetWindow *window)
{
  gtk_widget_destroy (GTK_WIDGET (window));
}

static void
tweet_window_cmd_preferences (GtkAction   *action,
                              TweetWindow *window)
{
  tweet_show_preferences_dialog (GTK_WINDOW (window),
                                 _("Preferences"),
                                 tweet_vbox_get_config (TWEET_VBOX (window->priv->vbox)));
}

static void
tweet_window_cmd_view_recent (GtkAction   *action,
                              TweetWindow *window)
{
  TweetVBox *vbox = TWEET_VBOX (window->priv->vbox);
  if (vbox->mode == TWEET_MODE_RECENT)
    return;

  vbox->mode = TWEET_MODE_RECENT;
  vbox->last_update.tv_sec = 0;

  tweet_vbox_clear (vbox);
  tweet_vbox_refresh (vbox);
}

static void
tweet_window_cmd_view_replies (GtkAction   *action,
                               TweetWindow *window)
{
  TweetVBox *vbox = TWEET_VBOX (window->priv->vbox);
  if (vbox->mode == TWEET_MODE_REPLIES)
    return;

  vbox->mode = TWEET_MODE_REPLIES;
  vbox->last_update.tv_sec = 0;

  tweet_vbox_clear (vbox);
  tweet_vbox_refresh (vbox);
}

static void
tweet_window_cmd_view_archive (GtkAction   *action,
                               TweetWindow *window)
{
  TweetVBox *vbox = TWEET_VBOX (window->priv->vbox);
  if (vbox->mode == TWEET_MODE_ARCHIVE)
    return;

  vbox->mode = TWEET_MODE_ARCHIVE;
  vbox->last_update.tv_sec = 0;

  tweet_vbox_clear (vbox);
  tweet_vbox_refresh (vbox);
}

static void
tweet_window_cmd_view_favorites (GtkAction   *action,
                                 TweetWindow *window)
{
  TweetVBox *vbox = TWEET_VBOX (window->priv->vbox);
  if (vbox->mode == TWEET_MODE_FAVORITES)
    return;

  vbox->mode = TWEET_MODE_FAVORITES;
  vbox->last_update.tv_sec = 0;

  tweet_vbox_clear (vbox);
  tweet_vbox_refresh (vbox);
}

static void
tweet_window_cmd_view_reload (GtkAction   *action,
                              TweetWindow *window)
{
  TweetVBox *vbox = TWEET_VBOX (window->priv->vbox);

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
}

static void
about_url_hook (GtkAboutDialog *dialog,
                const gchar    *link_,
                gpointer        user_data)
{
  GdkScreen *screen;
  gint pid;
  GError *error;
  gchar **argv;

  if (gtk_widget_has_screen (GTK_WIDGET (dialog)))
    screen = gtk_widget_get_screen (GTK_WIDGET (dialog));
  else
    screen = gdk_screen_get_default ();

  argv = g_new (gchar*, 3);
  argv[0] = g_strdup ("xdg-open");
  argv[1] = g_strdup (link_);
  argv[2] = NULL;

  error = NULL;
  gdk_spawn_on_screen (screen,
                       NULL,
                       argv, NULL,
                       G_SPAWN_SEARCH_PATH,
                       NULL, NULL,
                       &pid, &error);
  if (error)
    {
      g_warning ("Unable to launch xdg-open: %s", error->message);
      g_error_free (error);
    }

  g_strfreev (argv);
}

static void
tweet_window_cmd_help_about (GtkAction   *action,
                             TweetWindow *window)
{
  const gchar *authors[] = {
    "Emmanuele Bassi <ebassi@gnome.org>",
    NULL
  };

  const gchar *translator_credits = _("translator-credits");
  const gchar *copyright = "Copyright \xc2\xa9 2008 Emmanuele Bassi";

  gtk_about_dialog_set_url_hook (about_url_hook, NULL, NULL);

  gtk_show_about_dialog (GTK_WINDOW (window),
                         "program-name", "Tweet",
                         "title", _("About Tweet"),
                         "comments", _("Twitter desktop client"),
                         "version", VERSION,
                         "copyright", copyright,
                         "authors", authors,
                         "translator-credits", translator_credits,
                         "website", "http://live.gnome.org/Tweet",
                         NULL);
}

static void
tweet_window_class_init (TweetWindowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TweetWindowPrivate));

  gobject_class->dispose = tweet_window_dispose;
}

static const GtkActionEntry action_entries[] = {
  { "TweetFileAction", NULL, N_("_File") },
    {
      "TweetQuit", GTK_STOCK_QUIT, NULL, "<control>Q",
      N_("Quit Tweet"),
      G_CALLBACK (tweet_window_cmd_quit)
    },

  { "TweetEditAction", NULL, N_("_Edit") },
    {
      "TweetPreferences", GTK_STOCK_PREFERENCES, NULL, NULL,
      N_("Edit Tweet Preferences"),
      G_CALLBACK (tweet_window_cmd_preferences)
    },

  { "TweetViewAction", NULL, N_("_View") },
    {
      "TweetRecent", NULL, N_("_Recent statuses"), NULL, NULL,
      G_CALLBACK (tweet_window_cmd_view_recent)
    },
    {
      "TweetReplies", NULL, N_("R_eplies"), NULL, NULL,
      G_CALLBACK (tweet_window_cmd_view_replies)
    },
    {
      "TweetFavorites", NULL, N_("_Favorites"), NULL, NULL,
      G_CALLBACK (tweet_window_cmd_view_favorites)
    },
    {
      "TweetArchive", NULL, N_("_Archive"), NULL, NULL,
      G_CALLBACK (tweet_window_cmd_view_archive)
    },
    {
      "TweetReload", GTK_STOCK_REFRESH, N_("_Reload"), "<control>R",
      N_("Display the latest statuses"),
      G_CALLBACK (tweet_window_cmd_view_reload)
    },

  { "TweetHelpAction", NULL, N_("_Help") },
    {
      "TweetAbout", GTK_STOCK_ABOUT, N_("_About"), NULL, NULL,
      G_CALLBACK (tweet_window_cmd_help_about)
    }
};

static void
tweet_window_init (TweetWindow *window)
{
  TweetWindowPrivate *priv;
  GtkAccelGroup *accel_group;
  GError *error;

  GTK_WINDOW (window)->type = GTK_WINDOW_TOPLEVEL;
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (window), TWEET_VBOX_WIDTH, 600);
  gtk_window_set_title (GTK_WINDOW (window), "Tweet");

  window->priv = priv = TWEET_WINDOW_GET_PRIVATE (window);

  priv->vbox = tweet_vbox_new ();
  gtk_container_add (GTK_CONTAINER (window), priv->vbox);
  gtk_widget_show (priv->vbox);

  TWEET_VBOX (priv->vbox)->mode = TWEET_MODE_RECENT;

  priv->action_group = gtk_action_group_new ("TweetActions");
  gtk_action_group_set_translation_domain (priv->action_group, NULL);
  gtk_action_group_add_actions (priv->action_group,
                                action_entries,
                                G_N_ELEMENTS (action_entries),
                                window);

  priv->manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (priv->manager,
                                      priv->action_group,
                                      0);

  accel_group = gtk_ui_manager_get_accel_group (priv->manager);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

  error = NULL;
  if (!gtk_ui_manager_add_ui_from_file (priv->manager,
                                        PKGDATADIR G_DIR_SEPARATOR_S "tweet.ui",
                                        &error))
    {
     g_critical ("Building menus failed: %s",
                 error->message);
     g_error_free (error);
    }
  else
    {
      priv->menubar = gtk_ui_manager_get_widget (priv->manager, "/TweetMenubar");
      gtk_container_add (GTK_CONTAINER (priv->vbox), priv->menubar);
      gtk_box_reorder_child (GTK_BOX (priv->vbox), priv->menubar, 0);
      gtk_widget_show (priv->menubar);
    }

  gtk_widget_show (GTK_WIDGET (window));

  gtk_widget_realize (tweet_vbox_get_canvas (TWEET_VBOX (priv->vbox)));
}

GtkWidget *
tweet_window_new (void)
{
  return g_object_new (TWEET_TYPE_WINDOW, NULL);
}
