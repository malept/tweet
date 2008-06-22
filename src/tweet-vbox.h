/* tweet-vbox.h: Tweet interaction pane/box
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
#ifndef __TWEET_VBOX_H__
#define __TWEET_VBOX_H__

#include <gtk/gtkvbox.h>
#include "twitter-client.h"
#include "tweet-config.h"

G_BEGIN_DECLS

#define TWEET_CANVAS_WIDTH    350
#define TWEET_CANVAS_HEIGHT   500
#define TWEET_CANVAS_PADDING  6

#define TWEET_VBOX_WIDTH    (TWEET_CANVAS_WIDTH + (2 * TWEET_CANVAS_PADDING))

typedef enum {
  TWEET_MODE_RECENT,
  TWEET_MODE_REPLIES,
  TWEET_MODE_ARCHIVE,
  TWEET_MODE_FAVORITES
} TweetMode;

#define TWEET_TYPE_VBOX               (tweet_vbox_get_type ())
#define TWEET_VBOX(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), TWEET_TYPE_VBOX, TweetVBox))
#define TWEET_IS_VBOX(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TWEET_TYPE_VBOX))
#define TWEET_VBOX_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), TWEET_TYPE_VBOX, TweetVBoxClass))
#define TWEET_IS_VBOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), TWEET_TYPE_VBOX))
#define TWEET_VBOX_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), TWEET_TYPE_VBOX, TweetVBoxClass))

typedef struct _TweetVBox             TweetVBox;
typedef struct _TweetVBoxPrivate      TweetVBoxPrivate;
typedef struct _TweetVBoxClass        TweetVBoxClass;

struct _TweetVBox
{
  GtkVBox parent_instance;

  TweetVBoxPrivate *priv;

  TwitterClient *client;
  TweetMode mode;

  GTimeVal last_update;

  gint n_status_received;

  guint refresh_id;
};

struct _TweetVBoxClass
{
  GtkVBoxClass parent_class;
};

GType        tweet_vbox_get_type        (void) G_GNUC_CONST;
GtkWidget   *tweet_vbox_new             (void);
GtkWidget   *tweet_vbox_get_canvas      (TweetVBox *vbox);
TweetConfig *tweet_vbox_get_config      (TweetVBox *vbox);
void         tweet_vbox_set_mode        (TweetVBox *vbox, TweetMode mode);
inline void  tweet_vbox_clear           (TweetVBox *vbox);
inline void  tweet_vbox_refresh         (TweetVBox *vbox);
gboolean     tweet_vbox_refresh_timeout (TweetVBox *vbox);

G_END_DECLS

#endif
