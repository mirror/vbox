/** @file
 * VBox Packing VisibleRegion information
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "packer.h"
#include "cr_opcodes.h"
#include "cr_error.h"

#ifdef WINDOWS
#include <windows.h>
#endif

void PACK_APIENTRY crPackWindowVisibleRegion( GLint window, GLint cRects, GLint * pRects )
{
    GLint i, size, cnt;

    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    size = 16 + cRects * 4 * sizeof(GLint);
    GET_BUFFERED_POINTER( pc, size );
    WRITE_DATA( 0, GLint, size );
    WRITE_DATA( 4, GLenum, CR_WINDOWVISIBLEREGION_EXTEND_OPCODE );
    WRITE_DATA( 8, GLint, window );
    WRITE_DATA( 12, GLint, cRects );

    cnt = 16;
    for (i=0; i<cRects; ++i)
    {
        WRITE_DATA(cnt, GLint, (GLint) pRects[4*i+0]);
        WRITE_DATA(cnt+4, GLint, (GLint) pRects[4*i+1]);
        WRITE_DATA(cnt+8, GLint, (GLint) pRects[4*i+2]);
        WRITE_DATA(cnt+12, GLint, (GLint) pRects[4*i+3]);
        cnt += 16;
    }
    WRITE_OPCODE( pc, CR_EXTEND_OPCODE );
}

void PACK_APIENTRY crPackWindowVisibleRegionSWAP( GLint window, GLint cRects, GLint * pRects )
{
    crError( "crPackWindowVisibleRegionSWAP unimplemented and shouldn't be called" );
}
