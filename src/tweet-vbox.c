/* tweet-vbox.c: Tweet interaction pane/box
 *
 * This file is part of Tweet.
 * Copyright (C) 2008  Emmanuele Bassi  <ebassi@gnome.org>
 *                     Mark Lee  <gnome@lazymalevolence.com>
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

#ifdef HAVE_NM_GLIB
#include <libnm_glib.h>
#endif

#include <gtk/gtk.h>

#include <clutter/clutter.h>
#include <clutter-gtk/gtk-clutter-embed.h>

#include <tidy/tidy-finger-scroll.h>
#include <tidy/tidy-stylable.h>

#include <twitter-glib/twitter-glib.h>

#include "tweet-animation.h"
#include "tweet-config.h"
#include "tweet-spinner.h"
#include "tweet-status-info.h"
#include "tweet-status-model.h"
#include "tweet-status-view.h"
#include "tweet-utils.h"
#include "tweet-vbox.h"

#define TWEET_VBOX_GET_PRIVATE(obj)   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TWEET_TYPE_VBOX, TweetVBoxPrivate))

struct _TweetVBoxPrivate
{
  GtkWidget *canvas;
  GtkWidget *entry;
  GtkWidget *send_button;
  GtkWidget *counter;

  ClutterActor *spinner;
  ClutterActor *status_view;
  ClutterActor *scroll;
  ClutterActor *info;

  TwitterUser *user;

  TweetConfig *config;
  TweetStatusModel *status_model;

  gint press_x;
  gint press_y;
  gint press_row;
  guint in_press : 1;

#ifdef HAVE_NM_GLIB
  libnm_glib_ctx *nm_context;
  guint nm_id;
  libnm_glib_state nm_state;
#endif
};

G_DEFINE_TYPE (TweetVBox, tweet_vbox, GTK_TYPE_VBOX);

static void
tweet_vbox_dispose (GObject *gobject)
{
  TweetVBox *vbox = TWEET_VBOX (gobject);
  TweetVBoxPrivate *priv = vbox->priv;

  if (vbox->refresh_id)
    {
      g_source_remove (vbox->refresh_id);
      vbox->refresh_id = 0;
    }

  if (priv->user)
    {
      g_object_unref (priv->user);
      priv->user = NULL;
    }

  if (vbox->client)
    {
      twitter_client_end_session (vbox->client);
      g_object_unref (vbox->client);
      vbox->client = NULL;
    }

  if (priv->status_model)
    {
      g_object_unref (priv->status_model);
      priv->status_model = NULL;
    }

#ifdef HAVE_NM_GLIB
  if (priv->nm_id)
    {
      libnm_glib_unregister_callback (priv->nm_context, priv->nm_id);
      libnm_glib_shutdown (priv->nm_context);

      priv->nm_id = 0;
      priv->nm_context = NULL;
    }
#endif /* HAVE_NM_GLIB */

  G_OBJECT_CLASS (tweet_vbox_parent_class)->dispose (gobject);
}

static void
on_status_received (TwitterClient *client,
                    TwitterStatus *status,
                    const GError  *error,
                    TweetVBox   *vbox)
{
  TweetVBoxPrivate *priv = vbox->priv;

  if (error)
    {
      tweet_spinner_stop (TWEET_SPINNER (priv->spinner));
      tweet_actor_animate (priv->spinner, TWEET_LINEAR, 500,
                           "opacity", tweet_interval_new (G_TYPE_UCHAR, 127, 0),
                           NULL);

      /* if the content was not modified since the last update,
       * silently ignore the error; Twitter-GLib still emits it
       * so that clients can notify the user anyway
       */
      if (error->domain == TWITTER_ERROR &&
          error->code == TWITTER_ERROR_NOT_MODIFIED)
        {
          g_get_current_time (&vbox->last_update);
          return;
        }

      vbox->n_status_received = 0;

      /* TODO change into libnotify msg? */
      g_warning ("Unable to retrieve status from Twitter: %s", error->message);
    }
  else
    {
      if (!priv->status_model)
        {
          priv->status_model = TWEET_STATUS_MODEL (tweet_status_model_new ());
          tidy_list_view_set_model (TIDY_LIST_VIEW (priv->status_view),
                                    CLUTTER_MODEL (priv->status_model));
        }

      if (tweet_status_model_prepend_status (priv->status_model, status));
        vbox->n_status_received += 1;
    }
}

static void
on_timeline_complete (TwitterClient *client,
                      TweetVBox   *vbox)
{
  TweetVBoxPrivate *priv = vbox->priv;

  tweet_spinner_stop (TWEET_SPINNER (priv->spinner));
  tweet_actor_animate (priv->spinner, TWEET_LINEAR, 500,
                       "opacity", tweet_interval_new (G_TYPE_UCHAR, 127, 0),
                       NULL);

  /* TODO add libnotify msg of # of new statuses received? */

  vbox->n_status_received = 0;

  g_get_current_time (&vbox->last_update);
}

static void
on_entry_changed (GtkEntry *entry,
                  TweetVBox *vbox)
{
  TweetVBoxPrivate *priv = vbox->priv;
  const gchar *status_text = gtk_entry_get_text (entry);
  const gchar *color;
  gchar *count_text;

  if (strlen (status_text) == 0)
    {
      gtk_widget_set_sensitive (priv->send_button, FALSE);
      color = "green";
    }
  else
    {
      gtk_widget_set_sensitive (priv->send_button, TRUE);

      if (strlen (status_text) < 140)
        color = "green";
      else
        color = "red";
    }

  count_text = g_strdup_printf ("<span color='%s'>%d</span>",
                                color,
                                strlen (status_text));

  gtk_label_set_text (GTK_LABEL (priv->counter), count_text);
  gtk_label_set_use_markup (GTK_LABEL (priv->counter), TRUE);

  g_free (count_text);
}

static void
on_entry_activate (GtkEntry *entry,
                   TweetVBox *vbox)
{
  const gchar *text;

  text = gtk_entry_get_text (entry);
  if (!text || *text == '\0')
    return;

  twitter_client_add_status (vbox->client, text);

  gtk_entry_set_text (entry, "");
}

static void
on_info_destroy (TweetAnimation *animation,
                 TweetVBox    *vbox)
{
  clutter_actor_destroy (vbox->priv->info);
  vbox->priv->info = NULL;
}

static gboolean
on_info_button_press (ClutterActor       *actor,
                      ClutterButtonEvent *event,
                      TweetVBox        *vbox)
{
  TweetVBoxPrivate *priv = vbox->priv;
  TweetAnimation *animation;

  animation =
    tweet_actor_animate (actor, TWEET_LINEAR, 250,
                         "opacity", tweet_interval_new (G_TYPE_UCHAR, 224, 0),
                         NULL);
  g_signal_connect (animation,
                    "completed", G_CALLBACK (on_info_destroy),
                    vbox);

  clutter_actor_set_reactive (priv->status_view, TRUE);
  tweet_actor_animate (priv->status_view, TWEET_LINEAR, 250,
                       "opacity", tweet_interval_new (G_TYPE_UCHAR, 128, 255),
                       NULL);

  return TRUE;
}

static void
on_star_clicked (TweetStatusInfo *info,
                 TweetVBox     *vbox)
{
  TwitterStatus *status;

  status = tweet_status_info_get_status (info);
  if (!status)
    return;

  twitter_client_add_favorite (vbox->client, twitter_status_get_id (status));
}

static void
on_reply_clicked (TweetStatusInfo *info,
                  TweetVBox     *vbox)
{
  TweetVBoxPrivate *priv = vbox->priv;
  TwitterStatus *status;
  TwitterUser *user;
  gchar *reply_to;

  status = tweet_status_info_get_status (info);
  if (!status)
    return;

  user = twitter_status_get_user (status);
  if (!user)
    return;

  reply_to = g_strdup_printf ("@%s ", twitter_user_get_screen_name (user));

  gtk_entry_set_text (GTK_ENTRY (priv->entry), reply_to);
  gtk_editable_set_position (GTK_EDITABLE (priv->entry), -1);

  g_free (reply_to);
}

static void
on_icon_clicked (TweetStatusInfo *info,
                 TweetVBox       *vbox)
{
  TwitterStatus *status;
  TwitterUser *user;
  GdkScreen *screen;
  gint pid;
  GError *error;
  gchar **argv;

  status = tweet_status_info_get_status (info);
  g_assert (TWITTER_IS_STATUS (status));

  user = twitter_status_get_user (status);
  if (!user)
    return;

  if (gtk_widget_has_screen (GTK_WIDGET (vbox)))
    screen = gtk_widget_get_screen (GTK_WIDGET (vbox));
  else
    screen = gdk_screen_get_default ();

  argv = g_new (gchar*, 3);
  argv[0] = g_strdup ("xdg-open");
  argv[1] = g_strdup_printf ("http://twitter.com/%s",
                             twitter_user_get_screen_name (user));
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
      g_critical ("Unable to launch xdg-open: %s", error->message);
      g_error_free (error);
    }

  g_strfreev (argv);
}

static void
on_status_info_visible (TweetAnimation *animation,
                        TweetVBox    *vbox)
{
  vbox->priv->in_press = FALSE;
  clutter_actor_set_reactive (vbox->priv->info, TRUE);
}

static gboolean
on_status_view_button_press (ClutterActor       *actor,
                             ClutterButtonEvent *event,
                             TweetVBox        *vbox)
{
  TweetVBoxPrivate *priv = vbox->priv;
  gint row;

  /* this should not happen, but just in case... */
  if (priv->info)
    return FALSE;

  row = tidy_list_view_get_row_at_pos (TIDY_LIST_VIEW (actor),
                                       event->x,
                                       event->y);

  if (row >= 0)
    {
      priv->in_press = TRUE;

      priv->press_x = event->x;
      priv->press_y = event->y;
      priv->press_row = row;
    }

  return FALSE;
}

static gboolean
on_status_view_button_release (ClutterActor       *actor,
                               ClutterButtonEvent *event,
                               TweetVBox        *vbox)
{
  TweetVBoxPrivate *priv = vbox->priv;

/* in case of a crappy touchscreen */
#define JITTER  5

  if (priv->in_press)
    {
      TweetAnimation *animation;
      TwitterStatus *status;
      ClutterModelIter *iter;
      ClutterGeometry geometry = { 0, };
      ClutterActor *stage;
      ClutterColor info_color = { 255, 255, 255, 255 };

      priv->in_press = FALSE;

      if (abs (priv->press_y - event->y) > JITTER)
        {
          priv->in_press = FALSE;
          return FALSE;
        }

      iter = clutter_model_get_iter_at_row (CLUTTER_MODEL (priv->status_model),
                                            priv->press_row);
      if (!iter)
        {
          priv->in_press = FALSE;
          return FALSE;
        }

      status = tweet_status_model_get_status (priv->status_model, iter);
      if (!status)
        {
          g_object_unref (iter);
          priv->in_press = FALSE;
          return FALSE;
        }

      tweet_status_view_get_cell_geometry (TWEET_STATUS_VIEW (priv->status_view),
                                           priv->press_row,
                                           TRUE,
                                           &geometry);

      stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (priv->canvas));

      priv->info = tweet_status_info_new (status);
      tweet_overlay_set_color (TWEET_OVERLAY (priv->info), &info_color);
      g_signal_connect (priv->info,
                        "button-press-event", G_CALLBACK (on_info_button_press),
                        vbox);
      g_signal_connect (priv->info,
                        "star-clicked", G_CALLBACK (on_star_clicked),
                        vbox);
      g_signal_connect (priv->info,
                        "reply-clicked", G_CALLBACK (on_reply_clicked),
                        vbox);
      g_signal_connect (priv->info,
                        "icon-clicked", G_CALLBACK (on_icon_clicked),
                        vbox);
                                
      clutter_container_add_actor (CLUTTER_CONTAINER (stage), priv->info);
      clutter_actor_set_position (priv->info,
                                  geometry.x + TWEET_CANVAS_PADDING,
                                  geometry.y + TWEET_CANVAS_PADDING);
      clutter_actor_set_size (priv->info, geometry.width - TWEET_CANVAS_PADDING, 16);
      clutter_actor_set_opacity (priv->info, 0);
      clutter_actor_set_reactive (priv->info, FALSE);
      clutter_actor_show (priv->info);

      /* the status info is non-reactive until it has
       * been fully "unrolled" by the animation
       */
      animation =
        tweet_actor_animate (priv->info, TWEET_LINEAR, 250,
                             "y", tweet_interval_new (G_TYPE_INT, geometry.y + TWEET_CANVAS_PADDING, 100 + TWEET_CANVAS_PADDING),
                             "height", tweet_interval_new (G_TYPE_INT, 16, (TWEET_CANVAS_HEIGHT - (100 * 2))),
                             "opacity", tweet_interval_new (G_TYPE_UCHAR, 0, 224),
                             NULL);
      g_signal_connect (animation,
                        "completed", G_CALLBACK (on_status_info_visible),
                        vbox);

      /* set the status view as not reactive to avoid opening
       * the status info on double tap
       */
      clutter_actor_set_reactive (priv->status_view, FALSE);
      tweet_actor_animate (priv->status_view, TWEET_LINEAR, 250,
                           "opacity", tweet_interval_new (G_TYPE_UCHAR, 255, 128),
                           NULL);

      g_object_unref (status);
      g_object_unref (iter);
    }

#undef JITTER

  return FALSE;
}

inline void
tweet_vbox_clear (TweetVBox *vbox)
{
  TweetVBoxPrivate *priv = vbox->priv;

  tidy_list_view_set_model (TIDY_LIST_VIEW (priv->status_view), NULL);
  g_object_unref (priv->status_model);
  priv->status_model = NULL;
}

inline void
tweet_vbox_refresh (TweetVBox *vbox)
{
  TweetVBoxPrivate *priv = vbox->priv;

  clutter_actor_show (priv->spinner);
  tweet_spinner_start (TWEET_SPINNER (priv->spinner));
  tweet_actor_animate (priv->spinner, TWEET_LINEAR, 500,
                       "opacity", tweet_interval_new (G_TYPE_UCHAR, 0, 127),
                       NULL);

  /* check for the user */
  if (!priv->user)
    twitter_client_show_user_from_email (vbox->client,
                                         tweet_config_get_username (priv->config));

  switch (vbox->mode)
    {
    case TWEET_MODE_RECENT:
      vbox->n_status_received = 0;
      twitter_client_get_friends_timeline (vbox->client,
                                           NULL,
                                           vbox->last_update.tv_sec);
      break;

    case TWEET_MODE_REPLIES:
      twitter_client_get_replies (vbox->client);
      break;

    case TWEET_MODE_ARCHIVE:
      twitter_client_get_user_timeline (vbox->client,
                                        NULL,
                                        0,
                                        vbox->last_update.tv_sec);
      break;

    case TWEET_MODE_FAVORITES:
      twitter_client_get_favorites (vbox->client, NULL, 0);
      break;
    }

}

gboolean
tweet_vbox_refresh_timeout (TweetVBox *vbox)
{
  tweet_vbox_refresh (vbox);

  return TRUE;
}

#ifdef HAVE_NM_GLIB
static void
on_user_received (TwitterClient *client,
                  TwitterUser   *user,
                  const GError  *error,
                  TweetVBox     *vbox)
{
  TweetVBoxPrivate *priv = vbox->priv;

  if (error)
    {
      priv->user = NULL;
      /* TODO change into libnotify msg? */
      g_warning ("Unable to retrieve user `%s': %s",
                 tweet_config_get_username (priv->config),
                 error->message);
      return;
    }

  if (priv->user)
    g_object_unref (priv->user);

  /* keep a reference on ourselves */
  priv->user = g_object_ref (user);
}

static void
nm_context_callback (libnm_glib_ctx *libnm_ctx,
                     gpointer        user_data)
{
  TweetVBox *vbox = user_data;
  TweetVBoxPrivate *priv = vbox->priv;
  libnm_glib_state nm_state;
  gint refresh_time;

  nm_state = libnm_glib_get_network_state (libnm_ctx);

  if (nm_state == priv->nm_state)
    return;

  switch (nm_state)
    {
    case LIBNM_ACTIVE_NETWORK_CONNECTION:
      refresh_time = tweet_config_get_refresh_time (priv->config);

      if (refresh_time > 0)
        {
          if (vbox->refresh_id)
            break;

          tweet_vbox_refresh (vbox);
          vbox->refresh_id = g_timeout_add_seconds (refresh_time,
                                                    tweet_vbox_refresh_timeout,
                                                    vbox);
        }
      else
        tweet_vbox_refresh (vbox);

      break;

    case LIBNM_NO_DBUS:
    case LIBNM_NO_NETWORKMANAGER:
      g_critical ("No NetworkManager running");
      break;

    case LIBNM_NO_NETWORK_CONNECTION:
      g_source_remove (vbox->refresh_id);
      vbox->refresh_id = 0;
      tweet_vbox_clear (vbox);
      break;

    case LIBNM_INVALID_CONTEXT:
      g_critical ("Invalid NetworkManager-GLib context");
      break;
    }

  priv->nm_state = nm_state;
}
#endif /* HAVE_NM_GLIB */

static void
tweet_vbox_constructed (GObject *gobject)
{
  TweetVBox *vbox = TWEET_VBOX (gobject);
  TweetVBoxPrivate *priv = vbox->priv;
  ClutterActor *stage;
  ClutterActor *img;
#ifdef HAVE_NM_GLIB
  libnm_glib_state nm_state;
#endif /* HAVE_NM_GLIB */

  stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (priv->canvas));

  img = tweet_texture_new_from_stock (GTK_WIDGET (vbox),
                                      GTK_STOCK_REFRESH,
                                      GTK_ICON_SIZE_DIALOG);
  if (!img)
    g_critical ("Unable to load the `%s' stock icon", GTK_STOCK_REFRESH);

  priv->spinner = tweet_spinner_new ();
  tweet_spinner_set_image (TWEET_SPINNER (priv->spinner), img);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), priv->spinner);
  clutter_actor_set_size (priv->spinner, 128, 128);
  clutter_actor_set_anchor_point (priv->spinner, 64, 64);
  clutter_actor_set_position (priv->spinner,
                              TWEET_VBOX_WIDTH / 2,
                              TWEET_CANVAS_HEIGHT / 2);
  clutter_actor_show (priv->spinner);
  tweet_spinner_start (TWEET_SPINNER (priv->spinner));
  tweet_actor_animate (priv->spinner, TWEET_LINEAR, 500,
                       "opacity", tweet_interval_new (G_TYPE_UCHAR, 0, 127),
                       NULL);

  gtk_widget_show_all (GTK_WIDGET (vbox));

#ifdef HAVE_NM_GLIB
  priv->nm_context = libnm_glib_init ();

  nm_state = libnm_glib_get_network_state (priv->nm_context);
  if (nm_state == LIBNM_ACTIVE_NETWORK_CONNECTION)
    {
      const gchar *email_address;
      gint refresh_time;

      g_signal_connect (vbox->client,
                        "user-received", G_CALLBACK (on_user_received),
                        vbox);

      email_address = tweet_config_get_username (priv->config);
      twitter_client_show_user_from_email (vbox->client, email_address);

      twitter_client_get_friends_timeline (vbox->client, NULL, 0);

      refresh_time = tweet_config_get_refresh_time (priv->config);
      if (refresh_time > 0)
        vbox->refresh_id = g_timeout_add_seconds (refresh_time,
                                                  (GSourceFunc)tweet_vbox_refresh_timeout,
                                                  vbox);
    }
  else
    {
      tweet_spinner_stop (TWEET_SPINNER (priv->spinner));
      tweet_actor_animate (priv->spinner, TWEET_LINEAR, 500,
                           "opacity", tweet_interval_new (G_TYPE_UCHAR, 127, 0),
                           NULL);
    }

  priv->nm_state = nm_state;
  priv->nm_id = libnm_glib_register_callback (priv->nm_context,
                                              nm_context_callback,
                                              vbox,
                                              NULL);
#else
  twitter_client_get_friends_timeline (vbox->client, NULL, 0);

  if (tweet_config_get_refresh_time (priv->config) > 0)
    vbox->refresh_id =
      g_timeout_add_seconds (tweet_config_get_refresh_time (priv->config),
                             (GSourceFunc)tweet_vbox_refresh_timeout,
                             vbox);
#endif /* HAVE_NM_GLIB */
}

void
tweet_vbox_set_mode (TweetVBox *vbox,
                     TweetMode  mode)
{
  if (vbox->mode == mode)
    return;

  vbox->mode = mode;
  vbox->last_update.tv_sec = 0;

  tweet_vbox_clear (vbox);
  tweet_vbox_refresh (vbox);
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
      g_critical ("Unable to launch xdg-open: %s", error->message);
      g_error_free (error);
    }

  g_strfreev (argv);
}

void
tweet_vbox_show_about_dialog (GtkWidget *vbox)
{
  GtkWindow *window;

  const gchar *authors[] = {
    "Emmanuele Bassi <ebassi@gnome.org>",
    NULL
  };

  const gchar *artists[] = {
    "Ulisse Perusin <uli.peru@gmail.com>",
    NULL
  };

  const gchar *translator_credits = _("translator-credits");
  const gchar *copyright = "Copyright \xc2\xa9 2008 Emmanuele Bassi";

  const gchar *license_text =
    _("This program is free software: you can redistribute it and/or "
      "modify it under the terms of the GNU General Public License as "
      "published by the Free Software Foundation, either version 3 of "
      "the License, or (at your option) any later version.");

  gtk_about_dialog_set_url_hook (about_url_hook, NULL, NULL);

  window = GTK_WINDOW (gtk_widget_get_parent (vbox));

  gtk_show_about_dialog (window,
                         "program-name", "Tweet",
                         "title", _("About Tweet"),
                         "comments", _("Twitter desktop client"),
                         "logo-icon-name", "tweet",
                         "version", VERSION,
                         "copyright", copyright,
                         "authors", authors,
                         "artists", artists,
                         "translator-credits", translator_credits,
                         "website", "http://live.gnome.org/Tweet",
                         "license", license_text,
                         "wrap-license", TRUE,
                         NULL);
}

static void
tweet_vbox_style_set (GtkWidget *widget,
                      GtkStyle  *old_style)
{
  TweetVBoxPrivate *priv = TWEET_VBOX (widget)->priv;
  ClutterColor active_color = { 0, };
  ClutterColor text_color = { 0, };
  ClutterColor bg_color = { 0, };
  gchar *font_name;

  tweet_widget_get_base_color (widget, GTK_STATE_SELECTED, &active_color);
  tweet_widget_get_text_color (widget, GTK_STATE_NORMAL, &text_color);
  tweet_widget_get_bg_color (widget, GTK_STATE_NORMAL, &bg_color);

  font_name = pango_font_description_to_string (widget->style->font_desc);

  tidy_stylable_set (TIDY_STYLABLE (priv->scroll),
                     "active-color", &active_color,
                     "bg-color", &bg_color,
                     NULL);
  tidy_stylable_set (TIDY_STYLABLE (priv->status_view),
                     "active-color", &active_color,
                     "bg-color", &bg_color,
                     "text-color", &text_color,
                     "font-name", font_name,
                     NULL);

  if (tweet_config_get_use_gtk_bg (priv->config))
    {
      ClutterActor *stage;

      stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (priv->canvas));
      clutter_stage_set_color (CLUTTER_STAGE (stage), &bg_color);
    }

  g_free (font_name);
}

static gboolean
on_canvas_focus_in (GtkWidget     *widget,
                    GdkEventFocus *event,
                    TweetVBox     *vbox)
{
  GtkClutterEmbed *embed = GTK_CLUTTER_EMBED (widget);
  ClutterActor *stage = gtk_clutter_embed_get_stage (embed);

  gtk_widget_queue_draw (widget);

  clutter_stage_set_key_focus (CLUTTER_STAGE (stage),
                               vbox->priv->status_view);

  gtk_widget_grab_focus (widget);

  return FALSE;
}

static gboolean
on_canvas_focus_out (GtkWidget     *widget,
                     GdkEventFocus *event,
                     TweetVBox     *vbox)
{
  GtkClutterEmbed *embed = GTK_CLUTTER_EMBED (widget);
  ClutterActor *stage = gtk_clutter_embed_get_stage (embed);

  gtk_widget_queue_draw (widget);

  clutter_stage_set_key_focus (CLUTTER_STAGE (stage), NULL);

  return FALSE;
}

static void
tweet_vbox_class_init (TweetVBoxClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TweetVBoxPrivate));

  gobject_class->dispose = tweet_vbox_dispose;
  gobject_class->constructed = tweet_vbox_constructed;

  widget_class->style_set = tweet_vbox_style_set;
}

static void
tweet_vbox_init (TweetVBox *vbox)
{
  TweetVBoxPrivate *priv;
  GtkWidget *frame, *hbox, *button;
  ClutterActor *stage, *view;
  ClutterColor stage_color = { 0, 0, 0, 255 };

  vbox->priv = priv = TWEET_VBOX_GET_PRIVATE (vbox);

  vbox->mode = TWEET_MODE_RECENT;

  priv->status_model = TWEET_STATUS_MODEL (tweet_status_model_new ());

  priv->config = tweet_config_get_default ();
  vbox->client = twitter_client_new_for_user (tweet_config_get_username (priv->config),
                                              tweet_config_get_password (priv->config));
  g_signal_connect (vbox->client,
                    "status-received", G_CALLBACK (on_status_received),
                    vbox);
  g_signal_connect (vbox->client,
                    "timeline-complete", G_CALLBACK (on_timeline_complete),
                    vbox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (vbox), frame);
  gtk_widget_show (frame);

  priv->canvas = gtk_clutter_embed_new ();
  g_signal_connect (priv->canvas,
                    "focus-in-event", G_CALLBACK (on_canvas_focus_in),
                    vbox);
  g_signal_connect (priv->canvas,
                    "focus-out-event", G_CALLBACK (on_canvas_focus_out),
                    vbox);
  GTK_WIDGET_SET_FLAGS (priv->canvas, GTK_CAN_FOCUS);

  gtk_widget_set_size_request (priv->canvas,
                               TWEET_CANVAS_WIDTH + TWEET_CANVAS_PADDING,
                               TWEET_CANVAS_HEIGHT + TWEET_CANVAS_PADDING);
  gtk_container_add (GTK_CONTAINER (frame), priv->canvas);

  stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (priv->canvas));
  gtk_widget_set_size_request (priv->canvas,
                               TWEET_CANVAS_WIDTH + TWEET_CANVAS_PADDING,
                               TWEET_CANVAS_HEIGHT + TWEET_CANVAS_PADDING);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_set_size (stage,
                          TWEET_CANVAS_WIDTH + TWEET_CANVAS_PADDING,
                          TWEET_CANVAS_HEIGHT + TWEET_CANVAS_PADDING);

  view = tweet_status_view_new (priv->status_model);
  g_signal_connect (view,
                    "button-press-event", G_CALLBACK (on_status_view_button_press),
                    vbox);
  g_signal_connect (view,
                    "button-release-event", G_CALLBACK (on_status_view_button_release),
                    vbox);
  priv->scroll = tidy_finger_scroll_new (TIDY_FINGER_SCROLL_MODE_KINETIC);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->scroll), view);
  clutter_actor_show (view);
  clutter_actor_set_reactive (view, TRUE);
  priv->status_view = view;

  clutter_actor_set_size (priv->scroll, TWEET_CANVAS_WIDTH, TWEET_CANVAS_HEIGHT);
  clutter_actor_set_position (priv->scroll, TWEET_CANVAS_PADDING, TWEET_CANVAS_PADDING);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), priv->scroll);
  clutter_actor_set_reactive (priv->scroll, TRUE);
  clutter_actor_show (priv->scroll);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  priv->entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), priv->entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (priv->entry, "Update your status");
  gtk_widget_show (priv->entry);
  g_signal_connect (priv->entry,
                    "activate", G_CALLBACK (on_entry_activate),
                    vbox);
  g_signal_connect (priv->entry,
                    "changed", G_CALLBACK (on_entry_changed),
                    vbox);

  priv->counter = gtk_label_new ("<span color='green'>0</span>");
  gtk_label_set_use_markup (GTK_LABEL (priv->counter), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), priv->counter, FALSE, FALSE, 0);
  gtk_widget_show (priv->counter);

  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("document-send",
                                                      GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (button, FALSE);
  gtk_widget_show (button);
  g_signal_connect_swapped (button,
                            "clicked", G_CALLBACK (gtk_widget_activate),
                            priv->entry);
  priv->send_button = button;
}

GtkWidget *
tweet_vbox_new (void)
{
  return g_object_new (TWEET_TYPE_VBOX, NULL);
}

GtkWidget *
tweet_vbox_get_canvas (TweetVBox *vbox)
{
  return TWEET_VBOX_GET_PRIVATE (vbox)->canvas;
}

TweetConfig *
tweet_vbox_get_config (TweetVBox *vbox)
{
  return TWEET_VBOX_GET_PRIVATE (vbox)->config;
}
