/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2003, 2006 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <math.h>
#include "gth-image-selector.h"
#include "gth-iviewer.h"
#include "gthumb-marshal.h"
#include "glib-utils.h"
#include "pixbuf-utils.h"

#define DRAG_THRESHOLD 1
#define BORDER 3
#define BORDER2 (BORDER * 2)
#define STEP_INCREMENT  20.0  /* Scroll increment. */
#define CHECK_SIZE_LARGE 16
#define COLOR_GRAY_66 0x00666666
#define COLOR_GRAY_99 0x00999999
#define SCROLL_TIMEOUT 30          /* Autoscroll timeout in milliseconds */

#define IROUND(x) ((int)floor(((double)x) + 0.5))

typedef struct {
	int           ref_count;
	int           id;
	GdkRectangle  area;
	GdkCursor    *cursor;
} EventArea;


static EventArea *
event_area_new (int           id,
		GdkCursorType cursor_type)
{
	EventArea *event_area;

	event_area = g_new0 (EventArea, 1);

	event_area->ref_count = 1;
	event_area->id = id;
	event_area->area.x = 0;
	event_area->area.y = 0;
	event_area->area.width = 0;
	event_area->area.height = 0;
	event_area->cursor = gdk_cursor_new_for_display (gdk_display_get_default (), cursor_type);

	return event_area;
}


#if 0
static void
event_area_ref (EventArea *event_area)
{
	event_area->ref_count++;
}
#endif


static void
event_area_unref (EventArea *event_area)
{
	event_area->ref_count--;

	if (event_area->ref_count > 0)
		return;

	if (event_area->cursor != NULL)
		gdk_cursor_unref (event_area->cursor);
	g_free (event_area);
}


/**/


typedef enum {
	C_SELECTION_AREA,
	C_TOP_AREA,
	C_BOTTOM_AREA,
	C_LEFT_AREA,
	C_RIGHT_AREA,
	C_TOP_LEFT_AREA,
	C_TOP_RIGHT_AREA,
	C_BOTTOM_LEFT_AREA,
	C_BOTTOM_RIGHT_AREA
} GthEventAreaType;


static GthEventAreaType
get_opposite_event_area_on_x (GthEventAreaType type)
{
	GthEventAreaType opposite_type = C_SELECTION_AREA;
	switch (type) {
		case C_SELECTION_AREA:
			opposite_type = C_SELECTION_AREA;
			break;
		case C_TOP_AREA:
			opposite_type = C_TOP_AREA;
			break;
		case C_BOTTOM_AREA:
			opposite_type = C_BOTTOM_AREA;
			break;
		case C_LEFT_AREA:
			opposite_type = C_RIGHT_AREA;
			break;
		case C_RIGHT_AREA:
			opposite_type = C_LEFT_AREA;
			break;
		case C_TOP_LEFT_AREA:
			opposite_type = C_TOP_RIGHT_AREA;
			break;
		case C_TOP_RIGHT_AREA:
			opposite_type = C_TOP_LEFT_AREA;
			break;
		case C_BOTTOM_LEFT_AREA:
			opposite_type = C_BOTTOM_RIGHT_AREA;
			break;
		case C_BOTTOM_RIGHT_AREA:
			opposite_type = C_BOTTOM_LEFT_AREA;
			break;
	}
	return opposite_type;
}


static GthEventAreaType
get_opposite_event_area_on_y (GthEventAreaType type)
{
	GthEventAreaType opposite_type = C_SELECTION_AREA;
	switch (type) {
		case C_SELECTION_AREA:
			opposite_type = C_SELECTION_AREA;
			break;
		case C_TOP_AREA:
			opposite_type = C_BOTTOM_AREA;
			break;
		case C_BOTTOM_AREA:
			opposite_type = C_TOP_AREA;
			break;
		case C_LEFT_AREA:
			opposite_type = C_LEFT_AREA;
			break;
		case C_RIGHT_AREA:
			opposite_type = C_RIGHT_AREA;
			break;
		case C_TOP_LEFT_AREA:
			opposite_type = C_BOTTOM_LEFT_AREA;
			break;
		case C_TOP_RIGHT_AREA:
			opposite_type = C_BOTTOM_RIGHT_AREA;
			break;
		case C_BOTTOM_LEFT_AREA:
			opposite_type = C_TOP_LEFT_AREA;
			break;
		case C_BOTTOM_RIGHT_AREA:
			opposite_type = C_TOP_RIGHT_AREA;
			break;
	}
	return opposite_type;
}


enum {
	SELECTION_CHANGED,
	MOTION_NOTIFY,
	CLICKED,
	MASK_VISIBILITY_CHANGED,
	LAST_SIGNAL
};


static GtkWidgetClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };


struct _GthImageSelectorPriv {
	GthSelectorType  type;

	GdkPixbuf       *pixbuf, *background;
	GdkRectangle     pixbuf_area, background_area;

	gboolean         use_ratio;
	double           ratio;
	double           zoom;
	gboolean         zoom_to_fit;
	gboolean         doing_zoom_fit;

	gboolean         pressed;
	gboolean         double_click;
	gboolean         dragging;
	gboolean         active;
	gboolean         mask_visible;

	double           drag_start_x, drag_start_y;
	double           drag_x, drag_y;
	GdkRectangle     drag_start_selection_area;

	GdkGC           *selection_gc;
	GdkRectangle     selection_area;
	GdkRectangle     selection;

	GdkCursor       *default_cursor;
	GdkCursor       *current_cursor;
	GList           *event_list;
	EventArea       *current_area;

	GtkAdjustment   *vadj, *hadj;
	int              x_offset, y_offset;

	GdkPixbuf       *paint_pixbuf;
	gint             paint_max_width;
	gint             paint_max_height;
        int              paint_bps;
	GdkColorspace    paint_color_space;

	guint            timer_tag; 	    /* Timeout ID for
					     * autoscrolling */
	double           x_value_diff;      /* Change the adjustment value
					     * by this
					     * amount when autoscrolling */
	double           y_value_diff;
};


static gboolean
point_in_rectangle (int          x,
		    int          y,
		    GdkRectangle rect)
{
	return ((x >= rect.x)
		&& (x <= rect.x + rect.width)
		&& (y >= rect.y)
		&& (y <= rect.y + rect.height));
}


static gboolean
rectangle_in_rectangle (GdkRectangle r1,
			GdkRectangle r2)
{
	return (point_in_rectangle (r1.x, r1.y, r2)
		&& point_in_rectangle (r1.x + r1.width,
				       r1.y + r1.height,
				       r2));
}


static gboolean
rectangle_equal (GdkRectangle r1,
		 GdkRectangle r2)
{
	return ((r1.x == r2.x)
		&& (r1.y == r2.y)
		&& (r1.width == r2.width)
		&& (r1.height == r2.height));
}


static int
real_to_selector (GthImageSelector *selector,
		  int               value)
{
	return IROUND (selector->priv->zoom * value);
}


static void
convert_to_selection_area (GthImageSelector *selector,
			   GdkRectangle      real_area,
			   GdkRectangle     *selection_area)
{
	selection_area->x = real_to_selector (selector, real_area.x);
	selection_area->y = real_to_selector (selector, real_area.y);
	selection_area->width = real_to_selector (selector, real_area.width);
	selection_area->height = real_to_selector (selector, real_area.height);
}


static void
add_event_area (GthImageSelector *selector,
		int               area_id,
		GdkCursorType     cursor_type)
{
	GthImageSelectorPriv *priv = selector->priv;
	EventArea            *event_area;

	event_area = event_area_new (area_id, cursor_type);
	priv->event_list = g_list_prepend (priv->event_list, event_area);
}


static void
free_event_area_list (GthImageSelector *selector)
{
	GthImageSelectorPriv *priv = selector->priv;

	if (priv->event_list != NULL) {
		g_list_foreach (priv->event_list,
				(GFunc) event_area_unref,
				NULL);
		g_list_free (priv->event_list);
		priv->event_list = NULL;
	}
}


static EventArea*
get_event_area_from_position (GthImageSelector *selector,
			      int               x,
			      int               y)
{
	GthImageSelectorPriv *priv = selector->priv;
	GList                *scan;

	for (scan = priv->event_list; scan; scan = scan->next) {
		EventArea    *event_area = scan->data;
		GdkRectangle  widget_area;

		widget_area = event_area->area;
		widget_area.x += priv->background_area.x;
		widget_area.y += priv->background_area.y;

		if (point_in_rectangle (x, y, widget_area))
			return event_area;
	}

	return NULL;
}


static EventArea*
get_event_area_from_id (GthImageSelector *selector,
			int               event_id)
{
	GList *scan;

	for (scan = selector->priv->event_list; scan; scan = scan->next) {
		EventArea *event_area = scan->data;
		if (event_area->id == event_id)
			return event_area;
	}

	return NULL;
}


/**/


static void
update_event_areas (GthImageSelector *selector)
{
	GthImageSelectorPriv *priv = selector->priv;
	EventArea            *event_area;
	int                   x, y, width, height;

	if (! GTK_WIDGET_REALIZED (selector))
		return;

	x = priv->selection_area.x - 1;
	y = priv->selection_area.y - 1;
	width = priv->selection_area.width + 1;
	height = priv->selection_area.height + 1;

	event_area = get_event_area_from_id (selector, C_SELECTION_AREA);
	event_area->area.x = x + BORDER;
	event_area->area.y = y + BORDER;
	event_area->area.width = width - BORDER2;
	event_area->area.height = height - BORDER2;

	event_area = get_event_area_from_id (selector, C_TOP_AREA);
	event_area->area.x = x + BORDER;
	event_area->area.y = y - BORDER;
	event_area->area.width = width - BORDER2;
	event_area->area.height = BORDER2;

	event_area = get_event_area_from_id (selector, C_BOTTOM_AREA);
	event_area->area.x = x + BORDER;
	event_area->area.y = y + height - BORDER;
	event_area->area.width = width - BORDER2;
	event_area->area.height = BORDER2;

	event_area = get_event_area_from_id (selector, C_LEFT_AREA);
	event_area->area.x = x - BORDER;
	event_area->area.y = y + BORDER;
	event_area->area.width = BORDER2;
	event_area->area.height = height - BORDER2;

	event_area = get_event_area_from_id (selector, C_RIGHT_AREA);
	event_area->area.x = x + width - BORDER;
	event_area->area.y = y + BORDER;
	event_area->area.width = BORDER2;
	event_area->area.height = height - BORDER2;

	event_area = get_event_area_from_id (selector, C_TOP_LEFT_AREA);
	event_area->area.x = x - BORDER;
	event_area->area.y = y - BORDER;
	event_area->area.width = BORDER2;
	event_area->area.height = BORDER2;

	event_area = get_event_area_from_id (selector, C_TOP_RIGHT_AREA);
	event_area->area.x = x + width - BORDER;
	event_area->area.y = y - BORDER;
	event_area->area.width = BORDER2;
	event_area->area.height = BORDER2;

	event_area = get_event_area_from_id (selector, C_BOTTOM_LEFT_AREA);
	event_area->area.x = x - BORDER;
	event_area->area.y = y + height - BORDER;
	event_area->area.width = BORDER2;
	event_area->area.height = BORDER2;

	event_area = get_event_area_from_id (selector, C_BOTTOM_RIGHT_AREA);
	event_area->area.x = x + width - BORDER;
	event_area->area.y = y + height - BORDER;
	event_area->area.width = BORDER2;
	event_area->area.height = BORDER2;
}


static void
selection_changed (GthImageSelector *selector)
{
	update_event_areas (selector);
	g_signal_emit (G_OBJECT (selector),
		       signals[SELECTION_CHANGED],
		       0);
}


#if 0
static void
print_rectangle (GdkRectangle *r)
{
	g_print ("(%d,%d) [%d,%d]\n", r->x, r->y, r->width, r->height);
}
#endif


static void
paint (GthImageSelector *selector,
       GdkPixbuf        *pixbuf,
       int               dest_x,
       int               dest_y,
       int               src_x,
       int               src_y,
       int               width,
       int               height)
{
	GthImageSelectorPriv *priv = selector->priv;
	int                   bits_per_sample;
	GdkColorspace         color_space;

	bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
	color_space = gdk_pixbuf_get_colorspace (pixbuf);

	if ((priv->paint_pixbuf == NULL)
	    || (priv->paint_max_width < width)
	    || (priv->paint_max_height < height)
	    || (priv->paint_bps != bits_per_sample)
	    || (priv->paint_color_space != color_space)) {
		if (priv->paint_pixbuf != NULL)
			g_object_unref (priv->paint_pixbuf);
		priv->paint_pixbuf = gdk_pixbuf_new (color_space,
						     FALSE,
						     bits_per_sample,
						     width,
						     height);
		g_return_if_fail (priv->paint_pixbuf != NULL);

		priv->paint_max_width = width;
		priv->paint_max_height = height;
		priv->paint_color_space = color_space;
		priv->paint_bps = bits_per_sample;
	}

	if (gdk_pixbuf_get_has_alpha (pixbuf)) {
		gdk_pixbuf_composite_color (pixbuf,
					    priv->paint_pixbuf,
					    0, 0,
					    width, height,
					    (double) -src_x,
					    (double) -src_y,
					    priv->zoom,
					    priv->zoom,
					    GDK_INTERP_NEAREST,
					    255,
					    src_x, src_y,
					    CHECK_SIZE_LARGE,
					    COLOR_GRAY_66,
					    COLOR_GRAY_99);
	} else {
		gdk_pixbuf_scale (pixbuf,
				  priv->paint_pixbuf,
				  0, 0,
				  width, height,
				  (double) -src_x,
				  (double) -src_y,
				  priv->zoom,
				  priv->zoom,
				  GDK_INTERP_NEAREST);
	}

	gdk_draw_rgb_image_dithalign (GTK_WIDGET (selector)->window,
				      GTK_WIDGET (selector)->style->black_gc,
				      dest_x, dest_y,
				      width, height,
				      GDK_RGB_DITHER_MAX,
				      gdk_pixbuf_get_pixels (priv->paint_pixbuf),
				      gdk_pixbuf_get_rowstride (priv->paint_pixbuf),
				      dest_x, dest_y);
}


static void
paint_background (GthImageSelector *selector,
		  GdkRectangle     *event_area)
{
	GthImageSelectorPriv *priv = selector->priv;
	GdkRectangle          paint_area;

	if (! gdk_rectangle_intersect (&priv->background_area,
				       event_area,
				       &paint_area))
		return;

	paint (selector,
	       priv->background,
	       paint_area.x,
	       paint_area.y,
	       priv->x_offset + (paint_area.x - priv->background_area.x),
	       priv->y_offset + (paint_area.y - priv->background_area.y),
	       paint_area.width,
	       paint_area.height);
}


static void
paint_selection (GthImageSelector *selector,
		 GdkRectangle     *event_area)
{
	GthImageSelectorPriv *priv = selector->priv;
	GdkRectangle          selection_area, paint_area;

	selection_area = priv->selection_area;
	selection_area.x += priv->background_area.x;
	selection_area.y += priv->background_area.y;

	event_area->x += priv->x_offset;
	event_area->y += priv->y_offset;

	if (! gdk_rectangle_intersect (&selection_area,
				       event_area,
				       &paint_area))
		return;

	paint (selector,
	       priv->pixbuf,
	       paint_area.x - priv->x_offset,
	       paint_area.y - priv->y_offset,
	       paint_area.x - priv->background_area.x,
	       paint_area.y - priv->background_area.y,
	       paint_area.width,
	       paint_area.height);
}


static void
paint_image (GthImageSelector *selector,
	     GdkRectangle     *event_area)
{
	GthImageSelectorPriv *priv = selector->priv;
	GdkRectangle          paint_area;

	if (! gdk_rectangle_intersect (&priv->background_area,
				       event_area,
				       &paint_area))
		return;

	paint (selector,
	       priv->pixbuf,
	       paint_area.x,
	       paint_area.y,
	       priv->x_offset + (paint_area.x - priv->background_area.x),
	       priv->y_offset + (paint_area.y - priv->background_area.y),
	       paint_area.width,
	       paint_area.height);
}


static gboolean
expose (GtkWidget      *widget,
	GdkEventExpose *event)
{
	GthImageSelector     *selector = GTH_IMAGE_SELECTOR (widget);
	GthImageSelectorPriv *priv = selector->priv;

	if (priv->pixbuf == NULL)
		return FALSE;

	if (! priv->mask_visible) {
		paint_image (selector, &event->area);
		return FALSE;
	}

	paint_background (selector, &event->area);
	paint_selection (selector, &event->area);

	if (GTK_WIDGET_HAS_FOCUS (widget) /*!priv->active*/) {
		GdkRectangle area;

		area = priv->selection_area;
		area.x += priv->background_area.x - priv->x_offset;
		area.y += priv->background_area.y - priv->y_offset;

		gdk_draw_rectangle (widget->window,
				    priv->selection_gc,
				    FALSE,
				    area.x,
				    area.y,
				    area.width,
				    area.height);
	}

#if 0
	{
		GthImageSelectorPriv *priv = selector->priv;
		GList                *scan;
		for (scan = priv->event_list; scan; scan = scan->next) {
			EventArea    *event_area = scan->data;
			GdkRectangle  area;

			area = event_area->area;
			area.x += priv->background_area.x - priv->x_offset;
			area.y += priv->background_area.y - priv->y_offset;

			gdk_draw_rectangle (widget->window,
					    widget->style->black_gc,
					    FALSE,
					    area.x,
					    area.y,
					    area.width,
					    area.height);
		}
	}
#endif

	return FALSE;
}


static int
selector_to_real (GthImageSelector *selector,
		  int               value)
{
	return IROUND ((double) value / selector->priv->zoom);
}


static void
convert_to_real_selection (GthImageSelector *selector,
			   GdkRectangle      selection_area,
			   GdkRectangle     *real_area)
{
	real_area->x = selector_to_real (selector, selection_area.x);
	real_area->y = selector_to_real (selector, selection_area.y);
	real_area->width = selector_to_real (selector, selection_area.width);
	real_area->height = selector_to_real (selector, selection_area.height);
}


static void
queue_draw (GthImageSelector *selector,
	    GdkRectangle      area)
{
	GthImageSelectorPriv *priv = selector->priv;

	if (! GTK_WIDGET_REALIZED (selector))
		return;

	gtk_widget_queue_draw_area (
		    GTK_WIDGET (selector),
		    priv->background_area.x + area.x - priv->x_offset - BORDER,
		    priv->background_area.y + area.y - priv->y_offset - BORDER,
		    area.width + BORDER2,
		    area.height + BORDER2);
}


static void
set_selection_area (GthImageSelector *selector,
		    GdkRectangle      new_selection,
		    gboolean          force_update)
{
	GthImageSelectorPriv *priv = selector->priv;
	GdkRectangle          old_selection;

	if (!force_update && rectangle_equal (priv->selection_area, new_selection))
		return;

	old_selection = priv->selection_area;
	priv->selection_area = new_selection;
	queue_draw (selector, old_selection);
	queue_draw (selector, priv->selection_area);

	selection_changed (selector);
}


static void
set_selection (GthImageSelector *selector,
	       GdkRectangle      new_selection,
	       gboolean          force_update)
{
	GdkRectangle new_area;

	if (!force_update
	    && rectangle_equal (selector->priv->selection, new_selection))
		return;

	selector->priv->selection = new_selection;
	convert_to_selection_area (selector, new_selection, &new_area);
	set_selection_area (selector, new_area, force_update);
}


static void
set_cursor (GthImageSelector *selector,
	    GdkCursor        *cursor)
{
	if (selector->priv->current_cursor == cursor)
		return;
	gdk_window_set_cursor (GTK_WIDGET (selector)->window, cursor);
	selector->priv->current_cursor = cursor;
}


static void
set_active_area (GthImageSelector *selector,
		 EventArea        *event_area)
{
	GthImageSelectorPriv *priv = selector->priv;

	if (priv->active != (event_area != NULL)) {
		priv->active = !priv->active;
		queue_draw (selector, priv->selection_area);
	}

	if (priv->current_area != event_area)
		priv->current_area = event_area;

	if (priv->current_area == NULL)
		set_cursor (selector, priv->default_cursor);
	else
		set_cursor (selector, priv->current_area->cursor);
}


static void
update_cursor (GthImageSelector *selector,
	       int               x,
	       int               y)
{
	if (! selector->priv->mask_visible)
		return;
	set_active_area (selector, get_event_area_from_position (selector, x, y));
}


static gboolean
button_release  (GtkWidget      *widget,
		 GdkEventButton *event)
{
	GthImageSelector     *selector = GTH_IMAGE_SELECTOR (widget);
	GthImageSelectorPriv *priv = selector->priv;

	if (priv->dragging)
		gdk_pointer_ungrab (event->time);
	priv->dragging = FALSE;
	priv->pressed = FALSE;

	if (priv->timer_tag != 0) {
		g_source_remove (priv->timer_tag);
		priv->timer_tag = 0;
	}

	update_cursor (selector,
		       priv->background_area.x + priv->drag_x + priv->x_offset,
		       priv->background_area.y + priv->drag_y + priv->y_offset);

	return FALSE;
}


static void
grow_upward (GdkRectangle *bound,
	     GdkRectangle *r,
	     int           dy,
	     gboolean      check)
{
	if (check && (r->y + dy < 0))
		dy = -r->y;
	r->y += dy;
	r->height -= dy;
}


static void
grow_downward (GdkRectangle *bound,
	       GdkRectangle *r,
	       int           dy,
	       gboolean      check)
{
	if (check && (r->y + r->height + dy > bound->height))
		dy = bound->height - (r->y + r->height);
	r->height += dy;
}


static void
grow_leftward (GdkRectangle *bound,
	       GdkRectangle *r,
	       int           dx,
	       gboolean      check)
{
	if (check && (r->x + dx < 0))
		dx = -r->x;
	r->x += dx;
	r->width -= dx;
}


static void
grow_rightward (GdkRectangle *bound,
		GdkRectangle *r,
		int           dx,
		gboolean      check)
{
	if (check && (r->x + r->width + dx > bound->width))
		dx = bound->width - (r->x + r->width);
	r->width += dx;
}


static int
get_semiplane_no (int x1,
		  int y1,
		  int x2,
		  int y2,
		  int px,
		  int py)
{
	double a, b;

	a = atan ((double) (y1 - y2) / (x2 - x1));
	b = atan ((double) (y1 - py) / (px - x1));

	return (a <= b) && (b <= a + G_PI);
}


static void
check_and_set_new_selection (GthImageSelector *selector,
			     GdkRectangle      new_selection)
{
	GthImageSelectorPriv *priv = selector->priv;

	new_selection.width = MAX (0, new_selection.width);
	new_selection.height = MAX (0, new_selection.height);

	if (((priv->current_area == NULL)
	     || (priv->current_area->id != C_SELECTION_AREA))
	    && priv->use_ratio) {
		if (rectangle_in_rectangle (new_selection, priv->pixbuf_area))
			set_selection (selector, new_selection, FALSE);
		return;
	}

	if (new_selection.x < 0)
		new_selection.x = 0;
	if (new_selection.y < 0)
		new_selection.y = 0;
	if (new_selection.width > priv->pixbuf_area.width)
		new_selection.width = priv->pixbuf_area.width;
	if (new_selection.height > priv->pixbuf_area.height)
		new_selection.height = priv->pixbuf_area.height;

	if (new_selection.x + new_selection.width > priv->pixbuf_area.width)
		new_selection.x = priv->pixbuf_area.width - new_selection.width;
	if (new_selection.y + new_selection.height > priv->pixbuf_area.height)
		new_selection.y = priv->pixbuf_area.height - new_selection.height;

	set_selection (selector, new_selection, FALSE);
}


static gboolean
button_press (GtkWidget      *widget,
	      GdkEventButton *event)
{
	GthImageSelector     *selector = GTH_IMAGE_SELECTOR (widget);
	GthImageSelectorPriv *priv = selector->priv;
	GdkModifierType       mods;
	int                   x, y;

	if (! GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);

	if (priv->dragging)
		return FALSE;

	if ((event->type == GDK_2BUTTON_PRESS) ||
	    (event->type == GDK_3BUTTON_PRESS)) {
		priv->double_click = TRUE;
		return FALSE;
	} else
		priv->double_click = FALSE;

	if (event->button != 1)
		return FALSE;

	gdk_window_get_pointer (widget->window, &x, &y, &mods);
	x += priv->x_offset;
	y += priv->y_offset;

	if (! point_in_rectangle (x, y, priv->background_area))
		return FALSE;

	if (priv->current_area == NULL) {
		GdkRectangle new_selection;

		new_selection.x = selector_to_real (selector, x - priv->background_area.x);
		new_selection.y = selector_to_real (selector, y - priv->background_area.y);
		new_selection.width = selector_to_real (selector, 1);
		new_selection.height = selector_to_real (selector, 1);

		if (priv->type == GTH_SELECTOR_TYPE_REGION) {
			check_and_set_new_selection (selector, new_selection);
			set_active_area (selector, get_event_area_from_id (selector, C_BOTTOM_RIGHT_AREA));
			priv->dragging = TRUE;
		}
		else if (priv->type == GTH_SELECTOR_TYPE_POINT)
			g_signal_emit (G_OBJECT (selector),
		       		       signals[CLICKED],
		       		       0,
		       		       new_selection.x, new_selection.y);
	}

	if (priv->current_area != NULL) {
		priv->drag_start_x = x;
		priv->drag_start_y = y;
		priv->drag_x = x;
		priv->drag_y = y;
		priv->drag_start_selection_area = priv->selection_area;
		priv->pressed = TRUE;

		return TRUE;
	}

	return FALSE;
}


static void
update_mouse_selection (GthImageSelector *selector,
			int               new_x,
			int               new_y)
{
	GthImageSelectorPriv *priv = selector->priv;
	gboolean              check = !priv->use_ratio;
	int                   dx, dy;
	GdkRectangle          new_selection, tmp;
	int                   semiplane;
	GthEventAreaType      area_type = priv->current_area->id;
	EventArea            *event_area;

	priv->drag_x = new_x;
	priv->drag_y = new_y;

	dx = selector_to_real (selector, priv->drag_x - priv->drag_start_x);
	dy = selector_to_real (selector, priv->drag_y - priv->drag_start_y);

	convert_to_real_selection (selector,
				   priv->drag_start_selection_area,
				   &new_selection);

	if (((area_type == C_LEFT_AREA)
	     || (area_type == C_TOP_LEFT_AREA)
	     || (area_type == C_BOTTOM_LEFT_AREA))
	    && (dx > new_selection.width)) {
		new_selection.x += new_selection.width;
       		dx = - (2 * new_selection.width) + dx;
       		area_type = get_opposite_event_area_on_x (area_type);
	}
	else if (((area_type == C_RIGHT_AREA)
	          || (area_type == C_TOP_RIGHT_AREA)
	          || (area_type == C_BOTTOM_RIGHT_AREA))
	         && (-dx > new_selection.width)) {
	    	new_selection.x -= new_selection.width;
       		dx = (2 * new_selection.width) + dx;
       		area_type = get_opposite_event_area_on_x (area_type);
	}

	if (((area_type == C_TOP_AREA)
	     || (area_type == C_TOP_LEFT_AREA)
	     || (area_type == C_TOP_RIGHT_AREA))
	    && (dy > new_selection.height)) {
	    	new_selection.y += new_selection.height;
       		dy = - (2 * new_selection.height) + dy;
       		area_type = get_opposite_event_area_on_y (area_type);
	}
	else if (((area_type == C_BOTTOM_AREA)
	          || (area_type == C_BOTTOM_LEFT_AREA)
	          || (area_type == C_BOTTOM_RIGHT_AREA))
	         && (-dy > new_selection.height)) {
       		new_selection.y -= new_selection.height;
       		dy = (2 * new_selection.height) + dy;
       		area_type = get_opposite_event_area_on_y (area_type);
       	}

	event_area = get_event_area_from_id (selector, area_type);
	if (event_area != NULL)
		set_cursor (selector, event_area->cursor);

	switch (area_type) {
	case C_SELECTION_AREA:
		new_selection.x += dx;
		new_selection.y += dy;
		break;

	case C_TOP_AREA:
		grow_upward (&priv->pixbuf_area, &new_selection, dy, check);
		if (priv->use_ratio)
			grow_rightward (&priv->pixbuf_area,
					&new_selection,
					IROUND (-dy * priv->ratio),
					check);
		break;

	case C_BOTTOM_AREA:
		grow_downward (&priv->pixbuf_area, &new_selection, dy, check);
		if (priv->use_ratio)
			grow_leftward (&priv->pixbuf_area,
				       &new_selection,
				       IROUND (-dy * priv->ratio),
				       check);
		break;

	case C_LEFT_AREA:
		grow_leftward (&priv->pixbuf_area, &new_selection, dx, check);
		if (priv->use_ratio)
			grow_downward (&priv->pixbuf_area,
				       &new_selection,
				       IROUND (-dx / priv->ratio),
				       check);
		break;

	case C_RIGHT_AREA:
		grow_rightward (&priv->pixbuf_area, &new_selection, dx, check);
		if (priv->use_ratio)
			grow_upward (&priv->pixbuf_area,
				     &new_selection,
				     IROUND (-dx / priv->ratio),
				     check);
		break;

	case C_TOP_LEFT_AREA:
		if (priv->use_ratio) {
			tmp = priv->selection_area;
			semiplane = get_semiplane_no (
				      tmp.x + tmp.width,
				      tmp.y + tmp.height,
				      tmp.x,
				      tmp.y,
				      priv->drag_x - priv->background_area.x,
				      priv->drag_y - priv->background_area.y);
			if (semiplane == 1)
				dy = IROUND (dx / priv->ratio);
			else
				dx = IROUND (dy * priv->ratio);
		}
		grow_upward (&priv->pixbuf_area, &new_selection, dy, check);
		grow_leftward (&priv->pixbuf_area, &new_selection, dx, check);
		break;

	case C_TOP_RIGHT_AREA:
		if (priv->use_ratio) {
			tmp = priv->selection_area;
			semiplane = get_semiplane_no (
				      tmp.x,
				      tmp.y + tmp.height,
				      tmp.x + tmp.width,
				      tmp.y,
				      priv->drag_x - priv->background_area.x,
				      priv->drag_y - priv->background_area.y);
			if (semiplane == 1)
				dx = IROUND (-dy * priv->ratio);
			else
				dy = IROUND (-dx / priv->ratio);
		}
		grow_upward (&priv->pixbuf_area, &new_selection, dy, check);
		grow_rightward (&priv->pixbuf_area, &new_selection, dx, check);
		break;

	case C_BOTTOM_LEFT_AREA:
		if (priv->use_ratio) {
			tmp = priv->selection_area;
			semiplane = get_semiplane_no (
				      tmp.x + tmp.width,
				      tmp.y,
				      tmp.x,
				      tmp.y + tmp.height,
				      priv->drag_x - priv->background_area.x,
				      priv->drag_y - priv->background_area.y);
			if (semiplane == 1)
				dx = IROUND (-dy * priv->ratio);
			else
				dy = IROUND (-dx / priv->ratio);
		}
		grow_downward (&priv->pixbuf_area, &new_selection, dy, check);
		grow_leftward (&priv->pixbuf_area, &new_selection, dx, check);
		break;

	case C_BOTTOM_RIGHT_AREA:
		if (priv->use_ratio) {
			tmp = priv->selection_area;
			semiplane = get_semiplane_no (
				      tmp.x,
				      tmp.y,
				      tmp.x + tmp.width,
				      tmp.y + tmp.height,
				      priv->drag_x - priv->background_area.x,
				      priv->drag_y - priv->background_area.y);

			if (semiplane == 1)
				dy = IROUND (dx / priv->ratio);
			else
				dx = IROUND (dy * priv->ratio);
		}
		grow_downward (&priv->pixbuf_area, &new_selection, dy, check);
		grow_rightward (&priv->pixbuf_area, &new_selection, dx, check);
		break;

	default:
		break;
	}

	check_and_set_new_selection (selector, new_selection);
}


static gboolean
autoscroll_cb (gpointer data)
{
	GthImageSelector     *selector = GTH_IMAGE_SELECTOR (data);
	GthImageSelectorPriv *priv = selector->priv;
	double                max_value;
	double                value;
	int                   new_x, new_y;

	GDK_THREADS_ENTER ();

	max_value = priv->hadj->upper - priv->hadj->page_size;
	value = priv->hadj->value + priv->x_value_diff;
	if (value > max_value)
		value = max_value;
	gtk_adjustment_set_value (priv->hadj, value);
	new_x = priv->drag_x + priv->x_value_diff;

	max_value = priv->vadj->upper - priv->vadj->page_size;
	value = priv->vadj->value + priv->y_value_diff;
	if (value > max_value)
		value = max_value;
	gtk_adjustment_set_value (priv->vadj, value);
	new_y = priv->drag_y + priv->y_value_diff;

	update_mouse_selection (selector, new_x, new_y);

	GDK_THREADS_LEAVE();

	return TRUE;
}


static gboolean
motion_notify (GtkWidget      *widget,
	       GdkEventMotion *event)
{
	GthImageSelector     *selector = GTH_IMAGE_SELECTOR (widget);
	GthImageSelectorPriv *priv = selector->priv;
	GdkModifierType       mods;
	int                   x, y;
	int                   dx, dy;
	int                   absolute_x, absolute_y;

	gdk_window_get_pointer (widget->window, &x, &y, &mods);
	x += priv->x_offset;
	y += priv->y_offset;

	dx = x - priv->drag_start_x;
	dy = y - priv->drag_start_y;

	if (priv->type == GTH_SELECTOR_TYPE_POINT) {
		x = selector_to_real (selector, x - priv->background_area.x);
		y = selector_to_real (selector, y - priv->background_area.y);
		if (point_in_rectangle (x, y, priv->pixbuf_area)
		    && ((abs (dx) > DRAG_THRESHOLD) || (abs (dy) > DRAG_THRESHOLD)))
			g_signal_emit (G_OBJECT (selector),
				       signals[MOTION_NOTIFY],
		       	       	       0,
		       	       	       x, y);
		return TRUE;
	}

	/* priv->type == GTH_SELECTOR_TYPE_REGION */

	if (! priv->dragging
	    && priv->pressed
	    && ((abs (dx) > DRAG_THRESHOLD) || (abs (dy) > DRAG_THRESHOLD))
	    && (priv->current_area != NULL)) {
		int retval;

		retval = gdk_pointer_grab (widget->window,
					   FALSE,
					   (GDK_POINTER_MOTION_MASK
					    | GDK_POINTER_MOTION_HINT_MASK
					    | GDK_BUTTON_RELEASE_MASK),
					   NULL,
					   priv->current_area->cursor,
					   event->time);
		if (retval == 0)
			priv->dragging = TRUE;

		return FALSE;
	}

	if (! priv->dragging) {
		update_cursor (selector, x, y);
		return FALSE;
	}

	/* dragging == TRUE */

	update_mouse_selection (selector, x, y);

	/* If we are out of bounds, schedule a timeout that will do
	 * the scrolling */

	absolute_x = x - priv->x_offset;
	absolute_y = y - priv->y_offset;

	if ((absolute_y < 0) || (absolute_y > widget->allocation.height)
	    || (absolute_x < 0) || (absolute_x > widget->allocation.width)) {

		/* Make the steppings be relative to the mouse
		 * distance from the canvas.
		 * Also notice the timeout below is small to give a
		 * more smooth movement.
		 */
		if (absolute_x < 0)
			priv->x_value_diff = absolute_x;
		else if (absolute_x > widget->allocation.width)
			priv->x_value_diff = absolute_x - widget->allocation.width;
		else
			priv->x_value_diff = 0.0;
		priv->x_value_diff /= 2;

		if (absolute_y < 0)
			priv->y_value_diff = absolute_y;
		else if (absolute_y > widget->allocation.height)
			priv->y_value_diff = absolute_y - widget->allocation.height;
		else
			priv->y_value_diff = 0.0;
		priv->y_value_diff /= 2;

		if (priv->timer_tag == 0)
			priv->timer_tag = g_timeout_add (SCROLL_TIMEOUT,
							 autoscroll_cb,
							 selector);

	} else if (priv->timer_tag != 0) {
		g_source_remove (priv->timer_tag);
		priv->timer_tag = 0;
	}

	return FALSE;
}


static void
finalize (GObject *object)
{
	GthImageSelector *selector;

        g_return_if_fail (GTH_IS_IMAGE_SELECTOR (object));

	selector = GTH_IMAGE_SELECTOR (object);

	if (selector->priv != NULL) {
		GthImageSelectorPriv *priv = selector->priv;

		if (priv->paint_pixbuf != NULL) {
			g_object_unref (priv->paint_pixbuf);
			priv->paint_pixbuf = NULL;
		}

		if (priv->pixbuf != NULL) {
			g_object_unref (priv->pixbuf);
			priv->pixbuf = NULL;
		}

		if (priv->background != NULL) {
			g_object_unref (priv->background);
			priv->background = NULL;
		}

		g_free (selector->priv);
		selector->priv = NULL;
	}

        /* Chain up */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
init_selection (GthImageSelector *selector)
{
	GthImageSelectorPriv *priv = selector->priv;
	GdkRectangle          area;

	if (! priv->use_ratio) {
		area.width = IROUND (priv->pixbuf_area.width * 0.5);
		area.height = IROUND (priv->pixbuf_area.height * 0.5);

	} else {
		if (priv->ratio > 1.0) {
			area.width = IROUND (priv->pixbuf_area.width * 0.5);
			area.height = IROUND (area.width / priv->ratio);
		} else {
			area.height = IROUND (priv->pixbuf_area.height * 0.5);
			area.width = IROUND (area.height * priv->ratio);
		}
	}

	area.height = 0;
	area.width = 0;

	area.x = IROUND ((priv->pixbuf_area.width - area.width) / 2.0);
	area.y = IROUND ((priv->pixbuf_area.height - area.height) / 2.0);

	set_selection (selector, area, FALSE);
}


static void
realize (GtkWidget *widget)
{
	GthImageSelector     *selector;
	GthImageSelectorPriv *priv;
	GdkWindowAttr         attributes;
	int                   attributes_mask;

	g_return_if_fail (GTH_IS_IMAGE_SELECTOR (widget));

	selector = GTH_IMAGE_SELECTOR (widget);
	priv = selector->priv;

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x           = widget->allocation.x;
	attributes.y           = widget->allocation.y;
	attributes.width       = widget->allocation.width;
	attributes.height      = widget->allocation.height;
	attributes.wclass      = GDK_INPUT_OUTPUT;
	attributes.visual      = gtk_widget_get_visual (widget);
	attributes.colormap    = gtk_widget_get_colormap (widget);
	attributes.event_mask  = (gtk_widget_get_events (widget)
				  | GDK_EXPOSURE_MASK
				  | GDK_BUTTON_PRESS_MASK
				  | GDK_BUTTON_RELEASE_MASK
				  | GDK_POINTER_MOTION_MASK
				  | GDK_POINTER_MOTION_HINT_MASK
				  | GDK_BUTTON_MOTION_MASK);
	attributes_mask        = (GDK_WA_X
				  | GDK_WA_Y
				  | GDK_WA_VISUAL
				  | GDK_WA_COLORMAP);
	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes,
					 attributes_mask);
	gdk_window_set_user_data (widget->window, selector);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

	/**/

	priv->selection_gc = gdk_gc_new (widget->window);
	gdk_gc_copy (priv->selection_gc, widget->style->white_gc);
	gdk_gc_set_line_attributes (priv->selection_gc,
				    1,
				    GDK_LINE_ON_OFF_DASH /*GDK_LINE_SOLID*/,
				    GDK_CAP_BUTT,
				    GDK_JOIN_MITER);
	gdk_gc_set_function (priv->selection_gc, GDK_INVERT);

	if (priv->type == GTH_SELECTOR_TYPE_REGION)
		priv->default_cursor = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_CROSSHAIR /*GDK_LEFT_PTR*/);
	else if (priv->type == GTH_SELECTOR_TYPE_POINT)
		priv->default_cursor = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_CROSSHAIR);
	set_cursor (selector, priv->default_cursor);

	add_event_area (selector, C_SELECTION_AREA, GDK_FLEUR);
	add_event_area (selector, C_TOP_AREA, GDK_TOP_SIDE);
	add_event_area (selector, C_BOTTOM_AREA, GDK_BOTTOM_SIDE);
	add_event_area (selector, C_LEFT_AREA, GDK_LEFT_SIDE);
	add_event_area (selector, C_RIGHT_AREA, GDK_RIGHT_SIDE);
	add_event_area (selector, C_TOP_LEFT_AREA, GDK_TOP_LEFT_CORNER);
	add_event_area (selector, C_TOP_RIGHT_AREA, GDK_TOP_RIGHT_CORNER);
	add_event_area (selector, C_BOTTOM_LEFT_AREA, GDK_BOTTOM_LEFT_CORNER);
	add_event_area (selector, C_BOTTOM_RIGHT_AREA, GDK_BOTTOM_RIGHT_CORNER);

	init_selection (selector);
	update_event_areas (selector);
}


static void
unrealize (GtkWidget *widget)
{
	GthImageSelector     *selector;
	GthImageSelectorPriv *priv;

        g_return_if_fail (GTH_IS_IMAGE_SELECTOR (widget));

	selector = GTH_IMAGE_SELECTOR (widget);
	priv = selector->priv;

	if (priv != NULL) {
		if (priv->default_cursor != NULL) {
			gdk_cursor_unref (priv->default_cursor);
			priv->default_cursor = NULL;
		}

		if (priv->selection_gc != NULL) {
			g_object_unref (priv->selection_gc);
			priv->selection_gc = NULL;
		}

		free_event_area_list (selector);
	}

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}


static void
generic_set_zoom (GthImageSelector *selector,
		  double            new_zoom,
		  int               center_x,
		  int               center_y)
{
	GtkWidget            *widget = (GtkWidget*) selector;
	GthImageSelectorPriv *priv = selector->priv;
	GdkRectangle          selection;
	double                previous_zoom, zoom_ratio, vscroll, hscroll;

	gth_image_selector_get_selection (selector, &selection);

	previous_zoom = priv->zoom;
	priv->zoom = new_zoom;
	zoom_ratio = new_zoom / previous_zoom;

	hscroll = gtk_adjustment_get_value (priv->hadj);
	hscroll = ((hscroll + center_x) * zoom_ratio - widget->allocation.width / 2);

	vscroll = gtk_adjustment_get_value (priv->vadj);
	vscroll = ((vscroll + center_y) * zoom_ratio - widget->allocation.height / 2);

	set_selection (selector, selection, TRUE);
	gtk_adjustment_set_value (priv->vadj, vscroll);
	gtk_adjustment_set_value (priv->hadj, hscroll);

	if (! priv->doing_zoom_fit) {
		priv->zoom_to_fit = FALSE;
		gtk_widget_queue_resize (widget);
		gtk_widget_queue_draw (widget);
	}
}


static void
zoom_to_fit (GthImageSelector *selector)
{
	GtkWidget *widget = (GtkWidget *) selector;
	int        gdk_width, gdk_height;
	double     new_zoom_level;

	selector->priv->zoom_to_fit = TRUE;

	if (selector->priv->pixbuf == NULL)
		return;

	gdk_width = widget->allocation.width;
	gdk_height = widget->allocation.height;

	if ((gdk_width > gdk_pixbuf_get_width (selector->priv->pixbuf))
	    && (gdk_height > gdk_pixbuf_get_height (selector->priv->pixbuf)))
		new_zoom_level = 1.0;
	else {
		double x_level = (double) gdk_width / gdk_pixbuf_get_width (selector->priv->pixbuf);
		double y_level = (double) gdk_height / gdk_pixbuf_get_height (selector->priv->pixbuf);
		new_zoom_level = (x_level < y_level) ? x_level : y_level;
	}

	if (new_zoom_level > 0.0) {
		selector->priv->doing_zoom_fit = TRUE;
		generic_set_zoom (selector,
			  	  new_zoom_level,
			  	  widget->allocation.width / 2,
			  	  widget->allocation.height / 2);
		selector->priv->doing_zoom_fit = FALSE;
	}
}


static void
size_allocate (GtkWidget     *widget,
	       GtkAllocation *allocation)
{
	GthImageSelector     *selector;
	GthImageSelectorPriv *priv;
	int                   gdk_width, gdk_height;

	selector = GTH_IMAGE_SELECTOR (widget);
	priv = selector->priv;

	widget->allocation = *allocation;
	gdk_width = allocation->width;
	gdk_height = allocation->height;

	if (priv->zoom_to_fit)
		zoom_to_fit (selector);

	if (priv->pixbuf == NULL) {
		priv->background_area.x = 0;
		priv->background_area.y = 0;
		priv->background_area.width = 0;
		priv->background_area.height = 0;
	} else {
		priv->background_area.width = real_to_selector (selector, priv->pixbuf_area.width);
		priv->background_area.height = real_to_selector (selector, priv->pixbuf_area.height);
		priv->background_area.x = MAX ((allocation->width
						- priv->background_area.width) / 2.0,
					       0);
		priv->background_area.y = MAX ((allocation->height
						- priv->background_area.height) / 2.0,
					       0);
		selection_changed (selector);
	}

	/* Check whether the offset is still valid. */

	if (priv->pixbuf != NULL) {
		int width, height;

		width = priv->background_area.width;
		height = priv->background_area.height;

		if (width > gdk_width)
			priv->x_offset = CLAMP (priv->x_offset,
						0,
						width - gdk_width);
		else
			priv->x_offset = 0;

		if (height > gdk_height)
			priv->y_offset = CLAMP (priv->y_offset,
						0,
						height - gdk_height);
		else
			priv->y_offset = 0;

		if ((width != priv->hadj->upper) || (height != priv->vadj->upper))
			gth_iviewer_size_changed (GTH_IVIEWER (selector));

		/* Change adjustment values. */

		priv->hadj->lower          = 0.0;
		priv->hadj->upper          = width;
		priv->hadj->value          = priv->x_offset;
		priv->hadj->step_increment = STEP_INCREMENT;
		priv->hadj->page_increment = gdk_width / 2;
		priv->hadj->page_size      = gdk_width;

		priv->vadj->lower          = 0.0;
		priv->vadj->upper          = height;
		priv->vadj->value          = priv->y_offset;
		priv->vadj->step_increment = STEP_INCREMENT;
		priv->vadj->page_increment = gdk_height / 2;
		priv->vadj->page_size      = gdk_height;
	} else {
		priv->hadj->lower     = 0.0;
		priv->hadj->upper     = 1.0;
		priv->hadj->value     = 0.0;
		priv->hadj->page_size = 1.0;

		priv->vadj->lower     = 0.0;
		priv->vadj->upper     = 1.0;
		priv->vadj->value     = 0.0;
		priv->vadj->page_size = 1.0;
	}

	g_signal_handlers_block_by_data (G_OBJECT (priv->hadj), widget);
	g_signal_handlers_block_by_data (G_OBJECT (priv->vadj), widget);
	gtk_adjustment_changed (priv->hadj);
	gtk_adjustment_changed (priv->vadj);
	g_signal_handlers_unblock_by_data (G_OBJECT (priv->hadj), widget);
	g_signal_handlers_unblock_by_data (G_OBJECT (priv->vadj), widget);

	/**/

	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (widget->window,
					allocation->x, allocation->y,
					allocation->width, allocation->height);

	gth_iviewer_size_changed (GTH_IVIEWER (selector));
}


static gint
focus_in (GtkWidget     *widget,
	  GdkEventFocus *event)
{
	GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
	gtk_widget_queue_draw (widget);
	return TRUE;
}


static gint
focus_out (GtkWidget     *widget,
	   GdkEventFocus *event)
{
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
	gtk_widget_queue_draw (widget);
	return TRUE;
}


static void
scroll_to (GthImageSelector *selector,
	   int              *x_offset,
	   int              *y_offset)
{
	selector->priv->x_offset = *x_offset;
	selector->priv->y_offset = *y_offset;

	if (! GTK_WIDGET_REALIZED (selector))
		return;

	gdk_window_invalidate_rect (GTK_WIDGET (selector)->window,
				    NULL,
				    TRUE);
}


static gboolean
hadj_value_changed (GtkObject        *adj,
		    GthImageSelector *selector)
{
	int x_ofs, y_ofs;

	x_ofs = (int) GTK_ADJUSTMENT (adj)->value;
	y_ofs = selector->priv->y_offset;
	scroll_to (selector, &x_ofs, &y_ofs);

	return FALSE;
}


static gboolean
vadj_value_changed (GtkObject        *adj,
		    GthImageSelector *selector)
{
	int x_ofs, y_ofs;

	x_ofs = selector->priv->x_offset;
	y_ofs = (int) GTK_ADJUSTMENT (adj)->value;
	scroll_to (selector, &x_ofs, &y_ofs);

	return FALSE;
}


static void
set_scroll_adjustments (GtkWidget     *widget,
			GtkAdjustment *hadj,
			GtkAdjustment *vadj)
{
	GthImageSelector     *selector;
	GthImageSelectorPriv *priv;

        g_return_if_fail (widget != NULL);
        g_return_if_fail (GTH_IS_IMAGE_SELECTOR (widget));

        selector = GTH_IMAGE_SELECTOR (widget);
	priv = selector->priv;

        if (hadj)
                g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
        else
                hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0,
							   0.0, 0.0, 0.0));

        if (vadj)
                g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
        else
                vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0,
							   0.0, 0.0, 0.0));

        if (priv->hadj && priv->hadj != hadj) {
		g_signal_handlers_disconnect_by_data (G_OBJECT (priv->hadj),
						      selector);
		g_object_unref (priv->hadj);
		priv->hadj = NULL;
        }

        if (priv->vadj && priv->vadj != vadj) {
		g_signal_handlers_disconnect_by_data (G_OBJECT (priv->vadj),
						      selector);
		g_object_unref (priv->vadj);
		priv->vadj = NULL;
        }

        if (priv->hadj != hadj) {
                priv->hadj = hadj;
                g_object_ref (priv->hadj);
                gtk_object_sink (GTK_OBJECT (priv->hadj));

		g_signal_connect (G_OBJECT (priv->hadj),
				  "value_changed",
				  G_CALLBACK (hadj_value_changed),
				  selector);
        }

        if (priv->vadj != vadj) {
		priv->vadj = vadj;
		g_object_ref (priv->vadj);
		gtk_object_sink (GTK_OBJECT (priv->vadj));

		g_signal_connect (G_OBJECT (priv->vadj),
				  "value_changed",
				  G_CALLBACK (vadj_value_changed),
				  selector);
        }
}


static gboolean scroll_event (GtkWidget *widget, GdkEventScroll *event);


static void
class_init (GthImageSelectorClass *class)
{
	GObjectClass   *gobject_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	widget_class = (GtkWidgetClass*) class;
	gobject_class = (GObjectClass*) class;

	signals[SELECTION_CHANGED] = g_signal_new (
			 "selection_changed",
			 G_TYPE_FROM_CLASS (class),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET (GthImageSelectorClass, selection_changed),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE,
			 0);
	signals[MOTION_NOTIFY] = g_signal_new (
			 "motion_notify",
			 G_TYPE_FROM_CLASS (class),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET (GthImageSelectorClass, motion_notify),
			 NULL, NULL,
			 gthumb_marshal_VOID__INT_INT,
			 G_TYPE_NONE,
			 2,
			 G_TYPE_INT, G_TYPE_INT);
	signals[CLICKED] = g_signal_new (
			 "clicked",
			 G_TYPE_FROM_CLASS (class),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET (GthImageSelectorClass, clicked),
			 NULL, NULL,
			 gthumb_marshal_VOID__INT_INT,
			 G_TYPE_NONE,
			 2,
			 G_TYPE_INT, G_TYPE_INT);
	signals[MASK_VISIBILITY_CHANGED] = g_signal_new (
			 "mask_visibility_changed",
			 G_TYPE_FROM_CLASS (class),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET (GthImageSelectorClass, mask_visibility_changed),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE,
			 0);

	gobject_class->finalize = finalize;

	widget_class->realize         = realize;
	widget_class->unrealize       = unrealize;
	widget_class->size_allocate   = size_allocate;
	widget_class->focus_in_event  = focus_in;
	widget_class->focus_out_event = focus_out;

	widget_class->expose_event         = expose;
	widget_class->button_press_event   = button_press;
	widget_class->button_release_event = button_release;
	widget_class->motion_notify_event  = motion_notify;
	widget_class->scroll_event         = scroll_event;

	class->set_scroll_adjustments = set_scroll_adjustments;
        widget_class->set_scroll_adjustments_signal =
		g_signal_new ("set_scroll_adjustments",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GthImageSelectorClass, set_scroll_adjustments),
			      NULL, NULL,
			      gthumb_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE,
			      2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

	class->selection_changed = NULL;
}


static void
init (GthImageSelector *selector)
{
	GTK_WIDGET_SET_FLAGS (selector, GTK_CAN_FOCUS);

	selector->priv = g_new0 (GthImageSelectorPriv, 1);
	selector->priv->type = GTH_SELECTOR_TYPE_REGION;
	selector->priv->zoom = 1.0;
	selector->priv->ratio = 1.0;

	selector->priv->mask_visible = TRUE;

	selector->priv->paint_pixbuf = NULL;
	selector->priv->paint_max_width = 0;
	selector->priv->paint_max_height = 0;
	selector->priv->paint_bps = 0;
	selector->priv->paint_color_space = GDK_COLORSPACE_RGB;

	/**/

	selector->priv->hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 1.0, 0.0,
								   1.0, 1.0, 1.0));
	selector->priv->vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 1.0, 0.0,
								   1.0, 1.0, 1.0));

	g_object_ref (selector->priv->hadj);
	gtk_object_sink (GTK_OBJECT (selector->priv->hadj));
	g_object_ref (selector->priv->vadj);
	gtk_object_sink (GTK_OBJECT (selector->priv->vadj));

	g_signal_connect (G_OBJECT (selector->priv->hadj),
			  "value_changed",
			  G_CALLBACK (hadj_value_changed),
			  selector);
	g_signal_connect (G_OBJECT (selector->priv->vadj),
			  "value_changed",
			  G_CALLBACK (vadj_value_changed),
			  selector);
}


static void
iviewer_set_zoom (GthIViewer *iviewer,
		  double      zoom)
{
	GthImageSelector *selector = (GthImageSelector*) iviewer;
	GtkWidget        *widget = (GtkWidget*) selector;

	generic_set_zoom (selector,
			  zoom,
			  widget->allocation.width / 2,
			  widget->allocation.height / 2);
}


static double
iviewer_get_zoom (GthIViewer *iviewer)
{
	GthImageSelector *selector = (GthImageSelector*) iviewer;
	return selector->priv->zoom;
}


static gdouble zooms[] = {0.15, 0.20, 0.30, 0.50, 0.75, 1.0,
			  1.5 , 2.0 , 3.0 , 5.0 , 7.5,  10.0};

static const gint nzooms = sizeof (zooms) / sizeof (gdouble);


static gdouble
get_next_zoom (gdouble zoom)
{
	gint i;

	i = 0;
	while ((i < nzooms) && (zooms[i] <= zoom))
		i++;
	i = CLAMP (i, 0, nzooms - 1);

	return zooms[i];
}


static gdouble
get_prev_zoom (gdouble zoom)
{
	gint i;

	i = nzooms - 1;
	while ((i >= 0) && (zooms[i] >= zoom))
		i--;
	i = CLAMP (i, 0, nzooms - 1);

	return zooms[i];
}


static void
iviewer_zoom_in (GthIViewer *iviewer)
{
	if (gth_iviewer_get_image (iviewer) == NULL)
		return;
	gth_iviewer_set_zoom (iviewer, get_next_zoom (gth_iviewer_get_zoom (iviewer)));
}


static void
iviewer_zoom_out (GthIViewer *iviewer)
{
	if (gth_iviewer_get_image (iviewer) == NULL)
		return;
	gth_iviewer_set_zoom (iviewer, get_prev_zoom (gth_iviewer_get_zoom (iviewer)));
}


static void
iviewer_zoom_to_fit (GthIViewer *iviewer)
{
	GthImageSelector *selector = (GthImageSelector*) iviewer;

	selector->priv->zoom_to_fit = TRUE;
	gtk_widget_queue_resize (GTK_WIDGET (selector));
}


static gboolean
scroll_event (GtkWidget      *widget,
	      GdkEventScroll *event)
{
	GthImageSelector *selector = (GthImageSelector *) widget;

	if (event->direction == GDK_SCROLL_UP) {
		generic_set_zoom (selector,
				  get_next_zoom (selector->priv->zoom),
				  (int) event->x,
				  (int) event->y);
		return TRUE;
	}

	if (event->direction == GDK_SCROLL_DOWN) {
		generic_set_zoom (selector,
				  get_prev_zoom (selector->priv->zoom),
				  (int) event->x,
				  (int) event->y);
		return TRUE;
	}

	return FALSE;
}


static GdkPixbuf*
iviewer_get_image (GthIViewer *iviewer)
{
	return GTH_IMAGE_SELECTOR (iviewer)->priv->pixbuf;
}


void
iviewer_get_adjustments (GthIViewer     *self,
			 GtkAdjustment **hadj,
			 GtkAdjustment **vadj)
{
	GthImageSelector *selector = (GthImageSelector *) self;

	if (hadj != NULL)
		*hadj = selector->priv->hadj;
	if (vadj != NULL)
		*vadj = selector->priv->vadj;
}


static void
gth_iviewer_interface_init (gpointer g_iface,
			    gpointer iface_data)
{
	GthIViewerInterface *iface = (GthIViewerInterface *)g_iface;

	iface->get_zoom = iviewer_get_zoom;
	iface->set_zoom = iviewer_set_zoom;
	iface->zoom_in = iviewer_zoom_in;
	iface->zoom_out = iviewer_zoom_out;
	iface->zoom_to_fit = iviewer_zoom_to_fit;
	iface->get_image = iviewer_get_image;
	iface->get_adjustments = iviewer_get_adjustments;
}


GType
gth_image_selector_get_type ()
{
        static GType type = 0;

        if (! type) {
                GTypeInfo type_info = {
			sizeof (GthImageSelectorClass),
			NULL,
			NULL,
			(GClassInitFunc) class_init,
			NULL,
			NULL,
			sizeof (GthImageSelector),
			0,
			(GInstanceInitFunc) init
		};
		static const GInterfaceInfo iviewer_info = {
			(GInterfaceInitFunc) gth_iviewer_interface_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_WIDGET,
					       "GthImageSelector",
					       &type_info,
					       0);
		g_type_add_interface_static (type,
					     GTH_TYPE_IVIEWER,
					     &iviewer_info);
	}

        return type;
}


GtkWidget*
gth_image_selector_new (GthSelectorType  type,
			GdkPixbuf       *pixbuf)
{
	GthImageSelector *selector;

	selector = (GthImageSelector*) g_object_new (GTH_IMAGE_SELECTOR_TYPE, NULL);
	selector->priv->type = type;
	if (selector->priv->type == GTH_SELECTOR_TYPE_POINT)
		selector->priv->mask_visible = FALSE;
	gth_image_selector_set_pixbuf (selector, pixbuf);

	return (GtkWidget*) selector;
}


void
gth_image_selector_set_pixbuf (GthImageSelector *selector,
			       GdkPixbuf        *pixbuf)
{
	GthImageSelectorPriv *priv = selector->priv;

	if (priv->pixbuf != NULL)
		g_object_unref (priv->pixbuf);
	priv->pixbuf = NULL;

	if (priv->background != NULL)
		g_object_unref (priv->background);
	priv->background = NULL;

	if (pixbuf == NULL) {
		priv->pixbuf_area.width = 0;
		priv->pixbuf_area.height = 0;
		priv->zoom = 1.0;
		return;
	}

	priv->pixbuf = pixbuf;
	g_object_ref (priv->pixbuf);

	priv->pixbuf_area.width = gdk_pixbuf_get_width (pixbuf);
	priv->pixbuf_area.height = gdk_pixbuf_get_height (pixbuf);

	priv->background = gdk_pixbuf_composite_color_simple (
			      priv->pixbuf,
			      gdk_pixbuf_get_width (priv->pixbuf),
			      gdk_pixbuf_get_height (priv->pixbuf),
			      GDK_INTERP_NEAREST,
			      196,
			      10,
			      0x00000000,
			      0x00000000);

	init_selection (selector);

	gtk_widget_queue_resize (GTK_WIDGET (selector));
	gtk_widget_queue_draw (GTK_WIDGET (selector));
}


GdkPixbuf*
gth_image_selector_get_pixbuf (GthImageSelector *selector)
{
	return selector->priv->pixbuf;
}


void
gth_image_selector_set_selection_x (GthImageSelector *selector,
				    int               x)
{
	GdkRectangle new_selection;

	new_selection = selector->priv->selection;
	new_selection.x = x;
	check_and_set_new_selection (selector, new_selection);
}


void
gth_image_selector_set_selection_y (GthImageSelector *selector,
				    int               y)
{
	GdkRectangle new_selection;

	new_selection = selector->priv->selection;
	new_selection.y = y;
	check_and_set_new_selection (selector, new_selection);
}


void
gth_image_selector_set_selection_width (GthImageSelector *selector,
					int               width)
{
	GdkRectangle new_selection;

	new_selection = selector->priv->selection;
	new_selection.width = width;
	if (selector->priv->use_ratio)
		new_selection.height = IROUND (width / selector->priv->ratio);
	check_and_set_new_selection (selector, new_selection);
}


void
gth_image_selector_set_selection_height (GthImageSelector *selector,
					 int               height)
{
	GdkRectangle new_selection;

	new_selection = selector->priv->selection;
	new_selection.height = height;
	if (selector->priv->use_ratio)
		new_selection.width = IROUND (height * selector->priv->ratio);
	check_and_set_new_selection (selector, new_selection);
}


void
gth_image_selector_set_selection (GthImageSelector *selector,
				  GdkRectangle      selection)
{
	set_selection (selector, selection, FALSE);
}


void
gth_image_selector_get_selection (GthImageSelector *selector,
				  GdkRectangle     *selection)
{
	GthImageSelectorPriv *priv = selector->priv;

	selection->x = MAX (priv->selection.x, 0);
	selection->y = MAX (priv->selection.y, 0);
	selection->width = MIN (priv->selection.width, priv->pixbuf_area.width - priv->selection.x);
	selection->height = MIN (priv->selection.height, priv->pixbuf_area.height - priv->selection.y);
}


void
gth_image_selector_set_ratio (GthImageSelector *selector,
			      gboolean          use_ratio,
			      double            ratio)
{
	GthImageSelectorPriv *priv = selector->priv;

	priv->use_ratio = use_ratio;
	priv->ratio = ratio;

	if (priv->use_ratio) {
		gth_image_selector_set_selection_width (selector, priv->selection.width);
		gth_image_selector_set_selection_height (selector, priv->selection.height);
	}
}


double
gth_image_selector_get_ratio (GthImageSelector *selector)
{
	return selector->priv->ratio;
}


gboolean
gth_image_selector_get_use_ratio (GthImageSelector *selector)
{
	return selector->priv->use_ratio;
}


void
gth_image_selector_set_mask_visible (GthImageSelector *selector,
				     gboolean          visible)
{
	if (visible == selector->priv->mask_visible)
		return;

	selector->priv->mask_visible = visible;
	if (selector->priv->paint_pixbuf != NULL) {
		g_object_unref (selector->priv->paint_pixbuf);
		selector->priv->paint_pixbuf = NULL;
	}
	gtk_widget_queue_draw (GTK_WIDGET (selector));

	g_signal_emit (G_OBJECT (selector),
		       signals[MASK_VISIBILITY_CHANGED],
		       0);
}


gboolean
gth_image_selector_get_mask_visible (GthImageSelector *selector)
{
	return selector->priv->mask_visible;
}
