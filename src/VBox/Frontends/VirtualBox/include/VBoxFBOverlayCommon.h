/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxFrameBuffer Overly classes declarations
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#ifndef __VBoxFBOverlayCommon_h__
#define __VBoxFBOverlayCommon_h__

#if defined(DEBUG) && !defined(DEBUG_sandervl)
# include "iprt/stream.h"
# define VBOXQGLLOG(_m) RTPrintf _m
# define VBOXQGLLOGREL(_m) do { RTPrintf _m ; LogRel( _m ); } while(0)
#else
# define VBOXQGLLOG(_m)    do {}while(0)
# define VBOXQGLLOGREL(_m) LogRel( _m )
#endif
#define VBOXQGLLOG_ENTER(_m)
//do{VBOXQGLLOG(("==>[%s]:", __FUNCTION__)); VBOXQGLLOG(_m);}while(0)
#define VBOXQGLLOG_EXIT(_m)
//do{VBOXQGLLOG(("<==[%s]:", __FUNCTION__)); VBOXQGLLOG(_m);}while(0)
#ifdef DEBUG
 #define VBOXQGL_ASSERTNOERR() \
    do { GLenum err = glGetError(); \
        if(err != GL_NO_ERROR) VBOXQGLLOG(("gl error ocured (0x%x)\n", err)); \
        Assert(err == GL_NO_ERROR); \
    }while(0)

 #define VBOXQGL_CHECKERR(_op) \
    do { \
        glGetError(); \
        _op \
        VBOXQGL_ASSERTNOERR(); \
    }while(0)
#else
 #define VBOXQGL_ASSERTNOERR() \
    do {}while(0)

 #define VBOXQGL_CHECKERR(_op) \
    do { \
        _op \
    }while(0)
#endif

#ifdef DEBUG
#include <iprt/time.h>

#define VBOXGETTIME() RTTimeNanoTS()

#define VBOXPRINTDIF(_nano, _m) do{\
        uint64_t cur = VBOXGETTIME(); \
        VBOXQGLLOG(_m); \
        VBOXQGLLOG(("(%Lu)\n", cur - (_nano))); \
    }while(0)

class VBoxVHWADbgTimeCounter
{
public:
    VBoxVHWADbgTimeCounter(const char* msg) {mTime = VBOXGETTIME(); mMsg=msg;}
    ~VBoxVHWADbgTimeCounter() {VBOXPRINTDIF(mTime, (mMsg));}
private:
    uint64_t mTime;
    const char* mMsg;
};

#define VBOXQGLLOG_METHODTIME(_m) VBoxVHWADbgTimeCounter _dbgTimeCounter(_m)

#define VBOXQG_CHECKCONTEXT() \
        { \
            const GLubyte * str; \
            VBOXQGL_CHECKERR(   \
                    str = glGetString(GL_VERSION); \
            ); \
            Assert(str); \
            if(str) \
            { \
                Assert(str[0]); \
            } \
        }
#else
#define VBOXQGLLOG_METHODTIME(_m)
#define VBOXQG_CHECKCONTEXT() do{}while(0)
#endif

#define VBOXQGLLOG_QRECT(_p, _pr, _s) do{\
    VBOXQGLLOG((_p " x(%d), y(%d), w(%d), h(%d)" _s, (_pr)->x(), (_pr)->y(), (_pr)->width(), (_pr)->height()));\
    }while(0)

#define VBOXQGLLOG_CKEY(_p, _pck, _s) do{\
    VBOXQGLLOG((_p " l(0x%x), u(0x%x)" _s, (_pck)->lower(), (_pck)->upper()));\
    }while(0)

#endif /* #ifndef __VBoxFBOverlayCommon_h__ */
