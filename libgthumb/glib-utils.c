/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2001, 2003 Free Software Foundation, Inc.
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

#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "glib-utils.h"


char *
_g_strdup_with_max_size (const char *s,
			 int         max_size)
{
	char *result;
	int   l = strlen (s);

	if (l > max_size) {
		char *first_half;
		char *second_half;
		int   offset;
		int   half_max_size = max_size / 2 + 1;

		first_half = g_strndup (s, half_max_size);
		offset = half_max_size + l - max_size;
		second_half = g_strndup (s + offset, half_max_size);

		result = g_strconcat (first_half, "...", second_half, NULL);

		g_free (first_half);
		g_free (second_half);
	} else
		result = g_strdup (s);

	return result;
}


/**
 * example 1 : "xxx##yy#" --> [0] = xxx
 *                            [1] = ##
 *                            [2] = yy
 *                            [3] = #
 *                            [4] = NULL
 *
 * example 2 : ""         --> [0] = NULL
 **/
char **
_g_get_template_from_text (const char *utf8_template)
{
	const char  *chunk_start = utf8_template;
	char       **str_vect;
	GList       *str_list = NULL, *scan;
	int          n = 0;

	if (utf8_template == NULL)
		return NULL;

	while (*chunk_start != 0) {
		gunichar    ch;
		gboolean    reading_sharps;
		char       *chunk;
		const char *chunk_end;
		int         chunk_len = 0;

		reading_sharps = (g_utf8_get_char (chunk_start) == '#');
		chunk_end = chunk_start;

		ch = g_utf8_get_char (chunk_end);
		while (reading_sharps
		       && (*chunk_end != 0)
		       && (ch == '#')) {
			chunk_end = g_utf8_next_char (chunk_end);
			ch = g_utf8_get_char (chunk_end);
			chunk_len++;
		}

		ch = g_utf8_get_char (chunk_end);
		while (! reading_sharps
		       && (*chunk_end != 0)
		       && (*chunk_end != '#')) {
			chunk_end = g_utf8_next_char (chunk_end);
			ch = g_utf8_get_char (chunk_end);
			chunk_len++;
		}

		chunk = _g_utf8_strndup (chunk_start, chunk_len);
		str_list = g_list_prepend (str_list, chunk);
		n++;

		chunk_start = chunk_end;
	}

	str_vect = g_new (char*, n + 1);

	str_vect[n--] = NULL;
	for (scan = str_list; scan; scan = scan->next)
		str_vect[n--] = scan->data;

	g_list_free (str_list);

	return str_vect;
}


char *
_g_get_name_from_template (char **utf8_template,
			   int    n)
{
	GString *s;
	int      i;
	char    *result;

	s = g_string_new (NULL);

	for (i = 0; utf8_template[i] != NULL; i++) {
		const char *chunk = utf8_template[i];
		gunichar    ch = g_utf8_get_char (chunk);

		if (ch != '#')
			g_string_append (s, chunk);
		else {
			char *s_n;
			int   s_n_len;
			int   sharps_len = g_utf8_strlen (chunk, -1);

			s_n = g_strdup_printf ("%d", n);
			s_n_len = strlen (s_n);

			while (s_n_len < sharps_len) {
				g_string_append_c (s, '0');
				sharps_len--;
			}

			g_string_append (s, s_n);
			g_free (s_n);
		}
	}

	result = s->str;
	g_string_free (s, FALSE);

	return result;
}


char *
_g_substitute (const char *from,
	       const char  this,
	       const char *with_this)
{
	char       *result;
	GString    *r;
	const char *s;

	if ((from == NULL) || (with_this == NULL))
		return g_strdup ("");

	if (strchr (from, this) == NULL)
		return g_strdup (from);

	r = g_string_new (NULL);
	for (s = from; *s != 0; s++)
		if (*s == this)
			g_string_append (r, with_this);
		else
			g_string_append_c (r, *s);

	result = r->str;
	g_string_free (r, FALSE);

	return result;
}


char *
_g_substitute_pattern (const char *utf8_text,
		       gunichar    pattern,
		       const char *value)
{
	const char *s;
	GString    *r;
	char       *r_str;

	if (utf8_text == NULL)
		return NULL;

	if (g_utf8_strchr (utf8_text, -1, '%') == NULL)
		return g_strdup (utf8_text);

	r = g_string_new (NULL);
	for (s = utf8_text; *s != 0; s = g_utf8_next_char (s)) {
		gunichar ch = g_utf8_get_char (s);

		if (ch == '%') {
			s = g_utf8_next_char (s);

			if (*s == 0) {
				g_string_append_unichar (r, ch);
				break;
			}

			ch = g_utf8_get_char (s);
			if (ch == pattern) {
				if (value)
					g_string_append (r, value);
			}
			else {
				g_string_append (r, "%");
				g_string_append_unichar (r, ch);
			}

		} else
			g_string_append_unichar (r, ch);
	}

	r_str = r->str;
	g_string_free (r, FALSE);

	return r_str;
}


char *
_g_utf8_strndup (const char *str,
		 gsize       n)
{
	const char *s = str;
	char       *result;

	while (n && *s) {
		s = g_utf8_next_char (s);
		n--;
	}

	result = g_strndup (str, s - str);

	return result;
}


char **
_g_utf8_strsplit (const char     *str,
		  const gunichar  delimiter)
{
	GSList      *slist = NULL, *scan;
	char       **str_array;
	const char  *s, *t;
	guint        n = 0;

	if (str == NULL)
		return g_new0 (char *, 1);

	t = s = str;
	do {
		gunichar ch = g_utf8_get_char (t);
		if ((ch == delimiter) || (*t == 0)) {
			if (t != s) {
				n++;
				slist = g_slist_prepend (slist, g_strndup (s, t - s));
			}

			if (*t != 0)
				t = s = g_utf8_next_char (t);
			else
				break;
		} else
			t = g_utf8_next_char (t);
	} while (TRUE);

	str_array = g_new (char*, n + 1);

	str_array[n--] = NULL;
	for (scan = slist; scan; scan = scan->next)
		str_array[n--] = scan->data;

	g_slist_free (slist);

	return str_array;
}


char *
_g_utf8_strstrip (const char *str)
{
	if (str == NULL)
		return NULL;
	return g_strstrip (g_strdup (str));
}


gboolean
_g_utf8_all_spaces (const char *utf8_string)
{
	gunichar c;

	c = g_utf8_get_char (utf8_string);
	while (c != 0) {
		if (! g_unichar_isspace (c))
			return FALSE;
		utf8_string = g_utf8_next_char (utf8_string);
		c = g_utf8_get_char (utf8_string);
	}

	return TRUE;
}


void
debug (const char *file,
       int         line,
       const char *function,
       const char *format, ...)
{
	static   gboolean first_time = 0;
	static   gboolean print_debug_info = 0;
	va_list  args;
	char    *str;

	if (! first_time) {
		first_time = 1;
		if (g_getenv ("GTHUMB_DEBUG")) 
			print_debug_info = 1;
	}

	if (! print_debug_info)
		return;

	g_return_if_fail (format != NULL);

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_fprintf (stderr, "[GTHUMB] %s:%d (%s):\n\t%s\n", file, line, function, str);

	g_free (str);
}


GList *
get_file_list_from_url_list (char *url_list)
{
	GList *list = NULL;
	int    i;
	char  *url_start, *url_end;

	url_start = url_list;
	while (url_start[0] != '\0') {
		char *url;

		if (strncmp (url_start, "file:", 5) == 0) {
			url_start += 5;
			if ((url_start[0] == '/')
			    && (url_start[1] == '/')) url_start += 2;
		}

		i = 0;
		while ((url_start[i] != '\0')
		       && (url_start[i] != '\r')
		       && (url_start[i] != '\n')) i++;
		url_end = url_start + i;

		url = g_strndup (url_start, url_end - url_start);
		list = g_list_prepend (list, url);

		url_start = url_end;
		i = 0;
		while ((url_start[i] != '\0')
		       && ((url_start[i] == '\r')
			   || (url_start[i] == '\n'))) i++;
		url_start += i;
	}

	return g_list_reverse (list);
}


int
strcmp_null_tolerant (const char *s1, const char *s2)
{
        if ((s1 == NULL) && (s2 == NULL))
                return 0;
        else if (s2 == NULL)
                return 1;
        else if (s1 == NULL)
                return -1;
        else
                return strcmp (s1, s2);
}


GList *
_g_list_insert_list_before (GList *list1,
			    GList *sibling,
			    GList *list2)
{
  if (!list2)
    {
      return list1;
    }
  else if (!list1)
    {
      return list2;
    }
  else if (sibling)
    {
      GList *list2_last = g_list_last (list2);
      if (sibling->prev)
	{
	  sibling->prev->next = list2;
	  list2->prev = sibling->prev;
	  sibling->prev = list2_last;
	  list2_last->next = sibling;
	  return list1;
	}
      else
	{
	  sibling->prev = list2_last;
	  list2_last->next = sibling;
	  return list2;
	}
    }
  else
    {
      return g_list_concat (list1, list2);
    }
}


gboolean
str_ends_with (const char *s1,
	       const char *s2)
{
	int s1_len, s2_len;

	if ((s1 == NULL) || (s2 == NULL))
		return FALSE;

	s1_len = strlen (s1);
	s2_len = strlen (s2);

	if (s2_len > s1_len)
		return FALSE;

	return strncmp (s1 + (s1_len - s2_len), s2, s2_len) == 0;
}


/**/


IdleCall*
idle_call_new (DoneFunc func,
	       gpointer data)
{
	IdleCall *call = g_new0(IdleCall, 1);
	call->func = func;
	call->data = data;
	return call;
}


void
idle_call_free (IdleCall *call)
{
	g_free (call);
}


static gboolean
idle_call_exec_cb (gpointer data)
{
	IdleCall *call = data;
	(*call->func) (call->data);
	return FALSE;
}


guint
idle_call_exec (IdleCall *call,
		gboolean  use_idle_cb)
{
	if (use_idle_cb)
		return g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
					idle_call_exec_cb,
					call,
					(GDestroyNotify) idle_call_free);
	else {
		(*call->func) (call->data);
		return 0;
	}
}


GHashTable *static_strings = NULL;


const char *
get_static_string (const char *s)
{
	const char *result;

	if (s == NULL)
		return NULL;

	if (static_strings == NULL)
		static_strings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	if (! g_hash_table_lookup_extended (static_strings, s, (gpointer*) &result, NULL)) {
		result = g_strdup (s);
		g_hash_table_insert (static_strings,
				     (gpointer) result,
				     GINT_TO_POINTER (1));
	}

	return result;
}
