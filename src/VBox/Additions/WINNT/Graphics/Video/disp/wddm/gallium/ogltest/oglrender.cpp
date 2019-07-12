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


/*
 * Texture2D.
 */
class OGLRenderTexture2D : public OGLRender
{
    virtual HRESULT InitRender();
    virtual HRESULT DoRender();
    GLuint texName;
    static const int texWidth = 8;
    static const int texHeight = 8;
};

HRESULT OGLRenderTexture2D::InitRender()
{
    static GLubyte texImage[texHeight][texWidth][4];
    for (int y = 0; y < texHeight; ++y)
    {
       for (int x = 0; x < texWidth; ++x)
       {
          GLubyte v = 255;
          if (   (texHeight/4 <= y && y < 3*texHeight/4)
              && (texWidth/4 <= x && x < 3*texWidth/4))
          {
              if (y < x)
              {
                  v = 0;
              }
          }

          texImage[y][x][0] = v;
          texImage[y][x][1] = 0;
          texImage[y][x][2] = 0;
          texImage[y][x][3] = 255;
       }
    }

    glClearColor(0.0, 0.0, 1.0, 1.0);

    glGenTextures(1, &texName);
    glBindTexture(GL_TEXTURE_2D, texName);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texImage);

    glBindTexture(GL_TEXTURE_2D, 0);

    return S_OK;
}

HRESULT OGLRenderTexture2D::DoRender()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, texName);

    glBegin(GL_TRIANGLES);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, 0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, 0.0f);

    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.0f, -1.0f, 0.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);

    glDisable(GL_TEXTURE_2D);

    glFlush();

    return S_OK;
}


OGLRender *CreateRender(int iRenderId)
{
    OGLRender *pRender = NULL;
    switch (iRenderId)
    {
        case 0: pRender = new OGLRenderTriangle(); break;
        case 1: pRender = new OGLRenderTexture2D(); break;
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
