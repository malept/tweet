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

#ifdef HAVE_NM_GLIB
#include <libnm_glib.h>
#endif

#include "tweet-preferences.h"
#include "tweet-vbox.h"
#include "tweet-window.h"

typedef enum {
  TWEET_STATUS_ERROR,
  TWEET_STATUS_RECEIVED,
  TWEET_STATUS_NO_CONNECTION,
  TWEET_STATUS_MESSAGE
} TweetStatusMode;

#define TWEET_WINDOW_GET_PRIVATE(obj)   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TWEET_TYPE_WINDOW, TweetWindowPrivate))

struct _TweetWindowPrivate
{
  GtkWidget *vbox;
  GtkWidget *menubar;

  GtkStatusIcon *status_icon;


  GtkUIManager *manager;
  GtkActionGroup *action_group;

#ifdef HAVE_NM_GLIB
  guint nm_id;
#endif
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

  if (priv->status_icon)
    {
      g_object_unref (priv->status_icon);
      priv->status_icon = NULL;
    }

  if (priv->action_group)
    {
      g_object_unref (priv->action_group);
      priv->action_group = NULL;
    }

#ifdef HAVE_NM_GLIB
  if (priv->nm_id)
    {
      TweetVBox *vbox = TWEET_VBOX (priv->vbox);
      libnm_glib_unregister_callback (vbox->nm_context, priv->nm_id);

      priv->nm_id = 0;
    }
#endif /* HAVE_NM_GLIB */

  G_OBJECT_CLASS (tweet_window_parent_class)->dispose (gobject);
}

static void
on_status_icon_activate (GtkStatusIcon *icon,
                         TweetWindow   *window)
{
  gtk_window_present (GTK_WINDOW (window));

  gtk_status_icon_set_visible (window->priv->status_icon, FALSE);
}

static void
tweet_window_status_message (TweetWindow     *window,
                             TweetStatusMode  status_mode,
                             const gchar     *format,
                             ...)
{
  TweetWindowPrivate *priv = window->priv;
  gboolean show_status_icon = FALSE;
  va_list args;
  gchar *message;

  if (!priv->status_icon)
    {
      priv->status_icon = gtk_status_icon_new_from_icon_name ("tweet");
      g_signal_connect (priv->status_icon,
                        "activate", G_CALLBACK (on_status_icon_activate),
                        window);
    }


  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  switch (status_mode)
    {
    case TWEET_STATUS_MESSAGE:
      break;

    case TWEET_STATUS_ERROR:
      gtk_status_icon_set_from_icon_name (priv->status_icon, "tweet-error");
      gtk_status_icon_set_tooltip (priv->status_icon, message);
      show_status_icon = TRUE;
      break;

    case TWEET_STATUS_NO_CONNECTION:
      gtk_status_icon_set_from_icon_name (priv->status_icon, "tweet-no-connection");
      gtk_status_icon_set_tooltip (priv->status_icon, message);
      show_status_icon = TRUE;
      break;

    case TWEET_STATUS_RECEIVED:
      gtk_status_icon_set_from_icon_name (priv->status_icon, "tweet-new-status");
      gtk_status_icon_set_tooltip (priv->status_icon, message);
      show_status_icon = TRUE;
      break;
    }

  g_free (message);

  /* avoid flickering the status icon if we have focus */
  if (show_status_icon && !GTK_WIDGET_HAS_FOCUS (window))
    gtk_status_icon_set_visible (priv->status_icon, TRUE);
}

static void
on_status_received (TwitterClient *client,
                    TwitterStatus *status,
                    const GError  *error,
                    TweetWindow   *window)
{
  if (error)
    {
      /* if the content was not modified since the last update,
       * silently ignore the error; Twitter-GLib still emits it
       * so that clients can notify the user anyway
       */
      if (error->domain != TWITTER_ERROR ||
          error->code != TWITTER_ERROR_NOT_MODIFIED)
        {
          tweet_window_status_message (window, TWEET_STATUS_ERROR,
                                       _("Unable to retrieve status from Twitter: %s"),
                                       error->message);
        }
    }
}

static void
on_timeline_complete (TwitterClient *client,
                      TweetWindow   *window)
{
  TweetWindowPrivate *priv = window->priv;

  if (TWEET_VBOX (priv->vbox)->n_status_received > 0)
    {
      gchar *msg;

      msg = g_strdup_printf (ngettext ("Received a new status",
                                       "Received %d new statuses",
                                       TWEET_VBOX (priv->vbox)->n_status_received),
                             TWEET_VBOX (priv->vbox)->n_status_received);

      tweet_window_status_message (window, TWEET_STATUS_RECEIVED, msg);

      g_free (msg);
    }
}

static void
on_user_received (TwitterClient *client,
                  TwitterUser   *user,
                  const GError  *error,
                  TweetWindow   *window)
{
  if (error)
    {
      tweet_window_status_message (window, TWEET_STATUS_ERROR,
                                   _("Unable to retrieve user `%s': %s"),
                                   tweet_config_get_username (tweet_config_get_default ()),
                                   error->message);
    }
}

#ifdef HAVE_NM_GLIB
static void
nm_context_callback (libnm_glib_ctx *libnm_ctx,
                     gpointer        user_data)
{
  TweetWindow *window = user_data;
  TweetWindowPrivate *priv = window->priv;
  libnm_glib_state nm_state;

  nm_state = libnm_glib_get_network_state (libnm_ctx);

  if (nm_state == TWEET_VBOX (priv->vbox)->nm_state)
    return;

  switch (nm_state)
    {
    case LIBNM_NO_NETWORK_CONNECTION:
      tweet_window_status_message (window, TWEET_STATUS_NO_CONNECTION,
                                   _("No network connection available"));
      break;

    case LIBNM_ACTIVE_NETWORK_CONNECTION:
    case LIBNM_NO_DBUS:
    case LIBNM_NO_NETWORKMANAGER:
    case LIBNM_INVALID_CONTEXT:
      break;
    }
}
#endif /* HAVE_NM_GLIB */

static void
tweet_window_constructed (GObject *gobject)
{
  TweetWindow *window = TWEET_WINDOW (gobject);
  TweetWindowPrivate *priv = window->priv;
  TweetVBox *vbox = TWEET_VBOX (priv->vbox);
#ifdef HAVE_NM_GLIB
  if (vbox->nm_state == LIBNM_ACTIVE_NETWORK_CONNECTION)
    {
      g_signal_connect (vbox->client,
                        "user-received", G_CALLBACK (on_user_received),
                        window);
    }
  else
    {
      tweet_window_status_message (window, TWEET_STATUS_NO_CONNECTION,
                                   _("No network connection available"));
    }

  priv->nm_id = libnm_glib_register_callback (vbox->nm_context,
                                              nm_context_callback,
                                              window,
                                              NULL);
#else
    g_signal_connect (vbox->client,
                      "user-received", G_CALLBACK (on_user_received),
                      window);
#endif /* HAVE_NM_GLIB */
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
  tweet_vbox_set_mode (TWEET_VBOX (window->priv->vbox),
                       TWEET_MODE_RECENT);
}

static void
tweet_window_cmd_view_replies (GtkAction   *action,
                               TweetWindow *window)
{
  tweet_vbox_set_mode (TWEET_VBOX (window->priv->vbox),
                       TWEET_MODE_REPLIES);
}

static void
tweet_window_cmd_view_archive (GtkAction   *action,
                               TweetWindow *window)
{
  tweet_vbox_set_mode (TWEET_VBOX (window->priv->vbox),
                       TWEET_MODE_ARCHIVE);
}

static void
tweet_window_cmd_view_favorites (GtkAction   *action,
                                 TweetWindow *window)
{
  tweet_vbox_set_mode (TWEET_VBOX (window->priv->vbox),
                       TWEET_MODE_FAVORITES);
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
tweet_window_cmd_help_about (GtkAction   *action,
                             TweetWindow *window)
{
  tweet_vbox_show_about_dialog (window->priv->vbox);
}

static gboolean
tweet_window_focus_in (GtkWidget     *widget,
                       GdkEventFocus *event)
{
  TweetWindowPrivate *priv = TWEET_WINDOW (widget)->priv;

  if (priv->status_icon)
    gtk_status_icon_set_visible (priv->status_icon, FALSE);

  return GTK_WIDGET_CLASS (tweet_window_parent_class)->focus_in_event (widget, event);
}

static void
tweet_window_class_init (TweetWindowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TweetWindowPrivate));

  gobject_class->constructed = tweet_window_constructed;
  gobject_class->dispose = tweet_window_dispose;

  widget_class->focus_in_event = tweet_window_focus_in;
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
  gtk_window_set_default_size (GTK_WINDOW (window), TWEET_VBOX_WIDTH, 600);
  gtk_window_set_title (GTK_WINDOW (window), "Tweet");

  window->priv = priv = TWEET_WINDOW_GET_PRIVATE (window);

  priv->vbox = tweet_vbox_new ();
  gtk_container_add (GTK_CONTAINER (window), priv->vbox);
  gtk_widget_show (priv->vbox);

  TWEET_VBOX (priv->vbox)->mode = TWEET_MODE_RECENT;

  g_signal_connect (TWEET_VBOX (priv->vbox)->client,
                    "status-received", G_CALLBACK (on_status_received),
                    window);

  g_signal_connect (TWEET_VBOX (priv->vbox)->client,
                    "timeline-complete", G_CALLBACK (on_timeline_complete),
                    window);

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
     g_critical ("Building menus failed: %s", error->message);
     g_error_free (error);
    }
  else
    {
      priv->menubar = gtk_ui_manager_get_widget (priv->manager, "/TweetMenubar");
      gtk_box_pack_start (GTK_BOX (priv->vbox), priv->menubar, FALSE, FALSE, 0);
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
