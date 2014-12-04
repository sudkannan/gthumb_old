/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2003 Free Software Foundation, Inc.
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
#include <stdlib.h>
#include <time.h>
#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>

#include "file-utils.h"
#include "gth-exif-utils.h"
#include "glib-utils.h"


ExifData *
gth_exif_data_new_from_uri (const char *uri)
{
        char     *local_file = NULL;
	ExifData *new_exif_data;

	if (uri == NULL)
		return NULL;

	local_file = get_cache_filename_from_uri (uri);
	new_exif_data = exif_data_new_from_file (local_file);
	g_free (local_file);

	return new_exif_data;
}


char *
get_exif_tag (const char *uri,
	      ExifTag     etag)
{
	ExifData     *edata;
	unsigned int  i, j;

	if (uri == NULL)
		return g_strdup ("-");

	edata = gth_exif_data_new_from_uri (uri);
	if (edata == NULL) 
		return g_strdup ("-");

	for (i = 0; i < EXIF_IFD_COUNT; i++) {
		ExifContent *content = edata->ifd[i];

		if (! edata->ifd[i] || ! edata->ifd[i]->count) 
			continue;

		for (j = 0; j < content->count; j++) {
			ExifEntry *e = content->entries[j];

			if (! content->entries[j]) 
				continue;

			if (e->tag == etag) {
				const char *value;
				char *retval = NULL;

				value = get_exif_entry_value (e);
				if (value != NULL)
					retval = g_locale_to_utf8 (value, -1, 0, 0, 0);
				else
					retval = g_strdup ("-");
				exif_data_unref (edata);

				return retval;
			}
		}
	}

	exif_data_unref (edata);

	return g_strdup ("-");
}


ExifShort
get_exif_tag_short (const char *uri,
		    ExifTag     etag)
{
	ExifData     *edata;
	unsigned int  i, j;

	if (uri == NULL)
		return 0;

	edata = gth_exif_data_new_from_uri (uri);
	if (edata == NULL) 
		return 0;

	for (i = 0; i < EXIF_IFD_COUNT; i++) {
		ExifContent *content = edata->ifd[i];

		if (! edata->ifd[i] || ! edata->ifd[i]->count) 
			continue;

		for (j = 0; j < content->count; j++) {
			ExifEntry *e = content->entries[j];

			if (! content->entries[j]) 
				continue;

			if (e->tag == etag) {
				ExifByteOrder o;
				ExifShort retval = 0;

				o = exif_data_get_byte_order (e->parent->parent);
				if (e->data != NULL)
					retval = exif_get_short (e->data, o);
				exif_data_unref (edata);

				return retval;
			}
		}
	}

	exif_data_unref (edata);

	return 0;
}


time_t
exif_string_to_time_t (char *string) 
{
        char       *data;
        struct tm   tm = { 0, };

	if (	(string == NULL) 	/* check for non-existent field */
	     || (strlen (string) < 10)	/* check for too-small field */
	     || (string[0] == 0)	/* check for empty field */
	     				/* check for year 1xxx or 2xxx */
	     || !((string[0] == '1') || (string[0] == '2')) )
		return (time_t) 0;

        data = g_strdup (string);

        data[4] = data[7] = data[10] = '\0';

        tm.tm_year = atoi (data) - 1900;
        tm.tm_mon  = atoi (data + 5) - 1;
        tm.tm_mday = atoi (data + 8);
        tm.tm_hour = 0;
        tm.tm_min  = 0;
        tm.tm_sec  = 0;
        tm.tm_isdst = -1;

        if (strlen (string) > 10) {
                data[13] = data[16] = '\0';
                tm.tm_hour = atoi (data + 11);
                tm.tm_min  = atoi (data + 14);
                tm.tm_sec  = atoi (data + 17);
        }

        g_free (data);
        return mktime (&tm);
}

	
static time_t
get_exif_time (const char *uri)
{
	char   *local_file = NULL;
        char    date_string[64] = {0};
        time_t  time = 0;

        if (uri == NULL)
                return (time_t) 0;

        local_file = get_cache_filename_from_uri (uri);
	if (local_file == NULL)
                return (time_t) 0;

        gth_minimal_exif_tag_read (local_file, EXIF_TAG_DATE_TIME_ORIGINAL, date_string, 20);
	time = exif_string_to_time_t (date_string);

	if (time <= (time_t) 0) {
		gth_minimal_exif_tag_read (local_file, EXIF_TAG_DATE_TIME_DIGITIZED, date_string, 20);
		time = exif_string_to_time_t (date_string);
	}

	if (time <= (time_t) 0) {
		gth_minimal_exif_tag_read (local_file, EXIF_TAG_DATE_TIME, date_string, 20);
		time = exif_string_to_time_t (date_string);
	}

	g_free (local_file);

	if (time < (time_t) 0)
		return (time_t) 0;
	else
        	return time;
}


time_t
get_metadata_time (const char *mime_type, 
		   const char *uri)
{
	if (mime_type == NULL)
		mime_type = get_mime_type (uri);

	if (mime_type_is (mime_type, "image/jpeg"))
		return get_exif_time (uri);

	return (time_t) 0;
}


char *
get_exif_aperture_value (const char *uri)
{
	ExifData     *edata;
	unsigned int  i, j;

	if (uri == NULL)
		return g_strdup ("-");

	edata = gth_exif_data_new_from_uri (uri);
	if (edata == NULL) 
		return g_strdup ("-");

	for (i = 0; i < EXIF_IFD_COUNT; i++) {
		ExifContent *content = edata->ifd[i];

		if (! edata->ifd[i] || ! edata->ifd[i]->count) 
			continue;

		for (j = 0; j < content->count; j++) {
			ExifEntry   *e = content->entries[j];
			const char  *value = NULL;
			char        *retval = NULL;

			if (! content->entries[j]) 
				continue;

			if ((e->tag != EXIF_TAG_APERTURE_VALUE) &&
			    (e->tag != EXIF_TAG_FNUMBER))
				continue;

			value = get_exif_entry_value (e);
			if (value)
				retval = g_locale_to_utf8 (value, -1, 0, 0, 0);
			else
				retval = g_strdup ("-");
			exif_data_unref (edata);

			return retval;
		}
	}

	exif_data_unref (edata);

	return g_strdup ("-");
}


gboolean 
have_exif_time (const char *uri)
{
	return get_metadata_time (NULL, uri) != (time_t)0;
}


#define VALUE_LEN 1024


const char *
get_exif_entry_value (ExifEntry *entry)
{
	char value[VALUE_LEN + 1];
	return exif_entry_get_value (entry, value, VALUE_LEN);
}


void 
save_exif_data_to_uri (const char *uri,
		       ExifData   *edata)
{
	char      *local_file = NULL;
	JPEGData  *jdata;

	local_file = get_cache_filename_from_uri (uri);
	jdata = jpeg_data_new_from_file (local_file);
	if (jdata == NULL) 
		return;

	if (edata != NULL)
		jpeg_data_set_exif_data (jdata, edata);

	jpeg_data_save_file (jdata, local_file);
	jpeg_data_unref (jdata);

        g_free (local_file);
}


void 
copy_exif_data (const char *uri_src,
		const char *uri_dest)
{
	ExifData *edata;

	if (! image_is_jpeg (uri_src) || ! image_is_jpeg (uri_dest))
		return;

	edata = gth_exif_data_new_from_uri (uri_src);
	if (edata == NULL) 
		return;
	save_exif_data_to_uri (uri_dest, edata);

	exif_data_unref (edata);
}


const char leth[]  = {0x49, 0x49, 0x2a, 0x00}; // Little endian TIFF header
const char beth[]  = {0x4d, 0x4d, 0x00, 0x2a}; // Big endian TIFF header
const char types[] = {0x00, 0x01, 0x01, 0x02, 0x04, 0x08, 0x00, 0x08, 0x00, 0x04, 0x08}; // size in bytes for EXIF types
 
#define DE_ENDIAN16(val) endian == G_BIG_ENDIAN ? GUINT16_FROM_BE(val) : GUINT16_FROM_LE(val)
#define DE_ENDIAN32(val) endian == G_BIG_ENDIAN ? GUINT32_FROM_BE(val) : GUINT32_FROM_LE(val)
 
#define ENDIAN16_IT(val) endian == G_BIG_ENDIAN ? GUINT16_TO_BE(val) : GUINT16_TO_LE(val)
#define ENDIAN32_IT(val) endian == G_BIG_ENDIAN ? GUINT32_TO_BE(val) : GUINT32_TO_LE(val)
 
#define IFD_OFFSET_PUSH(val) if (oi < sizeof(offsets) - 1) offsets[oi++] = val; else return PATCH_EXIF_TOO_MANY_IFDS;
#define IFD_OFFSET_PULL()    offsets[--oi]
 
#define IFD_NAME_PUSH(val) names[ni++] = val;
#define IFD_NAME_PULL()    names[--ni]

static unsigned short de_get16(void *ptr, guint endian)
{
	unsigned short val;

	memcpy(&val, ptr, sizeof(val));
	val = DE_ENDIAN16(val);

	return val;
}

static unsigned int de_get32(void *ptr, guint endian)
{
	unsigned int val;

	memcpy(&val, ptr, sizeof(val));
	val = DE_ENDIAN32(val);

	return val;
}

int static
gth_minimal_exif_tag_action (const char *local_file,
			     ExifTag     etag,
			     void       *data,
			     int         size,
			     int         access)
{
	/* This function updates/reads ONLY the affected tag. Unlike libexif, it does
	   not attempt to correct or re-format other data. This helps preserve
	   the integrity of certain MakerNote entries, by avoiding re-positioning
	   of the data whenever possible. Some MakerNotes incorporate absolute
	   offsets and others use relative offsets. The offset issue is 
	   avoided if tags are not moved or resized if not required. */

 	FILE *jf;            	// File descriptor to file to patch
 	char  buf[1024 * 64]; 	// Working buffer
 	int   i;               	// index into working buffer
 	int   tag;             	// endianed version of 'etag' in call
 	int   gpsifd;          	// endianed gps ifd pointer tag
 	int   exififd;         	// endianed exif ifd pointer tag
 	int   offset;          	// de-endianed offset in various situations
 	int   tags;            	// number of tags in current ifd
 	int   type;            	// de-endianed type of tag used as index into types[]
 	int   count;           	// de-endianed count of elements in a tag
 	int   stitch;          	// offset in data buffer in type size increments
        int   tiff = 0;   	// offset to active tiff header
        int   endian = 0;   	// detected endian of data
 	int   readsize = 0;   	// number of read bytes from file into buffer
 	int   writesize = 0;   	// number of written bytes from buffer to file
 	int   patches = 0;   	// number of values changed
 
 	// IFD stack variables
 	unsigned long offsets[32];  // Offsets in working buffer to start of IFDs
        char         *names[32];    // Printable names of IFD:s (for debug mostly)
        int           oi = 0;       // index into offsets
        int           ni = 0;       // iundex into names
 	int           cifdi = 0;    // curret ifd index
 
 	debug (DEBUG_INFO, "gth_minimal_exif_tag_access (%s, %04x, %08x, %d)\n", local_file, etag, data, size);

	// local files only
	g_assert (uri_has_scheme (local_file) == FALSE);

        // Init IFD stack
 	IFD_OFFSET_PUSH(0);
 	IFD_NAME_PUSH("START");
 
 	// open file r (read (r))
 	if ((jf = fopen (local_file, "r")) == NULL)
 		return PATCH_EXIF_FILE_ERROR;
 
        // Fill buffer
        readsize = fread (buf, 1, sizeof (buf), jf);
 
 	// close file during prcessing
 	fclose (jf);
 
        // Check for TIFF header and catch endianess
 	i = 0;
 	while (i < readsize) {
 
 		// Little endian TIFF header
 		if (bcmp (&buf[i], leth, 4) == 0) { 
 			endian = G_LITTLE_ENDIAN;
                }
 
 		// Big endian TIFF header
 		else if (bcmp (&buf[i], beth, 4) == 0) { 
 			endian = G_BIG_ENDIAN;
                }
 
 		// Keep looking through buffer
 		else {
 			i++;
 			continue;
 		}
 		// We have found either big or little endian TIFF header
 		tiff = i;
 		break;
        }
 
 	// So did we find a TIFF header or did we just hit end of buffer?
 	if (tiff == 0) return PATCH_EXIF_NO_TIFF;
 
        // Endian some tag values that we will look for
 	exififd = ENDIAN16_IT(0x8769);
 	gpsifd  = ENDIAN16_IT(0x8825);
        tag     = ENDIAN16_IT(etag);
 
        // Read out the offset
        offset  = de_get32(&buf[i] + 4, endian);
 	i       = i + offset;
 
	// Make sure the address is within bounds
	if (i >= readsize) return PATCH_EXIF_NO_TIFF;

        // Start out with IFD0 (and add more IFDs while we go)
 	IFD_OFFSET_PUSH(i);
 	IFD_NAME_PUSH("IFD0");
 
        // As long as we find more IFDs check out the tags in each
 	while ((oi >=0 && (i = IFD_OFFSET_PULL()) > 0 && i < readsize - 2)) {
 	  
 		cifdi    = oi; // remember which ifd we are at
 
 		debug (DEBUG_INFO, "%s:\n", IFD_NAME_PULL());
 		tags    = de_get16(&buf[i], endian);
 		i       = i + 2;
 
 		// Check this IFD for tags of interest
 		while (tags-- && i < readsize - 12) {
 			type   = de_get16(&buf[i + 2], endian);
 			count  = de_get32(&buf[i + 4], endian);
 			offset = de_get32(&buf[i + 8], endian);
 
 			debug (DEBUG_INFO, "TAG: %04x type:%02d count:%02d offset:%04lx ", 
			       de_get16(&buf[i], endian), type, count, offset);
 
 			// Our tag?
 			if (bcmp (&buf[i], (char *)&tag, 2) == 0) { 
 
				// Write TAG value
				if (access == 1) { 

					// Local value that can be patched directly in TAG table 
					if ((types[type] * count) <= 4) {
 
						// Fake TIFF offset
						offset = i + 8 - tiff; 
						patches++;
					}
					// Offseted value of same or larger size that we can patch
					else if (types[type] * count >= size) {
						
						// Adjust count to new length
						count = size / types[type]; 
						*((unsigned short*)(&buf[i + 4])) = ENDIAN32_IT(count);
						patches++;
					}
					// Otherwise we are not able to patch the new value, at least not here
					else {
						fprintf (stderr, "gth_minimal_exif_tag_write: New TAG value does not fit, no patch applied\n");
						i = i + 12;
						continue;
					}
					
					if (offset + tiff + count * types[type] > sizeof (buf)) return PATCH_EXIF_TRASHED_IFD;

					// Copy the data
					stitch = 0;
					while (stitch < count) {
						switch (types[type]) {
						case 1:
							buf[tiff + offset + stitch] = *(char *) (data + stitch);
							break;
						case 2:
							*((unsigned short*)(&buf[tiff + offset + stitch * 2])) = 
								ENDIAN16_IT(*(guint16 *) (data + stitch * 2));
							break;
						case 4:
							*((unsigned long*) (&buf[tiff + offset + stitch * 4])) = 
								ENDIAN32_IT(*(guint32 *) (data + stitch * 4));
							break;
						default:
							fprintf(stderr, "gth_minimal_exif_tag_write:unsupported element size\n");
							return PATCH_EXIF_UNSUPPORTED_TYPE;
						}
						stitch++;
					}
				}
				// Read a TAG value
				else{ 
					int ret = PATCH_EXIF_OK;

					// Local value that can be read directly in TAG table 
					if ((types[type] * count) <= 4 && size >= (types[type] * count)) {
 
						// Fake TIFF offset
						offset = i + 8 - tiff; 
						patches++;
					}
					// Offseted value of less or equal size than buffer
					else if (types[type] * count <= size) {
						patches++;
					}
					// Otherwise we are not able to read the value
					else {
						fprintf(stderr, "gth_minimal_exif_tag_read: TAG value does not fit, Can't read full value\n");
						// Adjust count and warn for partial data
						count = size / types[type];
						ret = PATCH_EXIF_TAGVAL_OVERFLOW;
					}
					
					stitch = 0;
					while (stitch < count) {
						switch (types[type]) {
						case 1:
							*(char *) (data + stitch) = buf[tiff + offset + stitch];
							break;
						case 2:
							*(guint16 *) (data + stitch * 2) = 
								de_get16(&buf[tiff + offset + stitch * 2], endian);
							
							break;
						case 4:
							*(guint32 *) (data + stitch * 4) =
								de_get32(&buf[tiff + offset + stitch * 4], endian);
							
							break;
						default:
							fprintf(stderr, "gth_minimal_exif_tag_read:unsupported element size\n");
							return PATCH_EXIF_UNSUPPORTED_TYPE;
						}
						stitch++;
					}					
					return ret;
				}
			}
 			// EXIF pointer tag?
 			else if (bcmp (&buf[i], (char *) &exififd, 2) == 0) { 
 				IFD_OFFSET_PUSH(offset + tiff);
 				IFD_NAME_PUSH("EXIF");
 			}
 			// GPS pointer tag?
 			else if (bcmp (&buf[i], (char *) &gpsifd, 2) == 0) { 
 				IFD_OFFSET_PUSH(offset + tiff);
 				IFD_NAME_PUSH("GPS");
 			}
 
 			debug (DEBUG_INFO, "\n");
 					
 			i = i + 12;
 		}

 		// Check for a valid next pointer and assume that to be IFD1 if we just checked IFD0
 		if (cifdi == 1 && i < readsize - tiff && (i = de_get32(&buf[i], endian)) != 0) { 
 			i = i + tiff;
 			IFD_OFFSET_PUSH(i);
 			IFD_NAME_PUSH("IFD1");
 		}
        }

 	// Check if we need to save
 	if (patches == 0)
 		return PATCH_EXIF_NO_TAGS;
 
 	// open file rb+ (read (r) and modify (+), b indicates binary file accces on non-Unix systems
 	if ((jf = fopen (local_file, "rb+")) == NULL)
 		return PATCH_EXIF_FILE_ERROR;
 
        // Save changes
 	writesize = fwrite (buf, 1, readsize, jf);
 	fclose (jf);
 
 	return (readsize == writesize) ? PATCH_EXIF_OK : PATCH_EXIF_FILE_ERROR; 
}


int
gth_minimal_exif_tag_read (const char *local_file,
 	                   ExifTag     etag,
 			   void       *data,
 			   int         size)
{
 	debug (DEBUG_INFO, "gth_minimal_exif_tag_read(%s, %04x, %08x, %d)\n", local_file, etag, data, size); 
	return gth_minimal_exif_tag_action (local_file, etag, data, size, 0);
}


int
gth_minimal_exif_tag_write (const char *local_file,
 	                    ExifTag     etag,
 			    void       *data,
 			    int         size)
{
 	debug (DEBUG_INFO, "gth_minimal_exif_tag_write(%s, %04x, %08x, %d)\n", local_file, etag, data, size); 
	return gth_minimal_exif_tag_action (local_file, etag, data, size, 1);
}


GthTransform
read_orientation_field (const char *local_file)
{
	ExifShort orientation;
	guint16   tf;

	if (local_file == NULL)
		return GTH_TRANSFORM_NONE;

	gth_minimal_exif_tag_read (local_file, EXIF_TAG_ORIENTATION, &tf, 2);
	orientation = (GthTransform) tf;
	if ((orientation >= 1) && (orientation <= 8))
		return orientation;
	else
		return GTH_TRANSFORM_NONE;
}


void
write_orientation_field (const char   *local_file,
			 GthTransform  transform)
{
	guint16 tf = (guint16) transform;

	if (local_file == NULL)
		return;
	gth_minimal_exif_tag_write (local_file, EXIF_TAG_ORIENTATION, &tf, 2);
}
