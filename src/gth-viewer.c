/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2005 Free Software Foundation, Inc.
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

#include <config.h>

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-mime.h>

#include <libexif/exif-data.h>
#include "jpegutils/jpeg-data.h"

#ifdef HAVE_LIBIPTCDATA
#include <libiptcdata/iptc-data.h>
#include <libiptcdata/iptc-jpeg.h>
#endif /* HAVE_LIBIPTCDATA */

#include "comments.h"
#include "dlg-save-image.h"
#include "dlg-categories.h"
#include "dlg-comment.h"
#include "file-data.h"
#include "file-utils.h"
#include "gconf-utils.h"
#include "glib-utils.h"
#include "gth-viewer.h"
#include "gth-viewer-ui.h"
#include "gth-exif-data-viewer.h"
#include "gth-exif-utils.h"
#include "gth-fullscreen.h"
#include "gtk-utils.h"
#include "main.h"
#include "gth-nav-window.h"
#include "image-viewer.h"
#include "preferences.h"
#include "rotation-utils.h"
#include "typedefs.h"

#include "icons/pixbufs.h"

#define GLADE_EXPORTER_FILE "gthumb_png_exporter.glade"
#define DEFAULT_WIN_WIDTH 200
#define DEFAULT_WIN_HEIGHT 400
#define DEFAULT_COMMENT_PANE_SIZE 100
#define DISPLAY_PROGRESS_DELAY 750
#define PANE_MIN_SIZE 60
#define GCONF_NOTIFICATIONS 5
#define OPEN_TOOLITEM_POS 0


GthViewer *SingleViewer = NULL;
GthViewer *LastFocusedViewer = NULL;


enum {
	TARGET_PLAIN,
	TARGET_PLAIN_UTF8,
	TARGET_URILIST,
};

static GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, TARGET_URILIST },
};


struct _GthViewerPrivateData {
	GtkUIManager    *ui;
	GtkWidget       *toolbar;
	GtkWidget       *statusbar;
	GtkWidget       *image_popup_menu;
	GtkWidget       *viewer;
	GtkWidget       *image_data_hpaned;
	GtkWidget       *image_comment;
	GtkWidget       *exif_data_viewer;
	GtkWidget       *image_info;            /* statusbar widgets. */
	GtkWidget       *image_info_frame;
	GtkWidget       *zoom_info;
	GtkWidget       *zoom_info_frame;

	GtkWidget       *image_main_pane;

	GtkWidget       *comment_button;
	GtkWidget       *fullscreen;

	GtkActionGroup  *actions;

	GtkToolItem     *open_with_tool_item;
	GtkWidget       *open_with_popup_menu;
	GtkToolItem     *rotate_tool_item;

	GtkTooltips     *tooltips;
        guint            help_message_cid;
        guint            image_info_cid;
        guint            progress_cid;

	gboolean         first_time_show;
	guint            first_timeout_handle;

	gboolean         image_data_visible;
	FileData        *image;
	ExifData        *exif_data;
	gboolean         image_error;

#ifdef HAVE_LIBIPTCDATA
	IptcData        *iptc_data;
#endif /* HAVE_LIBIPTCDATA */

	/* misc */

	guint            cnxn_id[GCONF_NOTIFICATIONS];

	/* save_pixbuf data */

	gboolean         image_modified;
	gboolean         saving_modified_image;
	ImageSavedFunc   image_saved_func;
	FileData        *new_image;

	GthPixbufOp     *pixop;
	gboolean         pixop_preview;
	gboolean         closing;

	/* progress dialog */

	GladeXML        *progress_gui;
	GtkWidget       *progress_dialog;
	GtkWidget       *progress_progressbar;
	GtkWidget       *progress_info;
	guint            progress_timeout;

	GdkPixbuf       *folder;
};

static GthWindowClass *parent_class = NULL;


static void
set_action_active (GthViewer   *viewer,
		   const char  *action_name,
		   gboolean     is_active)
{
	GtkAction *action;
	action = gtk_action_group_get_action (viewer->priv->actions, action_name);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), is_active);
}


static void
set_action_sensitive (GthViewer  *viewer,
	       const char *action_name,
	       gboolean    sensitive)
{
	GtkAction *action;
	action = gtk_action_group_get_action (viewer->priv->actions, action_name);
	g_object_set (action, "sensitive", sensitive, NULL);
}


static void
viewer_update_zoom_sensitivity (GthViewer *viewer)
{
	GthViewerPrivateData  *priv = viewer->priv;
	ImageViewer           *image_viewer = (ImageViewer*) priv->viewer;
	gboolean               image_is_visible;
	gboolean               image_is_void;
	int                    zoom;

	image_is_visible = (priv->image != NULL) && ! priv->image_error;
	image_is_void = image_viewer_is_void (image_viewer);
	zoom = (int) (image_viewer->zoom_level * 100.0);

	set_action_sensitive (viewer,
		       "View_Zoom100",
		       image_is_visible && !image_is_void && (zoom != 100));
	set_action_sensitive (viewer,
		       "View_ZoomIn",
		       image_is_visible && !image_is_void && (zoom != 10000));
	set_action_sensitive (viewer,
		       "View_ZoomOut",
		       image_is_visible && !image_is_void && (zoom != 5));
	set_action_sensitive (viewer,
		       "View_ZoomFit",
		       image_is_visible && !image_is_void);
        set_action_sensitive (viewer,
                       "View_ZoomWidth",
                       image_is_visible && !image_is_void);
}


static void
viewer_update_sensitivity (GthViewer *viewer)
{
	GthViewerPrivateData *priv = viewer->priv;
	gboolean image_is_void;
	gboolean image_is_ani;
	gboolean playing;

	image_is_void = image_viewer_is_void (IMAGE_VIEWER (priv->viewer));
	image_is_ani = image_viewer_is_animation (IMAGE_VIEWER (priv->viewer));
	playing = image_viewer_is_playing_animation (IMAGE_VIEWER (priv->viewer));

	set_action_sensitive (viewer, "File_Save", ! image_is_void && priv->image_modified);
	set_action_sensitive (viewer, "File_SaveAs", ! image_is_void);
	set_action_sensitive (viewer, "File_Revert", ! image_is_void && priv->image_modified);
	set_action_sensitive (viewer, "File_Print", ! image_is_void);
	set_action_sensitive (viewer, "ToolBar_Print", ! image_is_void);

	set_action_sensitive (viewer, "Edit_Undo", gth_window_get_can_undo (GTH_WINDOW (viewer)));
	set_action_sensitive (viewer, "Edit_Redo", gth_window_get_can_redo (GTH_WINDOW (viewer)));

	set_action_sensitive (viewer, "AlterImage_Rotate90", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Rotate90CC", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Flip", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Mirror", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Desaturate", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Resize", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_ColorBalance", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_HueSaturation", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_RedeyeRemoval", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_BrightnessContrast", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Invert", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Posterize", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Equalize", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_AdjustLevels", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Crop", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Dither_BW", ! image_is_void && ! image_is_ani);
	set_action_sensitive (viewer, "AlterImage_Dither_Web", ! image_is_void && ! image_is_ani);

	set_action_sensitive (viewer, "View_PlayAnimation", image_is_ani);
	set_action_sensitive (viewer, "View_StepAnimation", image_is_ani && ! playing);

	viewer_update_zoom_sensitivity (viewer);
}


static void
gth_viewer_finalize (GObject *object)
{
	GthViewer *viewer = GTH_VIEWER (object);
	int        i;

	debug (DEBUG_INFO, "Gth::Viewer::Finalize");

	if (viewer->priv != NULL) {
		GthViewerPrivateData *priv = viewer->priv;

		for (i = 0; i < GCONF_NOTIFICATIONS; i++)
			if (viewer->priv->cnxn_id[i] != -1)
				eel_gconf_notification_remove (viewer->priv->cnxn_id[i]);

		if (priv->exif_data != NULL) {
			exif_data_unref (priv->exif_data);
			priv->exif_data = NULL;
		}

#ifdef HAVE_LIBIPTCDATA
		if (priv->iptc_data != NULL) {
			iptc_data_unref (priv->iptc_data);
			priv->iptc_data = NULL;
		}
#endif /* HAVE_LIBIPTCDATA */

		file_data_unref (priv->image);
		file_data_unref (priv->new_image);

		g_free (viewer->priv);
		viewer->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
menu_item_select_cb (GtkMenuItem *proxy,
		     GthViewer   *viewer)
{
        GtkAction *action;
        char      *message;

        action = gtk_widget_get_action (GTK_WIDGET (proxy));
        g_return_if_fail (action != NULL);

        g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
        if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (viewer->priv->statusbar),
				    viewer->priv->help_message_cid, message);
		g_free (message);
        }
}


static void
menu_item_deselect_cb (GtkMenuItem *proxy,
		       GthViewer   *viewer)
{
        gtk_statusbar_pop (GTK_STATUSBAR (viewer->priv->statusbar),
                           viewer->priv->help_message_cid);
}


static void
disconnect_proxy_cb (GtkUIManager *manager,
                     GtkAction    *action,
                     GtkWidget    *proxy,
                     GthViewer    *viewer)
{
        if (GTK_IS_MENU_ITEM (proxy)) {
                g_signal_handlers_disconnect_by_func
                        (proxy, G_CALLBACK (menu_item_select_cb), viewer);
                g_signal_handlers_disconnect_by_func
                        (proxy, G_CALLBACK (menu_item_deselect_cb), viewer);
        }
}


static void
connect_proxy_cb (GtkUIManager *manager,
                  GtkAction    *action,
                  GtkWidget    *proxy,
		  GthViewer    *viewer)
{
        if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_connect (proxy, "select",
				  G_CALLBACK (menu_item_select_cb), viewer);
		g_signal_connect (proxy, "deselect",
				  G_CALLBACK (menu_item_deselect_cb), viewer);
	}
}


static void
gth_viewer_set_toolbar_visibility (GthViewer *viewer,
				   gboolean   visible)
{
	g_return_if_fail (viewer != NULL);

	set_action_active (viewer, "View_Toolbar", visible);
	if (visible)
		gtk_widget_show (viewer->priv->toolbar);
	else
		gtk_widget_hide (viewer->priv->toolbar);
}


static void
gth_viewer_notify_update_toolbar_style (GthViewer *viewer)
{
	GthToolbarStyle toolbar_style;
	GtkToolbarStyle prop = GTK_TOOLBAR_BOTH;

	toolbar_style = pref_get_real_toolbar_style ();

	switch (toolbar_style) {
	case GTH_TOOLBAR_STYLE_TEXT_BELOW:
		prop = GTK_TOOLBAR_BOTH; break;
	case GTH_TOOLBAR_STYLE_TEXT_BESIDE:
		prop = GTK_TOOLBAR_BOTH_HORIZ; break;
	case GTH_TOOLBAR_STYLE_ICONS:
		prop = GTK_TOOLBAR_ICONS; break;
	case GTH_TOOLBAR_STYLE_TEXT:
		prop = GTK_TOOLBAR_TEXT; break;
	default:
		break;
	}

	gtk_toolbar_set_style (GTK_TOOLBAR (viewer->priv->toolbar), prop);
}


static void
pref_ui_toolbar_style_changed (GConfClient *client,
			       guint        cnxn_id,
			       GConfEntry  *entry,
			       gpointer     user_data)
{
	GthViewer *viewer = user_data;
	gth_viewer_notify_update_toolbar_style (viewer);
}


static void
pref_ui_toolbar_visible_changed (GConfClient *client,
				 guint        cnxn_id,
				 GConfEntry  *entry,
				 gpointer     user_data)
{
	GthViewer *viewer = user_data;
	gth_viewer_set_toolbar_visibility (viewer, gconf_value_get_bool (gconf_entry_get_value (entry)));
}


static void
gth_viewer_set_statusbar_visibility  (GthViewer *viewer,
				      gboolean   visible)
{
	g_return_if_fail (viewer != NULL);

	set_action_active (viewer, "View_Statusbar", visible);
	if (visible)
		gtk_widget_show (viewer->priv->statusbar);
	else
		gtk_widget_hide (viewer->priv->statusbar);
}


static void
pref_ui_statusbar_visible_changed (GConfClient *client,
				   guint        cnxn_id,
				   GConfEntry  *entry,
				   gpointer     user_data)
{
	GthViewer *viewer = user_data;
	gth_viewer_set_statusbar_visibility (viewer, gconf_value_get_bool (gconf_entry_get_value (entry)));
}


static void
pref_ui_single_window_changed (GConfClient *client,
			       guint        cnxn_id,
			       GConfEntry  *entry,
			       gpointer     user_data)
{
	GthViewer *viewer = user_data;
	gboolean   value = gconf_value_get_bool (gconf_entry_get_value (entry));

	set_action_active (viewer, "View_SingleWindow", value);
}


static void
gth_viewer_realize (GtkWidget *widget)
{
	GthViewer *viewer;
	GthViewerPrivateData *priv;

	viewer = GTH_VIEWER (widget);
	priv = viewer->priv;

	GTK_WIDGET_CLASS (parent_class)->realize (widget);
}


static void
save_window_size (GthViewer *viewer)
{
	int w, h;

	gdk_drawable_get_size (GTK_WIDGET (viewer)->window, &w, &h);
	eel_gconf_set_integer (PREF_UI_VIEWER_WIDTH, w);
	eel_gconf_set_integer (PREF_UI_VIEWER_HEIGHT, h);
}


static void
gth_viewer_unrealize (GtkWidget *widget)
{
	GthViewer            *viewer;
	GthViewerPrivateData *priv;

	viewer = GTH_VIEWER (widget);
	priv = viewer->priv;

	/* save ui preferences. */

	save_window_size (viewer);
	eel_gconf_set_integer (PREF_UI_COMMENT_PANE_SIZE, _gtk_widget_get_height (widget) - gtk_paned_get_position (GTK_PANED (viewer->priv->image_main_pane)));

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}


static gboolean
first_time_idle (gpointer callback_data)
{
	GthViewer            *viewer = callback_data;
	GthViewerPrivateData *priv = viewer->priv;

	g_source_remove (priv->first_timeout_handle);

	gtk_widget_grab_focus (priv->viewer);
	if (priv->image != NULL)
		image_viewer_load_image (IMAGE_VIEWER (priv->viewer), priv->image);

	return FALSE;
}


static void
gth_viewer_show (GtkWidget *widget)
{
	GthViewer *viewer = GTH_VIEWER (widget);
	gboolean   view_foobar;

	GTK_WIDGET_CLASS (parent_class)->show (widget);

	if (!viewer->priv->first_time_show)
		return;
	viewer->priv->first_time_show = FALSE;

	view_foobar = eel_gconf_get_boolean (PREF_UI_TOOLBAR_VISIBLE, TRUE);
	gth_viewer_set_toolbar_visibility (viewer, view_foobar);

	view_foobar = eel_gconf_get_boolean (PREF_UI_STATUSBAR_VISIBLE, TRUE);
	gth_viewer_set_statusbar_visibility (viewer, view_foobar);

	if (viewer->priv->first_timeout_handle == 0)
		viewer->priv->first_timeout_handle = g_idle_add (first_time_idle, viewer);
}


static void
viewer_update_statusbar_zoom_info (GthViewer *viewer)
{
	GthViewerPrivateData *priv = viewer->priv;
	gboolean              image_is_visible;
	int                   zoom;
	char                 *text;

	viewer_update_zoom_sensitivity (viewer);

	/**/

	image_is_visible = (priv->image != NULL) && ! priv->image_error;
	if (! image_is_visible) {
		if (! GTK_WIDGET_VISIBLE (priv->zoom_info_frame))
			return;
		gtk_widget_hide (priv->zoom_info_frame);
		return;
	}

	if (! GTK_WIDGET_VISIBLE (priv->zoom_info_frame))
		gtk_widget_show (priv->zoom_info_frame);

	zoom = (int) (IMAGE_VIEWER (priv->viewer)->zoom_level * 100.0);
	text = g_strdup_printf (" %d%% ", zoom);
	gtk_label_set_text (GTK_LABEL (priv->zoom_info), text);
	g_free (text);
}


static void
viewer_update_statusbar_image_info (GthViewer *viewer)
{
	GthViewerPrivateData *priv = viewer->priv;
	char                 *text;
	char                  time_txt[50], *utf8_time_txt;
	char                 *size_txt;
	char                 *file_size_txt;
	int                   width, height;
	time_t                timer = 0;
	struct tm            *tm;

	if ((priv->image == NULL) || priv->image_error) {
		if (! GTK_WIDGET_VISIBLE (priv->image_info_frame))
			return;
		gtk_widget_hide (priv->image_info_frame);
		return;
	} 
	else if (! GTK_WIDGET_VISIBLE (priv->image_info_frame))
		gtk_widget_show (priv->image_info_frame);

	if (!image_viewer_is_void (IMAGE_VIEWER (priv->viewer))) {
		width = image_viewer_get_image_width (IMAGE_VIEWER (priv->viewer));
		height = image_viewer_get_image_height (IMAGE_VIEWER (priv->viewer));
	} 
	else {
		width = 0;
		height = 0;
	}

	timer = get_metadata_time (NULL, priv->image->path);
	if (timer == 0)
		timer = priv->image->mtime;
	tm = localtime (&timer);
	strftime (time_txt, 50, _("%d %B %Y, %H:%M"), tm);
	utf8_time_txt = g_locale_to_utf8 (time_txt, -1, 0, 0, 0);

	size_txt = g_strdup_printf (_("%d x %d pixels"), width, height);
	file_size_txt = gnome_vfs_format_file_size_for_display (priv->image->size);

	/**/

	if (! priv->image_modified)
		text = g_strdup_printf (" %s - %s - %s     ",
					size_txt,
					file_size_txt,
					utf8_time_txt);
	else
		text = g_strdup_printf (" %s - %s     ",
					_("Modified"),
					size_txt);

	gtk_label_set_text (GTK_LABEL (priv->image_info), text);

	/**/

	g_free (size_txt);
	g_free (file_size_txt);
	g_free (text);
	g_free (utf8_time_txt);
}


static void
update_image_comment (GthViewer *viewer)
{
	GthViewerPrivateData *priv = viewer->priv;
	CommentData          *cdata;
	char                 *comment;
	GtkTextBuffer        *text_buffer;

#ifdef HAVE_LIBIPTCDATA
	if (priv->iptc_data != NULL) {
		iptc_data_unref (priv->iptc_data);
		priv->iptc_data = NULL;
	}
	if (priv->image != NULL) {
		char *local_file = get_cache_filename_from_uri (priv->image->path);
		priv->iptc_data = iptc_data_new_from_jpeg (local_file);
		g_free (local_file);
	}
#endif /* HAVE_LIBIPTCDATA */

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->image_comment));

	if (priv->image == NULL) {
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds (text_buffer, &start, &end);
		gtk_text_buffer_delete (text_buffer, &start, &end);
                return;
	}

	cdata = comments_load_comment (priv->image->path, TRUE);

	if (cdata == NULL) {
		GtkTextIter  iter;
		const char  *click_here = _("[Press 'c' to add a comment]");
		GtkTextIter  start, end;
		GtkTextTag  *tag;

		gtk_text_buffer_set_text (text_buffer, click_here, strlen (click_here));
		gtk_text_buffer_get_iter_at_line (text_buffer, &iter, 0);
		gtk_text_buffer_place_cursor (text_buffer, &iter);

		gtk_text_buffer_get_bounds (text_buffer, &start, &end);
		tag = gtk_text_buffer_create_tag (text_buffer, NULL,
						  "style", PANGO_STYLE_ITALIC,
						  NULL);
		gtk_text_buffer_apply_tag (text_buffer, tag, &start, &end);

		return;
	}

	comment = comments_get_comment_as_string (cdata, "\n\n", " - ");

	if (comment != NULL) {
		GtkTextIter iter;
		gtk_text_buffer_set_text (text_buffer, comment, strlen (comment));
		gtk_text_buffer_get_iter_at_line (text_buffer, &iter, 0);
		gtk_text_buffer_place_cursor (text_buffer, &iter);
	} 
	else {
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds (text_buffer, &start, &end);
		gtk_text_buffer_delete (text_buffer, &start, &end);
	}

	g_free (comment);
	comment_data_free (cdata);
}


static void
viewer_update_image_info (GthViewer *viewer)
{
	JPEGData *jdata = NULL;
	
	viewer_update_statusbar_image_info (viewer);
	viewer_update_statusbar_zoom_info (viewer);

	/* Load EXIF data */

	if (viewer->priv->exif_data != NULL) {
		exif_data_unref (viewer->priv->exif_data);
		viewer->priv->exif_data = NULL;
	}

	if (viewer->priv->image != NULL) {
		char *local_file;
		
		local_file = get_cache_filename_from_uri (viewer->priv->image->path);
		if (local_file != NULL) {
			jdata = jpeg_data_new_from_file (local_file);
			g_free (local_file);
		}
	}

	if (jdata != NULL) {
		viewer->priv->exif_data = jpeg_data_get_exif_data (jdata);
		jpeg_data_unref (jdata);
	}

	/**/

	gth_exif_data_viewer_update (GTH_EXIF_DATA_VIEWER (viewer->priv->exif_data_viewer),
				     IMAGE_VIEWER (viewer->priv->viewer),
				     viewer->priv->image,
				     viewer->priv->exif_data);

	update_image_comment (viewer);
}


static void
viewer_update_title (GthViewer *viewer)
{
	GthViewerPrivateData *priv = viewer->priv;
	char                 *title = NULL;
	char                 *modified;

	modified = priv->image_modified ? _("[modified]") : "";

	if (viewer->priv->image == NULL)
		title = g_strdup (_("No image"));
	else {
		char *image_name = basename_for_display (viewer->priv->image->path);
		title = g_strdup_printf ("%s %s", image_name, modified);
		g_free (image_name);
	}

	gtk_window_set_title (GTK_WINDOW (viewer), title);
	g_free (title);
}


static void
open_with_menu_item_activate_cb (GtkMenuItem *menuitem,
				 gpointer     user_data)
{
	GthViewer               *viewer = user_data;
	GnomeVFSMimeApplication *app;
	GList                   *uris;

	if (viewer->priv->image == NULL)
		return;

	app = g_object_get_data (G_OBJECT (menuitem), "app");
	uris = g_list_prepend (NULL, viewer->priv->image->path);
	gnome_vfs_mime_application_launch (app, uris);
	g_list_free (uris);
}


static void
viewer_update_open_with_menu (GthViewer *viewer)
{
	GthViewerPrivateData *priv = viewer->priv;
	GList                *menu_items, *scan;
	const char           *mime_type = NULL;
	GtkWidget            *mitem;
	int                   pos = 0, i;

	menu_items = gtk_container_get_children (GTK_CONTAINER (priv->open_with_popup_menu));
	for (i = 0, scan = menu_items; i < g_list_length (menu_items) - 1; i++, scan = scan->next)
		gtk_widget_destroy ((GtkWidget*) scan->data);
	g_list_free (menu_items);

	if (priv->image != NULL)
		mime_type = priv->image->mime_type;

	if (mime_type != NULL) {
		GList        *apps = gnome_vfs_mime_get_all_applications (mime_type);
		GtkIconTheme *theme = gtk_icon_theme_get_default ();
		int           icon_size = get_folder_pixbuf_size_for_list (GTK_WIDGET (viewer));

		for (scan = apps; scan; scan = scan->next) {
			GnomeVFSMimeApplication *app = scan->data;
			GtkWidget               *mitem;

			/* do not include gthumb itself */
			if (strncmp (gnome_vfs_mime_application_get_exec (app), "gthumb", 6) == 0)
				continue;

			mitem = gtk_image_menu_item_new_with_label (gnome_vfs_mime_application_get_name (app));
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mitem), create_image (theme, gnome_vfs_mime_application_get_icon (app), icon_size));
			g_object_set_data_full (G_OBJECT (mitem), "app", app, (GDestroyNotify)gnome_vfs_mime_application_free);
			g_signal_connect (mitem, "activate",
					  G_CALLBACK (open_with_menu_item_activate_cb),
					  viewer);
			gtk_widget_show_all (mitem);
			gtk_menu_insert (priv->open_with_popup_menu, mitem, pos++);
		}
		g_list_free (apps);
	} 
	else {
		mitem = gtk_menu_item_new_with_label (_("_None"));
		gtk_widget_set_sensitive (mitem, FALSE);
		gtk_widget_show (mitem);
		gtk_menu_insert (priv->open_with_popup_menu, mitem, pos++);
	}

	mitem = gtk_separator_menu_item_new ();
	gtk_widget_show (mitem);
	gtk_menu_insert (priv->open_with_popup_menu, mitem, pos++);
}


/* ask_whether_to_save */


static void
save_pixbuf__jpeg_data_saved_cb (const char     *uri,
				 GnomeVFSResult  result,
    				 gpointer        data)
{
	GthViewer *viewer = data;
	gboolean   closing = viewer->priv->closing;
	
	viewer->priv->image_modified = FALSE;
	viewer->priv->saving_modified_image = FALSE;
	if (viewer->priv->image_saved_func != NULL)
		(*viewer->priv->image_saved_func) (NULL, viewer);

	if (closing)
		return;

	if ((viewer->priv->image != NULL) && ! same_uri (viewer->priv->image->path, uri)) {
		/*FIXME: gtk_widget_show (gth_viewer_new (uri));*/
		file_data_set_path (viewer->priv->image, uri);
		gth_viewer_load (viewer, viewer->priv->image);
	}
	else {
		viewer_update_statusbar_image_info (viewer);
		viewer_update_image_info (viewer);
		viewer_update_title (viewer);
		viewer_update_sensitivity (viewer);
	}	
}


static CopyData*
save_jpeg_data (GthViewer    *viewer,
		FileData     *file,
		CopyDoneFunc  done_func,
		gpointer      done_data)
{
	GthViewerPrivateData  *priv = viewer->priv;
	gboolean               data_to_save = FALSE;
	JPEGData              *jdata;
        char                  *local_file = NULL;

	local_file = get_cache_filename_from_uri (file->path);
	if (local_file == NULL)
		return update_file_from_cache (file, done_func, done_data);

	if (! image_is_jpeg (local_file))
		return update_file_from_cache (file, done_func, done_data);

	if (priv->exif_data != NULL)
		data_to_save = TRUE;

#ifdef HAVE_LIBIPTCDATA
	if (priv->iptc_data != NULL)
		data_to_save = TRUE;
#endif /* HAVE_LIBIPTCDATA */

	if (! data_to_save)
		return update_file_from_cache (file, done_func, done_data);

	jdata = jpeg_data_new_from_file (local_file);
	if (jdata == NULL)
		return update_file_from_cache (file, done_func, done_data);

#ifdef HAVE_LIBIPTCDATA
	if (priv->iptc_data != NULL) {
		guchar *out_buf, *iptc_buf;
		guint   iptc_len, ps3_len;

		out_buf = g_malloc (256*256);
		iptc_data_save (priv->iptc_data, &iptc_buf, &iptc_len);
		ps3_len = iptc_jpeg_ps3_save_iptc (NULL, 0, iptc_buf,
						   iptc_len, out_buf, 256*256);
		iptc_data_free_buf (priv->iptc_data, iptc_buf);
		if (ps3_len > 0)
			jpeg_data_set_header_data (jdata,
						   JPEG_MARKER_APP13,
						   out_buf, 
						   ps3_len);
		g_free (out_buf);
	}
#endif /* HAVE_LIBIPTCDATA */

	if (priv->exif_data != NULL)
		jpeg_data_set_exif_data (jdata, priv->exif_data);

	jpeg_data_save_file (jdata, local_file);
	jpeg_data_unref (jdata);

	/* The exif orientation tag, if present, must be reset to "top-left",
   	   because the jpeg was saved from a gthumb-generated pixbuf, and
   	   the pixbug image loader always rotates the pixbuf to account for
   	   the orientation tag. */
	write_orientation_field (local_file, GTH_TRANSFORM_NONE);
        g_free (local_file);
        
        return update_file_from_cache (file, done_func, done_data);
}


static void
save_pixbuf__image_saved_cb (FileData *file,
			     gpointer  data)
{
	GthViewer *viewer = data;
	
	if (file != NULL)
		save_jpeg_data (viewer, 
				file, 
				save_pixbuf__jpeg_data_saved_cb,
				viewer);
	else
		viewer->priv->saving_modified_image = FALSE;
}


static void
ask_whether_to_save__image_saved_cb (FileData *file,
				     gpointer  data)
{
	save_pixbuf__image_saved_cb (file, data);
}


static void
ask_whether_to_save__response_cb (GtkWidget *dialog,
				  int        response_id,
				  GthViewer *viewer)
{
	GthViewerPrivateData *priv = viewer->priv;

        gtk_widget_destroy (dialog);

        if (response_id == GTK_RESPONSE_YES) {
		dlg_save_image_as (GTK_WINDOW (viewer),
				   priv->image->path,
				   image_viewer_get_current_pixbuf (IMAGE_VIEWER (priv->viewer)),
				   ask_whether_to_save__image_saved_cb,
				   viewer);
		priv->saving_modified_image = TRUE;
	} 
	else {
		priv->saving_modified_image = FALSE;
		priv->image_modified = FALSE;
		if (priv->image_saved_func != NULL)
			(*priv->image_saved_func) (NULL, viewer);
	}
}


static gboolean
ask_whether_to_save (GthViewer      *viewer,
		     ImageSavedFunc  image_saved_func)
{
	GthViewerPrivateData *priv = viewer->priv;
	GtkWidget            *d;

	if (! eel_gconf_get_boolean (PREF_MSG_SAVE_MODIFIED_IMAGE, TRUE))
		return FALSE;

	d = _gtk_yesno_dialog_with_checkbutton_new (
			    GTK_WINDOW (viewer),
			    GTK_DIALOG_MODAL,
			    _("The current image has been modified, do you want to save it?"),
			    _("Do Not Save"),
			    GTK_STOCK_SAVE_AS,
			    _("_Do not display this message again"),
			    PREF_MSG_SAVE_MODIFIED_IMAGE);

	priv->saving_modified_image = TRUE;
	priv->image_saved_func = image_saved_func;
	g_signal_connect (G_OBJECT (d),
			  "response",
			  G_CALLBACK (ask_whether_to_save__response_cb),
			  viewer);

	gtk_widget_show (d);

	return TRUE;
}


static void
real_set_void (FileData *file,
	       gpointer  data)
{
	GthViewer *viewer = data;

	if (! viewer->priv->image_error) {
		file_data_unref (viewer->priv->image);
		viewer->priv->image = NULL;
		viewer->priv->image_modified = FALSE;
	}

	image_viewer_set_void (IMAGE_VIEWER (viewer->priv->viewer));

	viewer_update_statusbar_image_info (viewer);
 	viewer_update_image_info (viewer);
	viewer_update_title (viewer);
	viewer_update_open_with_menu (viewer);
	viewer_update_sensitivity (viewer);
}


static void
viewer_set_void (GthViewer *viewer,
		 gboolean   error)
{
	GthViewerPrivateData *priv = viewer->priv;

	priv->image_error = error;
	if (priv->image_modified)
		if (ask_whether_to_save (viewer, real_set_void))
			return;
	real_set_void (NULL, viewer);
}


static void
image_loaded_cb (GtkWidget  *widget,
		 GthViewer  *viewer)
{
	GthViewerPrivateData *priv = viewer->priv;

	if (image_viewer_is_void (IMAGE_VIEWER (priv->viewer))) {
		viewer_set_void (viewer, TRUE);
		return;
	}

	file_data_update_info (priv->image); /* FIXME: check if this is necessary */
	priv->image_modified = FALSE;

	viewer_update_image_info (viewer);
	viewer_update_title (viewer);
	viewer_update_open_with_menu (viewer);
	viewer_update_sensitivity (viewer);

	if (StartInFullscreen) {
		StartInFullscreen = FALSE;
		gth_window_set_fullscreen (GTH_WINDOW (viewer), TRUE);
	}
}


static gboolean
zoom_changed_cb (GtkWidget  *widget,
		 GthViewer  *viewer)
{
	viewer_update_statusbar_zoom_info (viewer);
	return TRUE;
}


static void
viewer_drag_data_get  (GtkWidget        *widget,
		       GdkDragContext   *context,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             time,
		       gpointer          data)
{
	GthViewer            *viewer = data;
	GthViewerPrivateData *priv = viewer->priv;
	char                 *path;

	if (IMAGE_VIEWER (priv->viewer)->is_void)
		return;

	path = image_viewer_get_image_filename (IMAGE_VIEWER (priv->viewer));
	gtk_selection_data_set (selection_data,
				selection_data->target,
				8,
				(guchar*)path, strlen (path));
	g_free (path);
}


static void
viewer_drag_data_received  (GtkWidget          *widget,
			    GdkDragContext     *context,
			    int                 x,
			    int                 y,
			    GtkSelectionData   *data,
			    guint               info,
			    guint               time,
			    gpointer            extra_data)
{
	GthViewer *viewer = extra_data;
	GList     *list;
	GList     *scan;

	if (! ((data->length >= 0) && (data->format == 8))) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	gtk_drag_finish (context, TRUE, FALSE, time);

	list = get_file_list_from_url_list ((char *) data->data);
	for (scan = list; scan; scan = scan->next) {
		char *filename = scan->data;
		if (scan == list)
			gth_viewer_load_from_uri (viewer, filename);
		else
			gtk_widget_show (gth_viewer_new (filename));
	}

	path_list_free (list);
}


static gboolean
viewer_key_press_cb (GtkWidget   *widget,
		     GdkEventKey *event,
		     gpointer     data)
{
	GthViewer   *viewer = data;
	GthWindow   *window = (GthWindow*) viewer;
	gboolean     retval = FALSE;

	switch (gdk_keyval_to_lower (event->keyval)) {
		/* Toggle comment visibility. */
	case GDK_i:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (viewer->priv->comment_button),
					      !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (viewer->priv->comment_button)));
		retval = TRUE;
		break;

		/* Toggle animation. */
	case GDK_a:
		gth_window_set_animation (window, ! gth_window_get_animation (window));
		break;

		/* Step animation. */
	case GDK_j:
        	gth_window_step_animation (window);
		break;

		/* Rotate clockwise without saving */
	case GDK_r:
		gth_window_activate_action_alter_image_rotate90 (NULL, window);
		return TRUE;

		/* Rotate counter-clockwise without saving */
	case GDK_e:
		gth_window_activate_action_alter_image_rotate90cc (NULL, window);
		return TRUE;

		/* Lossless clockwise rotation. */
	case GDK_bracketright:
		gth_window_activate_action_tools_jpeg_rotate_right (NULL, window);
		return TRUE;

		/* Lossless counter-clockwise rotation */
	case GDK_bracketleft:
		gth_window_activate_action_tools_jpeg_rotate_left (NULL, window);
		return TRUE;

		/* Flip image */
	case GDK_l:
		gth_window_activate_action_alter_image_flip (NULL, window);
		return TRUE;

		/* Mirror image */
	case GDK_m:
		gth_window_activate_action_alter_image_mirror (NULL, window);
		return TRUE;

		/* Full screen view. */
	case GDK_v:
	case GDK_f:
		gth_window_set_fullscreen (GTH_WINDOW (viewer), TRUE);
		retval = TRUE;
		break;

		/* Open images. */
	case GDK_o:
		gth_window_activate_action_file_open_with (NULL, viewer);
		return TRUE;

		/* Edit comment */
	case GDK_c:
		gth_window_edit_comment (GTH_WINDOW (viewer));
		return TRUE;

		/* Edit keywords */
	case GDK_k:
		gth_window_edit_categories (GTH_WINDOW (viewer));
		return TRUE;

	default:
		break;
	}

	return retval;
}


static int
image_comment_button_press_cb (GtkWidget      *widget,
			       GdkEventButton *event,
			       gpointer        data)
{
	GthViewer *viewer = data;

	if ((event->button == 1) && (event->type == GDK_2BUTTON_PRESS)) {
		gth_window_edit_comment (GTH_WINDOW (viewer));
		return TRUE;
	}

	return FALSE;
}


void
gth_viewer_set_metadata_visible (GthViewer *viewer,
				 gboolean   visible)
{
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (viewer->priv->comment_button), visible);
}


void
gth_viewer_set_single_window (GthViewer *viewer,
			      gboolean   value)
{
	SingleViewer = NULL;
	if (value && (LastFocusedViewer != NULL))
		SingleViewer = LastFocusedViewer;
	eel_gconf_set_boolean (PREF_SINGLE_WINDOW, value);
}


static gboolean
comment_button_toggled_cb (GtkToggleButton *button,
			   GthViewer       *viewer)
{
	gboolean visible = gtk_toggle_button_get_active (button);

	set_action_active (viewer, "View_ShowMetadata", visible);
	viewer->priv->image_data_visible = visible;
	if (visible)
		gtk_widget_show (viewer->priv->image_data_hpaned);
	else
		gtk_widget_hide (viewer->priv->image_data_hpaned);

	return TRUE;
}


static void
zoom_quality_radio_action (GtkAction      *action,
			   GtkRadioAction *current,
			   GthViewer      *viewer)
{
	GthViewerPrivateData  *priv = viewer->priv;
	GthZoomQuality         quality;

	quality = gtk_radio_action_get_current_value (current);

	gtk_radio_action_get_current_value (current);
	image_viewer_set_zoom_quality (IMAGE_VIEWER (priv->viewer), quality);
	image_viewer_update_view (IMAGE_VIEWER (priv->viewer));
	pref_set_zoom_quality (quality);
}


static gboolean
progress_cancel_cb (GtkButton *button,
		    GthViewer *viewer)
{
	if (viewer->priv->pixop != NULL)
		gth_pixbuf_op_stop (viewer->priv->pixop);
	return TRUE;
}


static gboolean
progress_delete_cb (GtkWidget    *caller,
		    GdkEvent     *event,
		    GthViewer    *viewer)
{
	if (viewer->priv->pixop != NULL)
		gth_pixbuf_op_stop (viewer->priv->pixop);
	return TRUE;
}


static void
gth_viewer_init (GthViewer *viewer)
{
	GthViewerPrivateData *priv;

	priv = viewer->priv = g_new0 (GthViewerPrivateData, 1);
	priv->first_time_show = TRUE;
	priv->image = NULL;
	priv->image_error = FALSE;

	if (SingleViewer == NULL)
		SingleViewer = viewer;
}


static void
monitor_update_metadata_cb (GthMonitor *monitor,
			    const char *filename,
			    GthViewer  *viewer)
{
	g_return_if_fail (viewer != NULL);
	viewer_update_image_info (viewer);
}


static void
monitor_update_files_cb (GthMonitor      *monitor,
			 GthMonitorEvent  event,
			 GList           *list,
			 GthViewer       *viewer)
{
	g_return_if_fail (viewer != NULL);

	if (viewer->priv->image == NULL)
		return;

	if (g_list_find_custom (list,
				viewer->priv->image->path,
				(GCompareFunc) uricmp) == NULL)
		return;

	switch (event) {
	case GTH_MONITOR_EVENT_CREATED:
	case GTH_MONITOR_EVENT_CHANGED:
		gth_window_reload_current_image (GTH_WINDOW (viewer));
		break;

	case GTH_MONITOR_EVENT_DELETED:
		if (! viewer->priv->image_modified)
			gth_window_close (GTH_WINDOW (viewer));
		break;

	default:
		break;
	}
}


static void
monitor_file_renamed_cb (GthMonitor *monitor,
			 const char *old_name,
			 const char *new_name,
			 GthViewer  *viewer)
{
	char *uri;
	
	if (viewer->priv->image == NULL)
		return;

	if (! same_uri (old_name, viewer->priv->image->path))
		return;

	uri = add_scheme_if_absent (new_name);
	file_data_set_path (viewer->priv->image, uri);
	g_free (uri);
	 
	gth_window_reload_current_image (GTH_WINDOW (viewer));
}


static void
monitor_update_icon_theme_cb (GthMonitor *monitor,
			      GthViewer  *viewer)
{
	viewer_update_open_with_menu (viewer);
}


static void
set_action_important (GthViewer  *viewer,
		      char       *action_name,
		      gboolean    is_important)
{
	GtkAction *action;

	action = gtk_action_group_get_action (viewer->priv->actions, action_name);
	if (action != NULL)
		g_object_set (action, "is_important", is_important, NULL);
}


static void
sync_menu_with_preferences (GthViewer *viewer)
{
	char *prop;

	set_action_important (viewer, "Image_OpenWith", TRUE);
	set_action_important (viewer, "File_Save", TRUE);
	set_action_important (viewer, "View_Fullscreen", TRUE);
	set_action_important (viewer, "View_ShowMetadata", TRUE);

	set_action_active (viewer, "View_PlayAnimation", TRUE);

	switch (pref_get_zoom_quality ()) {
	case GTH_ZOOM_QUALITY_HIGH:
		prop = "View_ZoomQualityHigh";
		break;
	case GTH_ZOOM_QUALITY_LOW:
	default:
		prop = "View_ZoomQualityLow";
		break;
	}
	set_action_active (viewer, prop, TRUE);

	set_action_active (viewer, "View_ShowMetadata", eel_gconf_get_boolean (PREF_SHOW_IMAGE_DATA, FALSE));
	set_action_active (viewer, "View_SingleWindow", eel_gconf_get_boolean (PREF_SINGLE_WINDOW, FALSE));
}


static void
add_open_with_toolbar_item (GthViewer *viewer)
{
	GthViewerPrivateData *priv = viewer->priv;

	gtk_ui_manager_ensure_update (priv->ui);

	if (priv->open_with_tool_item != NULL) {
		gtk_toolbar_insert (GTK_TOOLBAR (priv->toolbar),
				    priv->open_with_tool_item,
				    OPEN_TOOLITEM_POS);
		return;
	}

	priv->open_with_popup_menu = gtk_ui_manager_get_widget (priv->ui, "/OpenWithMenu");

	priv->open_with_tool_item = gtk_menu_tool_button_new (gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_LARGE_TOOLBAR), _("_Open With"));

	g_object_ref (priv->open_with_tool_item);
	gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (priv->open_with_tool_item),
				       priv->open_with_popup_menu);
	gtk_tool_item_set_homogeneous (priv->open_with_tool_item, FALSE);
	gtk_tool_item_set_tooltip (priv->open_with_tool_item, priv->tooltips, _("Open selected images with an application"), NULL);
	gtk_menu_tool_button_set_arrow_tooltip (GTK_MENU_TOOL_BUTTON (priv->open_with_tool_item), priv->tooltips, _("Open selected images with an application"), NULL);
	gtk_action_connect_proxy (gtk_ui_manager_get_action (priv->ui, "/MenuBar/File/Image_OpenWith"),
				  GTK_WIDGET (priv->open_with_tool_item));

	gtk_widget_show (GTK_WIDGET (priv->open_with_tool_item));
	gtk_toolbar_insert (GTK_TOOLBAR (priv->toolbar),
			    priv->open_with_tool_item,
			    OPEN_TOOLITEM_POS);
}


static void
gth_viewer_construct (GthViewer   *viewer,
		      const gchar *filename)
{
	GthViewerPrivateData *priv = viewer->priv;
	GtkWidget            *menubar, *toolbar;
	GtkWidget            *image_vbox;
	GtkWidget            *button;
	GtkWidget            *image;
	GtkWidget            *image_vpaned;
	GtkWidget            *nav_window;
	GtkWidget            *scrolled_window;
	GtkActionGroup       *actions;
	GtkUIManager         *ui;
	GError               *error = NULL;
	int                   i;

	/* Create the widgets. */

	priv->tooltips = gtk_tooltips_new ();

	/* Build the menu and the toolbar. */

	priv->actions = actions = gtk_action_group_new ("Actions");
	gtk_action_group_set_translation_domain (actions, NULL);

	gtk_action_group_add_actions (actions,
				      gth_window_action_entries,
				      gth_window_action_entries_size,
				      viewer);
	gtk_action_group_add_toggle_actions (actions,
					     gth_window_action_toggle_entries,
					     gth_window_action_toggle_entries_size,
					     viewer);
	gtk_action_group_add_radio_actions (actions,
					    gth_window_zoom_quality_entries,
					    gth_window_zoom_quality_entries_size,
					    GTH_ZOOM_QUALITY_HIGH,
					    G_CALLBACK (zoom_quality_radio_action),
					    viewer);

	gtk_action_group_add_actions (actions,
				      gth_viewer_action_entries,
				      gth_viewer_action_entries_size,
				      viewer);
	gtk_action_group_add_toggle_actions (actions,
					     gth_viewer_action_toggle_entries,
					     gth_viewer_action_toggle_entries_size,
					     viewer);

	priv->ui = ui = gtk_ui_manager_new ();

	g_signal_connect (ui,
			  "connect_proxy",
			  G_CALLBACK (connect_proxy_cb),
			  viewer);
	g_signal_connect (ui,
			  "disconnect_proxy",
			  G_CALLBACK (disconnect_proxy_cb),
			  viewer);

	gtk_ui_manager_insert_action_group (ui, actions, 0);
	gtk_window_add_accel_group (GTK_WINDOW (viewer),
				    gtk_ui_manager_get_accel_group (ui));

	if (!gtk_ui_manager_add_ui_from_string (ui, viewer_window_ui_info, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}
	menubar = gtk_ui_manager_get_widget (ui, "/MenuBar");
	gtk_widget_show (menubar);

	gth_window_attach (GTH_WINDOW (viewer), menubar, GTH_WINDOW_MENUBAR);

	priv->toolbar = toolbar = gtk_ui_manager_get_widget (ui, "/ToolBar");
	gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), TRUE);

	gth_window_attach (GTH_WINDOW (viewer), toolbar, GTH_WINDOW_TOOLBAR);

	priv->image_popup_menu = gtk_ui_manager_get_widget (ui, "/ImagePopupMenu");

	add_open_with_toolbar_item (viewer);

	/* Create the statusbar. */

	priv->statusbar = gtk_statusbar_new ();
	priv->help_message_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), "help_message");
	priv->image_info_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), "image_info");
	priv->progress_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), "progress");
	gth_window_attach (GTH_WINDOW (viewer), priv->statusbar, GTH_WINDOW_STATUSBAR);

	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (priv->statusbar), TRUE);

	/* Statusbar: zoom info */

	priv->zoom_info = gtk_label_new (NULL);
	gtk_widget_show (priv->zoom_info);

	priv->zoom_info_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (priv->zoom_info_frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (priv->zoom_info_frame), priv->zoom_info);
	gtk_box_pack_start (GTK_BOX (priv->statusbar), priv->zoom_info_frame, FALSE, FALSE, 0);

	/* Statusbar: image info */

	priv->image_info = gtk_label_new (NULL);
	gtk_widget_show (priv->image_info);

	priv->image_info_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (priv->image_info_frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (priv->image_info_frame), priv->image_info);
	gtk_box_pack_start (GTK_BOX (priv->statusbar), priv->image_info_frame, FALSE, FALSE, 0);

	/* Image viewer. */

	priv->viewer = image_viewer_new ();
	gtk_widget_set_size_request (priv->viewer, PANE_MIN_SIZE, PANE_MIN_SIZE);

	gtk_drag_source_set (priv->viewer,
			     GDK_BUTTON2_MASK,
			     target_table, G_N_ELEMENTS (target_table),
			     GDK_ACTION_MOVE | GDK_ACTION_COPY);

	gtk_drag_dest_set (priv->viewer,
			   GTK_DEST_DEFAULT_ALL,
			   target_table, G_N_ELEMENTS (target_table),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect (G_OBJECT (priv->viewer),
			  "image_loaded",
			  G_CALLBACK (image_loaded_cb),
			  viewer);
	g_signal_connect (G_OBJECT (priv->viewer),
			  "zoom_changed",
			  G_CALLBACK (zoom_changed_cb),
			  viewer);

	g_signal_connect (G_OBJECT (priv->viewer),
			  "drag_data_get",
			  G_CALLBACK (viewer_drag_data_get),
			  viewer);

	g_signal_connect (G_OBJECT (priv->viewer),
			  "drag_data_received",
			  G_CALLBACK (viewer_drag_data_received),
			  viewer);
	g_signal_connect (G_OBJECT (priv->viewer),
			  "key_press_event",
			  G_CALLBACK (viewer_key_press_cb),
			  viewer);

	/*
	g_signal_connect (G_OBJECT (IMAGE_VIEWER (window->viewer)->loader),
			  "image_progress",
			  G_CALLBACK (image_loader_progress_cb),
			  window);
	g_signal_connect (G_OBJECT (IMAGE_VIEWER (window->viewer)->loader),
			  "image_done",
			  G_CALLBACK (image_loader_done_cb),
			  window);
	g_signal_connect (G_OBJECT (IMAGE_VIEWER (window->viewer)->loader),
			  "image_error",
			  G_CALLBACK (image_loader_done_cb),
			  window);
	*/

	nav_window = gth_nav_window_new (GTH_IVIEWER (priv->viewer));

	/* Image comment */

	priv->image_comment = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (priv->image_comment), FALSE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->image_comment), GTK_WRAP_WORD);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (priv->image_comment), TRUE);

	g_signal_connect (G_OBJECT (priv->image_comment),
			  "button_press_event",
			  G_CALLBACK (image_comment_button_press_cb),
			  viewer);

	/* Exif data viewer */

	priv->exif_data_viewer = gth_exif_data_viewer_new (TRUE);

	/* Comment button */

	image = _gtk_image_new_from_inline (preview_data_16_rgba);
	priv->comment_button = button = gtk_toggle_button_new ();
	gtk_container_add (GTK_CONTAINER (button), image);

	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_tooltips_set_tip (priv->tooltips,
			      button,
			      _("Image comment"),
			      NULL);
	g_signal_connect (G_OBJECT (button),
			  "toggled",
			  G_CALLBACK (comment_button_toggled_cb),
			  viewer);

	/* Pack the widgets:

	   image_vbox
             |
             +- image_vpaned
                  |
                  +- nav_window
                  |    |
                  |    +- priv->viewer
                  |
                  +- priv->image_data_hpaned
                       |
                       +- scrolled_window
                       |    |
                       |    +- priv->image_comment
                       |
                       +- priv->exif_data_viewer

	*/

	image_vbox = gtk_vbox_new (FALSE, 0);

	/* * image_vpaned */

	priv->image_main_pane = image_vpaned = gtk_vpaned_new ();
	gtk_paned_set_position (GTK_PANED (viewer->priv->image_main_pane), eel_gconf_get_integer (PREF_UI_VIEWER_HEIGHT, DEFAULT_WIN_HEIGHT) - eel_gconf_get_integer (PREF_UI_COMMENT_PANE_SIZE, DEFAULT_COMMENT_PANE_SIZE));

	/* ** priv->image_data_hpaned */

	priv->image_data_hpaned = gtk_hpaned_new ();
	gtk_paned_set_position (GTK_PANED (priv->image_data_hpaned), eel_gconf_get_integer (PREF_UI_VIEWER_WIDTH, DEFAULT_WIN_WIDTH) / 2);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_ETCHED_IN);
	gtk_container_add (GTK_CONTAINER (scrolled_window), priv->image_comment);

	gtk_paned_pack2 (GTK_PANED (priv->image_data_hpaned), scrolled_window, TRUE, FALSE);
	gtk_paned_pack1 (GTK_PANED (priv->image_data_hpaned), priv->exif_data_viewer, FALSE, FALSE);

	/**/

	gtk_paned_pack1 (GTK_PANED (image_vpaned), nav_window, FALSE, FALSE);
	gtk_paned_pack2 (GTK_PANED (image_vpaned), priv->image_data_hpaned, TRUE, FALSE);

	gtk_box_pack_start (GTK_BOX (image_vbox), image_vpaned, TRUE, TRUE, 0);
        gtk_widget_show_all (image_vbox);
	gtk_widget_hide (priv->image_data_hpaned); /* FIXME */

	gth_window_attach (GTH_WINDOW (viewer), image_vbox, GTH_WINDOW_CONTENTS);

	/**/

	image_viewer_set_zoom_quality (IMAGE_VIEWER (priv->viewer),
				       pref_get_zoom_quality ());
	image_viewer_set_zoom_change  (IMAGE_VIEWER (priv->viewer),
				       pref_get_zoom_change ());
	image_viewer_set_check_type   (IMAGE_VIEWER (priv->viewer),
				       pref_get_check_type ());
	image_viewer_set_check_size   (IMAGE_VIEWER (priv->viewer),
				       pref_get_check_size ());
	image_viewer_set_transp_type  (IMAGE_VIEWER (priv->viewer),
				       pref_get_transp_type ());
	image_viewer_set_black_background (IMAGE_VIEWER (priv->viewer),
					   eel_gconf_get_boolean (PREF_BLACK_BACKGROUND, FALSE));

	gtk_window_set_default_size (GTK_WINDOW (viewer),
				     eel_gconf_get_integer (PREF_UI_VIEWER_WIDTH, DEFAULT_WIN_WIDTH),
				     eel_gconf_get_integer (PREF_UI_VIEWER_HEIGHT, DEFAULT_WIN_HEIGHT));

	/**/

	/* Add notification callbacks. */

	i = 0;

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_UI_TOOLBAR_STYLE,
					   pref_ui_toolbar_style_changed,
					   viewer);
	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   "/desktop/gnome/interface/toolbar_style",
					   pref_ui_toolbar_style_changed,
					   viewer);
	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_UI_TOOLBAR_VISIBLE,
					   pref_ui_toolbar_visible_changed,
					   viewer);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_UI_STATUSBAR_VISIBLE,
					   pref_ui_statusbar_visible_changed,
					   viewer);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_SINGLE_WINDOW,
					   pref_ui_single_window_changed,
					   viewer);

	/* Progress dialog */

	priv->progress_gui = glade_xml_new (GTHUMB_GLADEDIR "/" GLADE_EXPORTER_FILE, NULL, NULL);
	if (! priv->progress_gui) {
		priv->progress_dialog = NULL;
		priv->progress_progressbar = NULL;
		priv->progress_info = NULL;
	} 
	else {
		GtkWidget *cancel_button;

		priv->progress_dialog = glade_xml_get_widget (priv->progress_gui, "progress_dialog");
		priv->progress_progressbar = glade_xml_get_widget (priv->progress_gui, "progress_progressbar");
		priv->progress_info = glade_xml_get_widget (priv->progress_gui, "progress_info");
		cancel_button = glade_xml_get_widget (priv->progress_gui, "progress_cancel");

		gtk_window_set_transient_for (GTK_WINDOW (priv->progress_dialog), GTK_WINDOW (viewer));
		gtk_window_set_modal (GTK_WINDOW (priv->progress_dialog), FALSE);

		g_signal_connect (G_OBJECT (cancel_button),
				  "clicked",
				  G_CALLBACK (progress_cancel_cb),
				  viewer);
		g_signal_connect (G_OBJECT (priv->progress_dialog),
				  "delete_event",
				  G_CALLBACK (progress_delete_cb),
				  viewer);
	}

	gth_viewer_notify_update_toolbar_style (viewer);
	sync_menu_with_preferences (viewer);

	/**/

	priv->folder = get_folder_pixbuf (get_folder_pixbuf_size_for_menu (GTK_WIDGET (viewer)));

	/**/

	g_signal_connect (G_OBJECT (gth_monitor),
			  "update_files",
			  G_CALLBACK (monitor_update_files_cb),
			  viewer);
	g_signal_connect (G_OBJECT (gth_monitor),
			  "update_metadata",
			  G_CALLBACK (monitor_update_metadata_cb),
			  viewer);
	g_signal_connect (G_OBJECT (gth_monitor),
			  "file_renamed",
			  G_CALLBACK (monitor_file_renamed_cb),
			  viewer);
	g_signal_connect (G_OBJECT (gth_monitor),
			  "update_icon_theme",
			  G_CALLBACK (monitor_update_icon_theme_cb),
			  viewer);

	/**/

	if (filename != NULL) {
		priv->image = file_data_new (filename, NULL);
		file_data_update_all (priv->image, FALSE); /* FIXME: always slow mime type ? */
	}
}


GtkWidget *
gth_viewer_new (const char *filename)
{
	GthViewer *viewer;

	viewer = (GthViewer*) g_object_new (GTH_TYPE_VIEWER, NULL);
	gth_viewer_construct (viewer, filename);

	return (GtkWidget*) viewer;
}


static void
load_image__image_saved_cb (FileData *file,
			    gpointer  data)
{
	GthViewer *viewer = data;

	if (viewer->priv->new_image == NULL) {
		file_data_unref (viewer->priv->image);
		viewer->priv->image = NULL;
		viewer_set_void (viewer, FALSE);
	}
	else {
		file_data_unref (viewer->priv->image);
		viewer->priv->image = file_data_ref (viewer->priv->new_image);

		image_viewer_load_image (IMAGE_VIEWER (viewer->priv->viewer), 
					 viewer->priv->image);
	}
}


void
gth_viewer_load (GthViewer *viewer,
		 FileData  *file)
{
	GthViewerPrivateData *priv = viewer->priv;

	file_data_unref (priv->new_image);
	priv->new_image = NULL;
	if (file != NULL)
		priv->new_image = file_data_ref (file);

	if (priv->image_modified) {
		if (priv->saving_modified_image)
			return;
		if (ask_whether_to_save (viewer, load_image__image_saved_cb))
			return;
	}

	load_image__image_saved_cb (NULL, viewer);
}


void
gth_viewer_load_from_uri (GthViewer  *viewer,
		          const char *uri)
{
	FileData *file;
	
	file = file_data_new (uri, NULL);
	file_data_update_all (file, FALSE); /* FIXME: always slow mime type ? */
	gth_viewer_load (viewer, file);
	file_data_unref (file);
}


/* gth_viewer_close */


static void
close__step2 (FileData *file,
	      gpointer  data)
{
	GthViewer *viewer = data;

	if (viewer->priv->pixop != NULL)
		g_object_unref (viewer->priv->pixop);

	if (viewer->priv->progress_gui != NULL)
		g_object_unref (viewer->priv->progress_gui);

	gtk_object_destroy (GTK_OBJECT (viewer->priv->tooltips));

	if (viewer->priv->image_popup_menu != NULL) {
		gtk_widget_destroy (viewer->priv->image_popup_menu);
		viewer->priv->image_popup_menu = NULL;
	}

	if (viewer->priv->open_with_popup_menu != NULL) {
		gtk_widget_destroy (viewer->priv->open_with_popup_menu);
		viewer->priv->open_with_popup_menu = NULL;
	}

	if (viewer->priv->folder != NULL) {
		g_object_unref (viewer->priv->folder);
		viewer->priv->folder = NULL;
	}

	if (SingleViewer == viewer)
		SingleViewer = NULL;
	if (LastFocusedViewer == viewer)
		LastFocusedViewer = NULL;

	gtk_widget_destroy (GTK_WIDGET (viewer));
}


static void
gth_viewer_close (GthWindow *window)
{
	GthViewer            *viewer = (GthViewer*) window;
	GthViewerPrivateData *priv = viewer->priv;

	debug(DEBUG_INFO, "Gth::Viewer::Close");

	priv->closing = TRUE;

	g_signal_handlers_disconnect_by_data (G_OBJECT (gth_monitor), viewer);

	if (priv->fullscreen != NULL)
		g_signal_handlers_disconnect_by_data (G_OBJECT (priv->fullscreen),
						      viewer);

	if (priv->image_modified)
		if (ask_whether_to_save (viewer, close__step2))
			return;
	close__step2 (NULL, viewer);
}


static ImageViewer *
gth_viewer_get_image_viewer (GthWindow *window)
{
	GthViewer *viewer = (GthViewer*) window;
	return (ImageViewer*) viewer->priv->viewer;
}


static FileData *
gth_viewer_get_image_data (GthWindow *window)
{
	GthViewer *viewer = (GthViewer*) window;
	return viewer->priv->image;
}


static gboolean
gth_viewer_get_image_modified (GthWindow *window)
{
	GthViewer *viewer = (GthViewer*) window;
	return viewer->priv->image_modified;
}


static void
gth_viewer_set_image_modified (GthWindow *window,
			       gboolean   value)
{
	GthViewer            *viewer = (GthViewer*) window;
	GthViewerPrivateData *priv = viewer->priv;

	priv->image_modified = value;
	viewer_update_statusbar_image_info (viewer);
	viewer_update_title (viewer);

	set_action_sensitive (viewer, "File_Save", ! image_viewer_is_void (IMAGE_VIEWER (priv->viewer)) && priv->image_modified);
	set_action_sensitive (viewer, "File_Revert", ! image_viewer_is_void (IMAGE_VIEWER (priv->viewer)) && priv->image_modified);
	set_action_sensitive (viewer, "Edit_Undo", gth_window_get_can_undo (window));
	set_action_sensitive (viewer, "Edit_Redo", gth_window_get_can_redo (window));
}


static void
gth_viewer_save_pixbuf (GthWindow *window,
			GdkPixbuf *pixbuf,
			FileData  *file)
{
	GthViewer            *viewer = (GthViewer*) window;
	GthViewerPrivateData *priv = viewer->priv;
	char                 *current_folder = NULL;

	if (priv->image != NULL)
		current_folder = g_strdup (priv->image->path);

	if (file == NULL)
		dlg_save_image_as (GTK_WINDOW (viewer),
				   current_folder,
				   pixbuf,
				   save_pixbuf__image_saved_cb,
				   viewer);
	else
		dlg_save_image (GTK_WINDOW (viewer),
				file,
				pixbuf,
				save_pixbuf__image_saved_cb,
				viewer);

	g_free (current_folder);
}


/* -- image operations -- */


static void
pixbuf_op_done_cb (GthPixbufOp   *pixop,
		   gboolean       completed,
		   GthViewer     *viewer)
{
	GthViewerPrivateData *priv = viewer->priv;
	ImageViewer          *image_viewer = IMAGE_VIEWER (priv->viewer);

	if (completed) {
		if (priv->pixop_preview)
			image_viewer_set_pixbuf (image_viewer, priv->pixop->dest);
		else {
			gth_window_set_image_pixbuf (GTH_WINDOW (viewer), priv->pixop->dest);
			gth_window_set_image_modified (GTH_WINDOW (viewer), TRUE);
		}
	}

	g_object_unref (priv->pixop);
	priv->pixop = NULL;

	if (priv->progress_dialog != NULL)
		gtk_widget_hide (priv->progress_dialog);
}


static void
pixbuf_op_progress_cb (GthPixbufOp  *pixop,
		       float         p,
		       GthViewer    *viewer)
{
	if (viewer->priv->progress_dialog != NULL)
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (viewer->priv->progress_progressbar), p);
}


static int
viewer__display_progress_dialog (gpointer data)
{
	GthViewer            *viewer = data;
	GthViewerPrivateData *priv = viewer->priv;

	if (priv->progress_timeout != 0) {
		g_source_remove (priv->progress_timeout);
		priv->progress_timeout = 0;
	}

	if (priv->pixop != NULL)
		gtk_widget_show (priv->progress_dialog);

	return FALSE;
}


static void
gth_viewer_exec_pixbuf_op (GthWindow   *window,
			   GthPixbufOp *pixop,
			   gboolean     preview)
{
	GthViewer            *viewer = (GthViewer*) window;
	GthViewerPrivateData *priv = viewer->priv;

	if (priv->pixop != NULL)
		return;

	priv->pixop = pixop;
	g_object_ref (priv->pixop);
	priv->pixop_preview = preview;

	gtk_label_set_text (GTK_LABEL (priv->progress_info),
			    _("Wait please..."));

	g_signal_connect (G_OBJECT (pixop),
			  "pixbuf_op_done",
			  G_CALLBACK (pixbuf_op_done_cb),
			  viewer);
	g_signal_connect (G_OBJECT (pixop),
			  "pixbuf_op_progress",
			  G_CALLBACK (pixbuf_op_progress_cb),
			  viewer);

	if (priv->progress_dialog != NULL)
		priv->progress_timeout = g_timeout_add (DISPLAY_PROGRESS_DELAY, viewer__display_progress_dialog, viewer);

	gth_pixbuf_op_start (priv->pixop);
}


static void
reload_current_image__step2 (FileData *file,
			     gpointer  data)
{
	GthViewer *viewer = data;
	
	if (viewer->priv->image != NULL)
		gth_viewer_load_from_uri (viewer, viewer->priv->image->path);
	else
		gth_viewer_load (viewer, NULL);
}


static void
gth_viewer_reload_current_image (GthWindow *window)
{
	GthViewer *viewer = GTH_VIEWER (window);

	if (viewer->priv->image == NULL)
		return;

	if (viewer->priv->image_modified)
		if (ask_whether_to_save (viewer, reload_current_image__step2))
			return;

	reload_current_image__step2 (NULL, viewer);
}


static void
gth_viewer_update_current_image_metadata (GthWindow *window)
{
	GthViewer *viewer = GTH_VIEWER (window);

	if (viewer->priv->image == NULL)
		return;
	update_image_comment (viewer);
}


static GList *
gth_viewer_get_file_list_selection (GthWindow *window)
{
	GthViewer *viewer = GTH_VIEWER (window);

	if (viewer->priv->image == NULL)
		return NULL;
	return g_list_prepend (NULL, g_strdup (viewer->priv->image->path));
}


static GList *
gth_viewer_get_file_list_selection_as_fd (GthWindow *window)
{
	GthViewer *viewer = GTH_VIEWER (window);

	if (viewer->priv->image == NULL)
		return NULL;
	return g_list_prepend (NULL, file_data_ref (viewer->priv->image));
}


static void
gth_viewer_set_animation (GthWindow *window,
			  gboolean   play)
{
	GthViewer   *viewer = GTH_VIEWER (window);
	ImageViewer *image_viewer = IMAGE_VIEWER (viewer->priv->viewer);

	set_action_active (viewer, "View_PlayAnimation", play);
	set_action_sensitive (viewer, "View_StepAnimation", ! play);
	if (play)
		image_viewer_start_animation (image_viewer);
	else
		image_viewer_stop_animation (image_viewer);
}


static gboolean
gth_viewer_get_animation (GthWindow *window)
{
	GthViewer *viewer = GTH_VIEWER (window);
	return IMAGE_VIEWER (viewer->priv->viewer)->play_animation;
}


static void
gth_viewer_step_animation (GthWindow *window)
{
	GthViewer *viewer = GTH_VIEWER (window);
	image_viewer_step_animation (IMAGE_VIEWER (viewer->priv->viewer));
}


static gboolean
fullscreen_destroy_cb (GtkWidget *widget,
		       GthViewer *viewer)
{
	viewer->priv->fullscreen = NULL;
	gth_window_set_fullscreen (GTH_WINDOW (viewer), FALSE);
	return FALSE;
}


static void
gth_viewer_set_fullscreen (GthWindow *window,
			   gboolean   fullscreen)
{
	GthViewer *viewer = GTH_VIEWER (window);
	GthViewerPrivateData *priv = viewer->priv;

	if (fullscreen && (priv->fullscreen == NULL)) {
		GdkPixbuf *image = NULL;
		
		if (! image_viewer_is_animation (IMAGE_VIEWER (priv->viewer)))
			image = image_viewer_get_current_pixbuf (IMAGE_VIEWER (priv->viewer));
		priv->fullscreen = gth_fullscreen_new (image, priv->image, gth_viewer_get_file_list_selection_as_fd (window));	
		g_signal_connect (priv->fullscreen,
				  "destroy",
				  G_CALLBACK (fullscreen_destroy_cb),
				  viewer);
		gtk_widget_show (priv->fullscreen);
	} 
	else if (! fullscreen && (priv->fullscreen != NULL))
		gtk_widget_destroy (priv->fullscreen);
}


static void
gth_viewer_set_slideshow (GthWindow *window,
			  gboolean   value)
{
}


static gboolean
gth_viewer_focus_in_event (GtkWidget     *widget,
			   GdkEventFocus *event)
{
	LastFocusedViewer = (GthViewer*) widget;
	SingleViewer = LastFocusedViewer;
	return GTK_WIDGET_CLASS (parent_class)->focus_in_event (widget, event);
}


static void
gth_viewer_class_init (GthViewerClass *class)
{
	GObjectClass   *gobject_class;
	GtkWidgetClass *widget_class;
	GthWindowClass *window_class;

	parent_class = g_type_class_peek_parent (class);
	gobject_class = (GObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	window_class = (GthWindowClass*) class;

	gobject_class->finalize = gth_viewer_finalize;

	widget_class->realize = gth_viewer_realize;
	widget_class->unrealize = gth_viewer_unrealize;
	widget_class->show = gth_viewer_show;
	widget_class->focus_in_event = gth_viewer_focus_in_event;

	window_class->close = gth_viewer_close;
	window_class->get_image_viewer = gth_viewer_get_image_viewer;
	window_class->get_image_data = gth_viewer_get_image_data;
	window_class->get_image_modified = gth_viewer_get_image_modified;
	window_class->set_image_modified = gth_viewer_set_image_modified;
	window_class->save_pixbuf = gth_viewer_save_pixbuf;
	window_class->exec_pixbuf_op = gth_viewer_exec_pixbuf_op;

	window_class->reload_current_image = gth_viewer_reload_current_image;
	window_class->update_current_image_metadata = gth_viewer_update_current_image_metadata;
	window_class->get_file_list_selection = gth_viewer_get_file_list_selection;
	window_class->get_file_list_selection_as_fd = gth_viewer_get_file_list_selection_as_fd;

	window_class->set_animation = gth_viewer_set_animation;
	window_class->get_animation = gth_viewer_get_animation;
	window_class->step_animation = gth_viewer_step_animation;
	window_class->set_fullscreen = gth_viewer_set_fullscreen;
	window_class->set_slideshow = gth_viewer_set_slideshow;
}


GType
gth_viewer_get_type ()
{
        static GType type = 0;

        if (! type) {
                GTypeInfo type_info = {
			sizeof (GthViewerClass),
			NULL,
			NULL,
			(GClassInitFunc) gth_viewer_class_init,
			NULL,
			NULL,
			sizeof (GthViewer),
			0,
			(GInstanceInitFunc) gth_viewer_init
		};

		type = g_type_register_static (GTH_TYPE_WINDOW,
					       "GthViewer",
					       &type_info,
					       0);
	}

        return type;
}


GtkWidget *
gth_viewer_get_current_viewer (void)
{
	GList *windows = gth_window_get_window_list ();
	GList *scan;

	if (SingleViewer == NULL)
		for (scan = windows; scan; scan = scan->next) {
			GthWindow *window = scan->data;
			if (GTH_IS_VIEWER (window)) {
				SingleViewer = (GthViewer*) window;
				break;
			}
		}

	return (GtkWidget*) SingleViewer;
}
