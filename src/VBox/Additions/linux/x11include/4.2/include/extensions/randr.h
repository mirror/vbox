/*
 * $XFree86: xc/include/extensions/randr.h,v 1.4 2001/11/24 07:24:58 keithp Exp $
 *
 * Copyright © 2000 Compaq Computer Corporation, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Compaq not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Compaq makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * COMPAQ DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL COMPAQ
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Jim Gettys, Compaq Computer Corporation, Inc.
 */

#ifndef _RANDR_H_
#define _RANDR_H_

typedef unsigned short	Rotation;
typedef unsigned short	VisualGroupID;
typedef unsigned short	GroupOfVisualGroupID;
typedef unsigned short	SizeID;

#define RANDR_NAME		"RANDR"
#define RANDR_MAJOR		0
#define RANDR_MINOR		0

#define RRNumberErrors		0
#define RRNumberEvents		1

#define X_RRQueryVersion	0
#define X_RRGetScreenInfo	1
#define X_RRSetScreenConfig	2
#define X_RRScreenChangeSelectInput	3

#define RRScreenChangeNotify	0

#define RR_Rotate_0		1
#define RR_Rotate_90		2
#define RR_Rotate_180		4
#define RR_Rotate_270		8

#define RRSetConfigSuccess		0
#define RRSetConfigInvalidConfigTime	1
#define RRSetConfigInvalidTime		2
#define RRSetConfigFailed		3

#endif	/* _RANDR_H_ */
