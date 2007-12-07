/** @file
 *
 * VBox OpenGL
 *
 * Simple buffered OpenGL functions
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "vboxgl.h"


/* OpenGL functions */
void vboxglFinish (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    /** @todo if no back buffer, then draw to screen */
    VBOX_OGL_GEN_SYNC_OP(Finish);
}

void vboxglFlush (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    /** @todo if no back buffer, then draw to screen */
    VBOX_OGL_GEN_SYNC_OP(Flush);
}

void vboxglGenLists (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP1_RET(GLuint, GenLists, GLsizei);
}

void vboxglIsEnabled (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    /** @todo cache? */
    VBOX_OGL_GEN_SYNC_OP1_RET(GLboolean, IsEnabled, GLenum);
}

void vboxglIsList (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP1_RET(GLboolean, IsList, GLuint);
}

void vboxglIsTexture (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP1_RET(GLboolean, IsTexture, GLuint);
}



