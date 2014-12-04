%{
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "albumtheme-private.h"

void  yyerror (char *fmt, ...);
int   yywrap  (void);
int   yylex   (void);

#define YY_NO_UNPUT

%}

%union {
	char         *text;
	int           ivalue;
        GString      *string;
	GthVar       *var;
	GthTag       *tag;
	GthExpr      *expr;
	GList        *list;
	GthCondition *cond;
}

%nonassoc       IF ELSE ELSE_IF END
%token          END_TAG
%token <text>   NAME STRING
%token <ivalue> NUMBER
%token <ivalue> HEADER FOOTER 
%token <ivalue> LANGUAGE 
%token <ivalue> IMAGE 
%token <ivalue> IMAGE_LINK 
%token <ivalue> IMAGE_IDX 
%token <ivalue> IMAGE_DIM 
%token <ivalue> IMAGES 
%token <ivalue> FILENAME
%token <ivalue> FILEPATH
%token <ivalue> FILESIZE
%token <ivalue> COMMENT 
%token <ivalue> PLACE
%token <ivalue> DATE_TIME 
%token <ivalue> PAGE_LINK 
%token <ivalue> PAGE_IDX 
%token <ivalue> PAGES 
%token <ivalue> TABLE 
%token <ivalue> THUMBS
%token <ivalue> DATE 
%token <ivalue> TEXT TEXT_END
%token <ivalue> EXIF_EXPOSURE_TIME
%token <ivalue> EXIF_EXPOSURE_MODE
%token <ivalue> EXIF_FLASH
%token <ivalue> EXIF_SHUTTER_SPEED
%token <ivalue> EXIF_APERTURE_VALUE
%token <ivalue> EXIF_FOCAL_LENGTH
%token <ivalue> EXIF_DATE_TIME
%token <ivalue> EXIF_CAMERA_MODEL

%token <ivalue> SET_VAR 
%token <ivalue> EVAL

%token <text> HTML

%type <list>   document
%type <tag>    gthumb_tag
%type <tag>    gthumb_text_tag
%type <cond>   gthumb_if
%type <cond>   gthumb_else_if
%type <cond>   opt_else
%type <list>   opt_if_list
%type <ivalue> tag_name
%type <list>   arg_list
%type <var>    arg
%type <expr>   expr
%type <expr>   quoted_expr
%type <string> constant_list
%type <string> constant
%type <string> constant1

%left  <ivalue> BOOL_OP
%left  <ivalue> COMPARE
%left  '+' '-' '*' '/' '!' 
%right UNARY_OP

%%

all		: document { 
			yy_parsed_doc = $1; 

			if (yy_parsed_doc == NULL)
				YYABORT;
			else
				YYACCEPT;
		}
		;

document	: HTML document { 
			$$ = g_list_prepend ($2, gth_tag_new_html ($1));
			g_free ($1);
		}

		| gthumb_tag document { 
			$$ = g_list_prepend ($2, $1); 
		}

		| gthumb_if document opt_if_list opt_else gthumb_end document {
			GList  *cond_list;
			GthTag *tag;

			gth_condition_add_document ($1, $2);
			cond_list = g_list_prepend ($3, $1);
			if ($4 != NULL)
				cond_list = g_list_append (cond_list, $4);
			
			tag = gth_tag_new_condition (cond_list);
			$$ = g_list_prepend ($6, tag);
		}

		| gthumb_text_tag HTML TEXT_END document {
			GthTag *tag = gth_tag_new_html ($2);
			GList *child_doc = g_list_append (NULL, tag);
			gth_tag_add_document ($1, child_doc);
			$$ = g_list_prepend ($4, $1);
			g_free ($2);
		}

		| /* empty */ {
			$$ = NULL;
		}

		| error document { 
			if ($2 != NULL)
				gth_parsed_doc_free ($2);
			$$ = NULL;
		}
		;

gthumb_if	: IF expr END_TAG {
			$$ = gth_condition_new ($2);
		}
		| IF '"' quoted_expr '"' END_TAG {
			$$ = gth_condition_new ($3);
		}
		;

opt_if_list     : gthumb_else_if document opt_if_list {
			gth_condition_add_document ($1, $2);
			$$ = g_list_prepend ($3, $1);
		}
		
		| /* empty */ {
			$$ = NULL;
		}
		;

gthumb_else_if  : ELSE_IF expr END_TAG {
			$$ = gth_condition_new ($2);
		}
		| ELSE_IF '"' quoted_expr '"' END_TAG {
			$$ = gth_condition_new ($3);
		}
		;

opt_else        : gthumb_else document {
			GthExpr      *else_expr;
			GthCondition *cond;

			else_expr = gth_expr_new ();
			gth_expr_push_constant (else_expr, 1);
			cond = gth_condition_new (else_expr);
			gth_condition_add_document (cond, $2);

			$$ = cond;
		}

		| /* empty */ {
			$$ = NULL;
		}
		;

gthumb_else	: ELSE END_TAG
		;

gthumb_end	: END END_TAG
		;

expr		: '(' expr ')' {
			$$ = $2;
		}

		| expr COMPARE expr {
			GthExpr *e = gth_expr_new ();

			gth_expr_push_expr (e, $1);
			gth_expr_push_expr (e, $3);
			gth_expr_push_op (e, $2);

			gth_expr_unref ($1);
			gth_expr_unref ($3);

			$$ = e;
		}

		| expr '+' expr {
			GthExpr *e = gth_expr_new ();

			gth_expr_push_expr (e, $1);
			gth_expr_push_expr (e, $3);
			gth_expr_push_op (e, GTH_OP_ADD);

			gth_expr_unref ($1);
			gth_expr_unref ($3);

			$$ = e;
		}

		| expr '-' expr {
			GthExpr *e = gth_expr_new ();

			gth_expr_push_expr (e, $1);
			gth_expr_push_expr (e, $3);
			gth_expr_push_op (e, GTH_OP_SUB);

			gth_expr_unref ($1);
			gth_expr_unref ($3);

			$$ = e;
		}

		| expr '*' expr {
			GthExpr *e = gth_expr_new ();

			gth_expr_push_expr (e, $1);
			gth_expr_push_expr (e, $3);
			gth_expr_push_op (e, GTH_OP_MUL);

			gth_expr_unref ($1);
			gth_expr_unref ($3);

			$$ = e;
		}

		| expr '/' expr {
			GthExpr *e = gth_expr_new ();

			gth_expr_push_expr (e, $1);
			gth_expr_push_expr (e, $3);
			gth_expr_push_op (e, GTH_OP_DIV);

			gth_expr_unref ($1);
			gth_expr_unref ($3);

			$$ = e;
		}

		| expr BOOL_OP expr {
			GthExpr *e = gth_expr_new ();

			gth_expr_push_expr (e, $1);
			gth_expr_push_expr (e, $3);
			gth_expr_push_op (e, $2);

			gth_expr_unref ($1);
			gth_expr_unref ($3);

			$$ = e;
		}

		| '+' expr %prec UNARY_OP {
			$$ = $2;
		}

		| '-' expr %prec UNARY_OP {
			gth_expr_push_op ($2, GTH_OP_NEG);
			$$ = $2;
		}

		| '!' expr %prec UNARY_OP {
			gth_expr_push_op ($2, GTH_OP_NOT);
			$$ = $2;
		}

		| NAME {
			GthExpr *e = gth_expr_new ();
			gth_expr_push_var (e, $1);
			g_free ($1);
			$$ = e;
		}

		| NUMBER {
			GthExpr *e = gth_expr_new ();
			gth_expr_push_constant (e, $1);
			$$ = e;
		}
		;
quoted_expr     : expr {
			$$ = $1;
		}
		| constant1 constant constant_list {
			GthExpr *e = gth_expr_new ();
			g_string_append($1, $2->str);
			g_string_free($2, TRUE);
			if ($3 != NULL)
			{
				g_string_append($1, $3->str);
				g_string_free($3, TRUE);
			}
			gth_expr_push_var(e, $1->str);
			g_string_free($1, TRUE);
			$$ = e;
		}
		;
constant1       : NAME {
			GString* s = g_string_new($1);
			g_free($1);
			$$ = s;
		}
		;
constant        : NAME {
			GString* s = g_string_new($1);
			g_string_prepend_c(s, ' ');
			g_free($1);
			$$ = s;
		}
		| NUMBER {
			GString* s = g_string_new("");
			g_string_sprintf(s, " %i", $1);
			$$ = s;
		}
		;
constant_list   : constant constant_list {
			if ($2 != NULL)
			{
				g_string_append($1, $2->str);
				g_string_free($2, TRUE);
			}
			$$ = $1;
		}
		| /* empty */ {
			$$ = NULL;
		}
		;
gthumb_text_tag	: TEXT arg_list END_TAG {
			$$ = gth_tag_new ($1, $2);
		}
		;

gthumb_tag 	: tag_name arg_list END_TAG {
			$$ = gth_tag_new ($1, $2);
		}
		;

tag_name	: HEADER              { $$ = $1; }
		| FOOTER              { $$ = $1; }
		| LANGUAGE            { $$ = $1; }
		| IMAGE               { $$ = $1; }
		| IMAGE_LINK          { $$ = $1; }
		| IMAGE_IDX           { $$ = $1; }
		| IMAGE_DIM           { $$ = $1; }
		| IMAGES              { $$ = $1; }
		| FILENAME            { $$ = $1; }
		| FILEPATH            { $$ = $1; }
		| FILESIZE            { $$ = $1; }
		| COMMENT             { $$ = $1; }
		| PLACE               { $$ = $1; }
		| DATE_TIME           { $$ = $1; }
		| PAGE_LINK           { $$ = $1; }
		| PAGE_IDX            { $$ = $1; }
		| PAGES               { $$ = $1; }
		| TABLE               { $$ = $1; }
		| THUMBS              { $$ = $1; }
		| DATE                { $$ = $1; }
		| EXIF_EXPOSURE_TIME  { $$ = $1; }
		| EXIF_EXPOSURE_MODE  { $$ = $1; }
		| EXIF_FLASH          { $$ = $1; }
		| EXIF_SHUTTER_SPEED  { $$ = $1; }
		| EXIF_APERTURE_VALUE { $$ = $1; }
		| EXIF_FOCAL_LENGTH   { $$ = $1; }
		| EXIF_DATE_TIME      { $$ = $1; }
		| EXIF_CAMERA_MODEL   { $$ = $1; }
		| SET_VAR             { $$ = $1; }
		| EVAL                { $$ = $1; }
		;

arg_list	: arg arg_list {
			$$ = g_list_prepend ($2, $1);
		}

		| /* empty */ {
			$$ = NULL;
		}
		;

arg		: NAME '=' expr { 
			$$ = gth_var_new_expression ($1, $3);
			g_free ($1);
		}

		| NAME '=' '"' quoted_expr '"' { 
			$$ = gth_var_new_expression ($1, $4);
			g_free($1);
		}

		| NAME {
		  	GthExpr *e = gth_expr_new ();
			gth_expr_push_constant (e, 1);
			$$ = gth_var_new_expression ($1, e);
			g_free ($1);
		}
		;

                

%%

int
yywrap (void)
{
	return 1;
}


void yyerror (char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	va_end (ap);
}


#include "lex.albumtheme.c"
