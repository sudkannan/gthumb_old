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

/* Base on gs-fade.c from gnome-screensaver: 
 *
 * Copyright (C) 2004-2005 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#include "config.h"

#ifdef HAVE_GDKX

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "gs-fade.h"

#define STEPS_PER_SEC 30
#define DEFAULT_TIMEOUT 1000

/* XFree86 4.x+ Gamma fading */

#ifdef HAVE_XF86VMODE_GAMMA

#include <X11/extensions/xf86vmode.h>

typedef struct {
        XF86VidModeGamma vmg;
        int              size;
        unsigned short  *r;
        unsigned short  *g;
        unsigned short  *b;
} xf86_gamma_info;

#endif /* HAVE_XF86VMODE_GAMMA */

static void     gs_fade_class_init (GSFadeClass *klass);
static void     gs_fade_init       (GSFade      *fade);
static void     gs_fade_finalize   (GObject        *object);

#define GS_FADE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GS_TYPE_FADE, GSFadePrivate))

struct GSFadePrivate
{
        GsFadeDirection    direction;
        gboolean           fading;
        guint              step_id;

        guint              timeout;
        guint              step;
        guint              num_steps;
        guint              msecs_per_step;
        gdouble            alpha;

        int                fade_type;
        int                num_screens;
#ifdef HAVE_XF86VMODE_GAMMA
        xf86_gamma_info   *gamma_info;
#endif /* HAVE_XF86VMODE_GAMMA */

};

enum { 
        FADED,
        LAST_SIGNAL
};

enum {
        PROP_0
};

enum {
        FADE_TYPE_NONE,
        FADE_TYPE_GAMMA_NUMBER,
        FADE_TYPE_GAMMA_RAMP
};

static GObjectClass *parent_class = NULL;
static guint         signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GSFade, gs_fade, G_TYPE_OBJECT)

static gpointer fade_object = NULL;

#ifdef HAVE_XF86VMODE_GAMMA

/* This is needed because the VidMode extension doesn't work
   on remote displays -- but if the remote display has the extension
   at all, XF86VidModeQueryExtension returns true, and then
   XF86VidModeQueryVersion dies with an X error.
*/

static gboolean error_handler_hit = FALSE;

static int
ignore_all_errors_ehandler (Display     *dpy,
                            XErrorEvent *error)
{
        error_handler_hit = TRUE;

        return 0;
}

static Bool
safe_XF86VidModeQueryVersion (Display *dpy,
                              int     *majP,
                              int     *minP)
{
        Bool          result;
        XErrorHandler old_handler;

        XSync (dpy, False);
        error_handler_hit = FALSE;
        old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

        result = XF86VidModeQueryVersion (dpy, majP, minP);

        XSync (dpy, False);
        XSetErrorHandler (old_handler);
        XSync (dpy, False);

        return (error_handler_hit
                ? False
                : result);
}

static gboolean
xf86_whack_gamma (int              screen,
                  xf86_gamma_info *info,
                  float            ratio)
{
        Bool status;

        if (ratio < 0)
                ratio = 0;
        if (ratio > 1)
                ratio = 1;

        if (info->size == 0) {
                /* we only have a gamma number, not a ramp. */

                XF86VidModeGamma g2;

                g2.red   = info->vmg.red   * ratio;
                g2.green = info->vmg.green * ratio;
                g2.blue  = info->vmg.blue  * ratio;

# ifdef XF86_MIN_GAMMA
                if (g2.red < XF86_MIN_GAMMA)
                        g2.red = XF86_MIN_GAMMA;
                if (g2.green < XF86_MIN_GAMMA)
                        g2.green = XF86_MIN_GAMMA;
                if (g2.blue < XF86_MIN_GAMMA)
                        g2.blue = XF86_MIN_GAMMA;
# endif

                status = XF86VidModeSetGamma (GDK_DISPLAY (), screen, &g2);
        } else {

# ifdef HAVE_XF86VMODE_GAMMA_RAMP
                unsigned short *r, *g, *b;
                int i;

                r = (unsigned short *) malloc (info->size * sizeof (unsigned short));
                g = (unsigned short *) malloc (info->size * sizeof (unsigned short));
                b = (unsigned short *) malloc (info->size * sizeof (unsigned short));

                for (i = 0; i < info->size; i++) {
                        r[i] = info->r[i] * ratio;
                        g[i] = info->g[i] * ratio;
                        b[i] = info->b[i] * ratio;
                }

                status = XF86VidModeSetGammaRamp (GDK_DISPLAY (), screen, info->size, r, g, b);

                free (r);
                free (g);
                free (b);

# else  /* !HAVE_XF86VMODE_GAMMA_RAMP */
                /*abort ();*/
		status = FALSE;
# endif /* !HAVE_XF86VMODE_GAMMA_RAMP */
        }

        gdk_flush ();

        return status;
}

#endif /* HAVE_XF86VMODE_GAMMA */

/* VidModeExtension version 2.0 or better is needed to do gamma.
   2.0 added gamma values; 2.1 added gamma ramps.
*/
# define XF86_VIDMODE_GAMMA_MIN_MAJOR 2
# define XF86_VIDMODE_GAMMA_MIN_MINOR 0
# define XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR 2
# define XF86_VIDMODE_GAMMA_RAMP_MIN_MINOR 1

static int
check_gamma_extension (void)
{
#ifdef HAVE_XF86VMODE_GAMMA
        int event, error, major, minor;

        if (! XF86VidModeQueryExtension (GDK_DISPLAY (), &event, &error))
                return FADE_TYPE_NONE;  /* display doesn't have the extension. */

        if (! safe_XF86VidModeQueryVersion (GDK_DISPLAY (), &major, &minor))
                return FADE_TYPE_NONE;  /* unable to get version number? */

        if (major < XF86_VIDMODE_GAMMA_MIN_MAJOR || 
            (major == XF86_VIDMODE_GAMMA_MIN_MAJOR &&
             minor < XF86_VIDMODE_GAMMA_MIN_MINOR))
                return FADE_TYPE_NONE;  /* extension is too old for gamma. */

        if (major < XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR || 
            (major == XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR &&
             minor < XF86_VIDMODE_GAMMA_RAMP_MIN_MINOR))
                return FADE_TYPE_GAMMA_NUMBER;  /* extension is too old for gamma ramps. */

        /* Copacetic */
        return FADE_TYPE_GAMMA_RAMP;
#else
        return FADE_TYPE_NONE;
#endif /* HAVE_XF86VMODE_GAMMA */
}

static gboolean
gamma_info_init (GSFade *fade)
{
#ifdef HAVE_XF86VMODE_GAMMA
        int              screen;
        xf86_gamma_info *info;

# ifndef HAVE_XF86VMODE_GAMMA_RAMP
        if (FADE_TYPE_GAMMA_RAMP == fade->priv->fade_type) {
                /* server is newer than client! */
                fade->priv->fade_type = FADE_TYPE_GAMMA_NUMBER;
        }
# endif

        info = g_new0 (xf86_gamma_info, fade->priv->num_screens);
        fade->priv->gamma_info = info;
        
        /* Get the current gamma maps for all screens.
           Bug out and return -1 if we can't get them for some screen.
        */
        for (screen = 0; screen < fade->priv->num_screens; screen++) {

                if (FADE_TYPE_GAMMA_NUMBER == fade->priv->fade_type) {
                        /* only have gamma parameter, not ramps. */

                        if (! XF86VidModeGetGamma (GDK_DISPLAY (), screen, &info [screen].vmg))
                                goto FAIL;
                }

# ifdef HAVE_XF86VMODE_GAMMA_RAMP

                else if (FADE_TYPE_GAMMA_RAMP == fade->priv->fade_type) {
                        /* have ramps */

                        if (! XF86VidModeGetGammaRampSize (GDK_DISPLAY (), screen, &info [screen].size))
                                goto FAIL;
                        if (info [screen].size <= 0)
                                goto FAIL;

                        info [screen].r = (unsigned short *)
                                calloc (info[screen].size, sizeof (unsigned short));
                        info [screen].g = (unsigned short *)
                                calloc (info[screen].size, sizeof (unsigned short));
                        info [screen].b = (unsigned short *)
                                calloc (info[screen].size, sizeof (unsigned short));

                        if (! (info [screen].r && info [screen].g && info [screen].b))
                                goto FAIL;

                        if (! XF86VidModeGetGammaRamp (GDK_DISPLAY (),
                                                       screen,
                                                       info [screen].size,
                                                       info [screen].r,
                                                       info [screen].g,
                                                       info [screen].b))
                                goto FAIL;
                }
# endif /* HAVE_XF86VMODE_GAMMA_RAMP */
                else {
                        /*abort ();*/
			goto FAIL;
                }
        }

        return TRUE;
 FAIL:
#endif /* HAVE_XF86VMODE_GAMMA */
        return FALSE;
}

static void
gamma_info_free (GSFade *fade)
{
#ifdef HAVE_XF86VMODE_GAMMA

        if (fade->priv->gamma_info) {
                int screen;

                for (screen = 0; screen < fade->priv->num_screens; screen++) {
                        if (fade->priv->gamma_info [screen].r)
                                g_free (fade->priv->gamma_info[screen].r);
                        if (fade->priv->gamma_info [screen].g)
                                g_free (fade->priv->gamma_info[screen].g);
                        if (fade->priv->gamma_info [screen].b)
                                g_free (fade->priv->gamma_info[screen].b);
                }

                g_free (fade->priv->gamma_info);
                fade->priv->gamma_info = NULL;
        }

#endif /* HAVE_XF86VMODE_GAMMA */
}

#define XF86_MIN_GAMMA  0.1

static gboolean
gs_fade_set_alpha_gamma (GSFade *fade,
                         gdouble alpha)
{
#ifdef HAVE_XF86VMODE_GAMMA
        int      screen;
        gboolean res;

        for (screen = 0; screen < fade->priv->num_screens; screen++) {
                res = xf86_whack_gamma (screen, &fade->priv->gamma_info [screen], alpha);
        }

        return TRUE;
#else
        return FALSE;
#endif /* HAVE_XF86VMODE_GAMMA */
}

static gboolean
gs_fade_set_alpha (GSFade *fade,
                   gdouble alpha)
{
        gboolean ret;

        switch (fade->priv->fade_type) {
        case FADE_TYPE_GAMMA_RAMP:
        case FADE_TYPE_GAMMA_NUMBER:
                ret = gs_fade_set_alpha_gamma (fade, alpha);
                break;
        case FADE_TYPE_NONE:
                ret = FALSE;
                break;
        default:
                g_warning ("Unknown fade type");
                ret = FALSE;
                break;
        }

        return ret;
}

static void
gs_fade_set_property (GObject            *object,
                      guint               prop_id,
                      const GValue       *value,
                      GParamSpec         *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gs_fade_get_property (GObject            *object,
                      guint               prop_id,
                      GValue             *value,
                      GParamSpec         *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gs_fade_class_init (GSFadeClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize     = gs_fade_finalize;
        object_class->get_property = gs_fade_get_property;
        object_class->set_property = gs_fade_set_property;

        signals [FADED] =
                g_signal_new ("faded",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GSFadeClass, faded),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE,
                              1, G_TYPE_INT);

        g_type_class_add_private (klass, sizeof (GSFadePrivate));
}

static void
gs_fade_init (GSFade *fade)
{
        GdkDisplay *display;

        fade->priv = GS_FADE_GET_PRIVATE (fade);

	fade->priv->direction = GS_FADE_DIRECTION_OUT;
	fade->priv->fading = FALSE;
        fade->priv->alpha = 0.0;
        gs_fade_set_timeout (fade, DEFAULT_TIMEOUT);

        fade->priv->fade_type = check_gamma_extension ();

        display = gdk_display_get_default ();
        fade->priv->num_screens = gdk_display_get_n_screens (display);

        gamma_info_init (fade);
}

static void
gs_fade_stop (GSFade *fade)
{
        if (fade->priv->step_id != 0) {
                g_source_remove (fade->priv->step_id);
                fade->priv->step_id = 0;
        }
        fade->priv->fading = FALSE;
}

static void
gs_fade_finalize (GObject *object)
{
        GSFade *fade;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GS_IS_FADE (object));

        fade = GS_FADE (object);

        g_return_if_fail (fade->priv != NULL);

        gs_fade_stop (fade);
        gamma_info_free (fade);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

GSFade *
gs_fade_new (void)
{
        if (fade_object) {
                g_object_ref (fade_object);
        } else {
                fade_object = g_object_new (GS_TYPE_FADE, NULL);
                g_object_add_weak_pointer (fade_object,
                                           (gpointer *) &fade_object);
        }

        return GS_FADE (fade_object);
}

void
gs_fade_set_timeout (GSFade *fade,
                     guint   timeout)
{
        fade->priv->timeout = timeout;
        fade->priv->step = 0;
        fade->priv->num_steps = (fade->priv->timeout / 1000) * STEPS_PER_SEC;
        fade->priv->msecs_per_step = 1000 / STEPS_PER_SEC;
}

static gboolean
gs_fade_faded (GSFade *fade)
{
        fade->priv->step_id = 0;
        fade->priv->fading = FALSE;

        g_signal_emit (fade, signals[FADED], 0, fade->priv->direction);

        return FALSE;
}

static gboolean
gs_fade_step (gpointer data)
{ 
        GSFade  *fade = data;
        gdouble  sign;

        if (!gs_fade_set_alpha (fade, fade->priv->alpha)) 
                return gs_fade_faded (fade);

        fade->priv->step++;
        if (fade->priv->step > fade->priv->num_steps) 
                return gs_fade_faded (fade);

        sign = 1.0;
        if (fade->priv->direction == GS_FADE_DIRECTION_OUT)
                sign = -1.0;
        fade->priv->alpha += (sign) * (gdouble)fade->priv->step / (gdouble)fade->priv->num_steps;

        return TRUE;
}

void
gs_fade_in (GSFade *fade)
{
        if (fade->priv->fading)
                return;

        fade->priv->fading = TRUE;

        fade->priv->direction = GS_FADE_DIRECTION_IN;
        fade->priv->alpha = 0.0;
        fade->priv->step = 0;
        fade->priv->step_id = g_timeout_add (fade->priv->msecs_per_step, 
                                             gs_fade_step,
                                             fade);
}

void
gs_fade_out (GSFade *fade)
{       
        if (fade->priv->fading)
                return;

        fade->priv->fading = TRUE;

        fade->priv->direction = GS_FADE_DIRECTION_OUT;
        fade->priv->alpha = 1.0;
        fade->priv->step = 0;
        fade->priv->step_id = g_timeout_add (fade->priv->msecs_per_step, 
                                             gs_fade_step,
                                             fade);
}

void
gs_fade_complete (GSFade *fade)
{
        gs_fade_stop (fade);
	fade->priv->direction = GS_FADE_DIRECTION_IN;
	gs_fade_set_alpha (fade, 1.0);
        gs_fade_faded (fade);           
}

void
gs_fade_reset (GSFade *fade)
{ 
        gs_fade_stop (fade);
        gs_fade_set_alpha (fade, 1.0);
}

gboolean
gs_fade_is_fading (GSFade *fade)
{
	return fade->priv->fading;
}

gboolean
gs_fade_is_fading_in (GSFade *fade)
{
	return fade->priv->fading && (fade->priv->direction = GS_FADE_DIRECTION_IN);
}

gboolean
gs_fade_is_fading_out (GSFade *fade)
{
	return fade->priv->fading && (fade->priv->direction = GS_FADE_DIRECTION_OUT);
}

#endif /* HAVE_GDKX */
