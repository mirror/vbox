/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef CR_STRING_H
#define CR_STRING_H

#include <iprt/cdefs.h>

RT_BEGIN_DECLS

DECLEXPORT(char *)  crStrdup( const char *str );
DECLEXPORT(char *)  crStrndup( const char *str, unsigned int len );
DECLEXPORT(int)     crStrlen( const char *str );
DECLEXPORT(int)     crStrcmp( const char *str1, const char *str2 );
DECLEXPORT(int)     crStrncmp( const char *str1, const char *str2, int n );
DECLEXPORT(int)     crStrcasecmp( const char *str1, const char *str2 );
DECLEXPORT(void)    crStrcpy( char *dst, const char *src );
DECLEXPORT(void)    crStrncpy( char *dst, const char *src, unsigned int len );
DECLEXPORT(void)    crStrcat( char *dst, const char *src );
DECLEXPORT(char *)  crStrjoin( const char *src1, const char *src2 );
DECLEXPORT(char *)  crStrjoin3( const char *src1, const char *src2, const char *src3 );
DECLEXPORT(char *)  crStrstr( const char *str, const char *pat );
DECLEXPORT(char *)  crStrchr( const char *str, char c );
DECLEXPORT(char *)  crStrrchr( const char *str, char c );
DECLEXPORT(int)     crStrToInt( const char *str );
DECLEXPORT(float)   crStrToFloat( const char *str );
DECLEXPORT(char **) crStrSplit( const char *str, const char *splitstr );
DECLEXPORT(char **) crStrSplitn( const char *str, const char *splitstr, int n );
DECLEXPORT(void)    crFreeStrings( char **strings );
DECLEXPORT(char *)  crStrIntersect( const char *s1, const char *s2 );
DECLEXPORT(int)     crIsDigit( char c );

DECLEXPORT(void)    crBytesToString( char *string, int nstring, void *data, int ndata );
DECLEXPORT(void)    crWordsToString( char *string, int nstring, void *data, int ndata );

RT_END_DECLS

#endif /* CR_STRING_H */
