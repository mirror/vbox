/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "server_dispatch.h"
#include "server.h"
#include "cr_error.h"
#include "cr_mem.h"
#include "cr_string.h"
#include "cr_pixeldata.h"

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
            /*crServerNewMuralTiling(mural, muralWidth, muralHeight, numTiles, tileBounds);
            mural->viewportValidated = GL_FALSE;*/
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
    case GL_SHARE_CONTEXT_RESOURCES_CR:
        crStateShareContext(value);
        break;
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
    crFree(pCreateInfo);
}

GLint crServerGenerateID(GLint *pCounter)
{
    return (*pCounter)++;
}

/*#define CR_DUMP_BLITS*/

#ifdef CR_DUMP_BLITS
static int blitnum=0;
static int copynum=0;
#endif

void SERVER_DISPATCH_APIENTRY 
crServerDispatchCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    /*@todo pbo/fbo disabled for now as it's slower, check on other gpus*/
    static int siHavePBO = 0;
    static int siHaveFBO = 0;

    if ((target!=GL_TEXTURE_2D) || (height>=0))
    {
        cr_server.head_spu->dispatch_table.CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);

#ifdef CR_DUMP_BLITS
        {
            SPUDispatchTable *gl = &cr_server.head_spu->dispatch_table;
            void *img;
            GLint w, h;
            char fname[200];

            copynum++;

            gl->GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
            gl->GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

            img = crAlloc(w*h*4);
            CRASSERT(img);

            gl->GetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, img);
            sprintf(fname, "copy_blit%i_copy_%i.tga", blitnum, copynum);
            crDumpNamedTGA(fname, w, h, img);
            crFree(img);
        }
#endif
    }
    else /* negative height, means we have to Yinvert the source pixels while copying */
    {
        SPUDispatchTable *gl = &cr_server.head_spu->dispatch_table;

        if (siHavePBO<0)
        {
            const char *ext = (const char*)gl->GetString(GL_EXTENSIONS);
            siHavePBO = crStrstr(ext, "GL_ARB_pixel_buffer_object") ? 1:0;
        }

        if (siHaveFBO<0)
        {
            const char *ext = (const char*)gl->GetString(GL_EXTENSIONS);
            siHaveFBO = crStrstr(ext, "GL_EXT_framebuffer_object") ? 1:0;
        }

        if (siHavePBO==0 && siHaveFBO==0)
        {
#if 1
            GLint dRow, sRow;
            for (dRow=yoffset, sRow=y-height-1; dRow<yoffset-height; dRow++, sRow--)
            {
                gl->CopyTexSubImage2D(target, level, xoffset, dRow, x, sRow, width, 1);
            }
#else
            {
                GLint w, h, i;
                char *img1, *img2, *sPtr, *dPtr;
                CRContext *ctx = crStateGetCurrent();

                w = ctx->texture.unit[ctx->texture.curTextureUnit].currentTexture2D->level[0][level].width;
                h = ctx->texture.unit[ctx->texture.curTextureUnit].currentTexture2D->level[0][level].height;

                img1 = crAlloc(4*w*h);
                img2 = crAlloc(4*width*(-height));
                CRASSERT(img1 && img2);

                gl->CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, -height);
                gl->GetTexImage(target, level, GL_RGBA, GL_UNSIGNED_BYTE, img1);

                sPtr=img1+4*xoffset+4*w*yoffset;
                dPtr=img2+4*width*(-height-1);

                for (i=0; i<-height; ++i)
                {
                    crMemcpy(dPtr, sPtr, 4*width);
                    sPtr += 4*w;
                    dPtr -= 4*width;
                }

                gl->TexSubImage2D(target, level, xoffset, yoffset, width, -height, GL_RGBA, GL_UNSIGNED_BYTE, img2);

                crFree(img1);
                crFree(img2);
            }
#endif
        }
        else if (siHaveFBO==1) /*@todo more states to set and restore here*/
        {
            GLuint tID, fboID;
            GLenum status;
            CRContext *ctx = crStateGetCurrent();

            gl->GenTextures(1, &tID);
            gl->BindTexture(target, tID);
            gl->CopyTexImage2D(target, level, GL_RGBA, x, y, width, -height, 0);
            gl->GenFramebuffersEXT(1, &fboID);
            gl->BindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboID);
            gl->FramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, target, 
                                        ctx->texture.unit[ctx->texture.curTextureUnit].currentTexture2D->hwid, level);
            status = gl->CheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
            if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
            {
                crWarning("Framebuffer status 0x%x", status);
            }

            gl->Enable(target);
            gl->PushAttrib(GL_VIEWPORT_BIT);
            gl->Viewport(xoffset, yoffset, width, -height);
            gl->MatrixMode(GL_PROJECTION);
            gl->PushMatrix();
            gl->LoadIdentity();
            gl->MatrixMode(GL_MODELVIEW);
	        gl->PushMatrix();
            gl->LoadIdentity();

            gl->Disable(GL_DEPTH_TEST);
            gl->Disable(GL_CULL_FACE);
            gl->Disable(GL_STENCIL_TEST);
            gl->Disable(GL_SCISSOR_TEST);

            gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            gl->TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

            gl->Begin(GL_QUADS);
                gl->TexCoord2f(0.0f, 1.0f);
                gl->Vertex2f(-1.0, -1.0);

                gl->TexCoord2f(0.0f, 0.0f);
                gl->Vertex2f(-1.0f, 1.0f);

                gl->TexCoord2f(1.0f, 0.0f);
                gl->Vertex2f(1.0f, 1.0f);

                gl->TexCoord2f(1.0f, 1.0f);
                gl->Vertex2f(1.0f, -1.0f);
            gl->End();

            gl->PopMatrix();
            gl->MatrixMode(GL_PROJECTION);
            gl->PopMatrix();
            gl->PopAttrib();

            gl->FramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, target, 0, level);
            gl->BindFramebufferEXT(GL_FRAMEBUFFER_EXT, ctx->framebufferobject.drawFB ? ctx->framebufferobject.drawFB->hwid:0);
            gl->BindTexture(target, ctx->texture.unit[ctx->texture.curTextureUnit].currentTexture2D->hwid);
            gl->DeleteFramebuffersEXT(1, &fboID);
            gl->DeleteTextures(1, &tID);

#if 0
            {
                GLint dRow, sRow, w, h;
                void *img1, *img2;

                w = ctx->texture.unit[ctx->texture.curTextureUnit].currentTexture2D->level[0][level].width;
                h = ctx->texture.unit[ctx->texture.curTextureUnit].currentTexture2D->level[0][level].height;

                img1 = crAlloc(4*w*h);
                img2 = crAlloc(4*w*h);
                CRASSERT(img1 && img2);

                gl->GetTexImage(target, level, GL_BGRA, GL_UNSIGNED_BYTE, img1);

            
                for (dRow=yoffset, sRow=y-height-1; dRow<yoffset-height; dRow++, sRow--)
                {
                    gl->CopyTexSubImage2D(target, level, xoffset, dRow, x, sRow, width, 1);
                }

                gl->GetTexImage(target, level, GL_BGRA, GL_UNSIGNED_BYTE, img2);

                if (crMemcmp(img1, img2, 4*w*h))
                {
                    crDebug("MISMATCH! (%x, %i, ->%i,%i  <-%i, %i  [%ix%i])", target, level, xoffset, yoffset, x, y, width, height);
                    crDumpTGA(w, h, img1);
                    crDumpTGA(w, h, img2);
                    DebugBreak();
                }
                crFree(img1);
                crFree(img2);
            }
#endif
        }
        else
        {
            GLuint pboId, dRow, sRow;
            CRContext *ctx = crStateGetCurrent();

            gl->GenBuffersARB(1, &pboId);
            gl->BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboId);
            gl->BufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, -width*height*4, 0, GL_STATIC_COPY_ARB);

#if 1
            gl->ReadPixels(x, y, width, -height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
            gl->BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, ctx->bufferobject.packBuffer->hwid);

            gl->BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboId);
            for (dRow=yoffset, sRow=-height-1; dRow<yoffset-height; dRow++, sRow--)
            {
                gl->TexSubImage2D(target, level, xoffset, dRow, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void*)((uintptr_t)sRow*width*4));
            }
#else /*few times slower again*/
            for (dRow=0, sRow=y-height-1; dRow<-height; dRow++, sRow--)
            {
                gl->ReadPixels(x, sRow, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void*)((uintptr_t)dRow*width*4));
            }
            gl->BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, ctx->bufferobject.packBuffer->hwid);

            gl->BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboId);
            gl->TexSubImage2D(target, level, xoffset, yoffset, width, -height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#endif

            gl->BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->bufferobject.unpackBuffer->hwid);
            gl->DeleteBuffersARB(1, &pboId);
        }
    }
}

void SERVER_DISPATCH_APIENTRY 
crServerDispatchBlitFramebufferEXT(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                   GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, 
                                   GLbitfield mask, GLenum filter)
{
#ifdef CR_DUMP_BLITS
    SPUDispatchTable *gl = &cr_server.head_spu->dispatch_table;
    GLint rfb=0, dfb=0, dtex=0, dlev=-1, rb=0, db=0, ppb=0, pub=0, vp[4], otex, dstw, dsth;
    GLenum status;
    char fname[200];
    void *img;

    blitnum++;

    crDebug("[%i]BlitFramebufferEXT(%i, %i, %i, %i, %i, %i, %i, %i, %x, %x)", blitnum, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    crDebug("%i, %i <-> %i, %i", srcX1-srcX0, srcY1-srcY0, dstX1-dstX0, dstY1-dstY0);

    gl->GetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT, &rfb);
    gl->GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT, &dfb);
    gl->GetIntegerv(GL_READ_BUFFER, &rb);
    gl->GetIntegerv(GL_DRAW_BUFFER, &db);

    gl->GetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &ppb);
    gl->GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &pub);

    gl->GetIntegerv(GL_VIEWPORT, &vp[0]);

    gl->GetIntegerv(GL_TEXTURE_BINDING_2D, &otex);

    CRASSERT(!rfb && dfb);
    gl->GetFramebufferAttachmentParameterivEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT, &dtex);
    gl->GetFramebufferAttachmentParameterivEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT, &dlev);
    status = gl->CheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER_EXT);

    CRASSERT(status==GL_FRAMEBUFFER_COMPLETE_EXT
             && db==GL_COLOR_ATTACHMENT0_EXT
             && (rb==GL_FRONT || rb==GL_BACK)
             && !rfb && dfb && dtex && !dlev
             && !ppb && !pub);

    crDebug("Src[rb 0x%x, fbo %i] Dst[db 0x%x, fbo %i(0x%x), tex %i.%i]", rb, rfb, db, dfb, status, dtex, dlev);
    crDebug("Viewport [%i, %i, %i, %i]", vp[0], vp[1], vp[2], vp[3]);

    gl->PixelStorei(GL_PACK_ROW_LENGTH, 0);
    gl->PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl->PixelStorei(GL_PACK_SKIP_PIXELS, 0);
    gl->PixelStorei(GL_PACK_SKIP_ROWS, 0);

    gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->PixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    gl->PixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    gl->BindTexture(GL_TEXTURE_2D, dtex);
    gl->GetTexLevelParameteriv(GL_TEXTURE_2D, dlev, GL_TEXTURE_WIDTH, &dstw);
    gl->GetTexLevelParameteriv(GL_TEXTURE_2D, dlev, GL_TEXTURE_HEIGHT, &dsth);
    gl->BindTexture(GL_TEXTURE_2D, otex);
    crDebug("Dst is %i, %i", dstw, dsth);

    CRASSERT(vp[2]>=dstw && vp[3]>=dsth);
    img = crAlloc(vp[2]*vp[3]*4);
    CRASSERT(img);

    gl->ReadPixels(0, 0, vp[2], vp[3], GL_BGRA, GL_UNSIGNED_BYTE, img);
    sprintf(fname, "blit%iA_src.tga", blitnum);
    crDumpNamedTGA(fname, vp[2], vp[3], img);

    gl->BindTexture(GL_TEXTURE_2D, dtex);
    gl->GetTexImage(GL_TEXTURE_2D, dlev, GL_BGRA, GL_UNSIGNED_BYTE, img);
    sprintf(fname, "blit%iB_dst.tga", blitnum);
    crDumpNamedTGA(fname, dstw, dsth, img);
    gl->BindTexture(GL_TEXTURE_2D, otex);
#endif

    cr_server.head_spu->dispatch_table.BlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1,
                                                          dstX0, dstY0, dstX1, dstY1,
                                                          mask, filter);

#ifdef CR_DUMP_BLITS
    gl->BindTexture(GL_TEXTURE_2D, dtex);
    gl->GetTexImage(GL_TEXTURE_2D, dlev, GL_BGRA, GL_UNSIGNED_BYTE, img);
    sprintf(fname, "blit%iC_res.tga", blitnum);
    crDumpNamedTGA(fname, dstw, dsth, img);
    gl->BindTexture(GL_TEXTURE_2D, otex);
    crFree(img);
#endif
}
