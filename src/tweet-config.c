/* tweet-config.c: Configuration object
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

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <json-glib/json-glib.h>
#include <json-glib/json-gobject.h>

#ifdef USE_GNOME_KEYRING
#include <gnome-keyring.h>
#include <gnome-keyring-memory.h>
#endif

#include "tweet-config.h"

#define TWEET_CONFIG_GET_PRIVATE(obj)   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TWEET_TYPE_CONFIG, TweetConfigPrivate))

struct _TweetConfigPrivate
{
  gchar *username;
  gchar *password;

  guint refresh_time;

  guint use_gtk_bg : 1;
};

enum
{
  PROP_0,

  PROP_USERNAME,
#ifndef USE_GNOME_KEYRING
  PROP_PASSWORD,
#endif
  PROP_REFRESH_TIME,
  PROP_USE_GTK_BG
};

enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint config_signals[LAST_SIGNAL] = { 0, };

static TweetConfig *default_config = NULL;

G_DEFINE_TYPE (TweetConfig, tweet_config, G_TYPE_OBJECT);

static void
tweet_config_finalize (GObject *gobject)
{
  TweetConfigPrivate *priv = TWEET_CONFIG (gobject)->priv;

#ifdef USE_GNOME_KEYRING
  gnome_keyring_memory_free (priv->username);
  gnome_keyring_memory_free (priv->password);
#else
  g_free (priv->username);
  g_free (priv->password);
#endif

  G_OBJECT_CLASS (tweet_config_parent_class)->finalize (gobject);
}

static void
tweet_config_set_property (GObject      *gobject,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  TweetConfig *config = TWEET_CONFIG (gobject);

  switch (prop_id)
    {
    case PROP_USERNAME:
      tweet_config_set_username (config, g_value_get_string (value));
      break;

#ifndef USE_GNOME_KEYRING
    case PROP_PASSWORD:
      tweet_config_set_password (config, g_value_get_string (value));
      break;
#endif

    case PROP_REFRESH_TIME:
      tweet_config_set_refresh_time (config, g_value_get_int (value));
      break;

    case PROP_USE_GTK_BG:
      tweet_config_set_use_gtk_bg (config, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tweet_config_get_property (GObject    *gobject,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  TweetConfigPrivate *priv = TWEET_CONFIG (gobject)->priv;

  switch (prop_id)
    {
    case PROP_USERNAME:
      g_value_set_string (value, priv->username);
      break;

#ifndef USE_GNOME_KEYRING
    case PROP_PASSWORD:
      g_value_set_string (value, priv->password);
      break;
#endif

    case PROP_REFRESH_TIME:
      g_value_set_int (value, priv->refresh_time);
      break;

    case PROP_USE_GTK_BG:
      g_value_set_boolean (value, priv->use_gtk_bg);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tweet_config_class_init (TweetConfigClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TweetConfigPrivate));

  gobject_class->set_property = tweet_config_set_property;
  gobject_class->get_property = tweet_config_get_property;
  gobject_class->finalize = tweet_config_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_USERNAME,
                                   g_param_spec_string ("username",
                                                        "Username",
                                                        "Username",
                                                        NULL,
                                                        G_PARAM_READWRITE));
#ifndef USE_GNOME_KEYRING
  g_object_class_install_property (gobject_class,
                                   PROP_PASSWORD,
                                   g_param_spec_string ("password",
                                                        "Password",
                                                        "Password",
                                                        NULL,
                                                        G_PARAM_READWRITE));
#endif
  g_object_class_install_property (gobject_class,
                                   PROP_REFRESH_TIME,
                                   g_param_spec_int ("refresh-time",
                                                     "Refresh Time",
                                                     "Refresh interval",
                                                     -1, G_MAXINT,
                                                     300,
                                                     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_USE_GTK_BG,
                                   g_param_spec_boolean ("use-gtk-bg",
                                                         "Use GTK BG",
                                                         "Use GTK Background color",
                                                         TRUE,
                                                         G_PARAM_READWRITE));

  config_signals[CHANGED] =
    g_signal_new (g_intern_static_string ("changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TweetConfigClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
tweet_config_init (TweetConfig *config)
{
  config->priv = TWEET_CONFIG_GET_PRIVATE (config);

  config->priv->refresh_time = 300;
  config->priv->use_gtk_bg = TRUE;
}

TweetConfig *
tweet_config_get_default (void)
{
  if (G_UNLIKELY (default_config == NULL))
    {
      gchar *filename;
      gchar *contents;
      gsize length;
      GError *error = NULL;

      filename = g_build_filename (g_get_user_config_dir (),
                                   "tweet",
                                   "tweet.json",
                                   NULL);

      if (g_file_get_contents (filename, &contents, &length, &error))
        default_config = TWEET_CONFIG (json_construct_gobject (TWEET_TYPE_CONFIG,
                                                               contents, length,
                                                               &error));

      if (error)
        {
          /* report every error that it's not "file not found" */
          if (!(error->domain == G_FILE_ERROR &&
                error->code == G_FILE_ERROR_NOENT))
            {
              g_warning ("Unable to load configuration file at `%s': %s",
                         filename,
                         error->message);
            }

          g_error_free (error);
          default_config = NULL;
        }

      /* no configuration object - fall back to an empty one */
      if (!default_config)
        default_config = g_object_new (TWEET_TYPE_CONFIG, NULL);

#ifdef USE_GNOME_KEYRING
      TweetConfigPrivate *priv = TWEET_CONFIG_GET_PRIVATE (default_config);
      GnomeKeyringResult result;
      GList *passwords = NULL;

      result =
        gnome_keyring_find_network_password_sync (priv->username,
                                                  NULL, /* domain */
                                                  "twitter.com",
                                                  NULL, /* object */
                                                  "http",
                                                  "basic",
                                                  80,
                                                  &passwords);
      if (result == GNOME_KEYRING_RESULT_OK)
        {
          if (g_list_length (passwords) > 0)
            {
              GnomeKeyringNetworkPasswordData *data =
                (GnomeKeyringNetworkPasswordData*)passwords->data;

              tweet_config_set_password (default_config, data->password);
            }
          else
            {
              g_warning ("No password found.");
            }
           gnome_keyring_network_password_list_free (passwords);
        }
      else
        {
          g_warning ("Unable to retrieve password: %s",
                     gnome_keyring_result_to_message (result));
        }
#endif

      g_free (contents);
      g_free (filename);
    }

  return default_config;
}

void
tweet_config_set_username (TweetConfig *config,
                           const gchar *username)
{
  TweetConfigPrivate *priv;

  g_return_if_fail (TWEET_IS_CONFIG (config));
  g_return_if_fail (username != NULL);

  priv = config->priv;

#ifdef USE_GNOME_KEYRING
  gnome_keyring_memory_free (priv->username);
  priv->username = gnome_keyring_memory_strdup (username);
#else
  g_free (priv->username);
  priv->username = g_strdup (username);
#endif

  g_object_notify (G_OBJECT (config), "username");
}

G_CONST_RETURN gchar *
tweet_config_get_username (TweetConfig *config)
{
  g_return_val_if_fail (TWEET_IS_CONFIG (config), NULL);

  return config->priv->username;
}

void
tweet_config_set_password (TweetConfig *config,
                           const gchar *password)
{
  TweetConfigPrivate *priv;

  g_return_if_fail (TWEET_IS_CONFIG (config));
  g_return_if_fail (password != NULL);

  priv = config->priv;

#ifdef USE_GNOME_KEYRING
  gnome_keyring_memory_free (priv->password);
  priv->password = gnome_keyring_memory_strdup (password);
#else
  g_free (priv->password);
  priv->password = g_strdup (password);

  g_object_notify (G_OBJECT (config), "password");
#endif
}

G_CONST_RETURN gchar *
tweet_config_get_password (TweetConfig *config)
{
  g_return_val_if_fail (TWEET_IS_CONFIG (config), NULL);

  return config->priv->password;
}

void
tweet_config_set_refresh_time (TweetConfig *config,
                               gint         seconds)
{
  TweetConfigPrivate *priv;

  g_return_if_fail (TWEET_IS_CONFIG (config));

  priv = config->priv;

  if (priv->refresh_time != seconds)
    {
      priv->refresh_time = seconds;

      g_object_notify (G_OBJECT (config), "refresh-time");
    }
}

gint
tweet_config_get_refresh_time (TweetConfig *config)
{
  g_return_val_if_fail (TWEET_IS_CONFIG (config), 0);

  return config->priv->refresh_time;
}

void
tweet_config_set_use_gtk_bg (TweetConfig *config,
                             gboolean     value)
{
  TweetConfigPrivate *priv;

  g_return_if_fail (TWEET_IS_CONFIG (config));

  priv = config->priv;

  if (priv->use_gtk_bg != value)
    {
      priv->use_gtk_bg = value;

      g_object_notify (G_OBJECT (config), "use-gtk-bg");
    }
}

gboolean
tweet_config_get_use_gtk_bg (TweetConfig *config)
{
  g_return_val_if_fail (TWEET_IS_CONFIG (config), FALSE);

  return config->priv->use_gtk_bg;
}

#ifdef USE_GNOME_KEYRING
static void
store_password_complete (GnomeKeyringResult result, guint32 value)
{
  if (result != GNOME_KEYRING_RESULT_OK)
    {
      g_warning ("Unable to save password: %s",
                 gnome_keyring_result_to_message (result));
    }
}
#endif

void
tweet_config_save (TweetConfig *config)
{
  gchar *conf_dir;
  gchar *filename;
  gchar *buffer;
  gsize len;
  GError *error;

  conf_dir = g_build_filename (g_get_user_config_dir (), "tweet", NULL);
  if (g_mkdir_with_parents (conf_dir, 0700) == -1)
    {
      if (errno != EEXIST)
        g_warning ("Unable to create the configuration directory: %s",
                   g_strerror (errno));
      g_free (conf_dir);
      return;
    }

  filename = g_build_filename (conf_dir, "tweet.json", NULL);

  buffer = json_serialize_gobject (G_OBJECT (config), &len);
  if (!buffer)
    {
      g_warning ("Unable to serialize the configuration");
      g_free (filename);
      g_free (conf_dir);
      return;
    }

  error = NULL;
  g_file_set_contents (filename, buffer, len, &error);
  if (error)
    {
      g_warning ("Unable to save configuration file: %s", error->message);
      g_error_free (error);
    }

  if (g_chmod (filename, 0600) == -1)
    {
      g_warning ("Unable to set the permissions on the file: %s\n"
                 "The configuration file is readable to everyone.",
                 g_strerror (errno));
    }

#ifdef USE_GNOME_KEYRING
  gnome_keyring_set_network_password (GNOME_KEYRING_DEFAULT, /* keyring destination */
                                      tweet_config_get_username (config),
                                      NULL, /* domain */
                                      "twitter.com",
                                      NULL, /* object */
                                      "http",
                                      "basic",
                                      80,
                                      tweet_config_get_password (config),
                                      (GnomeKeyringOperationGetIntCallback)store_password_complete,
                                      NULL, NULL);
#endif

  g_free (buffer);
  g_free (filename);
  g_free (conf_dir);
}
