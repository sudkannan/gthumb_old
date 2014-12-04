/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2001, 2002 The Free Software Foundation, Inc.
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

#ifndef JPEGTRAN_H
#define JPEGTRAN_H

#include <glib.h>
#include <libexif/exif-data.h>
#include "transupp.h"

#define JPEGTRAN_ERROR_MCU 2

typedef enum {
	JPEG_MCU_ACTION_TRIM,
	JPEG_MCU_ACTION_DONT_TRIM,
	JPEG_MCU_ACTION_ABORT
} JpegMcuAction;

gboolean   jpegtran                   (const char     *input_filename,
		     		       const char     *output_filename,
		     		       JXFORM_CODE     transformation,
		     		       JpegMcuAction   mcu_action,
		     		       GError        **error);
void set_exif_orientation_to_top_left (ExifData *edata);

#endif /* JPEGTRAN_H */
