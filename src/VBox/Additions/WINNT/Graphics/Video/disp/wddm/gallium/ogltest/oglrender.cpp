/* $Id$ */
/** @file
 * OpenGL testcase. Simple OpenGL tests.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "oglrender.h"


/*
 * Old style glBegin/glEnd colored triangle
 */
class OGLRenderTriangle : public OGLRender
{
    virtual HRESULT InitRender();
    virtual HRESULT DoRender();
};

HRESULT OGLRenderTriangle::InitRender()
{
    return S_OK;
}

HRESULT OGLRenderTriangle::DoRender()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLES);
    glColor3f ( 1.0f,  0.0f, 0.0f);
    glVertex2f(-1.0f, -1.0f);
    glColor3f ( 0.0f,  1.0f, 0.0f);
    glVertex2f( 0.0f,  1.0f);
    glColor3f ( 0.0f,  0.0f, 1.0f);
    glVertex2f( 1.0f, -1.0f);
    glEnd();
    glFlush();
    return S_OK;
}


OGLRender *CreateRender(int iRenderId)
{
    OGLRender *pRender = NULL;
    switch (iRenderId)
    {
        case 0: pRender = new OGLRenderTriangle(); break;
        default:
            break;
    }
    return pRender;
}

void DeleteRender(OGLRender *pRender)
{
    if (pRender)
    {
        delete pRender;
    }
}
