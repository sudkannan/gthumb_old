/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2001, 2003 The Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 */

#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <libgnomeui/gnome-thumbnail.h>
#include "typedefs.h"
#include "file-data.h"

#define IMAGE_LOADER_TYPE            (image_loader_get_type ())
#define IMAGE_LOADER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IMAGE_LOADER_TYPE, ImageLoader))
#define IMAGE_LOADER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), IMAGE_LOADER_TYPE, ImageLoaderClass))
#define IS_IMAGE_LOADER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IMAGE_LOADER_TYPE))
#define IS_IMAGE_LOADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), IMAGE_LOADER_TYPE))
#define IMAGE_LOADER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), IMAGE_LOADER_TYPE, ImageLoaderClass))

typedef struct _ImageLoader            ImageLoader;
typedef struct _ImageLoaderClass       ImageLoaderClass;
typedef struct _ImageLoaderPrivateData ImageLoaderPrivateData;

struct _ImageLoader
{
	GObject                __parent;
	ImageLoaderPrivateData  *priv;
};

struct _ImageLoaderClass
{
	GObjectClass __parent_class;

	/* -- Signals -- */

	void (* image_error)    (ImageLoader *il);
	void (* image_done)     (ImageLoader *il);
	void (* image_progress) (ImageLoader *il,
				 float        percent);
};

typedef GdkPixbufAnimation * (*LoaderFunc) (FileData *file, GError **error, GnomeThumbnailFactory *, gpointer data);

GType                image_loader_get_type                (void);
GObject *            image_loader_new                     (gboolean           as_animation);
void                 image_loader_set_loader              (ImageLoader       *il,
						           LoaderFunc         loader,
						           gpointer           data);
void                 image_loader_set_file                (ImageLoader       *il,
						           FileData          *file);
FileData *           image_loader_get_file                (ImageLoader       *il);
void                 image_loader_set_path                (ImageLoader       *il,
						           const char        *path,
						           const char        *mime_type);
gchar *              image_loader_get_path                (ImageLoader       *il);
void                 image_loader_set_pixbuf              (ImageLoader       *il,
						           GdkPixbuf         *pixbuf);
GdkPixbuf *          image_loader_get_pixbuf              (ImageLoader       *il);
void                 image_loader_set_priority            (ImageLoader       *il,
						           int                priority);
GdkPixbufAnimation * image_loader_get_animation           (ImageLoader       *il);
gint                 image_loader_get_is_done             (ImageLoader       *il);
void                 image_loader_start                   (ImageLoader       *il);
void                 image_loader_stop                    (ImageLoader       *il,
						           DoneFunc           done_func,
						           gpointer           done_func_data);
void                 image_loader_stop_with_error         (ImageLoader       *il,
						           DoneFunc           done_func,
						           gpointer           done_func_data);
void                 image_loader_load_from_pixbuf_loader (ImageLoader       *il,
							   GdkPixbufLoader   *pl);
void                 image_loader_load_from_image_loader  (ImageLoader       *to,
							   ImageLoader       *from);

#endif /* IMAGE_LOADER_H */
