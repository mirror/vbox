/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "server_dispatch.h"
#include "server.h"
#include "cr_error.h"
#include "cr_mem.h"

void SERVER_DISPATCH_APIENTRY crServerDispatchSelectBuffer( GLsizei size, GLuint *buffer )
{
    (void) size;
    (void) buffer;
    crError( "Unsupported network glSelectBuffer call." );
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetChromiumParametervCR(GLenum target, GLuint index, GLenum type, GLsizei count, GLvoid *values)
{
    GLubyte local_storage[4096];
    GLint bytes = 0;

    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
         bytes = count * sizeof(GLbyte);
         break;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
         bytes = count * sizeof(GLshort);
         break;
    case GL_INT:
    case GL_UNSIGNED_INT:
         bytes = count * sizeof(GLint);
         break;
    case GL_FLOAT:
         bytes = count * sizeof(GLfloat);
         break;
    case GL_DOUBLE:
         bytes = count * sizeof(GLdouble);
         break;
    default:
         crError("Bad type in crServerDispatchGetChromiumParametervCR");
    }

    CRASSERT(bytes >= 0);
    CRASSERT(bytes < 4096);

    cr_server.head_spu->dispatch_table.GetChromiumParametervCR( target, index, type, count, local_storage );

    crServerReturnValue( local_storage, bytes );
}

void SERVER_DISPATCH_APIENTRY crServerDispatchChromiumParametervCR(GLenum target, GLenum type, GLsizei count, const GLvoid *values)
{
    CRMuralInfo *mural = cr_server.curClient->currentMural;
    static int gather_connect_count = 0;

    switch (target) {
    case GL_SET_MAX_VIEWPORT_CR:
        {
            GLint *maxDims = (GLint *)values;
            cr_server.limits.maxViewportDims[0] = maxDims[0];
            cr_server.limits.maxViewportDims[1] = maxDims[1];
        }
        break;

    case GL_TILE_INFO_CR:
        /* message from tilesort SPU to set new tile bounds */
        {
            GLint numTiles, muralWidth, muralHeight, server, tiles;
            GLint *tileBounds;
            CRASSERT(count >= 4);
            CRASSERT((count - 4) % 4 == 0); /* must be multiple of four */
            CRASSERT(type == GL_INT);
            numTiles = (count - 4) / 4;
            tileBounds = (GLint *) values;
            server = tileBounds[0];
            muralWidth = tileBounds[1];
            muralHeight = tileBounds[2];
            tiles = tileBounds[3];
            CRASSERT(tiles == numTiles);
            tileBounds += 4; /* skip over header values */
            crServerNewMuralTiling(mural, muralWidth, muralHeight, numTiles, tileBounds);
            mural->viewportValidated = GL_FALSE;
        }
        break;

    case GL_GATHER_DRAWPIXELS_CR:
        if (cr_server.only_swap_once && cr_server.curClient != cr_server.clients[0])
            break;
        cr_server.head_spu->dispatch_table.ChromiumParametervCR( target, type, count, values );
        break;

    case GL_GATHER_CONNECT_CR:
        /* 
         * We want the last connect to go through,
         * otherwise we might deadlock in CheckWindowSize()
         * in the readback spu
         */
        gather_connect_count++;
        if (cr_server.only_swap_once && (gather_connect_count != cr_server.numClients)) 
        {
            break;
        }
        cr_server.head_spu->dispatch_table.ChromiumParametervCR( target, type, count, values );
        gather_connect_count = 0;
        break;

    case GL_SERVER_VIEW_MATRIX_CR:
        /* Set this server's view matrix which will get premultiplied onto the
         * modelview matrix.  For non-planar tilesort and stereo.
         */
        CRASSERT(count == 18);
        CRASSERT(type == GL_FLOAT);
        /* values[0] is the server index. Ignored here but used in tilesort SPU */
        /* values[1] is the left/right eye index (0 or 1) */
        {
            const GLfloat *v = (const GLfloat *) values;
            const int eye = v[1] == 0.0 ? 0 : 1;
            crMatrixInitFromFloats(&cr_server.viewMatrix[eye], v + 2);
            
            crDebug("Got GL_SERVER_VIEW_MATRIX_CR:\n" 
                            "  %f %f %f %f\n"
                            "  %f %f %f %f\n"
                            "  %f %f %f %f\n"
                            "  %f %f %f %f",
                            cr_server.viewMatrix[eye].m00,
                            cr_server.viewMatrix[eye].m10,
                            cr_server.viewMatrix[eye].m20,
                            cr_server.viewMatrix[eye].m30,
                            cr_server.viewMatrix[eye].m01,
                            cr_server.viewMatrix[eye].m11,
                            cr_server.viewMatrix[eye].m21,
                            cr_server.viewMatrix[eye].m31,
                            cr_server.viewMatrix[eye].m02,
                            cr_server.viewMatrix[eye].m12,
                            cr_server.viewMatrix[eye].m22,
                            cr_server.viewMatrix[eye].m32,
                            cr_server.viewMatrix[eye].m03,
                            cr_server.viewMatrix[eye].m13,
                            cr_server.viewMatrix[eye].m23,
                            cr_server.viewMatrix[eye].m33);
        }
        cr_server.viewOverride = GL_TRUE;
        break;

    case GL_SERVER_PROJECTION_MATRIX_CR:
        /* Set this server's projection matrix which will get replace the user's
         * projection matrix.  For non-planar tilesort and stereo.
         */
        CRASSERT(count == 18);
        CRASSERT(type == GL_FLOAT);
        /* values[0] is the server index. Ignored here but used in tilesort SPU */
        /* values[1] is the left/right eye index (0 or 1) */
        {
            const GLfloat *v = (const GLfloat *) values;
            const int eye = v[1] == 0.0 ? 0 : 1;
            crMatrixInitFromFloats(&cr_server.projectionMatrix[eye], v + 2);
      
            crDebug("Got GL_SERVER_PROJECTION_MATRIX_CR:\n" 
                            "  %f %f %f %f\n"
                            "  %f %f %f %f\n"
                            "  %f %f %f %f\n"
                            "  %f %f %f %f",
                            cr_server.projectionMatrix[eye].m00,
                            cr_server.projectionMatrix[eye].m10,
                            cr_server.projectionMatrix[eye].m20,
                            cr_server.projectionMatrix[eye].m30,
                            cr_server.projectionMatrix[eye].m01,
                            cr_server.projectionMatrix[eye].m11,
                            cr_server.projectionMatrix[eye].m21,
                            cr_server.projectionMatrix[eye].m31,
                            cr_server.projectionMatrix[eye].m02,
                            cr_server.projectionMatrix[eye].m12,
                            cr_server.projectionMatrix[eye].m22,
                            cr_server.projectionMatrix[eye].m32,
                            cr_server.projectionMatrix[eye].m03,
                            cr_server.projectionMatrix[eye].m13,
                            cr_server.projectionMatrix[eye].m23,
                            cr_server.projectionMatrix[eye].m33);

            if (cr_server.projectionMatrix[eye].m33 == 0.0f) {
                float x = cr_server.projectionMatrix[eye].m00;
                float y = cr_server.projectionMatrix[eye].m11;
                float a = cr_server.projectionMatrix[eye].m20;
                float b = cr_server.projectionMatrix[eye].m21;
                float c = cr_server.projectionMatrix[eye].m22;
                float d = cr_server.projectionMatrix[eye].m32;
                float znear = -d / (1.0f - c);
                float zfar = (c - 1.0f) * znear / (c + 1.0f);
                float left = znear * (a - 1.0f) / x;
                float right = 2.0f * znear / x + left;
                float bottom = znear * (b - 1.0f) / y;
              float top = 2.0f * znear / y + bottom;
              crDebug("Frustum: left, right, bottom, top, near, far: %f, %f, %f, %f, %f, %f", left, right, bottom, top, znear, zfar);   
            }
            else {
                /* Todo: Add debug output for orthographic projection*/
            }

        }
        cr_server.projectionOverride = GL_TRUE;
        break;

    default:
        /* Pass the parameter info to the head SPU */
        cr_server.head_spu->dispatch_table.ChromiumParametervCR( target, type, count, values );
        break;
    }
}


void SERVER_DISPATCH_APIENTRY crServerDispatchChromiumParameteriCR(GLenum target, GLint value)
{
  switch (target) {
    case GL_SHARED_DISPLAY_LISTS_CR:
        cr_server.sharedDisplayLists = value;
        break;
    case GL_SHARED_TEXTURE_OBJECTS_CR:
        cr_server.sharedTextureObjects = value;
        break;
    case GL_SHARED_PROGRAMS_CR:
        cr_server.sharedPrograms = value;
        break;
    case GL_SERVER_CURRENT_EYE_CR:
        cr_server.currentEye = value ? 1 : 0;
        break;
    default:
        /* Pass the parameter info to the head SPU */
        cr_server.head_spu->dispatch_table.ChromiumParameteriCR( target, value );
    }
}


void SERVER_DISPATCH_APIENTRY crServerDispatchChromiumParameterfCR(GLenum target, GLfloat value)
{
  switch (target) {
    case GL_SHARED_DISPLAY_LISTS_CR:
        cr_server.sharedDisplayLists = (int) value;
        break;
    case GL_SHARED_TEXTURE_OBJECTS_CR:
        cr_server.sharedTextureObjects = (int) value;
        break;
    case GL_SHARED_PROGRAMS_CR:
        cr_server.sharedPrograms = (int) value;
        break;
    default:
        /* Pass the parameter info to the head SPU */
        cr_server.head_spu->dispatch_table.ChromiumParameterfCR( target, value );
    }
}

void crServerCreateInfoDeleteCB(void *data)
{
    CRCreateInfo_t *pCreateInfo = (CRCreateInfo_t *) data;
    if (pCreateInfo->pszDpyName)
        crFree(pCreateInfo->pszDpyName);
}

GLint crServerGenerateID(GLint *pCounter)
{
    return (*pCounter)++;
}

