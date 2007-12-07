/** @file
 * VBox OpenGL helper functions
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

#ifndef ___VBOGL_H
#define ___VBOGL_H

#ifdef RT_OS_WINDOWS
#include <windows.h>
#endif
#include <GL/gl.h>
#include <VBox/types.h>
#include "gldrv.h"
#include <VBox/HostServices/wglext.h>

typedef void (* PFN_VBOXGLWRAPPER)(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);

/**
 * Global init of VBox OpenGL
 *
 * @returns VBox error code
 */
int vboxglGlobalInit();

/**
 * Global deinit of VBox OpenGL
 *
 * @returns VBox error code
 */
int vboxglGlobalUnload();

/**
 * Client connect init
 *
 * @returns VBox error code
 * @param   pClient         Client context
 */
int vboxglConnect(PVBOXOGLCTX pClient);

/**
 * Client disconnect cleanup
 *
 * @returns VBox error code
 * @param   pClient         Client context
 */
int vboxglDisconnect(PVBOXOGLCTX pClient);

/**
 * Log to the debug output device
 *
 * @returns VBox error code
 * @param   pClient
 * @param   name        glGetString name parameter
 * @param   pString     String pointer
 * @param   pcbString   String length (in/out)
 */
int vboxglGetString(VBOXOGLCTX *pClient, GLenum name, char *pString, uint32_t *pcbString);

/**
 * Flush all queued OpenGL commands
 *
 * @returns VBox error code
 * @param   pClient         Client context
 * @param   pCmdBuffer      Command buffer
 * @param   cbCmdBuffer     Command buffer size
 * @param   cCommands       Number of commands in the buffer
 * @param   pLastError      Pointer to last error (out)
 * @param   pLastRetVal     Pointer to return val of last executed command (out)
 */
int vboxglFlushBuffer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer, uint32_t cbCmdBuffer, uint32_t cCommands, GLenum *pLastError, uint64_t *pLastRetVal);


/**
 * Initialize OpenGL extensions
 *
 * @returns VBox error code
 */
int vboxInitOpenGLExtensions();

/**
 * Check if an opengl extension function is available
 *
 * @returns VBox error code
 * @param   pszFunctionName     OpenGL extension function name
 */
#ifdef RT_OS_WINDOWS
bool vboxwglGetProcAddress(char *pszFunctionName);
#endif

/* OpenGL wrappers */
void vboxglArrayElement(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglBegin(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglBindTexture(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglBlendFunc(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglCallList(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor3b(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor3d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor3f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor3i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor3s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor3ub(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor3ui(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor3us(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor4b(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor4d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor4f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor4i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor4s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor4ub(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor4ui(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColor4us(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglClear(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglClearAccum(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglClearColor(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglClearDepth(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglClearIndex(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglClearStencil(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglAccum(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglAlphaFunc(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex2d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex2f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex2i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex2s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex3d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex3f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex3i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex3s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex4d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex4f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex4i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertex4s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord1d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord1f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord1i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord1s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord2d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord2f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord2i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord2s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord3d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord3f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord3i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord3s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord4d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord4f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord4i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoord4s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglNormal3b(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglNormal3d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglNormal3f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglNormal3i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglNormal3s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos2d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos2f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos2i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos2s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos3d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos3f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos3i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos3s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos4d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos4f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos4i(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRasterPos4s(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEvalCoord1d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEvalCoord1f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEvalCoord2d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEvalCoord2f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEvalPoint1(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEvalPoint2(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglIndexd(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglIndexf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglIndexi(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglIndexs(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglIndexub(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRotated(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRotatef(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglScaled(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglScalef(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTranslated(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTranslatef(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDepthFunc(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDepthMask(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglFinish(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglFlush(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDeleteLists(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglCullFace(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDeleteTextures(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDepthRange(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDisableClientState(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEnableClientState(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEvalMesh1(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEvalMesh2(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglFogf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglFogfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglFogi(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglFogiv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLightModelf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLightModelfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLightModeli(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLightModeliv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLightf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLightfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLighti(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLightiv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLineStipple(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLineWidth(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglListBase(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrawArrays(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrawBuffer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEdgeFlag(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEnd(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEndList(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglCopyTexImage1D(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglCopyTexImage2D(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColorMaterial(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMateriali(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMaterialf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMaterialfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMaterialiv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPopAttrib(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPopClientAttrib(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPopMatrix(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPopName(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPushAttrib(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPushClientAttrib(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPushMatrix(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPushName(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglReadBuffer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexGendv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexGenf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexGend(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexGeni(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexEnvi(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexEnvf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexEnviv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexEnvfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexGeniv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexGenfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexParameterf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexParameteri(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexParameterfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexParameteriv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLoadIdentity(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLoadName(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLoadMatrixd(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLoadMatrixf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglStencilFunc(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglShadeModel(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglStencilMask(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglStencilOp(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglScissor(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglViewport(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRectd(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRectf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRecti(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRects(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRectdv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRectfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRectiv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRectsv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMultMatrixd(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMultMatrixf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglNewList(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglHint(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglIndexMask(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglInitNames(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexCoordPointer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglVertexPointer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColorPointer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEdgeFlagPointer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglIndexPointer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglNormalPointer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPolygonStipple(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglCallLists(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglClipPlane(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglFrustum(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGenTextures(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMap1d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMap1f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMap2d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMap2f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMapGrid1d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMapGrid1f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMapGrid2d(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMapGrid2f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglCopyPixels(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexImage1D(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexImage2D(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexSubImage1D(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglTexSubImage2D(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglFeedbackBuffer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglSelectBuffer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglIsList(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglIsTexture(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglRenderMode(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglReadPixels(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglIsEnabled(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGenLists(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPixelTransferf(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPixelTransferi(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPixelZoom(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPixelStorei(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPixelStoref(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPixelMapfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPixelMapuiv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPixelMapusv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPointSize(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPolygonMode(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPolygonOffset(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPassThrough(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglOrtho(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglMatrixMode(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglLogicOp(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglColorMask(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglCopyTexSubImage1D(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglCopyTexSubImage2D(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglFrontFace(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDisable(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglEnable(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglPrioritizeTextures(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetBooleanv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetDoublev(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetFloatv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetIntegerv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetLightfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetLightiv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetMaterialfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetMaterialiv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetPixelMapfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetPixelMapuiv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetPixelMapusv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetTexEnviv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetTexEnvfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetTexGendv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetTexGenfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetTexGeniv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetTexParameterfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetTexParameteriv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetClipPlane(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetPolygonStipple(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetTexLevelParameterfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetTexLevelParameteriv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglGetTexImage (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);

#ifdef RT_OS_WINDOWS
void vboxwglSwapIntervalEXT (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxwglGetSwapIntervalEXT (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
#endif

/* after the above */
#include <VBox/HostServices/VBoxOpenGLSvc.h>
#include <VBox/HostServices/VBoxOGLOp.h>

#endif /* !___VBOGL_H */
