/*
 * $XFree86: xc/include/extensions/randrproto.h,v 1.5 2001/08/01 00:44:35 tsi Exp $
 *
 * Copyright © 2000 Compaq Computer Corporation
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

#ifndef _XRANDRP_H_
#define _XRANDRP_H_

#include <X11/extensions/randr.h>

#define Window CARD32
#define Drawable CARD32
#define Font CARD32
#define Pixmap CARD32
#define Cursor CARD32
#define Colormap CARD32
#define GContext CARD32
#define Atom CARD32
#define VisualID CARD32
#define Time CARD32
#define KeyCode CARD8
#define KeySym CARD32

#define Rotation CARD16
#define VisualGroupID CARD16
#define GroupOfVisualGroupID CARD16
#define SizeID CARD16

/*
 * data structures
 */

typedef struct {
    CARD16 widthInPixels B16;
    CARD16 heightInPixels B16;
    CARD16 widthInMillimeters B16;
    CARD16 heightInMillimeters B16;
    GroupOfVisualGroupID visualGroup B16;
    CARD16 pad1 B16;
} xScreenSizes;
#define sz_xScreenSizes 12



/* 
 * requests and replies
 */

typedef struct {
    CARD8   reqType;
    CARD8   randrReqType;
    CARD16  length B16;
    CARD32  majorVersion B32;
    CARD32  minorVersion B32;
} xRRQueryVersionReq;
#define sz_xRRQueryVersionReq   12

typedef struct {
    BYTE    type;   /* X_Reply */
    BYTE    pad1;
    CARD16  sequenceNumber B16;
    CARD32  length B32;
    CARD32  majorVersion B32;
    CARD32  minorVersion B32;
    CARD32  pad2 B32;
    CARD32  pad3 B32;
    CARD32  pad4 B32;
    CARD32  pad5 B32;
} xRRQueryVersionReply;
#define sz_xRRQueryVersionReply	32

typedef struct {
    CARD8   reqType;
    CARD8   randrReqType;
    CARD16  length B16;
    Window  window B32;
} xRRGetScreenInfoReq;
#define sz_xRRGetScreenInfoReq   8

typedef struct {
    BYTE    type;   /* X_Reply */
    BYTE    setOfRotations;
    CARD16  sequenceNumber B16;
    CARD32  length B32;
    Window  root B32;
    Time    timestamp B32;
    Time    configTimestamp B32;
    CARD16  nVisualGroups B16;
    CARD16  nGroupsOfVisualGroups B16;
    CARD16  nSizes B16;
    SizeID  sizeID B16;
    VisualGroupID  visualGroupID B16;
    Rotation  rotation B16;
} xRRGetScreenInfoReply;
#define sz_xRRGetScreenInfoReply	32

typedef struct {
    CARD8    reqType;
    CARD8    randrReqType;
    CARD16   length B16;
    Drawable drawable B32;
    Time     timestamp B32;
    Time     configTimestamp B32;
    SizeID   sizeID B16;
    Rotation rotation B16;
    VisualGroupID visualGroupID B16;
    CARD16   pad B16;
} xRRSetScreenConfigReq;
#define sz_xRRSetScreenConfigReq   24

typedef struct {
    BYTE    type;   /* X_Reply */
    CARD8   status;
    CARD16  sequenceNumber B16;
    CARD32  length B32;
    Time    newTimestamp B32;  
    Time    newConfigTimestamp B32;
    Window  root;
    CARD32  pad4 B32;
    CARD32  pad5 B32;
    CARD32  pad6 B32;
} xRRSetScreenConfigReply;
#define sz_xRRSetScreenConfigReply 32

typedef struct {
    CARD8   reqType;
    CARD8   randrReqType;
    CARD16  length B16;
    Window  window B32;
    BYTE    enable;	    /* xTrue -> send events */
    BYTE    pad1;
    CARD16  pad2 B16;
} xRRScreenChangeSelectInputReq;
#define sz_xRRScreenChangeSelectInputReq   12

/*
 * event
 */
typedef struct {
    CARD8 type;				/* always evBase + ScreenChangeNotify */
    CARD8 rotation;			/* new rotation */
    CARD16 sequenceNumber B16;
    Time timestamp B32;			/* time screen was changed */
    Time configTimestamp B32;		/* time config data was changed */
    Window root B32;			/* root window */
    Window window B32;			/* window requesting notification */
    SizeID sizeID B16;			/* new size ID */
    VisualGroupID visualGroupID B16;	/* new visual group ID */
    CARD16 widthInPixels B16;		/* new size */
    CARD16 heightInPixels B16;
    CARD16 widthInMillimeters B16;
    CARD16 heightInMillimeters B16;
} xRRScreenChangeNotifyEvent;
#define sz_xRRScreenChangeNotifyEvent	32

#undef Window
#undef Drawable
#undef Font
#undef Pixmap
#undef Cursor
#undef Colormap
#undef GContext
#undef Atom
#undef VisualID
#undef Time
#undef KeyCode
#undef KeySym
#undef Rotation
#undef VisualGroupID
#undef GroupOfVisualGroupID
#undef SizeID

#endif /* _XRANDRP_H_ */
