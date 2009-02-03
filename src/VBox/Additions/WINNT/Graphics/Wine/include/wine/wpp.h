/*
 * Exported functions of the Wine preprocessor
 *
 * Copyright 2002 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * Sun LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Sun elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */

#ifndef __WINE_WPP_H
#define __WINE_WPP_H

#include <stdio.h>

extern void wpp_add_define( const char *name, const char *value );
extern void wpp_del_define( const char *name );
extern void wpp_add_cmdline_define( const char *value );
extern void wpp_set_debug( int lex_debug, int parser_debug, int msg_debug );
extern void wpp_set_pedantic( int on );
extern void wpp_add_include_path( const char *path );
extern char *wpp_find_include( const char *name, const char *parent_name );
extern int wpp_parse( const char *input, FILE *output );
extern int wpp_parse_temp( const char *input, const char *output_base, char **output_name );

#endif  /* __WINE_WPP_H */
