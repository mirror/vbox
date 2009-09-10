/** @file
 * VBoxFBOverlay implementaion
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
#if defined (VBOX_GUI_USE_QGL)

#define LOG_GROUP LOG_GROUP_GUI

#include "VBoxFBOverlay.h"
#include "VBoxFrameBuffer.h"

#include "VBoxConsoleView.h"
#include "VBoxProblemReporter.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QGLWidget>

#include <iprt/asm.h>

#ifdef VBOX_WITH_VIDEOHWACCEL
#include <VBox/VBoxVideo.h>
#include <VBox/types.h>
#include <VBox/ssm.h>
#endif
#include <iprt/semaphore.h>

#include <QFile>
#include <QTextStream>

#ifdef VBOXQGL_PROF_BASE
# ifdef VBOXQGL_DBG_SURF
#  define VBOXQGL_PROF_WIDTH 1400
#  define VBOXQGL_PROF_HEIGHT 1050
# else
# define VBOXQGL_PROF_WIDTH 1400
# define VBOXQGL_PROF_HEIGHT 1050
//#define VBOXQGL_PROF_WIDTH 720
//#define VBOXQGL_PROF_HEIGHT 480
# endif
#endif

#define VBOXQGL_STATE_NAMEBASE "QGLVHWAData"
#define VBOXQGL_STATE_VERSION 1

//#define VBOXQGLOVERLAY_STATE_NAMEBASE "QGLOverlayVHWAData"
//#define VBOXQGLOVERLAY_STATE_VERSION 1

#ifdef DEBUG_misha
# define VBOXQGL_STATE_DEBUG
#endif

#ifdef VBOXQGL_STATE_DEBUG
#define VBOXQGL_STATE_START_MAGIC        0x12345678
#define VBOXQGL_STATE_STOP_MAGIC         0x87654321

#define VBOXQGL_STATE_SURFSTART_MAGIC    0x9abcdef1
#define VBOXQGL_STATE_SURFSTOP_MAGIC     0x1fedcba9

#define VBOXQGL_STATE_OVERLAYSTART_MAGIC 0x13579bdf
#define VBOXQGL_STATE_OVERLAYSTOP_MAGIC  0xfdb97531

#define VBOXQGL_SAVE_START(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VBOXQGL_STATE_START_MAGIC); AssertRC(rc);}while(0)
#define VBOXQGL_SAVE_STOP(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VBOXQGL_STATE_STOP_MAGIC); AssertRC(rc);}while(0)

#define VBOXQGL_SAVE_SURFSTART(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VBOXQGL_STATE_SURFSTART_MAGIC); AssertRC(rc);}while(0)
#define VBOXQGL_SAVE_SURFSTOP(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VBOXQGL_STATE_SURFSTOP_MAGIC); AssertRC(rc);}while(0)

#define VBOXQGL_SAVE_OVERLAYSTART(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VBOXQGL_STATE_OVERLAYSTART_MAGIC); AssertRC(rc);}while(0)
#define VBOXQGL_SAVE_OVERLAYSTOP(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VBOXQGL_STATE_OVERLAYSTOP_MAGIC); AssertRC(rc);}while(0)

#define VBOXQGL_LOAD_CHECK(_pSSM, _v) \
    do{ \
        uint32_t _u32; \
        int rc = SSMR3GetU32(_pSSM, &_u32); AssertRC(rc); \
        if(_u32 != (_v)) \
        { \
            VBOXQGLLOG(("load error: expected magic (0x%x), but was (0x%x)\n", (_v), _u32));\
        }\
        Assert(_u32 == (_v)); \
    }while(0)

#define VBOXQGL_LOAD_START(_pSSM) VBOXQGL_LOAD_CHECK(_pSSM, VBOXQGL_STATE_START_MAGIC)
#define VBOXQGL_LOAD_STOP(_pSSM) VBOXQGL_LOAD_CHECK(_pSSM, VBOXQGL_STATE_STOP_MAGIC)

#define VBOXQGL_LOAD_SURFSTART(_pSSM) VBOXQGL_LOAD_CHECK(_pSSM, VBOXQGL_STATE_SURFSTART_MAGIC)
#define VBOXQGL_LOAD_SURFSTOP(_pSSM) VBOXQGL_LOAD_CHECK(_pSSM, VBOXQGL_STATE_SURFSTOP_MAGIC)

#define VBOXQGL_LOAD_OVERLAYSTART(_pSSM) VBOXQGL_LOAD_CHECK(_pSSM, VBOXQGL_STATE_OVERLAYSTART_MAGIC)
#define VBOXQGL_LOAD_OVERLAYSTOP(_pSSM) VBOXQGL_LOAD_CHECK(_pSSM, VBOXQGL_STATE_OVERLAYSTOP_MAGIC)

#else

#define VBOXQGL_SAVE_START(_pSSM) do{}while(0)
#define VBOXQGL_SAVE_STOP(_pSSM) do{}while(0)

#define VBOXQGL_SAVE_SURFSTART(_pSSM) do{}while(0)
#define VBOXQGL_SAVE_SURFSTOP(_pSSM) do{}while(0)

#define VBOXQGL_SAVE_OVERLAYSTART(_pSSM) do{}while(0)
#define VBOXQGL_SAVE_OVERLAYSTOP(_pSSM) do{}while(0)

#define VBOXQGL_LOAD_START(_pSSM) do{}while(0)
#define VBOXQGL_LOAD_STOP(_pSSM) do{}while(0)

#define VBOXQGL_LOAD_SURFSTART(_pSSM) do{}while(0)
#define VBOXQGL_LOAD_SURFSTOP(_pSSM) do{}while(0)

#define VBOXQGL_LOAD_OVERLAYSTART(_pSSM) do{}while(0)
#define VBOXQGL_LOAD_OVERLAYSTOP(_pSSM) do{}while(0)

#endif

#define VBOXQGL_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
                ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))

#define FOURCC_AYUV VBOXQGL_MAKEFOURCC('A', 'Y', 'U', 'V')
#define FOURCC_UYVY VBOXQGL_MAKEFOURCC('U', 'Y', 'V', 'Y')
#define FOURCC_YUY2 VBOXQGL_MAKEFOURCC('Y', 'U', 'Y', '2')
#define FOURCC_YV12 VBOXQGL_MAKEFOURCC('Y', 'V', '1', '2')
#define VBOXVHWA_NUMFOURCC 4

typedef char GLchar;

#ifndef GL_COMPILE_STATUS
# define GL_COMPILE_STATUS 0x8b81
#endif
#ifndef GL_LINK_STATUS
# define GL_LINK_STATUS    0x8b82
#endif
#ifndef GL_FRAGMENT_SHADER
# define GL_FRAGMENT_SHADER 0x8b30
#endif
#ifndef GL_VERTEX_SHADER
# define GL_VERTEX_SHADER 0x8b31
#endif

/* GL_ARB_multitexture */
#ifndef GL_TEXTURE0
# define GL_TEXTURE0                    0x84c0
#endif
#ifndef GL_TEXTURE1
# define GL_TEXTURE1                    0x84c1
#endif
#ifndef GL_MAX_TEXTURE_COORDS
# define GL_MAX_TEXTURE_COORDS          0x8871
#endif
#ifndef GL_MAX_TEXTURE_IMAGE_UNITS
# define GL_MAX_TEXTURE_IMAGE_UNITS     0x8872
#endif

#ifndef APIENTRY
# define APIENTRY
#endif

typedef GLvoid (APIENTRY *PFNVBOXVHWA_ACTIVE_TEXTURE) (GLenum texture);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_MULTI_TEX_COORD2I) (GLenum texture, GLint v0, GLint v1);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_MULTI_TEX_COORD2F) (GLenum texture, GLfloat v0, GLfloat v1);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_MULTI_TEX_COORD2D) (GLenum texture, GLdouble v0, GLdouble v1);

/* GL_ARB_texture_rectangle */
#ifndef GL_TEXTURE_RECTANGLE
# define GL_TEXTURE_RECTANGLE 0x84F5
#endif

/* GL_ARB_shader_objects */
/* GL_ARB_fragment_shader */

typedef GLuint (APIENTRY *PFNVBOXVHWA_CREATE_SHADER)  (GLenum type);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_SHADER_SOURCE)  (GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_COMPILE_SHADER) (GLuint shader);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_DELETE_SHADER)  (GLuint shader);

typedef GLuint (APIENTRY *PFNVBOXVHWA_CREATE_PROGRAM) ();
typedef GLvoid (APIENTRY *PFNVBOXVHWA_ATTACH_SHADER)  (GLuint program, GLuint shader);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_DETACH_SHADER)  (GLuint program, GLuint shader);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_LINK_PROGRAM)   (GLuint program);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_USE_PROGRAM)    (GLuint program);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_DELETE_PROGRAM) (GLuint program);

typedef GLboolean (APIENTRY *PFNVBOXVHWA_IS_SHADER)   (GLuint shader);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_GET_SHADERIV)   (GLuint shader, GLenum pname, GLint *params);
typedef GLboolean (APIENTRY *PFNVBOXVHWA_IS_PROGRAM)  (GLuint program);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_GET_PROGRAMIV)  (GLuint program, GLenum pname, GLint *params);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_GET_ATTACHED_SHADERS) (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_GET_SHADER_INFO_LOG)  (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_GET_PROGRAM_INFO_LOG) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLint (APIENTRY *PFNVBOXVHWA_GET_UNIFORM_LOCATION) (GLint programObj, const GLchar *name);

typedef GLvoid (APIENTRY *PFNVBOXVHWA_UNIFORM1F)(GLint location, GLfloat v0);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_UNIFORM2F)(GLint location, GLfloat v0, GLfloat v1);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_UNIFORM3F)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_UNIFORM4F)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

typedef GLvoid (APIENTRY *PFNVBOXVHWA_UNIFORM1I)(GLint location, GLint v0);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_UNIFORM2I)(GLint location, GLint v0, GLint v1);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_UNIFORM3I)(GLint location, GLint v0, GLint v1, GLint v2);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_UNIFORM4I)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);

/* GL_ARB_pixel_buffer_object*/
#ifndef Q_WS_MAC
/* apears to be defined on mac */
typedef ptrdiff_t GLsizeiptr;
#endif

#ifndef GL_READ_ONLY
# define GL_READ_ONLY                   0x88B8
#endif
#ifndef GL_WRITE_ONLY
# define GL_WRITE_ONLY                  0x88B9
#endif
#ifndef GL_READ_WRITE
# define GL_READ_WRITE                  0x88BA
#endif
#ifndef GL_STREAM_DRAW
# define GL_STREAM_DRAW                 0x88E0
#endif
#ifndef GL_STREAM_READ
# define GL_STREAM_READ                 0x88E1
#endif
#ifndef GL_STREAM_COPY
# define GL_STREAM_COPY                 0x88E2
#endif

#ifndef GL_PIXEL_PACK_BUFFER
# define GL_PIXEL_PACK_BUFFER           0x88EB
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
# define GL_PIXEL_UNPACK_BUFFER         0x88EC
#endif
#ifndef GL_PIXEL_PACK_BUFFER_BINDING
# define GL_PIXEL_PACK_BUFFER_BINDING   0x88ED
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER_BINDING
# define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#endif

typedef GLvoid (APIENTRY *PFNVBOXVHWA_GEN_BUFFERS)(GLsizei n, GLuint *buffers);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_DELETE_BUFFERS)(GLsizei n, const GLuint *buffers);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_BIND_BUFFER)(GLenum target, GLuint buffer);
typedef GLvoid (APIENTRY *PFNVBOXVHWA_BUFFER_DATA)(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef GLvoid* (APIENTRY *PFNVBOXVHWA_MAP_BUFFER)(GLenum target, GLenum access);
typedef GLboolean (APIENTRY *PFNVBOXVHWA_UNMAP_BUFFER)(GLenum target);

/*****************/

/* functions */

PFNVBOXVHWA_ACTIVE_TEXTURE vboxglActiveTexture = NULL;
PFNVBOXVHWA_MULTI_TEX_COORD2I vboxglMultiTexCoord2i = NULL;
PFNVBOXVHWA_MULTI_TEX_COORD2D vboxglMultiTexCoord2d = NULL;
PFNVBOXVHWA_MULTI_TEX_COORD2F vboxglMultiTexCoord2f = NULL;


PFNVBOXVHWA_CREATE_SHADER   vboxglCreateShader  = NULL;
PFNVBOXVHWA_SHADER_SOURCE   vboxglShaderSource  = NULL;
PFNVBOXVHWA_COMPILE_SHADER  vboxglCompileShader = NULL;
PFNVBOXVHWA_DELETE_SHADER   vboxglDeleteShader  = NULL;

PFNVBOXVHWA_CREATE_PROGRAM  vboxglCreateProgram = NULL;
PFNVBOXVHWA_ATTACH_SHADER   vboxglAttachShader  = NULL;
PFNVBOXVHWA_DETACH_SHADER   vboxglDetachShader  = NULL;
PFNVBOXVHWA_LINK_PROGRAM    vboxglLinkProgram   = NULL;
PFNVBOXVHWA_USE_PROGRAM     vboxglUseProgram    = NULL;
PFNVBOXVHWA_DELETE_PROGRAM  vboxglDeleteProgram = NULL;

PFNVBOXVHWA_IS_SHADER       vboxglIsShader      = NULL;
PFNVBOXVHWA_GET_SHADERIV    vboxglGetShaderiv   = NULL;
PFNVBOXVHWA_IS_PROGRAM      vboxglIsProgram     = NULL;
PFNVBOXVHWA_GET_PROGRAMIV   vboxglGetProgramiv  = NULL;
PFNVBOXVHWA_GET_ATTACHED_SHADERS vboxglGetAttachedShaders = NULL;
PFNVBOXVHWA_GET_SHADER_INFO_LOG  vboxglGetShaderInfoLog   = NULL;
PFNVBOXVHWA_GET_PROGRAM_INFO_LOG vboxglGetProgramInfoLog  = NULL;

PFNVBOXVHWA_GET_UNIFORM_LOCATION vboxglGetUniformLocation = NULL;

PFNVBOXVHWA_UNIFORM1F vboxglUniform1f;
PFNVBOXVHWA_UNIFORM2F vboxglUniform2f;
PFNVBOXVHWA_UNIFORM3F vboxglUniform3f;
PFNVBOXVHWA_UNIFORM4F vboxglUniform4f;

PFNVBOXVHWA_UNIFORM1I vboxglUniform1i;
PFNVBOXVHWA_UNIFORM2I vboxglUniform2i;
PFNVBOXVHWA_UNIFORM3I vboxglUniform3i;
PFNVBOXVHWA_UNIFORM4I vboxglUniform4i;

PFNVBOXVHWA_GEN_BUFFERS vboxglGenBuffers = NULL;
PFNVBOXVHWA_DELETE_BUFFERS vboxglDeleteBuffers = NULL;
PFNVBOXVHWA_BIND_BUFFER vboxglBindBuffer = NULL;
PFNVBOXVHWA_BUFFER_DATA vboxglBufferData = NULL;
PFNVBOXVHWA_MAP_BUFFER vboxglMapBuffer = NULL;
PFNVBOXVHWA_UNMAP_BUFFER vboxglUnmapBuffer = NULL;

#if 0
#if defined Q_WS_WIN
#define VBOXVHWA_GETPROCADDRESS(_t, _n) (_t)wglGetProcAddress(_n)
#elif defined Q_WS_X11
#include <GL/glx.h>
#define VBOXVHWA_GETPROCADDRESS(_t, _n) (_t)glXGetProcAddress((const GLubyte *)(_n))
#else
#error "Port me!!!"
#endif
#endif

#define VBOXVHWA_GETPROCADDRESS(_c, _t, _n) ((_t)(_c).getProcAddress(QString(_n)))

#define VBOXVHWA_PFNINIT_SAME(_c, _t, _v, _rc) \
    do { \
        if((vboxgl##_v = VBOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v)) == NULL) \
        { \
            VBOXQGLLOG(("ERROR: '%s' function is not found\n", "gl"#_v));\
            AssertBreakpoint(); \
            if((vboxgl##_v = VBOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v"ARB")) == NULL) \
            { \
                VBOXQGLLOG(("ERROR: '%s' function is not found\n", "gl"#_v"ARB"));\
                AssertBreakpoint(); \
                if((vboxgl##_v = VBOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v"EXT")) == NULL) \
                { \
                    VBOXQGLLOG(("ERROR: '%s' function is not found\n", "gl"#_v"EXT"));\
                    AssertBreakpoint(); \
                    (_rc)++; \
                } \
            } \
        } \
    }while(0)

#define VBOXVHWA_PFNINIT(_c, _t, _v, _f,_rc) \
    do { \
        if((vboxgl##_v = VBOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_f)) == NULL) \
        { \
            VBOXQGLLOG(("ERROR: '%s' function is not found\n", "gl"#_f));\
            AssertBreakpoint(); \
            (_rc)++; \
        } \
    }while(0)

//#define VBOXVHWA_PFNINIT_OBJECT_ARB(_t, _v, _rc) VBOXVHWA_PFNINIT(_t, _v, #_v"ObjectARB" ,_rc)
#define VBOXVHWA_PFNINIT_OBJECT_ARB(_c, _t, _v, _rc) \
        do { \
            if((vboxgl##_v = VBOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v"ObjectARB")) == NULL) \
            { \
                VBOXQGLLOG(("ERROR: '%s' function is not found\n", "gl"#_v"ObjectARB"));\
                AssertBreakpoint(); \
                (_rc)++; \
            } \
        }while(0)

//#define VBOXVHWA_PFNINIT_ARB(_t, _v, _rc) VBOXVHWA_PFNINIT(_t, _v, #_v"ARB" ,_rc)
#define VBOXVHWA_PFNINIT_ARB(_c, _t, _v, _rc) \
        do { \
            if((vboxgl##_v = VBOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v"ARB")) == NULL) \
            { \
                VBOXQGLLOG(("ERROR: '%s' function is not found\n", "gl"#_v"ARB"));\
                AssertBreakpoint(); \
                (_rc)++; \
            } \
        }while(0)


static bool g_vboxVHWAGlSupportInitialized = false;
/* vbox version in the format 0x00mjmnbl
 * in case of a failure contains -1 (0xffffffff) */
static int g_vboxVHWAGlVersion = 0;

static bool g_GL_ARB_multitexture        = false;
static bool g_GL_ARB_shader_objects      = false;
static bool g_GL_ARB_fragment_shader     = false;
static bool g_GL_ARB_pixel_buffer_object = false;
static bool g_GL_ARB_texture_rectangle   = false;
static bool g_GL_EXT_texture_rectangle   = false;
static bool g_GL_NV_texture_rectangle         = false;
static bool g_GL_ARB_texture_non_power_of_two = false;

/* gl features supported */
static bool g_vboxVHWAGlShaderSupported = false;
static bool g_vboxVHWAGlTextureRectangleSupported = false;
static bool g_vboxVHWAGlTextureNP2Supported = false;
static bool g_vboxVHWAGlPBOSupported = false;
static int g_vboxVHWAGlMultiTexNumSupported = 1; /* 1 would mean it is not supported */

/* vhwa features supported */
static uint32_t g_vboxVHWAFourccSupportedList[VBOXVHWA_NUMFOURCC];
static uint32_t g_vboxVHWAFourccSupportedCount = 0;


static int vboxVHWAGlParseSubver(const GLubyte * ver, const GLubyte ** pNext, bool bSpacePrefixAllowed)
{
    int val = 0;

    for(;;++ver)
    {
        if(*ver >= '0' && *ver < '9')
        {
            if(!val)
            {
                if(*ver == '0')
                    continue;
            }
            else
            {
                val *= 10;
            }
            val += *ver - '0';
        }
        else if(*ver == '.')
        {
            *pNext = ver+1;
            break;
        }
        else if(*ver == '\0')
        {
            *pNext = NULL;
            break;
        }
        else if(*ver == ' ' || *ver == '\t' ||  *ver == 0x0d || *ver == 0x0a)
        {
            if(bSpacePrefixAllowed)
            {
                if(!val)
                {
                    continue;
                }
            }

            /* treat this as the end ov version string */
            *pNext = NULL;
            break;
        }
        else
        {
            Assert(0);
            val = -1;
            break;
        }
    }

    return val;
}

static int vboxVHWAGlParseVersion(const GLubyte * ver)
{
    int iVer = vboxVHWAGlParseSubver(ver, &ver, true);
    if(iVer)
    {
        iVer <<= 16;
        if(ver)
        {
            int tmp = vboxVHWAGlParseSubver(ver, &ver, false);
            if(tmp >= 0)
            {
                iVer |= tmp << 8;
                if(ver)
                {
                    tmp = vboxVHWAGlParseSubver(ver, &ver, false);
                    if(tmp >= 0)
                    {
                        iVer |= tmp;
                    }
                    else
                    {
                        Assert(0);
                        iVer = -1;
                    }
                }
            }
            else
            {
                Assert(0);
                iVer = -1;
            }
        }
    }
    return iVer;
}

static void vboxVHWAGlInitExtSupport(const QGLContext & context)
{
    int rc = 0;
    do
    {
        rc = 0;
        g_vboxVHWAGlMultiTexNumSupported = 1; /* default, 1 means not supported */
        if(g_vboxVHWAGlVersion >= 0x010201) /* ogl >= 1.2.1 */
        {
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_ACTIVE_TEXTURE, ActiveTexture, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_MULTI_TEX_COORD2I, MultiTexCoord2i, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_MULTI_TEX_COORD2D, MultiTexCoord2d, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_MULTI_TEX_COORD2F, MultiTexCoord2f, rc);
        }
        else if(g_GL_ARB_multitexture)
        {
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_ACTIVE_TEXTURE, ActiveTexture, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_MULTI_TEX_COORD2I, MultiTexCoord2i, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_MULTI_TEX_COORD2D, MultiTexCoord2d, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_MULTI_TEX_COORD2F, MultiTexCoord2f, rc);
        }
        else
        {
            break;
        }

        if(RT_FAILURE(rc))
            break;

        GLint maxCoords, maxUnits;
        glGetIntegerv(GL_MAX_TEXTURE_COORDS, &maxCoords);
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxUnits);

        VBOXQGLLOGREL(("Max Tex Coords (%d), Img Units (%d)\n", maxCoords, maxUnits));
        /* take the minimum of those */
        if(maxUnits < maxCoords)
            maxCoords = maxUnits;
        if(maxUnits < 2)
        {
            VBOXQGLLOGREL(("Max Tex Coord or Img Units < 2 disabling MultiTex support\n"));
            break;
        }

        g_vboxVHWAGlMultiTexNumSupported = maxUnits;
    }while(0);


    do
    {
        rc = 0;
        g_vboxVHWAGlPBOSupported = false;

        if(g_GL_ARB_pixel_buffer_object)
        {
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_GEN_BUFFERS, GenBuffers, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_DELETE_BUFFERS, DeleteBuffers, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_BIND_BUFFER, BindBuffer, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_BUFFER_DATA, BufferData, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_MAP_BUFFER, MapBuffer, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_UNMAP_BUFFER, UnmapBuffer, rc);
        }
        else
        {
            break;
        }

        if(RT_FAILURE(rc))
            break;

        g_vboxVHWAGlPBOSupported = true;
    } while(0);

    do
    {
        rc = 0;
        g_vboxVHWAGlShaderSupported = false;

        if(g_vboxVHWAGlVersion >= 0x020000)  /* if ogl >= 2.0*/
        {
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_CREATE_SHADER, CreateShader, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_SHADER_SOURCE, ShaderSource, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_COMPILE_SHADER, CompileShader, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_DELETE_SHADER, DeleteShader, rc);

            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_CREATE_PROGRAM, CreateProgram, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_ATTACH_SHADER, AttachShader, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_DETACH_SHADER, DetachShader, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_LINK_PROGRAM, LinkProgram, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_USE_PROGRAM, UseProgram, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_DELETE_PROGRAM, DeleteProgram, rc);

            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_IS_SHADER, IsShader, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_GET_SHADERIV, GetShaderiv, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_IS_PROGRAM, IsProgram, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_GET_PROGRAMIV, GetProgramiv, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_GET_ATTACHED_SHADERS, GetAttachedShaders,  rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_GET_SHADER_INFO_LOG, GetShaderInfoLog, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_GET_PROGRAM_INFO_LOG, GetProgramInfoLog, rc);

            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_GET_UNIFORM_LOCATION, GetUniformLocation, rc);

            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_UNIFORM1F, Uniform1f, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_UNIFORM2F, Uniform2f, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_UNIFORM3F, Uniform3f, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_UNIFORM4F, Uniform4f, rc);

            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_UNIFORM1I, Uniform1i, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_UNIFORM2I, Uniform2i, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_UNIFORM3I, Uniform3i, rc);
            VBOXVHWA_PFNINIT_SAME(context, PFNVBOXVHWA_UNIFORM4I, Uniform4i, rc);
        }
        else if(g_GL_ARB_shader_objects && g_GL_ARB_fragment_shader)
        {
            VBOXVHWA_PFNINIT_OBJECT_ARB(context, PFNVBOXVHWA_CREATE_SHADER, CreateShader, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_SHADER_SOURCE, ShaderSource, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_COMPILE_SHADER, CompileShader, rc);
            VBOXVHWA_PFNINIT(context, PFNVBOXVHWA_DELETE_SHADER, DeleteShader, DeleteObjectARB, rc);

            VBOXVHWA_PFNINIT_OBJECT_ARB(context, PFNVBOXVHWA_CREATE_PROGRAM, CreateProgram, rc);
            VBOXVHWA_PFNINIT(context, PFNVBOXVHWA_ATTACH_SHADER, AttachShader, AttachObjectARB, rc);
            VBOXVHWA_PFNINIT(context, PFNVBOXVHWA_DETACH_SHADER, DetachShader, DetachObjectARB, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_LINK_PROGRAM, LinkProgram, rc);
            VBOXVHWA_PFNINIT_OBJECT_ARB(context, PFNVBOXVHWA_USE_PROGRAM, UseProgram, rc);
            VBOXVHWA_PFNINIT(context, PFNVBOXVHWA_DELETE_PROGRAM, DeleteProgram, DeleteObjectARB, rc);

        //TODO:    VBOXVHWA_PFNINIT(PFNVBOXVHWA_IS_SHADER, IsShader, rc);
            VBOXVHWA_PFNINIT(context, PFNVBOXVHWA_GET_SHADERIV, GetShaderiv, GetObjectParameterivARB, rc);
        //TODO:    VBOXVHWA_PFNINIT(PFNVBOXVHWA_IS_PROGRAM, IsProgram, rc);
            VBOXVHWA_PFNINIT(context, PFNVBOXVHWA_GET_PROGRAMIV, GetProgramiv, GetObjectParameterivARB, rc);
            VBOXVHWA_PFNINIT(context, PFNVBOXVHWA_GET_ATTACHED_SHADERS, GetAttachedShaders, GetAttachedObjectsARB, rc);
            VBOXVHWA_PFNINIT(context, PFNVBOXVHWA_GET_SHADER_INFO_LOG, GetShaderInfoLog, GetInfoLogARB, rc);
            VBOXVHWA_PFNINIT(context, PFNVBOXVHWA_GET_PROGRAM_INFO_LOG, GetProgramInfoLog, GetInfoLogARB, rc);

            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_GET_UNIFORM_LOCATION, GetUniformLocation, rc);

            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_UNIFORM1F, Uniform1f, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_UNIFORM2F, Uniform2f, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_UNIFORM3F, Uniform3f, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_UNIFORM4F, Uniform4f, rc);

            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_UNIFORM1I, Uniform1i, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_UNIFORM2I, Uniform2i, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_UNIFORM3I, Uniform3i, rc);
            VBOXVHWA_PFNINIT_ARB(context, PFNVBOXVHWA_UNIFORM4I, Uniform4i, rc);
        }
        else
        {
            break;
        }

        if(RT_FAILURE(rc))
            break;

        g_vboxVHWAGlShaderSupported = true;
    } while(0);

    if(g_GL_ARB_texture_rectangle || g_GL_EXT_texture_rectangle || g_GL_NV_texture_rectangle)
    {
        g_vboxVHWAGlTextureRectangleSupported = true;
    }
    else
    {
        g_vboxVHWAGlTextureRectangleSupported = false;
    }

    g_vboxVHWAGlTextureNP2Supported = g_GL_ARB_texture_non_power_of_two;
}

static void vboxVHWAGlInitFeatureSupport()
{
    if(g_vboxVHWAGlShaderSupported && g_vboxVHWAGlTextureRectangleSupported)
    {
        uint32_t num = 0;
        g_vboxVHWAFourccSupportedList[num++] = FOURCC_AYUV;
        g_vboxVHWAFourccSupportedList[num++] = FOURCC_UYVY;
        g_vboxVHWAFourccSupportedList[num++] = FOURCC_YUY2;
        if(g_vboxVHWAGlMultiTexNumSupported >= 4)
        {
            /* YV12 currently requires 3 units (for each color component)
             * + 1 unit for dst texture for color-keying + 3 units for each color component
             * TODO: we could store YV12 data in one texture to eliminate this requirement*/
            g_vboxVHWAFourccSupportedList[num++] = FOURCC_YV12;
        }

        Assert(num <= VBOXVHWA_NUMFOURCC);
        g_vboxVHWAFourccSupportedCount = num;
    }
    else
    {
        g_vboxVHWAFourccSupportedCount = 0;
    }
}

static void vboxVHWAGlInit(const QGLContext * pContext)
{
    if(g_vboxVHWAGlSupportInitialized)
        return;

    static QGLWidget *pTmpContextHolder = NULL;
    const bool bHasGlContext = (pContext != NULL);

    if(!pContext)
    {
        if(!pTmpContextHolder)
        {
            QGLWidget *pWidget = new QGLWidget();
            pWidget->makeCurrent();
            pContext = pWidget->context();
            pTmpContextHolder = pWidget;
        }

//        QGLContext * pTmpContext = new QGLContext(QGLFormat::defaultFormat());
//        bool bCreated = pTmpContext->create();
//        Assert(pTmpContext);
//        Assert(pTmpContext->isValid());
//
//        pTmpContext->makeCurrent();
//        pTmpContextHolder = pTmpContext;
    }

    const GLubyte * str;
    VBOXQGL_CHECKERR(
            str = glGetString(GL_VERSION);
            );

    VBOXQGLLOGREL (("gl version string: 0%s\n", str));

    g_vboxVHWAGlVersion = vboxVHWAGlParseVersion(str);
    Assert(g_vboxVHWAGlVersion > 0);
    if(g_vboxVHWAGlVersion < 0)
    {
        g_vboxVHWAGlVersion = 0;
    }
    else
    {
        VBOXQGLLOGREL (("gl version: 0x%x\n", g_vboxVHWAGlVersion));
        VBOXQGL_CHECKERR(
                str = glGetString(GL_EXTENSIONS);
                );

        const char * pos = strstr((const char *)str, "GL_ARB_multitexture");
        g_GL_ARB_multitexture = pos != NULL;
        VBOXQGLLOGREL (("GL_ARB_multitexture: %d\n", g_GL_ARB_multitexture));

        pos = strstr((const char *)str, "GL_ARB_shader_objects");
        g_GL_ARB_shader_objects = pos != NULL;
        VBOXQGLLOGREL (("GL_ARB_shader_objects: %d\n", g_GL_ARB_shader_objects));

        pos = strstr((const char *)str, "GL_ARB_fragment_shader");
        g_GL_ARB_fragment_shader = pos != NULL;
        VBOXQGLLOGREL (("GL_ARB_fragment_shader: %d\n", g_GL_ARB_fragment_shader));

        pos = strstr((const char *)str, "GL_ARB_pixel_buffer_object");
        g_GL_ARB_pixel_buffer_object = pos != NULL;
        VBOXQGLLOGREL (("GL_ARB_pixel_buffer_object: %d\n", g_GL_ARB_pixel_buffer_object));

        pos = strstr((const char *)str, "GL_ARB_texture_rectangle");
        g_GL_ARB_texture_rectangle = pos != NULL;
        VBOXQGLLOGREL (("GL_ARB_texture_rectangle: %d\n", g_GL_ARB_texture_rectangle));

        pos = strstr((const char *)str, "GL_EXT_texture_rectangle");
        g_GL_EXT_texture_rectangle = pos != NULL;
        VBOXQGLLOGREL (("GL_EXT_texture_rectangle: %d\n", g_GL_EXT_texture_rectangle));

        pos = strstr((const char *)str, "GL_NV_texture_rectangle");
        g_GL_NV_texture_rectangle = pos != NULL;
        VBOXQGLLOGREL (("GL_NV_texture_rectangle: %d\n", g_GL_NV_texture_rectangle));

        pos = strstr((const char *)str, "GL_ARB_texture_non_power_of_two");
        g_GL_ARB_texture_non_power_of_two = pos != NULL;
        VBOXQGLLOGREL (("GL_ARB_texture_non_power_of_two: %d\n", g_GL_ARB_texture_non_power_of_two));

        vboxVHWAGlInitExtSupport(*pContext);

        vboxVHWAGlInitFeatureSupport();
    }

    if(!bHasGlContext)
    {
        pTmpContextHolder->doneCurrent();
    }

    g_vboxVHWAGlSupportInitialized = true;
}

static bool vboxVHWASupportedInternal()
{
    if(g_vboxVHWAGlVersion <= 0)
    {
        /* error occurred while gl info initialization */
        return false;
    }

#ifndef DEBUGVHWASTRICT
    /* in case we do not support shaders & multitexturing we can not supprt dst colorkey,
     * no sense to report Video Acceleration supported */
    if(!g_vboxVHWAGlShaderSupported)
        return false;
#endif
    if(g_vboxVHWAGlMultiTexNumSupported < 2)
        return false;

    /* color conversion now supported only GL_TEXTURE_RECTANGLE
     * in this case only stretching is accelerated
     * report as unsupported, TODO: probably should report as supported for stretch acceleration */
    if(!g_vboxVHWAGlTextureRectangleSupported)
        return false;

    return true;
}

class VBoxVHWACommandProcessEvent : public QEvent
{
public:
    VBoxVHWACommandProcessEvent (VBoxVHWACommandElement *pEl)
        : QEvent ((QEvent::Type) VBoxDefs::VHWACommandProcessType)
    {
        mCmdPipe.put(pEl);
    }
    VBoxVHWACommandElementPipe & pipe() { return mCmdPipe; }

    VBoxVHWACommandProcessEvent *mpNext;
private:
    VBoxVHWACommandElementPipe mCmdPipe;
};



VBoxVHWAHandleTable::VBoxVHWAHandleTable(uint32_t initialSize)
{
    mTable = new void*[initialSize];
    memset(mTable, 0, initialSize*sizeof(void*));
    mcSize = initialSize;
    mcUsage = 0;
    mCursor = 1; /* 0 is treated as invalid */
}

VBoxVHWAHandleTable::~VBoxVHWAHandleTable()
{
    delete[] mTable;
}

uint32_t VBoxVHWAHandleTable::put(void * data)
{
    Assert(data);
    if(!data)
        return VBOXVHWA_SURFHANDLE_INVALID;

    if(mcUsage == mcSize)
    {
        /* @todo: resize */
        Assert(0);
    }

    Assert(mcUsage < mcSize);
    if(mcUsage >= mcSize)
        return VBOXVHWA_SURFHANDLE_INVALID;

    for(int k = 0; k < 2; ++k)
    {
        Assert(mCursor != 0);
        for(uint32_t i = mCursor; i < mcSize; ++i)
        {
            if(!mTable[i])
            {
                doPut(i, data);
                mCursor = i+1;
                return i;
            }
        }
        mCursor = 1; /* 0 is treated as invalid */
    }

    Assert(0);
    return VBOXVHWA_SURFHANDLE_INVALID;
}

bool VBoxVHWAHandleTable::mapPut(uint32_t h, void * data)
{
    if(mcSize <= h)
        return false;
    if(h == 0)
        return false;
    if(mTable[h])
        return false;

    doPut(h, data);
    return true;
}

void* VBoxVHWAHandleTable::get(uint32_t h)
{
    Assert(h < mcSize);
    Assert(h > 0);
    return mTable[h];
}

void* VBoxVHWAHandleTable::remove(uint32_t h)
{
    Assert(mcUsage);
    Assert(h < mcSize);
    void* val = mTable[h];
    Assert(val);
    if(val)
    {
        doRemove(h);
    }
    return val;
}

void VBoxVHWAHandleTable::doPut(uint32_t h, void * data)
{
    ++mcUsage;
    mTable[h] = data;
}

void VBoxVHWAHandleTable::doRemove(uint32_t h)
{
    mTable[h] = 0;
    --mcUsage;
}

static VBoxVHWATexture* vboxVHWATextureCreate(const QRect & aRect, const VBoxVHWAColorFormat & aFormat, bool bVGA)
{
    if(!bVGA && g_GL_ARB_pixel_buffer_object)
    {
        VBOXQGLLOG(("VBoxVHWATextureNP2RectPBO\n"));
        return new VBoxVHWATextureNP2RectPBO(aRect, aFormat);
    }
    else if(g_vboxVHWAGlTextureRectangleSupported)
    {
        VBOXQGLLOG(("VBoxVHWATextureNP2Rect\n"));
        return new VBoxVHWATextureNP2Rect(aRect, aFormat);
    }
    else if(g_GL_ARB_texture_non_power_of_two)
    {
        VBOXQGLLOG(("VBoxVHWATextureNP2\n"));
        return new VBoxVHWATextureNP2(aRect, aFormat);
    }
    VBOXQGLLOG(("VBoxVHWATexture\n"));
    return new VBoxVHWATexture(aRect, aFormat);
}

class VBoxVHWAGlShader
{
public:
    VBoxVHWAGlShader(const char *aRcName, GLenum aType) :
        mShader(0),
        mRcName(aRcName),
        mType(aType)
    {}

//    virtual ~VBoxVHWAGlShader();

    int init();
//    virtual int initUniforms(class VBoxVHWAGlProgram * pProgram){}
    void uninit();
    bool isInitialized() { return mShader; }
    GLuint shader() {return mShader;}
private:
    GLuint mShader;
    const char *mRcName;
    GLenum mType;
};

int VBoxVHWAGlShader::init()
{
//    Assert(!isInitialized());
    if(isInitialized())
        return VINF_ALREADY_INITIALIZED;

    QFile fi(mRcName);
    if (!fi.open(QIODevice::ReadOnly))
    {
        Assert(0);
        return VERR_GENERAL_FAILURE;
    }

    QTextStream is(&fi);
    QString program = is.readAll();

    mShader = vboxglCreateShader(mType);
    Assert(mShader);
    if(!mShader)
        return VERR_GENERAL_FAILURE;

 //   int length = program.length();
    QByteArray asciiStr = program.toAscii();
    const char * contents = asciiStr.constData();
    GLint length = -1;

    VBOXQGL_CHECKERR(
            vboxglShaderSource(mShader, 1, &contents, &length);
            );

    VBOXQGL_CHECKERR(
            vboxglCompileShader(mShader);
            );

    GLint compiled;
    VBOXQGL_CHECKERR(
            vboxglGetShaderiv(mShader, GL_COMPILE_STATUS, &compiled);
            );

#ifdef DEBUG
    GLchar * pBuf = new GLchar[16300];
    vboxglGetShaderInfoLog(mShader, 16300, NULL, pBuf);
    VBOXQGLLOG(("compile log for shader:\n-----------\n%s\n---------\n", contents));
    VBOXQGLLOG(("%s\n**********\n", pBuf));
    delete pBuf;
#endif

    Assert(compiled);
    if(compiled)
    {
        return VINF_SUCCESS;
    }



    VBOXQGL_CHECKERR(
            vboxglDeleteShader(mShader);
            );
    mShader = 0;
    return VERR_GENERAL_FAILURE;
}

void VBoxVHWAGlShader::uninit()
{
    if(!isInitialized())
        return;

    VBOXQGL_CHECKERR(
            vboxglDeleteShader(mShader);
            );
    mShader = 0;
}

class VBoxVHWAGlProgram
{
public:
    VBoxVHWAGlProgram(VBoxVHWAGlShader ** apShaders, int acShaders);

    ~VBoxVHWAGlProgram();

    virtual int init();
    virtual void uninit();
    virtual int start();
    virtual int stop();
    bool isInitialized() { return mProgram; }
    GLuint program() {return mProgram;}
private:
    GLuint mProgram;
    VBoxVHWAGlShader ** mpShaders;
    int mcShaders;
};

VBoxVHWAGlProgram::VBoxVHWAGlProgram(VBoxVHWAGlShader ** apShaders, int acShaders) :
       mProgram(0),
       mpShaders(NULL),
       mcShaders(0)
{
    Assert(acShaders);
    if(acShaders)
    {
        mpShaders = (VBoxVHWAGlShader **)malloc(sizeof(VBoxVHWAGlShader *) * acShaders);
        memcpy(mpShaders, apShaders, sizeof(VBoxVHWAGlShader *) * acShaders);
        mcShaders = acShaders;
    }
}

VBoxVHWAGlProgram::~VBoxVHWAGlProgram()
{
    uninit();

    if(mpShaders)
    {
        free(mpShaders);
    }
}

int VBoxVHWAGlProgram::init()
{
    Assert(!isInitialized());
    if(isInitialized())
        return VINF_ALREADY_INITIALIZED;

    Assert(mcShaders);
    if(!mcShaders)
        return VERR_GENERAL_FAILURE;

    int rc = VINF_SUCCESS;
    for(int i = 0; i < mcShaders; i++)
    {
        int rc = mpShaders[i]->init();
        Assert(RT_SUCCESS(rc));
        if(RT_FAILURE(rc))
        {
            break;
        }
    }
    if(RT_FAILURE(rc))
    {
        return rc;
    }

    mProgram = vboxglCreateProgram();
    Assert(mProgram);
    if(mProgram)
    {
        for(int i = 0; i < mcShaders; i++)
        {
            VBOXQGL_CHECKERR(
                    vboxglAttachShader(mProgram, mpShaders[i]->shader());
                    );
        }

        VBOXQGL_CHECKERR(
                vboxglLinkProgram(mProgram);
                );


        GLint linked;
        vboxglGetProgramiv(mProgram, GL_LINK_STATUS, &linked);

#ifdef DEBUG
        GLchar * pBuf = new GLchar[16300];
        vboxglGetProgramInfoLog(mProgram, 16300, NULL, pBuf);
        VBOXQGLLOG(("link log: %s\n", pBuf));
        Assert(linked);
        delete pBuf;
#endif

        if(linked)
        {
            return VINF_SUCCESS;
        }

        VBOXQGL_CHECKERR(
                vboxglDeleteProgram(mProgram);
                );
        mProgram = 0;
    }
    return VERR_GENERAL_FAILURE;
}

void VBoxVHWAGlProgram::uninit()
{
    if(!isInitialized())
        return;

    VBOXQGL_CHECKERR(
            vboxglDeleteProgram(mProgram);
            );
    mProgram = 0;
}

int VBoxVHWAGlProgram::start()
{
    VBOXQGL_CHECKERR(
            vboxglUseProgram(mProgram);
            );
    return VINF_SUCCESS;
}

int VBoxVHWAGlProgram::stop()
{
    VBOXQGL_CHECKERR(
            vboxglUseProgram(0);
            );
    return VINF_SUCCESS;
}

#define VBOXVHWA_PROGRAM_DSTCOLORKEY  0x00000001
#define VBOXVHWA_PROGRAM_SRCCOLORKEY  0x00000002
#define VBOXVHWA_PROGRAM_COLORCONV    0x00000004

class VBoxVHWAGlProgramVHWA : public VBoxVHWAGlProgram
{
public:
    VBoxVHWAGlProgramVHWA(/*class VBoxVHWAGlProgramMngr *aMngr, */uint32_t type, uint32_t fourcc, VBoxVHWAGlShader ** apShaders, int acShaders);

    uint32_t type() const {return mType;}
    uint32_t fourcc() const {return mFourcc;}

    int setDstCKeyUpperRange(GLfloat r, GLfloat g, GLfloat b);

    int setDstCKeyLowerRange(GLfloat r, GLfloat g, GLfloat b);

    int setSrcCKeyUpperRange(GLfloat r, GLfloat g, GLfloat b);

    int setSrcCKeyLowerRange(GLfloat r, GLfloat g, GLfloat b);


    virtual int init();

    bool matches(uint32_t type, uint32_t fourcc)
    {
        return mType == type && mFourcc == fourcc;
    }

    bool equals(const VBoxVHWAGlProgramVHWA & other)
    {
        return matches(other.mType, other.mFourcc);
    }

private:
    uint32_t mType;
    uint32_t mFourcc;

    GLfloat mDstUpperR, mDstUpperG, mDstUpperB;
    GLint mUniDstUpperColor;

    GLfloat mDstLowerR, mDstLowerG, mDstLowerB;
    GLint mUniDstLowerColor;

    GLfloat mSrcUpperR, mSrcUpperG, mSrcUpperB;
    GLint mUniSrcUpperColor;

    GLfloat mSrcLowerR, mSrcLowerG, mSrcLowerB;
    GLint mUniSrcLowerColor;

    GLint mDstTex;
    GLint mUniDstTex;

    GLint mSrcTex;
    GLint mUniSrcTex;

    GLint mVTex;
    GLint mUniVTex;

    GLint mUTex;
    GLint mUniUTex;

//    VBoxVHWAGlProgram *mpProgram;
//
//    class VBoxVHWAGlProgramMngr *mpMngr;
};

VBoxVHWAGlProgramVHWA::VBoxVHWAGlProgramVHWA(/*VBoxVHWAGlProgramMngr *aMngr, */uint32_t type, uint32_t fourcc, VBoxVHWAGlShader ** apShaders, int acShaders) :
    VBoxVHWAGlProgram(apShaders, acShaders),
    mType(type),
    mFourcc(fourcc),
    mDstUpperR(0.0), mDstUpperG(0.0), mDstUpperB(0.0),
    mUniDstUpperColor(-1),
    mDstLowerR(0.0), mDstLowerG(0.0), mDstLowerB(0.0),
    mUniDstLowerColor(-1),
    mSrcUpperR(0.0), mSrcUpperG(0.0), mSrcUpperB(0.0),
    mUniSrcUpperColor(-1),
    mSrcLowerR(0.0), mSrcLowerG(0.0), mSrcLowerB(0.0),
    mUniSrcLowerColor(-1),
//    mpMngr(aMngr),
    mDstTex(-1),
    mUniDstTex(-1),
    mSrcTex(-1),
    mUniSrcTex(-1),
    mVTex(-1),
    mUniVTex(-1),
    mUTex(-1),
    mUniUTex(-1)
{}

int VBoxVHWAGlProgramVHWA::init()
{
    int rc = VBoxVHWAGlProgram::init();
    if(RT_FAILURE(rc))
        return rc;
    if(rc == VINF_ALREADY_INITIALIZED)
        return rc;

    start();

    rc = VERR_GENERAL_FAILURE;

    do
    {
        GLint tex = 0;
        mUniSrcTex = vboxglGetUniformLocation(program(), "uSrcTex");
        Assert(mUniSrcTex != -1);
        if(mUniSrcTex == -1)
            break;

        VBOXQGL_CHECKERR(
                vboxglUniform1i(mUniSrcTex, tex);
                );
        mSrcTex = tex;
        ++tex;

        if(type() & VBOXVHWA_PROGRAM_SRCCOLORKEY)
        {
            mUniSrcLowerColor = vboxglGetUniformLocation(program(), "uSrcClr");
            Assert(mUniSrcLowerColor != -1);
            if(mUniSrcLowerColor == -1)
                break;

            mSrcLowerR = 0.0; mSrcLowerG = 0.0; mSrcLowerB = 0.0;
            VBOXQGL_CHECKERR(
                    vboxglUniform4f(mUniSrcLowerColor, 0.0, 0.0, 0.0, 0.0);
                    );
        }

        if(type() & VBOXVHWA_PROGRAM_COLORCONV)
        {
            switch(fourcc())
            {
                case FOURCC_YV12:
                {
                    mUniVTex = vboxglGetUniformLocation(program(), "uVTex");
                    Assert(mUniVTex != -1);
                    if(mUniVTex == -1)
                        break;

                    VBOXQGL_CHECKERR(
                            vboxglUniform1i(mUniVTex, tex);
                            );
                    mVTex = tex;
                    ++tex;

                    mUniUTex = vboxglGetUniformLocation(program(), "uUTex");
                    Assert(mUniUTex != -1);
                    if(mUniUTex == -1)
                        break;
                    VBOXQGL_CHECKERR(
                            vboxglUniform1i(mUniUTex, tex);
                            );
                    mUTex = tex;
                    ++tex;

                    break;
                }
                case FOURCC_UYVY:
                case FOURCC_YUY2:
                case FOURCC_AYUV:
                    break;
                default:
                    Assert(0);
                    break;
            }
        }

        if(type() & VBOXVHWA_PROGRAM_DSTCOLORKEY)
        {

            mUniDstTex = vboxglGetUniformLocation(program(), "uDstTex");
            Assert(mUniDstTex != -1);
            if(mUniDstTex == -1)
                break;
            VBOXQGL_CHECKERR(
                    vboxglUniform1i(mUniDstTex, tex);
                    );
            mDstTex = tex;
            ++tex;

            mUniDstLowerColor = vboxglGetUniformLocation(program(), "uDstClr");
            Assert(mUniDstLowerColor != -1);
            if(mUniDstLowerColor == -1)
                break;

            mDstLowerR = 0.0; mDstLowerG = 0.0; mDstLowerB = 0.0;

            VBOXQGL_CHECKERR(
                    vboxglUniform4f(mUniDstLowerColor, 0.0, 0.0, 0.0, 0.0);
                    );
        }

        rc = VINF_SUCCESS;
    } while(0);


    stop();
    if(rc == VINF_SUCCESS)
        return VINF_SUCCESS;

    Assert(0);
    VBoxVHWAGlProgram::uninit();
    return VERR_GENERAL_FAILURE;
}

int VBoxVHWAGlProgramVHWA::setDstCKeyUpperRange(GLfloat r, GLfloat g, GLfloat b)
{
    Assert(isInitialized());
    if(!isInitialized())
        return VERR_GENERAL_FAILURE;
    if(mDstUpperR == r && mDstUpperG == g && mDstUpperB == b)
        return VINF_ALREADY_INITIALIZED;
    vboxglUniform4f(mUniDstUpperColor, r, g, b, 0.0);
    mDstUpperR = r;
    mDstUpperG = g;
    mDstUpperB = b;
    return VINF_SUCCESS;
}

int VBoxVHWAGlProgramVHWA::setDstCKeyLowerRange(GLfloat r, GLfloat g, GLfloat b)
{
    Assert(isInitialized());
    if(!isInitialized())
        return VERR_GENERAL_FAILURE;
    if(mDstLowerR == r && mDstLowerG == g && mDstLowerB == b)
        return VINF_ALREADY_INITIALIZED;

//    VBOXQGLLOG(("setDstCKeyLowerRange: r(%f), g(%f), b(%f)\n", r, g, b));
    VBOXQGL_CHECKERR(
            vboxglUniform4f(mUniDstLowerColor, r, g, b, 0.0);
            );

    mDstLowerR = r;
    mDstLowerG = g;
    mDstLowerB = b;
    return VINF_SUCCESS;
}

int VBoxVHWAGlProgramVHWA::setSrcCKeyUpperRange(GLfloat r, GLfloat g, GLfloat b)
{
    Assert(isInitialized());
    if(!isInitialized())
        return VERR_GENERAL_FAILURE;
    if(mSrcUpperR == r && mSrcUpperG == g && mSrcUpperB == b)
        return VINF_ALREADY_INITIALIZED;
    vboxglUniform4f(mUniSrcUpperColor, r, g, b, 0.0);
    mSrcUpperR = r;
    mSrcUpperG = g;
    mSrcUpperB = b;
    return VINF_SUCCESS;
}

int VBoxVHWAGlProgramVHWA::setSrcCKeyLowerRange(GLfloat r, GLfloat g, GLfloat b)
{
    Assert(isInitialized());
    if(!isInitialized())
        return VERR_GENERAL_FAILURE;
    if(mSrcLowerR == r && mSrcLowerG == g && mSrcLowerB == b)
        return VINF_ALREADY_INITIALIZED;
    VBOXQGL_CHECKERR(
            vboxglUniform4f(mUniSrcLowerColor, r, g, b, 0.0);
            );
    mSrcLowerR = r;
    mSrcLowerG = g;
    mSrcLowerB = b;
    return VINF_SUCCESS;
}

class VBoxVHWAGlProgramMngr
{
public:
    VBoxVHWAGlProgramMngr() :
        mShaderCConvApplyAYUV(":/cconvApplyAYUV.c", GL_FRAGMENT_SHADER),
        mShaderCConvAYUV(":/cconvAYUV.c", GL_FRAGMENT_SHADER),
//        mShaderCConvAYUVVoid(":/cconvAYUV_void.c", GL_FRAGMENT_SHADER),
        mShaderCConvBGR(":/cconvBGR.c", GL_FRAGMENT_SHADER),
//        mShaderCConvBGRVoid(":/cconvBGR_void.c", GL_FRAGMENT_SHADER),
        mShaderCConvUYVY(":/cconvUYVY.c", GL_FRAGMENT_SHADER),
//        mShaderCConvUYVYVoid(":/cconvUYVY_void.c", GL_FRAGMENT_SHADER),
        mShaderCConvYUY2(":/cconvYUY2.c", GL_FRAGMENT_SHADER),
//        mShaderCConvYUY2Void(":/cconvYUY2_void.c", GL_FRAGMENT_SHADER),
        mShaderCConvYV12(":/cconvYV12.c", GL_FRAGMENT_SHADER),
//        mShaderCConvYV12Void(":/cconvYV12_void.c", GL_FRAGMENT_SHADER),
        mShaderSplitBGRA(":/splitBGRA.c", GL_FRAGMENT_SHADER),
        mShaderCKeyDst(":/ckeyDst.c", GL_FRAGMENT_SHADER),
        mShaderCKeyDst2(":/ckeyDst2.c", GL_FRAGMENT_SHADER),
//        mShaderCKeyDstVoid(":/ckeyDst_void.c", GL_FRAGMENT_SHADER),
    //  mShaderCKeySrc;
    //  mShaderCKeySrcVoid;
        mShaderMainOverlay(":/mainOverlay.c", GL_FRAGMENT_SHADER),
        mShaderMainOverlayNoCKey(":/mainOverlayNoCKey.c", GL_FRAGMENT_SHADER)
    {}

    VBoxVHWAGlProgramVHWA * getProgram(bool bDstCKey, bool bSrcCKey, const VBoxVHWAColorFormat * pFrom, const VBoxVHWAColorFormat * pTo);

    void stopCurrentProgram()
    {
        VBOXQGL_CHECKERR(
            vboxglUseProgram(0);
            );
    }
private:
    VBoxVHWAGlProgramVHWA * searchProgram(uint32_t type, uint32_t fourcc, bool bCreate);

    VBoxVHWAGlProgramVHWA * createProgram(uint32_t type, uint32_t fourcc);

//    int startProgram(VBoxVHWAGlProgramVHWA * pProgram) {mCurrentProgram = pProgram; return pProgram->start();}

    typedef std::list <VBoxVHWAGlProgramVHWA*> ProgramList;

//    VBoxVHWAGlProgramVHWA * mCurrentProgram;
    ProgramList mPrograms;

    VBoxVHWAGlShader mShaderCConvApplyAYUV;

    VBoxVHWAGlShader mShaderCConvAYUV;
//    VBoxVHWAGlShader mShaderCConvAYUVVoid;
    VBoxVHWAGlShader mShaderCConvBGR;
//    VBoxVHWAGlShader mShaderCConvBGRVoid;
    VBoxVHWAGlShader mShaderCConvUYVY;
//    VBoxVHWAGlShader mShaderCConvUYVYVoid;
    VBoxVHWAGlShader mShaderCConvYUY2;
//    VBoxVHWAGlShader mShaderCConvYUY2Void;
    VBoxVHWAGlShader mShaderCConvYV12;
//    VBoxVHWAGlShader mShaderCConvYV12Void;
    VBoxVHWAGlShader mShaderSplitBGRA;

    /* expected the dst surface texture to be bound to the 1-st tex unit */
    VBoxVHWAGlShader mShaderCKeyDst;
    /* expected the dst surface texture to be bound to the 2-nd tex unit */
    VBoxVHWAGlShader mShaderCKeyDst2;
//    VBoxVHWAGlShader mShaderCKeyDstVoid;
//    VBoxVHWAGlShader mShaderCKeySrc;
//    VBoxVHWAGlShader mShaderCKeySrcVoid;

    VBoxVHWAGlShader mShaderMainOverlay;
    VBoxVHWAGlShader mShaderMainOverlayNoCKey;

    friend class VBoxVHWAGlProgramVHWA;
};

VBoxVHWAGlProgramVHWA * VBoxVHWAGlProgramMngr::createProgram(uint32_t type, uint32_t fourcc)
{
    VBoxVHWAGlShader * apShaders[16];
    uint32_t cShaders = 0;

    /* workaround for NVIDIA driver bug: ensure we attach the shader before those it is used in */
    /* reserve a slot for the mShaderCConvApplyAYUV,
     * in case it is not used the slot will be occupied by mShaderCConvBGR , which is ok */
    cShaders++;

    if(type &  VBOXVHWA_PROGRAM_DSTCOLORKEY)
    {
        if(fourcc == FOURCC_YV12)
        {
            apShaders[cShaders++] = &mShaderCKeyDst2;
        }
        else
        {
            apShaders[cShaders++] = &mShaderCKeyDst;
        }
    }
// ensure we don't have empty functions /* paranoya for for ATI on linux */
//    else
//    {
//        apShaders[cShaders++] = &mShaderCKeyDstVoid;
//    }

    if(type & VBOXVHWA_PROGRAM_SRCCOLORKEY)
    {
        Assert(0);
        /* disabled for now, not really necessary for video overlaying */
    }

    bool bFound = false;

//    if(type & VBOXVHWA_PROGRAM_COLORCONV)
    {
        if(fourcc == FOURCC_UYVY)
        {
            apShaders[cShaders++] = &mShaderCConvUYVY;
            bFound = true;
        }
        else if(fourcc == FOURCC_YUY2)
        {
            apShaders[cShaders++] = &mShaderCConvYUY2;
            bFound = true;
        }
        else if(fourcc == FOURCC_YV12)
        {
            apShaders[cShaders++] = &mShaderSplitBGRA;
            apShaders[cShaders++] = &mShaderCConvYV12;
            bFound = true;
        }
        else if(fourcc == FOURCC_AYUV)
        {
            apShaders[cShaders++] = &mShaderCConvAYUV;
            bFound = true;
        }
    }

    if(bFound)
    {
        type |= VBOXVHWA_PROGRAM_COLORCONV;
        apShaders[0] = &mShaderCConvApplyAYUV;
    }
    else
    {
        type &= (~VBOXVHWA_PROGRAM_COLORCONV);
        apShaders[0] = &mShaderCConvBGR;
    }

    if(type &  VBOXVHWA_PROGRAM_DSTCOLORKEY)
    {
        apShaders[cShaders++] = &mShaderMainOverlay;
    }
    else
    {
        // ensure we don't have empty functions /* paranoya for for ATI on linux */
        apShaders[cShaders++] = &mShaderMainOverlayNoCKey;
    }

    Assert(cShaders <= RT_ELEMENTS(apShaders));

    VBoxVHWAGlProgramVHWA *pProgram =  new VBoxVHWAGlProgramVHWA(/*this, */type, fourcc, apShaders, cShaders);
    pProgram->init();

    return pProgram;
}

VBoxVHWAGlProgramVHWA * VBoxVHWAGlProgramMngr::getProgram(bool bDstCKey, bool bSrcCKey, const VBoxVHWAColorFormat * pFrom, const VBoxVHWAColorFormat * pTo)
{
    Q_UNUSED(pTo);
    uint32_t type = 0;
    uint32_t fourcc = 0;
    if(bDstCKey)
    {
        type |= VBOXVHWA_PROGRAM_DSTCOLORKEY;
    }
    if(bSrcCKey)
    {
        type |= VBOXVHWA_PROGRAM_SRCCOLORKEY;
    }
    if(pFrom && pFrom->fourcc())
    {
        fourcc = pFrom->fourcc();
        type |= VBOXVHWA_PROGRAM_COLORCONV;
    }
    if(type)
        return searchProgram(type, fourcc, true);
    return NULL;
}

VBoxVHWAGlProgramVHWA * VBoxVHWAGlProgramMngr::searchProgram(uint32_t type, uint32_t fourcc, bool bCreate)
{
//    if(mCurrentProgram && mCurrentProgram->matches(type))
//        return mCurrentProgram;

    for (ProgramList::const_iterator it = mPrograms.begin();
         it != mPrograms.end(); ++ it)
    {
        if (!(*it)->matches(type, fourcc))
        {
            continue;
        }
        return *it;
    }
    if(bCreate)
    {
        VBoxVHWAGlProgramVHWA *pProgram = createProgram(type, fourcc);
        if(pProgram)
        {
            mPrograms.push_back(pProgram);
            return pProgram;
        }
    }
    return NULL;
}

int VBoxVHWASurfaceBase::setCKey(VBoxVHWAGlProgramVHWA * pProgram, const VBoxVHWAColorFormat * pFormat, const VBoxVHWAColorKey * pCKey, bool bDst)
{
    float r,g,b;
//    pProgram->start();
//    pFormat->pixel2Normalized(pCKey->upper(), &r, &g, &b);
//    int rcU = pProgram->setCKeyUpperRange(r, g, b);
//    Assert(RT_SUCCESS(rcU));
    pFormat->pixel2Normalized(pCKey->lower(), &r, &g, &b);
    int rcL = bDst ? pProgram->setDstCKeyLowerRange(r, g, b) : pProgram->setSrcCKeyLowerRange(r, g, b);
    Assert(RT_SUCCESS(rcL));
//    pProgram->stop();

    return RT_SUCCESS(rcL) /*&& RT_SUCCESS(rcU)*/ ? VINF_SUCCESS: VERR_GENERAL_FAILURE;
}



void VBoxVHWASurfaceBase::setAddress(uchar * addr)
{
    Assert(addr);
    if(!addr) return;
    if(addr == mAddress) return;

    if(mFreeAddress)
    {
        free(mAddress);
    }

    mAddress = addr;
    mFreeAddress = false;

    mpTex[0]->setAddress(mAddress);
    if(fourcc() == FOURCC_YV12)
    {
        uchar *pTexAddr = mAddress+mpTex[0]->memSize();
        mpTex[1]->setAddress(pTexAddr);
        pTexAddr = pTexAddr+mpTex[1]->memSize();
        mpTex[2]->setAddress(pTexAddr);
    }

//    makeCurrent();
//    updateTexture(&mRect);
    mUpdateMem2TexRect.set(mRect);
    Assert(!mUpdateMem2TexRect.isClear());
    Assert(mRect.contains(mUpdateMem2TexRect.rect()));
//    mUpdateTex2FBRect.clear();
//    Assert(mUpdateTex2FBRect.isClear());
}

void VBoxVHWASurfaceBase::globalInit()
{
    VBOXQGLLOG(("globalInit\n"));

//    glEnable(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_RECTANGLE);

    VBOXQGL_CHECKERR(
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            );
    VBOXQGL_CHECKERR(
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            );
//
//    VBOXQGL_CHECKERR(
//            vboxglActiveTexture(GL_TEXTURE1);
//        );
//    VBOXQGL_CHECKERR(
//            glEnable(GL_TEXTURE_2D);
//            );
//    VBOXQGL_CHECKERR(
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//            );
//    VBOXQGL_CHECKERR(
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//            );
//    VBOXQGL_CHECKERR(
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
//            );
//    VBOXQGL_CHECKERR(
//            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
//            );
//
//    VBOXQGL_CHECKERR(
//            vboxglActiveTexture(GL_TEXTURE0);
//        );
//    VBOXQGL_CHECKERR(
//            glEnable(GL_TEXTURE_2D);
//            );
//    VBOXQGL_CHECKERR(
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//            );
//    VBOXQGL_CHECKERR(
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//            );
//    VBOXQGL_CHECKERR(
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
//            );
//    VBOXQGL_CHECKERR(
//            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
//            );
}

VBoxVHWASurfaceBase::VBoxVHWASurfaceBase(class VBoxGLWidget *aWidget,
        const QSize & aSize,
        const QRect & aTargRect,
        const QRect & aSrcRect,
        const QRect & aVisTargRect,
        VBoxVHWAColorFormat & aColorFormat,
        VBoxVHWAColorKey * pSrcBltCKey, VBoxVHWAColorKey * pDstBltCKey,
                    VBoxVHWAColorKey * pSrcOverlayCKey, VBoxVHWAColorKey * pDstOverlayCKey,
                    bool bVGA) :
                mRect(0,0,aSize.width(),aSize.height()),
                mVisibleDisplayInitialized(false),
                mAddress(NULL),
                mColorFormat(aColorFormat),
                mpSrcBltCKey(NULL),
                mpDstBltCKey(NULL),
                mpSrcOverlayCKey(NULL),
                mpDstOverlayCKey(NULL),
                mpDefaultDstOverlayCKey(NULL),
                mpDefaultSrcOverlayCKey(NULL),
                mLockCount(0),
                mFreeAddress(false),
                mComplexList(NULL),
                mWidget(aWidget),
                mHGHandle(VBOXVHWA_SURFHANDLE_INVALID)
#ifdef DEBUG
                ,
                cFlipsCurr(0),
                cFlipsTarg(0)
#endif
{
    setDstBltCKey(pDstBltCKey);
    setSrcBltCKey(pSrcBltCKey);

    setDefaultDstOverlayCKey(pDstOverlayCKey);
    resetDefaultDstOverlayCKey();

    setDefaultSrcOverlayCKey(pSrcOverlayCKey);
    resetDefaultSrcOverlayCKey();

    mpTex[0] = vboxVHWATextureCreate(QRect(0,0,aSize.width(),aSize.height()), mColorFormat, bVGA);
    if(mColorFormat.fourcc() == FOURCC_YV12)
    {
        QRect rect(0,0,aSize.width()/2,aSize.height()/2);
        mpTex[1] = vboxVHWATextureCreate(rect, mColorFormat, bVGA);
        mpTex[2] = vboxVHWATextureCreate(rect, mColorFormat, bVGA);
    }

    doSetRectValuesInternal(aTargRect, aSrcRect, aVisTargRect);
//    mTargSize = QRect(0, 0, aTargSize->width(), aTargSize->height());

//    mBytesPerPixel = calcBytesPerPixel(mColorFormat.format(), mColorFormat.type());
//    mBytesPerLine = mRect.width() * mBytesPerPixel;
}

VBoxVHWASurfaceBase::~VBoxVHWASurfaceBase()
{
    uninit();
}

GLsizei VBoxVHWASurfaceBase::makePowerOf2(GLsizei val)
{
    int last = ASMBitLastSetS32(val);
    if(last>1)
    {
        last--;
        if((1 << last) != val)
        {
            Assert((1 << last) < val);
            val = (1 << (last+1));
        }
    }
    return val;
}

ulong VBoxVHWASurfaceBase::calcBytesPerPixel(GLenum format, GLenum type)
{
    /* we now support only common byte-aligned data */
    int numComponents = 0;
    switch(format)
    {
    case GL_COLOR_INDEX:
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_LUMINANCE:
        numComponents = 1;
        break;
    case GL_RGB:
    case GL_BGR_EXT:
        numComponents = 3;
        break;
    case GL_RGBA:
    case GL_BGRA_EXT:
        numComponents = 4;
        break;
    case GL_LUMINANCE_ALPHA:
        numComponents = 2;
        break;
    default:
        Assert(0);
        break;
    }

    int componentSize = 0;
    switch(type)
    {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
        componentSize = 1;
        break;
    //case GL_BITMAP:
    case  GL_UNSIGNED_SHORT:
    case GL_SHORT:
        componentSize = 2;
        break;
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_FLOAT:
        componentSize = 4;
        break;
    default:
        Assert(0);
        break;
    }
    return numComponents * componentSize;
}

void VBoxVHWASurfaceBase::uninit()
{
//    mState->makeCurrent(this);

    deleteDisplay();

    delete mpTex[0];
    if(fourcc() == FOURCC_YV12)
    {
        delete mpTex[1];
        delete mpTex[2];
    }

    if(mAddress && mFreeAddress)
    {
        free(mAddress);
        mAddress = NULL;
    }
}

ulong VBoxVHWASurfaceBase::memSize()
{
    ulong size = mpTex[0]->memSize();
    if(fourcc() == FOURCC_YV12)
    {
        size += mpTex[1]->memSize() + mpTex[2]->memSize();
    }
    return size;
}

void VBoxVHWASurfaceBase::init(VBoxVHWASurfaceBase * pPrimary, uchar *pvMem)
{
    if(pPrimary)
    {
        VBOXQGL_CHECKERR(
                vboxglActiveTexture(GL_TEXTURE1);
            );
    }

    int size = memSize();
    uchar * address = (uchar *)malloc(size);
#ifdef DEBUG_misha
    int tex0Size = mpTex[0]->memSize();
    if(pPrimary)
    {
        memset(address, 0xff, tex0Size);
        Assert(size >= tex0Size);
        if(size > tex0Size)
        {
            memset(address + tex0Size, 0x0, size - tex0Size);
        }
    }
    else
    {
        memset(address, 0x0f, tex0Size);
        Assert(size >= tex0Size);
        if(size > tex0Size)
        {
            memset(address + tex0Size, 0x3f, size - tex0Size);
        }
    }
#else
    memset(address, 0, size);
#endif

    mpTex[0]->init(address);
    if(fourcc() == FOURCC_YV12)
    {
        mpTex[1]->init(address);
        mpTex[2]->init(address);
    }


    if(pvMem)
    {
        mAddress = pvMem;
        free(address);
        mFreeAddress = false;

    }
    else
    {
        mAddress = address;
        mFreeAddress = true;
    }

    mpTex[0]->setAddress(mAddress);
    if(fourcc() == FOURCC_YV12)
    {
        uchar *pTexAddr = mAddress+mpTex[0]->memSize();
        mpTex[1]->setAddress(pTexAddr);
        pTexAddr = pTexAddr+mpTex[1]->memSize();
        mpTex[2]->setAddress(pTexAddr);
    }

    initDisplay(pPrimary);

    mUpdateMem2TexRect.set(mRect);
    Assert(!mUpdateMem2TexRect.isClear());
    Assert(mRect.contains(mUpdateMem2TexRect.rect()));

    if(pPrimary)
    {
        VBOXQGLLOG(("restoring to tex 0"));
        VBOXQGL_CHECKERR(
                vboxglActiveTexture(GL_TEXTURE0);
            );
    }

}

#ifdef DEBUGVHWASTRICT
bool g_DbgTest = false;
#endif

void VBoxVHWATexture::doUpdate(uchar * pAddress, const QRect * pRect)
{
#ifdef DEBUGVHWASTRICT
    if(g_DbgTest)
    {
        pAddress = (uchar*)malloc(memSize());
        uchar val = 0;
        for(uint32_t i = 0; i < memSize(); i++)
        {
            pAddress[i] = val;
            val+=64;
        }
    }
#endif

    GLenum tt = texTarget();
    if(pRect)
    {
        Assert(mRect.contains(*pRect));
    }
    else
    {
        pRect = &mRect;
    }

    Assert(glIsTexture(mTexture));
    VBOXQGL_CHECKERR(
            glBindTexture(tt, mTexture);
            );

    int x = pRect->x()/mColorFormat.widthCompression();
    int y = pRect->y()/mColorFormat.heightCompression();
    int width = pRect->width()/mColorFormat.widthCompression();
    int height = pRect->height()/mColorFormat.heightCompression();

    uchar * address = pAddress + pointOffsetTex(x, y);

    VBOXQGL_CHECKERR(
            glPixelStorei(GL_UNPACK_ROW_LENGTH, mRect.width()/mColorFormat.widthCompression());
            );

    VBOXQGL_CHECKERR(
            glTexSubImage2D(tt,
                0,
                x, y, width, height,
                mColorFormat.format(),
                mColorFormat.type(),
                address);
            );

#ifdef DEBUGVHWASTRICT
    if(g_DbgTest)
    {
        free(pAddress);
    }
#endif
}

void VBoxVHWATexture::texCoord(int x, int y)
{
    glTexCoord2f(((float)x)/mTexRect.width()/mColorFormat.widthCompression(), ((float)y)/mTexRect.height()/mColorFormat.heightCompression());
}

void VBoxVHWATexture::multiTexCoord(GLenum texUnit, int x, int y)
{
    vboxglMultiTexCoord2f(texUnit, ((float)x)/mTexRect.width()/mColorFormat.widthCompression(), ((float)y)/mTexRect.height()/mColorFormat.heightCompression());
}

void VBoxVHWATexture::uninit()
{
    if(mTexture)
    {
        glDeleteTextures(1,&mTexture);
    }
}

VBoxVHWATexture::VBoxVHWATexture(const QRect & aRect, const VBoxVHWAColorFormat &aFormat)
        : mAddress(0),
          mTexture(0)
{
    mColorFormat = aFormat;
    mRect = aRect;
    mBytesPerPixel = mColorFormat.bitsPerPixel()/8;
    mBytesPerPixelTex = mColorFormat.bitsPerPixelTex()/8;
    mBytesPerLine = mBytesPerPixel * mRect.width();
    GLsizei wdt = VBoxVHWASurfaceBase::makePowerOf2(mRect.width()/mColorFormat.widthCompression());
    GLsizei hgt = VBoxVHWASurfaceBase::makePowerOf2(mRect.height()/mColorFormat.heightCompression());
    mTexRect = QRect(0,0,wdt,hgt);
}

void VBoxVHWATexture::initParams()
{
    GLenum tt = texTarget();

    glTexParameteri(tt, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    VBOXQGL_ASSERTNOERR();
    glTexParameteri(tt, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    VBOXQGL_ASSERTNOERR();
    glTexParameteri(tt, GL_TEXTURE_WRAP_S, GL_CLAMP);
    VBOXQGL_ASSERTNOERR();

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    VBOXQGL_ASSERTNOERR();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    VBOXQGL_ASSERTNOERR();

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    VBOXQGL_ASSERTNOERR();
}

void VBoxVHWATexture::load()
{
    VBOXQGL_CHECKERR(
            glPixelStorei(GL_UNPACK_ROW_LENGTH, mTexRect.width());
            );

    VBOXQGL_CHECKERR(
        glTexImage2D(texTarget(),
                0,
                  mColorFormat.internalFormat(),
                  mTexRect.width(),
                  mTexRect.height(),
                  0,
                  mColorFormat.format(),
                  mColorFormat.type(),
                  (GLvoid *)mAddress);
        );
}

void VBoxVHWATexture::init(uchar *pvMem)
{
//    GLsizei wdt = mTexRect.width();
//    GLsizei hgt = mTexRect.height();

    VBOXQGL_CHECKERR(
            glGenTextures(1, &mTexture);
        );

    VBOXQGLLOG(("tex: %d", mTexture));

    bind();

    initParams();

    setAddress(pvMem);

    load();
}

VBoxVHWATexture::~VBoxVHWATexture()
{
    uninit();
}

void VBoxVHWATextureNP2Rect::texCoord(int x, int y)
{
    glTexCoord2f(((float)x)/mColorFormat.widthCompression(), ((float)y)/mColorFormat.heightCompression());
}

void VBoxVHWATextureNP2Rect::multiTexCoord(GLenum texUnit, int x, int y)
{
    vboxglMultiTexCoord2f(texUnit, x/mColorFormat.widthCompression(), y/mColorFormat.heightCompression());
}

GLenum VBoxVHWATextureNP2Rect::texTarget() {return GL_TEXTURE_RECTANGLE; }

bool VBoxVHWASurfaceBase::synchTexMem(const QRect * pRect)
{
    if(pRect)
    {
        Assert(mRect.contains(*pRect));
    }

    if(mUpdateMem2TexRect.isClear())
        return false;

    if(pRect && !mUpdateMem2TexRect.rect().intersects(*pRect))
        return false;

    mpTex[0]->update(&mUpdateMem2TexRect.rect());
    if(fourcc() == FOURCC_YV12)
    {
        QRect rect(mUpdateMem2TexRect.rect().x()/2, mUpdateMem2TexRect.rect().y()/2,
                mUpdateMem2TexRect.rect().width()/2, mUpdateMem2TexRect.rect().height()/2);
        mpTex[1]->update(&rect);
        mpTex[2]->update(&rect);
    }

#if 0
    mUpdateTex2FBRect.add(mUpdateMem2TexRect);
    Assert(!mUpdateTex2FBRect.isClear());
    Assert(mRect.contains(mUpdateTex2FBRect.rect()));
#endif
    mUpdateMem2TexRect.clear();
    Assert(mUpdateMem2TexRect.isClear());
//#ifdef DEBUG
//    VBOXPRINTDIF(dbgTime, ("texMem:"));
//#endif
    return true;
}

void VBoxVHWATextureNP2RectPBO::init(uchar *pvMem)
{
    VBOXQGL_CHECKERR(
            vboxglGenBuffers(1, &mPBO);
            );
    VBoxVHWATextureNP2Rect::init(pvMem);
}

void VBoxVHWATextureNP2RectPBO::doUpdate(uchar * pAddress, const QRect * pRect)
{
    Q_UNUSED(pAddress);
    Q_UNUSED(pRect);

    vboxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);

    GLvoid *buf;

    VBOXQGL_CHECKERR(
            buf = vboxglMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            );
    Assert(buf);
    if(buf)
    {
//    updateBuffer((uchar*)buf, pRect);
        memcpy(buf, mAddress, memSize());

        bool unmapped;
        VBOXQGL_CHECKERR(
                unmapped = vboxglUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                );

        Assert(unmapped);

        VBoxVHWATextureNP2Rect::doUpdate(0, &mRect);

        vboxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    else
    {
        VBOXQGLLOGREL(("failed to map PBO, trying fallback to non-PBO approach\n"));
        /* try fallback to non-PBO approach */
        vboxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        VBoxVHWATextureNP2Rect::doUpdate(pAddress, pRect);
    }
}

VBoxVHWATextureNP2RectPBO::~VBoxVHWATextureNP2RectPBO()
{
    VBOXQGL_CHECKERR(
            vboxglDeleteBuffers(1, &mPBO);
            );
}


void VBoxVHWATextureNP2RectPBO::load()
{
    VBoxVHWATextureNP2Rect::load();

    vboxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);

    vboxglBufferData(GL_PIXEL_UNPACK_BUFFER, memSize(), NULL, GL_STREAM_DRAW);

    GLvoid *buf = vboxglMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

//  updateBuffer((uchar*)buf, &mRect);
    memcpy(buf, mAddress, memSize());

    bool unmapped = vboxglUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    Assert(unmapped);

    vboxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

#if 0
void VBoxVHWASurfaceBase::synch(const QRect * aRect)
{
    synchFB(aRect);
    synchTex(aRect);
    synchMem(aRect);
}

void VBoxVHWASurfaceBase::synchFB(const QRect * pRect)
{
    Assert(isYInverted());

    if(pRect)
    {
        Assert(mRect.contains(*pRect));
    }

    synchTexMem(pRect);

    if(mUpdateTex2FBRect.isClear())
        return;

    if(pRect && !mUpdateTex2FBRect.rect().intersects(*pRect))
        return;

    mState->makeCurrent(this);

    VBOXQGL_CHECKERR(
            glBindTexture(GL_TEXTURE_2D, mTexture);
            );

    VBoxVHWAGlProgramMngr * pMngr = getGlProgramMngr();
    pMngr->stopCurrentProgram();

    doTex2FB(&mUpdateTex2FBRect.rect(), &mUpdateTex2FBRect.rect());

    mUpdateTex2FBRect.clear();
    Assert(mUpdateTex2FBRect.isClear());
}

void VBoxVHWASurfaceBase::synchMem(const QRect * pRect)
{
    if(pRect)
    {
        Assert(mRect.contains(*pRect));
    }

    if(mUpdateFB2MemRect.isClear())
        return;

    if(pRect && !mUpdateFB2MemRect.rect().intersects(*pRect))
        return;

    mState->makeYInvertedCurrent(this);
//    mState->makeCurrent(this);

    uchar * address = pointAddress(mUpdateFB2MemRect.rect().x(), mUpdateFB2MemRect.rect().y());

    VBOXQGL_CHECKERR(
            glPixelStorei(GL_PACK_ROW_LENGTH, mRect.width());
            );
    VBOXQGL_CHECKERR(
            glReadPixels(
                mUpdateFB2MemRect.rect().x(),
                mUpdateFB2MemRect.rect().y(),
                mUpdateFB2MemRect.rect().width(),
                mUpdateFB2MemRect.rect().height(),
                mColorFormat.format(),
                mColorFormat.type(),
                address);
            );

    mUpdateFB2MemRect.clear();
    Assert(mUpdateFB2TexRect.isClear());
}

int VBoxVHWASurfaceBase::performBlt(const QRect * pDstRect, VBoxVHWASurfaceBase * pSrcSurface, const QRect * pSrcRect, const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool blt)
{
//    pDstCKey = NULL;
//    pSrcCKey = NULL;

    GLuint tex = pSrcSurface->textureSynched(pSrcRect);

    if(pDstCKey)
    {
        synchTex(pDstRect);
    }

    mState->makeCurrent(this, blt);

    VBoxVHWAGlProgramMngr * pMngr = getGlProgramMngr();
    VBoxVHWAGlProgramVHWA * pProgram = pMngr->getProgram(pSrcCKey != NULL, &pSrcSurface->colorFormat(), &colorFormat());
    if(pProgram)
    {
        if(pSrcCKey != NULL)
        {
            pProgram->start();
            setCKey(pProgram, &pSrcSurface->colorFormat(), pSrcCKey);

            vboxglActiveTexture(GL_TEXTURE0);
        }
    }
    else
    {
        pMngr->stopCurrentProgram();
    }

//    if(blt)
    {
        VBOXQGL_CHECKERR(
                glBindTexture(GL_TEXTURE_2D, tex);
                );

        //TODO: setup strething params
        GLsizei wdt = pSrcSurface->mTexRect.width();
        GLsizei hgt = pSrcSurface->mTexRect.height();

        VBOXQGL_CHECKERR(
                glMatrixMode(GL_TEXTURE);
                );
        VBOXQGL_CHECKERR(
                glPushMatrix();
            );

        VBoxGLWidget::doSetupMatrix(QSize(wdt, hgt), true);
        VBOXQGL_CHECKERR(
                glMatrixMode(GL_MODELVIEW);
                );

        doTex2FB(pDstRect, pSrcRect);

        VBOXQGL_CHECKERR(
                glMatrixMode(GL_TEXTURE);
                );
        VBOXQGL_CHECKERR(
                glPopMatrix();
                );
        VBOXQGL_CHECKERR(
                glMatrixMode(GL_MODELVIEW);
                );
    }
//    else
//    {
//
//    }

    /* if dst color key */
    /* setup ckey shader */
    if(pDstCKey)
    {
        VBOXQGL_CHECKERR(
                glBindTexture(GL_TEXTURE_2D, mTexture);
                );
        pProgram = pMngr->getProgram(true, NULL, NULL);
        /* setup ckey values*/
        setCKey(pProgram, &colorFormat(), pDstCKey);
        pProgram->start();
        doTex2FB(pDstRect, pDstRect);
    }

    return VINF_SUCCESS;
}

int VBoxVHWASurfaceBase::overlay(VBoxVHWASurfaceBase * pOverlaySurface)
{
    VBOXQGLLOG(("overlay src(0x%x) ", pOverlaySurface));
    VBOXQGLLOG_QRECT("dst: ", &pOverlaySurface->mTargRect, "\n");
    VBOXQGLLOG_QRECT("src: ", &pOverlaySurface->mSrcRect,  "\n");
    VBOXQGLLOG_METHODTIME("time:");

    Assert(!pOverlaySurface->isHidden());

    if(pOverlaySurface->isHidden())
    {
        VBOXQGLLOG(("!!!hidden!!!\n"));
        return VINF_SUCCESS;
    }

    const QRect * pSrcRect = &pOverlaySurface->mSrcRect;
    const QRect * pDstRect = &pOverlaySurface->mTargRect;
    const VBoxVHWAColorKey * pSrcCKey = pOverlaySurface->srcOverlayCKey();
    /* we use src (overlay) surface to maintain overridden dst ckey info
     * to allow multiple overlays have different overridden dst keys for one primary surface */
    /* non-null dstOverlayCKey for overlay would mean the overlay surface contains the overridden
     * dst ckey value in defaultDstOverlayCKey
     * this allows the NULL to be a valid overridden value as well */
    const VBoxVHWAColorKey * pDstCKey = pOverlaySurface->dstOverlayCKey() ? pOverlaySurface->defaultDstOverlayCKey() : dstOverlayCKey();

    return performBlt(pDstRect, pOverlaySurface, pSrcRect, pDstCKey, pSrcCKey, false);
}

int VBoxVHWASurfaceBase::blt(const QRect * pDstRect, VBoxVHWASurfaceBase * pSrcSurface, const QRect * pSrcRect, const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey)
{
    if(pDstRect)
    {
        Assert(mRect.contains(*pDstRect));
    }
    else
    {
        pDstRect = &mRect;
    }

    if(pSrcRect)
    {
        Assert(pSrcSurface->mRect.contains(*pSrcRect));
    }
    else
    {
        pSrcRect = &pSrcSurface->mRect;
    }

    if(!pSrcCKey)
        pSrcCKey = pSrcSurface->srcBltCKey();
    if(!pDstCKey)
        pDstCKey = dstBltCKey();

    VBOXQGLLOG(("blt dst(0x%x), src(0x%x)", this, pSrcSurface));
    VBOXQGLLOG_QRECT("dst: ", pDstRect, "\n");
    VBOXQGLLOG_QRECT("src: ", pSrcRect, "\n");
    VBOXQGLLOG_METHODTIME("time:");
    int rc = performBlt(pDstRect, pSrcSurface, pSrcRect, pDstCKey, pSrcCKey, true);

    mUpdateFB2TexRect.add(*pDstRect);
    Assert(!mUpdateFB2TexRect.isClear());
    Assert(mRect.contains(mUpdateFB2TexRect.rect()));
//    synchTexture(pDstRect);
    mUpdateFB2MemRect.add(*pDstRect);
    Assert(!mUpdateFB2MemRect.isClear());
    Assert(mRect.contains(mUpdateFB2MemRect.rect()));

    return rc;
}
#endif
void VBoxVHWASurfaceBase::doTex2FB(const QRect * pDstRect, const QRect * pSrcRect)
{
    int tx1, ty1, tx2, ty2;
    pSrcRect->getCoords(&tx1, &ty1, &tx2, &ty2);
    int bx1, by1, bx2, by2;
    pDstRect->getCoords(&bx1, &by1, &bx2, &by2);
    tx2++; ty2++;bx2++; by2++;

#if 1
//    VBOXQGL_CHECKERR(
            VBOXQGLLOG_QRECT("texRect: ", &mpTex[0]->texRect(), "\n");
            glBegin(GL_QUADS);
//            glTexCoord2d(((double)tx1)/mpTex[0]->texRect().width(), ((double)ty1)/mpTex[0]->texRect().height());
//            glVertex2i(bx1, by1);
//            glTexCoord2d(((double)tx1)/mpTex[0]->texRect().width(), ((double)ty2)/mpTex[0]->texRect().height());
//            glVertex2i(bx1, by2);
//            glTexCoord2d(((double)tx2)/mpTex[0]->texRect().width(), ((double)ty2)/mpTex[0]->texRect().height());
//            glVertex2i(bx2, by2);
//            glTexCoord2d(((double)tx2)/mpTex[0]->texRect().width(), ((double)ty1)/mpTex[0]->texRect().height());
//            glVertex2i(bx2, by1);
            mpTex[0]->texCoord(tx1, ty1);
            glVertex2i(bx1, by1);
            mpTex[0]->texCoord(tx1, ty2);
            glVertex2i(bx1, by2);
            mpTex[0]->texCoord(tx2, ty2);
            glVertex2i(bx2, by2);
            mpTex[0]->texCoord(tx2, ty1);
            glVertex2i(bx2, by1);

            glEnd();
//            );
#else
        glBegin(GL_QUADS);
        glTexCoord2d(0.0, 0.0);
        glVertex2i(0, 0);
        glTexCoord2d(0.0, 1.0);
        glVertex2i(0, mRect.height());
        glTexCoord2d(1.0, 1.0);
        glVertex2i(mRect.width(), mRect.height());
        glTexCoord2d(1.0, 0.0);
        glVertex2i(mRect.width(), 0);
        glEnd();
#endif
}


void VBoxVHWASurfaceBase::doMultiTex2FB(const QRect * pDstRect, const QRect * pSrcRect, int cSrcTex)
{
    int tx1, ty1, tx2, ty2;
    pSrcRect->getCoords(&tx1, &ty1, &tx2, &ty2);
    int bx1, by1, bx2, by2;
    pDstRect->getCoords(&bx1, &by1, &bx2, &by2);
    tx2++; ty2++;bx2++; by2++;
    uint32_t t0width = mpTex[0]->rect().width();
    uint32_t t0height = mpTex[0]->rect().height();

//    VBOXQGL_CHECKERR(
            glBegin(GL_QUADS);
            for(int i = 0; i < cSrcTex; i++)
            {
//                vboxglMultiTexCoord2d(GL_TEXTURE0 + i, ((double)tx1)/mpTex[i]->texRect().width()/(width()/mpTex[i]->rect().width()),
//                        ((double)ty1)/mpTex[i]->texRect().height()/(height()/mpTex[i]->rect().height()));
                mpTex[i]->multiTexCoord(GL_TEXTURE0 + i, tx1/(t0width/mpTex[i]->texRect().width()), ty1/(t0height/mpTex[i]->rect().height()));
            }
            glVertex2i(bx1, by1);
            for(int i = 0; i < cSrcTex; i++)
            {
//                vboxglMultiTexCoord2d(GL_TEXTURE0 + i, ((double)tx1)/mpTex[i]->texRect().width()/(width()/mpTex[i]->rect().width()),
//                        ((double)ty2)/mpTex[i]->texRect().height()/(height()/mpTex[i]->rect().height()));
                mpTex[i]->multiTexCoord(GL_TEXTURE0 + i, tx1/(t0width/mpTex[i]->texRect().width()), ty2/(t0height/mpTex[i]->rect().height()));
            }
            glVertex2i(bx1, by2);
            for(int i = 0; i < cSrcTex; i++)
            {
//                vboxglMultiTexCoord2d(GL_TEXTURE0 + i, ((double)tx2)/mpTex[i]->texRect().width()/(width()/mpTex[i]->rect().width()),
//                        ((double)ty2)/mpTex[i]->texRect().height()/(height()/mpTex[i]->rect().height()));
                mpTex[i]->multiTexCoord(GL_TEXTURE0 + i, tx2/(t0width/mpTex[i]->texRect().width()), ty2/(t0height/mpTex[i]->rect().height()));

            }
            glVertex2i(bx2, by2);
            for(int i = 0; i < cSrcTex; i++)
            {
//                vboxglMultiTexCoord2d(GL_TEXTURE0 + i, ((double)tx2)/mpTex[i]->texRect().width()/(width()/mpTex[i]->rect().width()),
//                        ((double)ty1)/mpTex[i]->texRect().height()/(height()/mpTex[i]->rect().height()));
                mpTex[i]->multiTexCoord(GL_TEXTURE0 + i, tx2/(t0width/mpTex[i]->texRect().width()), ty1/(t0height/mpTex[i]->rect().height()));
            }
            glVertex2i(bx2, by1);
            glEnd();
//            );
}

void VBoxVHWASurfaceBase::doMultiTex2FB(const QRect * pDstRect, VBoxVHWATexture * pDstTex, const QRect * pSrcRect, int cSrcTex)
{
    int tx1, ty1, tx2, ty2;
    pSrcRect->getCoords(&tx1, &ty1, &tx2, &ty2);
    int bx1, by1, bx2, by2;
    pDstRect->getCoords(&bx1, &by1, &bx2, &by2);
    tx2++; ty2++;bx2++; by2++;
    uint32_t t0width = mpTex[0]->rect().width();
    uint32_t t0height = mpTex[0]->rect().height();

//    VBOXQGL_CHECKERR(
            glBegin(GL_QUADS);

            for(int i = 0; i < cSrcTex; i++)
            {
                mpTex[i]->multiTexCoord(GL_TEXTURE0 + i, tx1/(t0width/mpTex[i]->rect().width()), ty1/(t0height/mpTex[i]->rect().height()));
            }
            pDstTex->multiTexCoord(GL_TEXTURE0 + cSrcTex, bx1, by1);
            glVertex2i(bx1, by1);

            for(int i = 0; i < cSrcTex; i++)
            {
                mpTex[i]->multiTexCoord(GL_TEXTURE0 + i, tx1/(t0width/mpTex[i]->rect().width()), ty2/(t0height/mpTex[i]->rect().height()));
            }
            pDstTex->multiTexCoord(GL_TEXTURE0 + cSrcTex, bx1, by2);
            glVertex2i(bx1, by2);

            for(int i = 0; i < cSrcTex; i++)
            {
                mpTex[i]->multiTexCoord(GL_TEXTURE0 + i, tx2/(t0width/mpTex[i]->rect().width()), ty2/(t0height/mpTex[i]->rect().height()));
            }
            pDstTex->multiTexCoord(GL_TEXTURE0 + cSrcTex, bx2, by2);
            glVertex2i(bx2, by2);

            for(int i = 0; i < cSrcTex; i++)
            {
                mpTex[i]->multiTexCoord(GL_TEXTURE0 + i, tx2/(t0width/mpTex[i]->rect().width()), ty1/(t0height/mpTex[i]->rect().height()));
            }
            pDstTex->multiTexCoord(GL_TEXTURE0 + cSrcTex, bx2, by1);
            glVertex2i(bx2, by1);

            glEnd();
//            );

}

int VBoxVHWASurfaceBase::lock(const QRect * pRect, uint32_t flags)
{
    Q_UNUSED(flags);

    if(pRect)
    {
        Assert(mRect.contains(*pRect));
    }

    Assert(mLockCount >= 0);
    if(pRect && pRect->isEmpty())
        return VERR_GENERAL_FAILURE;
    if(mLockCount < 0)
        return VERR_GENERAL_FAILURE;

    VBOXQGLLOG(("lock (0x%x)", this));
    VBOXQGLLOG_QRECT("rect: ", pRect ? pRect : &mRect, "\n");
    VBOXQGLLOG_METHODTIME("time ");
//    if(!(flags & VBOXVHWA_LOCK_DISCARDCONTENTS))
//    {
//        synchMem(pRect);
//    }

    mUpdateMem2TexRect.add(pRect ? *pRect : mRect);

    Assert(!mUpdateMem2TexRect.isClear());
    Assert(mRect.contains(mUpdateMem2TexRect.rect()));
    return VINF_SUCCESS;
}

int VBoxVHWASurfaceBase::unlock()
{
    VBOXQGLLOG(("unlock (0x%x)\n", this));
    mLockCount = 0;
    return VINF_SUCCESS;
}

void VBoxVHWASurfaceBase::doSetRectValuesInternal(const QRect & aTargRect, const QRect & aSrcRect, const QRect & aVisTargRect)
{
    mVisibleTargRect = aVisTargRect.intersected(aTargRect);
    mTargRect = aTargRect;
    mSrcRect = aSrcRect;
    if(mVisibleTargRect.isEmpty() || mTargRect.isEmpty())
    {
        mVisibleSrcRect.setSize(QSize(0, 0));
    }
    else
    {
        float stretchX = float(mSrcRect.width()) / mTargRect.width();
        float stretchY = float(mSrcRect.height()) / mTargRect.height();
        int tx1, tx2, ty1, ty2, vtx1, vtx2, vty1, vty2;
        int sx1, sx2, sy1, sy2;
        mVisibleTargRect.getCoords(&vtx1, &vty1, &vtx2, &vty2);
        mTargRect.getCoords(&tx1, &ty1, &tx2, &ty2);
        mSrcRect.getCoords(&sx1, &sy1, &sx2, &sy2);
        int dx1 = vtx1 - tx1;
        int dy1 = vty1 - ty1;
        int dx2 = vtx2 - tx2;
        int dy2 = vty2 - ty2;
        int vsx1, vsy1, vsx2, vsy2;
        Assert(dx1 >= 0);
        Assert(dy1 >= 0);
        Assert(dx2 <= 0);
        Assert(dy2 <= 0);
        vsx1 = sx1 + int(dx1*stretchX);
        vsy1 = sy1 + int(dy1*stretchY);
        vsx2 = sx2 + int(dx2*stretchX);
        vsy2 = sy2 + int(dy2*stretchY);
        mVisibleSrcRect.setCoords(vsx1, vsy1, vsx2, vsy2);
        Assert(!mVisibleSrcRect.isEmpty());
        Assert(mSrcRect.contains(mVisibleSrcRect));
    }
}

void VBoxVHWASurfaceBase::setRects(VBoxVHWASurfaceBase *pPrimary, const QRect & aTargRect, const QRect & aSrcRect, const QRect & aVisTargRect, bool bForceReinit)
{
    QRect aVisibleTargRect = aVisTargRect.intersected(mTargRect);

    if(mTargRect != aTargRect || mSrcRect != aSrcRect || mVisibleTargRect != aVisibleTargRect)
    {
        doSetRectValuesInternal(aTargRect, aSrcRect, aVisTargRect);
        bForceReinit = true;
    }

    if(bForceReinit)
    {
        initDisplay(pPrimary);
    }
}

void VBoxVHWASurfaceBase::setTargRectPosition(VBoxVHWASurfaceBase *pPrimary, const QPoint & aPoint, const QRect & aVisibleTargRect)
{
    QRect tRect = targRect();
    tRect.moveTopLeft(aPoint);
    setRects(pPrimary, tRect, srcRect(), aVisibleTargRect, false);
}

void VBoxVHWASurfaceBase::updateVisibleTargRect(VBoxVHWASurfaceBase *pPrimary, const QRect & aVisibleTargRect)
{
    setRects(pPrimary, targRect(), srcRect(), aVisibleTargRect, false);
}

void VBoxVHWASurfaceBase::deleteDisplay(
//        bool bInverted
        )
{
    {
        if(mVisibleDisplayInitialized)
        {
            glDeleteLists(mVisibleDisplay, 1);
            mVisibleDisplayInitialized = false;
        }
    }
}

void VBoxVHWASurfaceBase::doDisplay(VBoxVHWASurfaceBase *pPrimary, VBoxVHWAGlProgramVHWA * pProgram, bool bBindDst)
{
    bool bInvokeMultiTex2 = false;

    if(pProgram)
    {
        pProgram->start();

//            if(pSrcCKey != NULL)
//            {
//                pProgram->start();
//                setCKey(pProgram, &pSrcSurface->colorFormat(), pSrcCKey);

//                vboxglActiveTexture(GL_TEXTURE0);
//            }

        if(bBindDst)
        {
            if(fourcc() == FOURCC_YV12)
            {
                vboxglActiveTexture(GL_TEXTURE1);
                mpTex[1]->bind();
                vboxglActiveTexture(GL_TEXTURE1+1);
                mpTex[2]->bind();

                vboxglActiveTexture(GL_TEXTURE1+2);
            }
            else
            {
                vboxglActiveTexture(GL_TEXTURE1);
            }
            pPrimary->mpTex[0]->bind();

            vboxglActiveTexture(GL_TEXTURE0);
            mpTex[0]->bind();
            bInvokeMultiTex2 = true;
        }
        else
        {
            if(fourcc() == FOURCC_YV12)
            {
                vboxglActiveTexture(GL_TEXTURE1);
                mpTex[1]->bind();
                vboxglActiveTexture(GL_TEXTURE0);
            }
            mpTex[0]->bind();
        }
    }
    else
    {
//        vboxglActiveTexture(GL_TEXTURE0);
        mpTex[0]->bind();
//        VBOXQGLLOG(("binding (primary??) texture: %d\n", mpTex[0]->texture()));
    }

    if(bInvokeMultiTex2)
    {
        doMultiTex2FB(&mVisibleTargRect, pPrimary->mpTex[0], &mVisibleSrcRect,
                (fourcc() == FOURCC_YV12) ? 2 : 1);
    }
    else
    {
        if(fourcc() == FOURCC_YV12)
        {
            doMultiTex2FB(&mVisibleTargRect, &mVisibleSrcRect, 2);
        }
        else
        {
            VBOXQGLLOG_QRECT("mVisibleTargRect: ", &mVisibleTargRect, "\n");
            VBOXQGLLOG_QRECT("mVisibleSrcRect: ", &mVisibleSrcRect, "\n");
            doTex2FB(&mVisibleTargRect, &mVisibleSrcRect);
        }
    }

    if(pProgram)
    {
        pProgram->stop();
    }
}

GLuint VBoxVHWASurfaceBase::createDisplay(VBoxVHWASurfaceBase *pPrimary)
{
    if(mVisibleTargRect.isEmpty())
    {
        Assert(mVisibleSrcRect.isEmpty());
        return 0;
    }
    Assert(!mVisibleSrcRect.isEmpty());
    /* just for the fallback */
    if(mVisibleSrcRect.isEmpty())
    {
        return 0;
    }

    VBoxVHWAGlProgramVHWA * pProgram = NULL;
    const VBoxVHWAColorKey * pSrcCKey = NULL, *pDstCKey = NULL;
    if(pPrimary)
    {
        pSrcCKey = getActiveSrcOverlayCKey();
        /* we use src (overlay) surface to maintain overridden dst ckey info
         * to allow multiple overlays have different overridden dst keys for one primary surface */
        /* non-null dstOverlayCKey for overlay would mean the overlay surface contains the overridden
         * dst ckey value in defaultDstOverlayCKey
         * this allows the NULL to be a valid overridden value as well */
        pDstCKey = getActiveDstOverlayCKey(pPrimary);
//        pSrcCKey = NULL;
//        pDstCKey = NULL;

        pProgram = mWidget->vboxVHWAGetGlProgramMngr()->getProgram(pDstCKey != NULL, pSrcCKey != NULL, &colorFormat(), &pPrimary->colorFormat());
    }

    GLuint display = glGenLists(1);
    VBOXQGL_ASSERTNOERR();
    glNewList(display, GL_COMPILE);

    doDisplay(pPrimary, pProgram, pDstCKey != NULL);

    glEndList();
    VBOXQGL_ASSERTNOERR();

    return display;
}

void VBoxVHWASurfaceBase::initDisplay(VBoxVHWASurfaceBase *pPrimary)
{
    deleteDisplay();

    mVisibleDisplay = createDisplay(pPrimary);
    mVisibleDisplayInitialized = true;
}

void VBoxVHWASurfaceBase::updatedMem(const QRect *rec)
{
    if(rec)
    {
        Assert(mRect.contains(*rec));
    }
    mUpdateMem2TexRect.add(*rec);
}

//void VBoxVHWASurfaceBase::setVisibleTargetRect(const QRect & aRect)
//{
//    Assert(mVisibleRect.contains(aRect));
//    mVisibleRect = mSrcRect.intersected(aRect);
//}

bool VBoxVHWASurfaceBase::performDisplay(VBoxVHWASurfaceBase *pPrimary, bool bForce)
{
    Assert(mVisibleDisplayInitialized);
    if(mVisibleDisplay == 0)
    {
        /* nothing to display, i.e. the surface is not visible,
         * in the sense that it's located behind the viewport ranges */
        Assert(mVisibleSrcRect.isEmpty());
        Assert(mVisibleTargRect.isEmpty());
        return false;
    }
    else
    {
        Assert(!mVisibleSrcRect.isEmpty());
        Assert(!mVisibleTargRect.isEmpty());
    }

    bForce |= synchTexMem(&mVisibleSrcRect);
    if(pPrimary && getActiveDstOverlayCKey(pPrimary))
    {
        bForce |= pPrimary->synchTexMem(&mVisibleTargRect);
    }

    if(!bForce)
        return false;

#ifdef DEBUG_misha
    if(0)
    {
        VBoxVHWAGlProgramVHWA * pProgram = NULL;
        const VBoxVHWAColorKey * pSrcCKey = NULL, *pDstCKey = NULL;
        if(pPrimary)
        {
            pSrcCKey = getActiveSrcOverlayCKey();
            /* we use src (overlay) surface to maintain overridden dst ckey info
             * to allow multiple overlays have different overridden dst keys for one primary surface */
            /* non-null dstOverlayCKey for overlay would mean the overlay surface contains the overridden
             * dst ckey value in defaultDstOverlayCKey
             * this allows the NULL to be a valid overridden value as well */
            pDstCKey = getActiveDstOverlayCKey(pPrimary);
    //        pSrcCKey = NULL;
    //        pDstCKey = NULL;

            pProgram = mWidget->vboxVHWAGetGlProgramMngr()->getProgram(pDstCKey != NULL, pSrcCKey != NULL, &colorFormat(), &pPrimary->colorFormat());
        }

        doDisplay(pPrimary, pProgram, pDstCKey != NULL);

//        doDisplay(pPrimary, NULL, false);
    }
    else
#endif
    {
        VBOXQGL_CHECKERR(
                glCallList(mVisibleDisplay);
                );
    }

    Assert(bForce);
    return true;
}


VBoxGLWidget::VBoxGLWidget (VBoxConsoleView *aView, QWidget *aParent)
    : QGLWidget (VBoxGLWidget::vboxGLFormat(), aParent),
    mSurfHandleTable(128), /* 128 should be enough */
    mpfnOp(NULL),
    mOpContext(NULL),
    mPixelFormat(0),
    mUsesGuestVRAM(false),
    mRepaintNeeded(false),
//    mbVGASurfCreated(false),
    mView(aView),
    mConstructingList(NULL),
    mcRemaining2Contruct(0)
{
    mpMngr = new VBoxVHWAGlProgramMngr();
//        /* No need for background drawing */
//        setAttribute (Qt::WA_OpaquePaintEvent);
}

const QGLFormat & VBoxGLWidget::vboxGLFormat()
{
    static QGLFormat vboxFormat = QGLFormat();
    vboxFormat.setAlpha(true);
    Assert(vboxFormat.alpha());
    vboxFormat.setSwapInterval(0);
    Assert(vboxFormat.swapInterval() == 0);
    vboxFormat.setAccum(false);
    Assert(!vboxFormat.accum());
    vboxFormat.setDepth(false);
    Assert(!vboxFormat.depth());
    return vboxFormat;
}

VBoxGLWidget::~VBoxGLWidget()
{
    delete mpMngr;
}


void VBoxGLWidget::doSetupMatrix(const QSize & aSize, bool bInverted)
{
    VBOXQGL_CHECKERR(
            glLoadIdentity();
            );
    if(bInverted)
    {
        VBOXQGL_CHECKERR(
                glScalef(1.0f/aSize.width(), 1.0f/aSize.height(), 1.0f);
                );
    }
    else
    {
        /* make display coordinates be scaled to pixel coordinates */
        VBOXQGL_CHECKERR(
                glTranslatef(0.0f, 1.0f, 0.0f);
                );
        VBOXQGL_CHECKERR(
                glScalef(1.0f/aSize.width(), 1.0f/aSize.height(), 1.0f);
                );
        VBOXQGL_CHECKERR(
                glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
                );
    }
}
void VBoxGLWidget::adjustViewport(const QSize &display, const QRect &viewport)
{
    /* viewport:  (viewport.x;viewport.y) (viewport.width;viewport.height)*/
    glViewport(-((int)display.width() + viewport.x()),
                -((int)display.height() - viewport.y() + display.height() - viewport.height()),
                2*display.width(),
                2*display.height());
}

void VBoxGLWidget::setupMatricies(const QSize &display)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(0., (GLdouble)display.width(), 0., (GLdouble)display.height(), 0., 0.);

    glMatrixMode(GL_MODELVIEW);
    //    doSetupMatrix(bInverted ? &mRect.size() : &mTargSize.size(), bInverted);
    doSetupMatrix(display, false);
}

VBoxVHWACommandElement * VBoxGLWidget::processCmdList(VBoxVHWACommandElement * pCmd)
{
    VBoxVHWACommandElement * pCur;
    do
    {
        pCur = pCmd;
        switch(pCmd->type())
        {
        case VBOXVHWA_PIPECMD_PAINT:
            vboxDoUpdateRect(&pCmd->rect());
            break;
#ifdef VBOX_WITH_VIDEOHWACCEL
        case VBOXVHWA_PIPECMD_VHWA:
            vboxDoVHWACmd(pCmd->vhwaCmd());
            break;
        case VBOXVHWA_PIPECMD_OP:
        {
            const VBOXVHWACALLBACKINFO & info = pCmd->op();
            (info.pThis->*(info.pfnCallback))(info.pContext);
            break;
        }
        case VBOXVHWA_PIPECMD_FUNC:
        {
            const VBOXVHWAFUNCCALLBACKINFO & info = pCmd->func();
            info.pfnCallback(info.pContext1, info.pContext2);
            break;
        }
#endif
        default:
            Assert(0);
        }
        pCmd = pCmd->mpNext;
    } while(pCmd);

    return pCur;
}

void VBoxGLWidget::vboxDoProcessVHWACommands(void *pContext)
{
    VBoxVHWACommandElementProcessor * pPipe = (VBoxVHWACommandElementProcessor*)pContext;
    VBoxVHWACommandElement * pFirst = pPipe->detachCmdList(NULL, NULL);
    do
    {
        VBoxVHWACommandElement * pLast = processCmdList(pFirst);

        pFirst = pPipe->detachCmdList(pFirst, pLast);
    } while(pFirst);

//    mDisplay.performDisplay();
}

#ifdef VBOX_WITH_VIDEOHWACCEL
void VBoxGLWidget::vboxDoVHWACmd(void *cmd)
{
    vboxDoVHWACmdExec(cmd);

    CDisplay display = mView->console().GetDisplay();
    Assert (!display.isNull());

    display.CompleteVHWACommand((BYTE*)cmd);
}

void VBoxGLWidget::vboxDoVHWACmdAndFree(void *cmd)
{
    vboxDoVHWACmdExec(cmd);

    free(cmd);
}

void VBoxGLWidget::vboxDoVHWACmdExec(void *cmd)
{
    struct _VBOXVHWACMD * pCmd = (struct _VBOXVHWACMD *)cmd;
    switch(pCmd->enmCmd)
    {
        case VBOXVHWACMD_TYPE_SURF_CANCREATE:
        {
            VBOXVHWACMD_SURF_CANCREATE * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_CANCREATE);
            pCmd->rc = vhwaSurfaceCanCreate(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_CREATE:
        {
            VBOXVHWACMD_SURF_CREATE * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_CREATE);
            pCmd->rc = vhwaSurfaceCreate(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_DESTROY:
        {
            VBOXVHWACMD_SURF_DESTROY * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_DESTROY);
            pCmd->rc = vhwaSurfaceDestroy(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_LOCK:
        {
            VBOXVHWACMD_SURF_LOCK * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_LOCK);
            pCmd->rc = vhwaSurfaceLock(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_UNLOCK:
        {
            VBOXVHWACMD_SURF_UNLOCK * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_UNLOCK);
            pCmd->rc = vhwaSurfaceUnlock(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_BLT:
        {
            VBOXVHWACMD_SURF_BLT * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_BLT);
            pCmd->rc = vhwaSurfaceBlt(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_FLIP:
        {
            VBOXVHWACMD_SURF_FLIP * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_FLIP);
            pCmd->rc = vhwaSurfaceFlip(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE:
        {
            VBOXVHWACMD_SURF_OVERLAY_UPDATE * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_OVERLAY_UPDATE);
            pCmd->rc = vhwaSurfaceOverlayUpdate(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_OVERLAY_SETPOSITION:
        {
            VBOXVHWACMD_SURF_OVERLAY_SETPOSITION * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_OVERLAY_SETPOSITION);
            pCmd->rc = vhwaSurfaceOverlaySetPosition(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_COLORKEY_SET:
        {
            VBOXVHWACMD_SURF_COLORKEY_SET * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_COLORKEY_SET);
            pCmd->rc = vhwaSurfaceColorkeySet(pBody);
        } break;
        case VBOXVHWACMD_TYPE_QUERY_INFO1:
        {
            VBOXVHWACMD_QUERYINFO1 * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO1);
            pCmd->rc = vhwaQueryInfo1(pBody);
        } break;
        case VBOXVHWACMD_TYPE_QUERY_INFO2:
        {
            VBOXVHWACMD_QUERYINFO2 * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO2);
            pCmd->rc = vhwaQueryInfo2(pBody);
        } break;
        case VBOXVHWACMD_TYPE_ENABLE:
        case VBOXVHWACMD_TYPE_DISABLE:
            pCmd->rc = VINF_SUCCESS;
            break;
        case VBOXVHWACMD_TYPE_HH_CONSTRUCT:
        {
            VBOXVHWACMD_HH_CONSTRUCT * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_HH_CONSTRUCT);
            pCmd->rc = vhwaConstruct(pBody);
        } break;
        default:
            Assert(0);
            pCmd->rc = VERR_NOT_IMPLEMENTED;
            break;
    }
}

int VBoxGLWidget::vhwaSurfaceCanCreate(struct _VBOXVHWACMD_SURF_CANCREATE *pCmd)
{
    VBOXQGLLOG_ENTER(("\n"));

    if(!(pCmd->SurfInfo.flags & VBOXVHWA_SD_CAPS))
    {
        Assert(0);
        pCmd->u.out.ErrInfo = -1;
        return VINF_SUCCESS;
    }

    if(pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_OFFSCREENPLAIN)
    {
#ifdef DEBUGVHWASTRICT
        Assert(0);
#endif
        pCmd->u.out.ErrInfo = -1;
        return VINF_SUCCESS;
    }

    if(pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_PRIMARYSURFACE)
    {
        pCmd->u.out.ErrInfo = 0;
        return VINF_SUCCESS;
    }


    Assert(/*pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_OFFSCREENPLAIN
            || */ pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_OVERLAY);

    if(pCmd->u.in.bIsDifferentPixelFormat)
    {
        if(!(pCmd->SurfInfo.flags & VBOXVHWA_SD_PIXELFORMAT))
        {
            Assert(0);
            pCmd->u.out.ErrInfo = -1;
            return VINF_SUCCESS;
        }

        if(pCmd->SurfInfo.PixelFormat.flags & VBOXVHWA_PF_RGB)
        {
            if(pCmd->SurfInfo.PixelFormat.c.rgbBitCount != 32
                    || pCmd->SurfInfo.PixelFormat.c.rgbBitCount != 24)
            {
                Assert(0);
                pCmd->u.out.ErrInfo = -1;
                return VINF_SUCCESS;
            }
        }
        else if(pCmd->SurfInfo.PixelFormat.flags & VBOXVHWA_PF_FOURCC)
        {
            /* detect whether we support this format */
            bool bFound = false;
            for(uint32_t i = 0; i < g_vboxVHWAFourccSupportedCount; i++)
            {
                if(g_vboxVHWAFourccSupportedList[i] == pCmd->SurfInfo.PixelFormat.fourCC)
                {
                    bFound = true;
                    break;
                }
            }
            Assert(bFound);
            if(!bFound)
            {
                Assert(0);
                pCmd->u.out.ErrInfo = -1;
                return VINF_SUCCESS;
            }
        }
        else
        {
            Assert(0);
            pCmd->u.out.ErrInfo = -1;
            return VINF_SUCCESS;
        }
    }

    pCmd->u.out.ErrInfo = 0;
    return VINF_SUCCESS;
}

int VBoxGLWidget::vhwaSurfaceCreate(struct _VBOXVHWACMD_SURF_CREATE *pCmd)
{
    VBOXQGLLOG_ENTER(("\n"));

    uint32_t handle = VBOXVHWA_SURFHANDLE_INVALID;
    if(pCmd->SurfInfo.hSurf != VBOXVHWA_SURFHANDLE_INVALID)
    {
        handle = pCmd->SurfInfo.hSurf;
        if(mSurfHandleTable.get(handle))
        {
//            do
//            {
//                if(!mcVGASurfCreated)
//                {
//                    /* check if it is a primary surface that needs handle adjusting*/
//                    if((pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_PRIMARYSURFACE)
//                            && handle2Surface(handle) == mDisplay.getVGA())
//                    {
//                        /* remove, the primary surface will be assigned to a new handle assumed by the guest */
//                        mSurfHandleTable.remove(handle);
//                        break;
//                    }
//                }
                Assert(0);
                return VERR_GENERAL_FAILURE;
//            }while(0);
        }
    }

    VBoxVHWASurfaceBase *surf = NULL;
//    VBoxVHWAColorFormat *pFormat = NULL, Format;
    bool bPrimary = false;

    VBoxVHWAColorKey *pDstBltCKey = NULL, DstBltCKey;
    VBoxVHWAColorKey *pSrcBltCKey = NULL, SrcBltCKey;
    VBoxVHWAColorKey *pDstOverlayCKey = NULL, DstOverlayCKey;
    VBoxVHWAColorKey *pSrcOverlayCKey = NULL, SrcOverlayCKey;
    if(pCmd->SurfInfo.flags & VBOXVHWA_SD_CKDESTBLT)
    {
        DstBltCKey = VBoxVHWAColorKey(pCmd->SurfInfo.DstBltCK.high, pCmd->SurfInfo.DstBltCK.low);
        pDstBltCKey = &DstBltCKey;
    }
    if(pCmd->SurfInfo.flags & VBOXVHWA_SD_CKSRCBLT)
    {
        SrcBltCKey = VBoxVHWAColorKey(pCmd->SurfInfo.SrcBltCK.high, pCmd->SurfInfo.SrcBltCK.low);
        pSrcBltCKey = &SrcBltCKey;
    }
    if(pCmd->SurfInfo.flags & VBOXVHWA_SD_CKDESTOVERLAY)
    {
        DstOverlayCKey = VBoxVHWAColorKey(pCmd->SurfInfo.DstOverlayCK.high, pCmd->SurfInfo.DstOverlayCK.low);
        pDstOverlayCKey = &DstOverlayCKey;
    }
    if(pCmd->SurfInfo.flags & VBOXVHWA_SD_CKSRCOVERLAY)
    {
        SrcOverlayCKey = VBoxVHWAColorKey(pCmd->SurfInfo.SrcOverlayCK.high, pCmd->SurfInfo.SrcOverlayCK.low);
        pSrcOverlayCKey = &SrcOverlayCKey;
    }

    if(pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_PRIMARYSURFACE)
    {
        bPrimary = true;
        VBoxVHWASurfaceBase * pVga = vboxGetVGASurface();

        if(pVga->handle() == VBOXVHWA_SURFHANDLE_INVALID)
        {
            Assert(pCmd->SurfInfo.PixelFormat.flags & VBOXVHWA_PF_RGB);
//            if(pCmd->SurfInfo.PixelFormat.flags & VBOXVHWA_PF_RGB)
            {
                Assert(pCmd->SurfInfo.width == pVga->width());
                Assert(pCmd->SurfInfo.height == pVga->height());
//                if(pCmd->SurfInfo.width == pVga->width()
//                        && pCmd->SurfInfo.height == pVga->height())
                {
                    VBoxVHWAColorFormat format(pCmd->SurfInfo.PixelFormat.c.rgbBitCount,
                                                pCmd->SurfInfo.PixelFormat.m1.rgbRBitMask,
                                                pCmd->SurfInfo.PixelFormat.m2.rgbGBitMask,
                                                pCmd->SurfInfo.PixelFormat.m3.rgbBBitMask);
                    Assert(pVga->colorFormat().equals(format));
//                    if(pVga->colorFormat().equals(format))
                    {
                        surf = pVga;

                        surf->setDstBltCKey(pDstBltCKey);
                        surf->setSrcBltCKey(pSrcBltCKey);

                        surf->setDefaultDstOverlayCKey(pDstOverlayCKey);
                        surf->resetDefaultDstOverlayCKey();

                        surf->setDefaultSrcOverlayCKey(pDstOverlayCKey);
                        surf->resetDefaultSrcOverlayCKey();
//                        mbVGASurfCreated = true;
                    }
                }
            }
        }
    }

    if(!surf)
    {
        if(pCmd->SurfInfo.PixelFormat.flags & VBOXVHWA_PF_RGB)
        {
            VBoxVHWAColorFormat format(pCmd->SurfInfo.PixelFormat.c.rgbBitCount,
                                            pCmd->SurfInfo.PixelFormat.m1.rgbRBitMask,
                                            pCmd->SurfInfo.PixelFormat.m2.rgbGBitMask,
                                            pCmd->SurfInfo.PixelFormat.m3.rgbBBitMask);
            QSize surfSize(pCmd->SurfInfo.width, pCmd->SurfInfo.height);
            QRect primaryRect = mDisplay.getPrimary()->rect();
            surf = new VBoxVHWASurfaceBase(this, surfSize,
//                        ((pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_OVERLAY) ? mDisplay.getPrimary()->rect().size() : &surfSize),
                        primaryRect,
                        QRect(0, 0, surfSize.width(), surfSize.height()),
                        mViewport,
                        format,
                        pSrcBltCKey, pDstBltCKey, pSrcOverlayCKey, pDstOverlayCKey,
                        bPrimary);
        }
        else if(pCmd->SurfInfo.PixelFormat.flags & VBOXVHWA_PF_FOURCC)
        {
            QSize surfSize(pCmd->SurfInfo.width, pCmd->SurfInfo.height);
            QRect primaryRect = mDisplay.getPrimary()->rect();

            VBoxVHWAColorFormat format(pCmd->SurfInfo.PixelFormat.fourCC);
            surf = new VBoxVHWASurfaceBase(this, surfSize,
            //                        ((pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_OVERLAY) ? mDisplay.getPrimary()->rect().size() : &QSize(pCmd->SurfInfo.width, pCmd->SurfInfo.height)),
                                    primaryRect,
                                    QRect(0, 0, surfSize.width(), surfSize.height()),
                                    mViewport,
                                    format,
                                    pSrcBltCKey, pDstBltCKey, pSrcOverlayCKey, pDstOverlayCKey,
                                    bPrimary);
        }
        else
        {
            Assert(0);
            VBOXQGLLOG_EXIT(("pSurf (0x%x)\n",surf));
            return VERR_GENERAL_FAILURE;
        }

        uchar * addr = vboxVRAMAddressFromOffset(pCmd->SurfInfo.offSurface);
        surf->init(mDisplay.getPrimary(), addr);

        if(pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_OVERLAY)
        {
            Assert(!bPrimary);

            if(!mConstructingList)
            {
                mConstructingList = new VBoxVHWASurfList();
                mcRemaining2Contruct = pCmd->SurfInfo.cBackBuffers+1;
                mDisplay.addOverlay(mConstructingList);
            }

            mConstructingList->add(surf);
            mcRemaining2Contruct--;
            if(!mcRemaining2Contruct)
            {
                mConstructingList = NULL;
            }
        }
        else if(bPrimary)
        {
            VBoxVHWASurfaceBase * pVga = vboxGetVGASurface();
            Assert(pVga->handle() != VBOXVHWA_SURFHANDLE_INVALID);
            Assert(pVga != surf);
//            Assert(mbVGASurfCreated);
            mDisplay.getVGA()->getComplexList()->add(surf);
#ifdef DEBUGVHWASTRICT
            Assert(pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_VISIBLE);
#endif
//            if(pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_VISIBLE)
            {
                Assert(surf->getComplexList() == mDisplay.getVGA()->getComplexList());
                surf->getComplexList()->setCurrentVisible(surf);
                mDisplay.updateVGA(surf);
            }
        }
        else if(pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_COMPLEX)
        {
            //TODO: impl fullscreen mode support
            Assert(0);
        }
        else
        {
            Assert(0);
        }
    }

    Assert(mDisplay.getVGA() == mDisplay.getPrimary());

    /* tell the guest how we think the memory is organized */
    VBOXQGLLOG(("bps: %d\n", surf->bitsPerPixel()));

    pCmd->SurfInfo.pitch        = surf->bitsPerPixel() * surf->width() / 8;
    pCmd->SurfInfo.sizeX = surf->memSize();
    pCmd->SurfInfo.sizeY = 1;

    if(handle != VBOXVHWA_SURFHANDLE_INVALID)
    {
        bool bSuccess = mSurfHandleTable.mapPut(handle, surf);
        Assert(bSuccess);
        if(!bSuccess)
        {
            /* @todo: this is very bad, should not be here */
            return VERR_GENERAL_FAILURE;
        }
    }
    else
    {
        /* tell the guest our handle */
        handle = mSurfHandleTable.put(surf);
        pCmd->SurfInfo.hSurf = (VBOXVHWA_SURFHANDLE)handle;
    }

    Assert(handle != VBOXVHWA_SURFHANDLE_INVALID);
    Assert(surf->handle() == VBOXVHWA_SURFHANDLE_INVALID);
    surf->setHandle(handle);
    Assert(surf->handle() == handle);

    VBOXQGLLOG_EXIT(("pSurf (0x%x)\n",surf));

    return VINF_SUCCESS;
}

int VBoxGLWidget::vhwaSurfaceDestroy(struct _VBOXVHWACMD_SURF_DESTROY *pCmd)
{
    VBoxVHWASurfaceBase *pSurf = handle2Surface(pCmd->u.in.hSurf);
    VBoxVHWASurfList *pList = pSurf->getComplexList();
    Assert(pSurf->handle() != VBOXVHWA_SURFHANDLE_INVALID);

    VBOXQGLLOG_ENTER(("pSurf (0x%x)\n",pSurf));
    if(pList != mDisplay.getVGA()->getComplexList())
    {
        Assert(pList);
        pList->remove(pSurf);
        if(pList->surfaces().empty())
        {
            mDisplay.removeOverlay(pList);
//                Assert(mConstructingList != pList);
            if(pList == mConstructingList)
            {
                mConstructingList = NULL;
                mcRemaining2Contruct = 0;
            }
            delete pList;
        }

        delete(pSurf);
    }
    else
    {
        Assert(pList->size() >= 1);
        if(pList->size() > 1)
        {
            if(pSurf == mDisplay.getVGA())
            {
                const SurfList & surfaces = pList->surfaces();

                for (SurfList::const_iterator it = surfaces.begin();
                         it != surfaces.end(); ++ it)
                {
                    VBoxVHWASurfaceBase *pCurSurf = (*it);
                    if(pCurSurf != pSurf)
                    {
                        mDisplay.updateVGA(pCurSurf);
                        pList->setCurrentVisible(pCurSurf);
                        break;
                    }
                }
            }

            pList->remove(pSurf);
            delete(pSurf);
        }
        else
        {
            pSurf->setHandle(VBOXVHWA_SURFHANDLE_INVALID);
        }
    }

    /* just in case we destroy a visible overlay sorface */
    mRepaintNeeded = true;

    void * test = mSurfHandleTable.remove(pCmd->u.in.hSurf);
    Assert(test);

    return VINF_SUCCESS;
}

#define VBOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_RB(_pr) \
             QRect((_pr)->left,                     \
                 (_pr)->top,                        \
                 (_pr)->right - (_pr)->left + 1,    \
                 (_pr)->bottom - (_pr)->top + 1)

#define VBOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(_pr) \
             QRect((_pr)->left,                     \
                 (_pr)->top,                        \
                 (_pr)->right - (_pr)->left,        \
                 (_pr)->bottom - (_pr)->top)

int VBoxGLWidget::vhwaSurfaceLock(struct _VBOXVHWACMD_SURF_LOCK *pCmd)
{
    VBoxVHWASurfaceBase *pSurf = handle2Surface(pCmd->u.in.hSurf);
    VBOXQGLLOG_ENTER(("pSurf (0x%x)\n",pSurf));
    vboxCheckUpdateAddress (pSurf, pCmd->u.in.offSurface);
    if(pCmd->u.in.rectValid)
    {
        QRect r = VBOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.rect);
        return pSurf->lock(&r, pCmd->u.in.flags);
    }
    return pSurf->lock(NULL, pCmd->u.in.flags);
}

int VBoxGLWidget::vhwaSurfaceUnlock(struct _VBOXVHWACMD_SURF_UNLOCK *pCmd)
{
    VBoxVHWASurfaceBase *pSurf = handle2Surface(pCmd->u.in.hSurf);
    VBOXQGLLOG_ENTER(("pSurf (0x%x)\n",pSurf));
    if(pCmd->u.in.xUpdatedMemValid)
    {
        QRect r = VBOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.xUpdatedMemRect);
        pSurf->updatedMem(&r);
    }
    return pSurf->unlock();
}

int VBoxGLWidget::vhwaSurfaceBlt(struct _VBOXVHWACMD_SURF_BLT *pCmd)
{
    /**/
    Q_UNUSED(pCmd);
    return VERR_NOT_IMPLEMENTED;
//    VBoxVHWASurfaceBase *pDstSurf = (VBoxVHWASurfaceBase*)pCmd->u.in.hDstSurf;
//    VBoxVHWASurfaceBase *pSrcSurf = (VBoxVHWASurfaceBase*)pCmd->u.in.hSrcSurf;
//    VBOXQGLLOG_ENTER(("pDstSurf (0x%x), pSrcSurf (0x%x)\n",pDstSurf,pSrcSurf));
//    QRect dstRect = VBOXVHWA_CONSTRUCT_QRECT_FROM_RECTL(&pCmd->u.in.dstRect);
//    QRect srcRect = VBOXVHWA_CONSTRUCT_QRECT_FROM_RECTL(&pCmd->u.in.srcRect);
//    vboxCheckUpdateAddress (pSrcSurf, pCmd->u.in.offSrcSurface);
//    vboxCheckUpdateAddress (pDstSurf, pCmd->u.in.offDstSurface);
////    pDstSurf->makeCurrent();
////    const VBoxVHWAColorKey DstCKey, *pDstCKey = NULL;
////    const VBoxVHWAColorKey SrcCKey, *pSrcCKey = NULL;
////    if(pCmd->u.in.flags & VBOXVHWA_BLT_KEYSRCOVERRIDE)
////    {
////        pSrcCKey = &
////    }
////    if(pCmd->u.in.flags & VBOXVHWA_BLT_KEYDESTOVERRIDE)
////    {
////
////    }
//    if(pCmd->u.in.xUpdatedSrcMemValid)
//    {
//        QRect r = VBOXVHWA_CONSTRUCT_QRECT_FROM_RECTL(&pCmd->u.in.xUpdatedSrcMemRect);
//        pSrcSurf->updatedMem(&r);
//    }
//
//    return pDstSurf->blt(&dstRect, pSrcSurf, &srcRect,
//            pCmd->u.in.flags & VBOXVHWA_BLT_KEYSRCOVERRIDE ? &VBoxVHWAColorKey(pCmd->u.in.desc.SrcCK.high, pCmd->u.in.desc.SrcCK.low) : NULL,
//            pCmd->u.in.flags & VBOXVHWA_BLT_KEYDESTOVERRIDE ? &VBoxVHWAColorKey(pCmd->u.in.desc.DstCK.high, pCmd->u.in.desc.DstCK.low) : NULL);
}

int VBoxGLWidget::vhwaSurfaceFlip(struct _VBOXVHWACMD_SURF_FLIP *pCmd)
{
    VBoxVHWASurfaceBase *pTargSurf = handle2Surface(pCmd->u.in.hTargSurf);
    VBoxVHWASurfaceBase *pCurrSurf = handle2Surface(pCmd->u.in.hCurrSurf);
    VBOXQGLLOG_ENTER(("pTargSurf (0x%x), pCurrSurf (0x%x)\n",pTargSurf,pCurrSurf));
    vboxCheckUpdateAddress (pCurrSurf, pCmd->u.in.offCurrSurface);
    vboxCheckUpdateAddress (pTargSurf, pCmd->u.in.offTargSurface);

//    Assert(pTargSurf->isYInverted());
//    Assert(!pCurrSurf->isYInverted());
    if(pCmd->u.in.xUpdatedTargMemValid)
    {
        QRect r = VBOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.xUpdatedTargMemRect);
        pTargSurf->updatedMem(&r);
    }
    pTargSurf->getComplexList()->setCurrentVisible(pTargSurf);

    mRepaintNeeded = true;
#ifdef DEBUG
    pCurrSurf->cFlipsCurr++;
    pTargSurf->cFlipsTarg++;
#endif
//    mDisplay.flip(pTargSurf, pCurrSurf);
//    Assert(!pTargSurf->isYInverted());
//    Assert(pCurrSurf->isYInverted());
    return VINF_SUCCESS;
}

void VBoxGLWidget::vhwaDoSurfaceOverlayUpdate(VBoxVHWASurfaceBase *pDstSurf, VBoxVHWASurfaceBase *pSrcSurf, struct _VBOXVHWACMD_SURF_OVERLAY_UPDATE *pCmd)
{
    if(pCmd->u.in.flags & VBOXVHWA_OVER_KEYDEST)
    {
        VBOXQGLLOG((", KEYDEST"));
        /* we use src (overlay) surface to maintain overridden dst ckey info
         * to allow multiple overlays have different overridden dst keys for one primary surface */
        /* non-null dstOverlayCKey for overlay would mean the overlay surface contains the overridden
         * dst ckey value in defaultDstOverlayCKey
         * this allows the NULL to be a valid overridden value as well
         *  i.e.
         * 1. indicate the value is NUL overridden, just set NULL*/
        pSrcSurf->setOverriddenDstOverlayCKey(NULL);
    }
    else if(pCmd->u.in.flags & VBOXVHWA_OVER_KEYDESTOVERRIDE)
    {
        VBOXQGLLOG((", KEYDESTOVERRIDE"));
        /* we use src (overlay) surface to maintain overridden dst ckey info
         * to allow multiple overlays have different overridden dst keys for one primary surface */
        /* non-null dstOverlayCKey for overlay would mean the overlay surface contains the overridden
         * dst ckey value in defaultDstOverlayCKey
         * this allows the NULL to be a valid overridden value as well
         *  i.e.
         * 1. indicate the value is overridden (no matter what we write here, bu it should be not NULL)*/
        VBoxVHWAColorKey ckey(pCmd->u.in.desc.DstCK.high, pCmd->u.in.desc.DstCK.low);
        VBOXQGLLOG_CKEY(" ckey: ",&ckey, "\n");
        pSrcSurf->setOverriddenDstOverlayCKey(&ckey);
        /* tell the ckey is enabled */
        pSrcSurf->setDefaultDstOverlayCKey(&ckey);
    }
    else
    {
        VBOXQGLLOG((", no KEYDEST"));
        /* we use src (overlay) surface to maintain overridden dst ckey info
         * to allow multiple overlays have different overridden dst keys for one primary surface */
        /* non-null dstOverlayCKey for overlay would mean the overlay surface contains the overridden
         * dst ckey value in defaultDstOverlayCKey
         * this allows the NULL to be a valid overridden value as well
         * i.e.
         * 1. indicate the value is overridden (no matter what we write here, bu it should be not NULL)*/
        VBoxVHWAColorKey dummyCKey(0, 0);
        pSrcSurf->setOverriddenDstOverlayCKey(&dummyCKey);
        /* tell the ckey is disabled */
        pSrcSurf->setDefaultDstOverlayCKey(NULL);
    }

    if(pCmd->u.in.flags & VBOXVHWA_OVER_KEYSRC)
    {
        VBOXQGLLOG((", KEYSRC"));
        pSrcSurf->resetDefaultSrcOverlayCKey();
    }
    else if(pCmd->u.in.flags & VBOXVHWA_OVER_KEYSRCOVERRIDE)
    {
        VBOXQGLLOG((", KEYSRCOVERRIDE"));
        VBoxVHWAColorKey ckey(pCmd->u.in.desc.SrcCK.high, pCmd->u.in.desc.SrcCK.low);
        pSrcSurf->setOverriddenSrcOverlayCKey(&ckey);
    }
    else
    {
        VBOXQGLLOG((", no KEYSRC"));
        pSrcSurf->setOverriddenSrcOverlayCKey(NULL);
    }
    VBOXQGLLOG(("\n"));
    if(pDstSurf)
    {
        QRect dstRect = VBOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.dstRect);
        QRect srcRect = VBOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.srcRect);

        VBOXQGLLOG(("*******overlay update*******\n"));
        VBOXQGLLOG(("dstSurfSize: w(%d), h(%d)\n", pDstSurf->width(), pDstSurf->height()));
        VBOXQGLLOG(("srcSurfSize: w(%d), h(%d)\n", pSrcSurf->width(), pSrcSurf->height()));
        VBOXQGLLOG_QRECT("dstRect:", &dstRect, "\n");
        VBOXQGLLOG_QRECT("srcRect:", &srcRect, "\n");

        const VBoxVHWAColorKey *pResSrcCKey = pSrcSurf->getActiveSrcOverlayCKey();
        const VBoxVHWAColorKey *pResDstCKey = pSrcSurf->getActiveDstOverlayCKey(pDstSurf);

        VBoxVHWAGlProgramVHWA *pProgram = vboxVHWAGetGlProgramMngr()->getProgram(pResDstCKey != NULL, pResSrcCKey != NULL, &pSrcSurf->colorFormat(), &pDstSurf->colorFormat());

        if(pProgram)
        {
            pProgram->start();
            if(pResSrcCKey || pResDstCKey)
            {
                if(pResSrcCKey)
                {
                    VBoxVHWASurfaceBase::setCKey(pProgram, &pSrcSurf->colorFormat(), pResSrcCKey, false);
                }
                if(pResDstCKey)
                {
                    VBoxVHWASurfaceBase::setCKey(pProgram, &pDstSurf->colorFormat(), pResDstCKey, true);
                }

            }

            switch(pSrcSurf->fourcc())
            {
                case 0:
                case FOURCC_AYUV:
                case FOURCC_YV12:
                    break;
                case FOURCC_UYVY:
                case FOURCC_YUY2:
                    break;
                default:
                    Assert(0);
                    break;
            }

            pProgram->stop();
        }

        pSrcSurf->setRects(pDstSurf, dstRect, srcRect, mViewport, true);
    }
}

int VBoxGLWidget::vhwaSurfaceOverlayUpdate(struct _VBOXVHWACMD_SURF_OVERLAY_UPDATE *pCmd)
{
    VBoxVHWASurfaceBase *pSrcSurf = handle2Surface(pCmd->u.in.hSrcSurf);
    VBoxVHWASurfList *pList = pSrcSurf->getComplexList();
    vboxCheckUpdateAddress (pSrcSurf, pCmd->u.in.offSrcSurface);
    VBOXQGLLOG(("OverlayUpdate: pSrcSurf (0x%x)\n",pSrcSurf));
    VBoxVHWASurfaceBase *pDstSurf = NULL;

    if(pCmd->u.in.hDstSurf)
    {
        pDstSurf = handle2Surface(pCmd->u.in.hDstSurf);
        vboxCheckUpdateAddress (pDstSurf, pCmd->u.in.offDstSurface);
        VBOXQGLLOG(("pDstSurf (0x%x)\n",pDstSurf));
#ifdef DEBUGVHWASTRICT
        Assert(pDstSurf == mDisplay.getVGA());
        Assert(mDisplay.getVGA() == mDisplay.getPrimary());
#endif
        Assert(pDstSurf->getComplexList() == mDisplay.getVGA()->getComplexList());

        if(pCmd->u.in.flags & VBOXVHWA_OVER_SHOW)
        {
            if(pDstSurf != mDisplay.getPrimary())
            {
                mDisplay.updateVGA(pDstSurf);
                pDstSurf->getComplexList()->setCurrentVisible(pDstSurf);
            }
        }
    }

    const SurfList & surfaces = pList->surfaces();

    for (SurfList::const_iterator it = surfaces.begin();
             it != surfaces.end(); ++ it)
    {
        VBoxVHWASurfaceBase *pCurSrcSurf = (*it);
        vhwaDoSurfaceOverlayUpdate(pDstSurf, pCurSrcSurf, pCmd);
    }

    if(pCmd->u.in.flags & VBOXVHWA_OVER_HIDE)
    {
        VBOXQGLLOG(("hide"));
        pList->setCurrentVisible(NULL);
    }
    else if(pCmd->u.in.flags & VBOXVHWA_OVER_SHOW)
    {
        VBOXQGLLOG(("show"));
        pList->setCurrentVisible(pSrcSurf);
    }

    mRepaintNeeded = true;

    return VINF_SUCCESS;
}

int VBoxGLWidget::vhwaSurfaceOverlaySetPosition(struct _VBOXVHWACMD_SURF_OVERLAY_SETPOSITION *pCmd)
{
    VBoxVHWASurfaceBase *pDstSurf = handle2Surface(pCmd->u.in.hDstSurf);
    VBoxVHWASurfaceBase *pSrcSurf = handle2Surface(pCmd->u.in.hSrcSurf);

    VBOXQGLLOG_ENTER(("pDstSurf (0x%x), pSrcSurf (0x%x)\n",pDstSurf,pSrcSurf));

    vboxCheckUpdateAddress (pSrcSurf, pCmd->u.in.offSrcSurface);
    vboxCheckUpdateAddress (pDstSurf, pCmd->u.in.offDstSurface);

    VBoxVHWASurfList *pList = pSrcSurf->getComplexList();
    const SurfList & surfaces = pList->surfaces();

    QPoint pos(pCmd->u.in.xPos, pCmd->u.in.yPos);

#ifdef DEBUGVHWASTRICT
    Assert(pDstSurf == mDisplay.getVGA());
    Assert(mDisplay.getVGA() == mDisplay.getPrimary());
#endif
    if(pSrcSurf->getComplexList()->current() != NULL)
    {
        Assert(pDstSurf);
        if(pDstSurf != mDisplay.getPrimary())
        {
            mDisplay.updateVGA(pDstSurf);
            pDstSurf->getComplexList()->setCurrentVisible(pDstSurf);
        }
    }

    mRepaintNeeded = true;

    for (SurfList::const_iterator it = surfaces.begin();
             it != surfaces.end(); ++ it)
    {
        VBoxVHWASurfaceBase *pCurSrcSurf = (*it);
        pCurSrcSurf->setTargRectPosition(pDstSurf, pos, mViewport);
    }

    return VINF_SUCCESS;
}

int VBoxGLWidget::vhwaSurfaceColorkeySet(struct _VBOXVHWACMD_SURF_COLORKEY_SET *pCmd)
{
    VBoxVHWASurfaceBase *pSurf = handle2Surface(pCmd->u.in.hSurf);

    VBOXQGLLOG_ENTER(("pSurf (0x%x)\n",pSurf));

    vboxCheckUpdateAddress (pSurf, pCmd->u.in.offSurface);

    mRepaintNeeded = true;

//    VBOXVHWA_CKEY_COLORSPACE
    if(pCmd->u.in.flags & VBOXVHWA_CKEY_DESTBLT)
    {
        VBoxVHWAColorKey ckey(pCmd->u.in.CKey.high, pCmd->u.in.CKey.low);
        pSurf->setDstBltCKey(&ckey);
    }
    if(pCmd->u.in.flags & VBOXVHWA_CKEY_DESTOVERLAY)
    {
        VBoxVHWAColorKey ckey(pCmd->u.in.CKey.high, pCmd->u.in.CKey.low);
        pSurf->setDefaultDstOverlayCKey(&ckey);
    }
    if(pCmd->u.in.flags & VBOXVHWA_CKEY_SRCBLT)
    {
        VBoxVHWAColorKey ckey(pCmd->u.in.CKey.high, pCmd->u.in.CKey.low);
        pSurf->setSrcBltCKey(&ckey);
    }
    if(pCmd->u.in.flags & VBOXVHWA_CKEY_SRCOVERLAY)
    {
        VBoxVHWAColorKey ckey(pCmd->u.in.CKey.high, pCmd->u.in.CKey.low);
        pSurf->setDefaultSrcOverlayCKey(&ckey);
    }

    return VINF_SUCCESS;
}

int VBoxGLWidget::vhwaQueryInfo1(struct _VBOXVHWACMD_QUERYINFO1 *pCmd)
{
    VBOXQGLLOG_ENTER(("\n"));
    bool bEnabled = false;
    if(vboxVHWASupportedInternal())
    {
        Assert(pCmd->u.in.guestVersion.maj == VBOXVHWA_VERSION_MAJ);
        if(pCmd->u.in.guestVersion.maj == VBOXVHWA_VERSION_MAJ)
        {
            Assert(pCmd->u.in.guestVersion.min == VBOXVHWA_VERSION_MIN);
            if(pCmd->u.in.guestVersion.min == VBOXVHWA_VERSION_MIN)
            {
                Assert(pCmd->u.in.guestVersion.bld == VBOXVHWA_VERSION_BLD);
                if(pCmd->u.in.guestVersion.bld == VBOXVHWA_VERSION_BLD)
                {
                    Assert(pCmd->u.in.guestVersion.reserved == VBOXVHWA_VERSION_RSV);
                    if(pCmd->u.in.guestVersion.reserved == VBOXVHWA_VERSION_RSV)
                    {
                        bEnabled = true;
                    }
                }
            }
        }
    }

    memset(pCmd, 0, sizeof(VBOXVHWACMD_QUERYINFO1));
    if(bEnabled)
    {
        pCmd->u.out.cfgFlags = VBOXVHWA_CFG_ENABLED;

        pCmd->u.out.caps =
//                        VBOXVHWA_CAPS_BLT | VBOXVHWA_CAPS_BLTSTRETCH | VBOXVHWA_CAPS_BLTQUEUE
//                                 // | VBOXVHWA_CAPS_BLTCOLORFILL not supported, although adding it is trivial
//                                 // | VBOXVHWA_CAPS_BLTFOURCC set below if shader support is available
                                 VBOXVHWA_CAPS_OVERLAY
                                 | VBOXVHWA_CAPS_OVERLAYSTRETCH
                                 | VBOXVHWA_CAPS_OVERLAYCANTCLIP
                                 // | VBOXVHWA_CAPS_OVERLAYFOURCC set below if shader support is available
                                 ;

        pCmd->u.out.caps2 = VBOXVHWA_CAPS2_CANRENDERWINDOWED
                                    | VBOXVHWA_CAPS2_WIDESURFACES;

        //TODO: setup stretchCaps
        pCmd->u.out.stretchCaps = 0;

        pCmd->u.out.numOverlays = 1;
        /* TODO: set curOverlays properly */
        pCmd->u.out.curOverlays = 0;

        pCmd->u.out.surfaceCaps =
                            VBOXVHWA_SCAPS_PRIMARYSURFACE
                    //        | VBOXVHWA_SCAPS_OFFSCREENPLAIN
                            | VBOXVHWA_SCAPS_FLIP
                            | VBOXVHWA_SCAPS_LOCALVIDMEM
                            | VBOXVHWA_SCAPS_OVERLAY
                    //        | VBOXVHWA_SCAPS_VIDEOMEMORY
                    //        | VBOXVHWA_SCAPS_COMPLEX
                    //        | VBOXVHWA_SCAPS_VISIBLE
                            ;

        if(g_vboxVHWAGlShaderSupported && g_vboxVHWAGlMultiTexNumSupported >= 2)
        {
            pCmd->u.out.caps |= VBOXVHWA_CAPS_COLORKEY
                            | VBOXVHWA_CAPS_COLORKEYHWASSIST
                            ;

            pCmd->u.out.colorKeyCaps =
//                          VBOXVHWA_CKEYCAPS_DESTBLT | VBOXVHWA_CKEYCAPS_DESTBLTCLRSPACE | VBOXVHWA_CKEYCAPS_SRCBLT| VBOXVHWA_CKEYCAPS_SRCBLTCLRSPACE |
//                          VBOXVHWA_CKEYCAPS_SRCOVERLAY | VBOXVHWA_CKEYCAPS_SRCOVERLAYONEACTIVE |
                            VBOXVHWA_CKEYCAPS_DESTOVERLAY          |
                            VBOXVHWA_CKEYCAPS_DESTOVERLAYONEACTIVE;
                            ;

            if(g_vboxVHWAGlTextureRectangleSupported)
            {
                pCmd->u.out.caps |= VBOXVHWA_CAPS_OVERLAYFOURCC
//                              | VBOXVHWA_CAPS_BLTFOURCC
                                ;

                pCmd->u.out.colorKeyCaps |=
//                               VBOXVHWA_CKEYCAPS_SRCOVERLAYYUV |
                                 VBOXVHWA_CKEYCAPS_DESTOVERLAYYUV;
                                 ;

//              pCmd->u.out.caps2 |= VBOXVHWA_CAPS2_COPYFOURCC;

                pCmd->u.out.numFourCC = g_vboxVHWAFourccSupportedCount;
            }
        }
    }

    return VINF_SUCCESS;
}

int VBoxGLWidget::vhwaQueryInfo2(struct _VBOXVHWACMD_QUERYINFO2 *pCmd)
{
    VBOXQGLLOG_ENTER(("\n"));

    Assert(pCmd->numFourCC >= g_vboxVHWAFourccSupportedCount);
    if(pCmd->numFourCC < g_vboxVHWAFourccSupportedCount)
        return VERR_GENERAL_FAILURE;

    pCmd->numFourCC = g_vboxVHWAFourccSupportedCount;
    for(uint32_t i = 0; i < g_vboxVHWAFourccSupportedCount; i++)
    {
        pCmd->FourCC[i] = g_vboxVHWAFourccSupportedList[i];
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) vboxQGLSaveExec(PSSMHANDLE pSSM, void *pvUser)
{
    VBoxGLWidget * pw = (VBoxGLWidget*)pvUser;
    pw->vhwaSaveExec(pSSM);
}

static DECLCALLBACK(int) vboxQGLLoadExec(PSSMHANDLE pSSM, void *pvUser, uint32_t u32Version, uint32_t uPass)
{
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);
    VBoxGLWidget * pw = (VBoxGLWidget*)pvUser;
    return pw->vhwaLoadExec(NULL, pSSM, u32Version);
}

int VBoxGLWidget::vhwaSaveSurface(struct SSMHANDLE * pSSM, VBoxVHWASurfaceBase *pSurf, uint32_t surfCaps)
{
    VBOXQGL_SAVE_SURFSTART(pSSM);

    uint64_t u64 = vboxVRAMOffset(pSurf);
    int rc;
    rc = SSMR3PutU32(pSSM, pSurf->handle());         AssertRC(rc);
    rc = SSMR3PutU64(pSSM, u64);         AssertRC(rc);
    rc = SSMR3PutU32(pSSM, pSurf->width());         AssertRC(rc);
    rc = SSMR3PutU32(pSSM, pSurf->height());         AssertRC(rc);
    rc = SSMR3PutU32(pSSM, surfCaps);         AssertRC(rc);
    uint32_t flags = 0;
    const VBoxVHWAColorKey * pDstBltCKey = pSurf->dstBltCKey();
    const VBoxVHWAColorKey * pSrcBltCKey = pSurf->srcBltCKey();
    const VBoxVHWAColorKey * pDstOverlayCKey = pSurf->dstOverlayCKey();
    const VBoxVHWAColorKey * pSrcOverlayCKey = pSurf->srcOverlayCKey();
    if(pDstBltCKey)
    {
        flags |= VBOXVHWA_SD_CKDESTBLT;
    }
    if(pSrcBltCKey)
    {
        flags |= VBOXVHWA_SD_CKSRCBLT;
    }
    if(pDstOverlayCKey)
    {
        flags |= VBOXVHWA_SD_CKDESTOVERLAY;
    }
    if(pSrcOverlayCKey)
    {
        flags |= VBOXVHWA_SD_CKSRCOVERLAY;
    }

    rc = SSMR3PutU32(pSSM, flags);         AssertRC(rc);
    if(pDstBltCKey)
    {
        rc = SSMR3PutU32(pSSM, pDstBltCKey->lower());         AssertRC(rc);
        rc = SSMR3PutU32(pSSM, pDstBltCKey->upper());         AssertRC(rc);
    }
    if(pSrcBltCKey)
    {
        rc = SSMR3PutU32(pSSM, pSrcBltCKey->lower());         AssertRC(rc);
        rc = SSMR3PutU32(pSSM, pSrcBltCKey->upper());         AssertRC(rc);
    }
    if(pDstOverlayCKey)
    {
        rc = SSMR3PutU32(pSSM, pDstOverlayCKey->lower());         AssertRC(rc);
        rc = SSMR3PutU32(pSSM, pDstOverlayCKey->upper());         AssertRC(rc);
    }
    if(pSrcOverlayCKey)
    {
        rc = SSMR3PutU32(pSSM, pSrcOverlayCKey->lower());         AssertRC(rc);
        rc = SSMR3PutU32(pSSM, pSrcOverlayCKey->upper());         AssertRC(rc);
    }

    const VBoxVHWAColorFormat & format = pSurf->colorFormat();
    flags = 0;
    if(format.fourcc())
    {
        flags |= VBOXVHWA_PF_FOURCC;
        rc = SSMR3PutU32(pSSM, flags);         AssertRC(rc);
        rc = SSMR3PutU32(pSSM, format.fourcc());         AssertRC(rc);
    }
    else
    {
        flags |= VBOXVHWA_PF_RGB;
        rc = SSMR3PutU32(pSSM, flags);         AssertRC(rc);
        rc = SSMR3PutU32(pSSM, format.bitsPerPixel());         AssertRC(rc);
        rc = SSMR3PutU32(pSSM, format.r().mask());         AssertRC(rc);
        rc = SSMR3PutU32(pSSM, format.g().mask());         AssertRC(rc);
        rc = SSMR3PutU32(pSSM, format.b().mask());         AssertRC(rc);
        rc = SSMR3PutU32(pSSM, format.a().mask());         AssertRC(rc);
    }

    VBOXQGL_SAVE_SURFSTOP(pSSM);

    return rc;
}

int VBoxGLWidget::vhwaLoadSurface(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version)
{
    Q_UNUSED(u32Version);

    VBOXQGL_LOAD_SURFSTART(pSSM);

    char *buf = (char*)malloc(VBOXVHWACMD_SIZE(VBOXVHWACMD_SURF_CREATE));
    memset(buf, 0, sizeof(buf));
    VBOXVHWACMD * pCmd = (VBOXVHWACMD*)buf;
    pCmd->enmCmd = VBOXVHWACMD_TYPE_SURF_CREATE;
    pCmd->Flags = VBOXVHWACMD_FLAG_HH_CMD;

    VBOXVHWACMD_SURF_CREATE * pCreateSurf = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_CREATE);
    int rc;
    uint32_t u32;
    rc = SSMR3GetU32(pSSM, &u32);         AssertRC(rc);
    pCreateSurf->SurfInfo.hSurf = (VBOXVHWA_SURFHANDLE)u32;
    if(RT_SUCCESS(rc))
    {
        rc = SSMR3GetU64(pSSM, &pCreateSurf->SurfInfo.offSurface);         AssertRC(rc);
        rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.width);         AssertRC(rc);
        rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.height);         AssertRC(rc);
        rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.surfCaps);         AssertRC(rc);
        rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.flags);         AssertRC(rc);
        if(pCreateSurf->SurfInfo.flags & VBOXVHWA_SD_CKDESTBLT)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.DstBltCK.low);         AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.DstBltCK.high);         AssertRC(rc);
        }
        if(pCreateSurf->SurfInfo.flags & VBOXVHWA_SD_CKSRCBLT)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.SrcBltCK.low);         AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.SrcBltCK.high);         AssertRC(rc);
        }
        if(pCreateSurf->SurfInfo.flags & VBOXVHWA_SD_CKDESTOVERLAY)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.DstOverlayCK.low);         AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.DstOverlayCK.high);         AssertRC(rc);
        }
        if(pCreateSurf->SurfInfo.flags & VBOXVHWA_SD_CKSRCOVERLAY)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.SrcOverlayCK.low);         AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.SrcOverlayCK.high);         AssertRC(rc);
        }

        rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.flags);         AssertRC(rc);
        if(pCreateSurf->SurfInfo.PixelFormat.flags & VBOXVHWA_PF_RGB)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.c.rgbBitCount);         AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.m1.rgbRBitMask);         AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.m2.rgbGBitMask);         AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.m3.rgbBBitMask);         AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.m4.rgbABitMask);         AssertRC(rc);
        }
        else if(pCreateSurf->SurfInfo.PixelFormat.flags & VBOXVHWA_PF_FOURCC)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.fourCC);         AssertRC(rc);
        }
        else
        {
            Assert(0);
        }

        pCmdList->push_back(pCmd);
//        vboxExecOnResize(&VBoxGLWidget::vboxDoVHWACmdAndFree, pCmd); AssertRC(rc);
//        if(RT_SUCCESS(rc))
//        {
//            rc = pCmd->rc;
//            AssertRC(rc);
//        }
    }

    VBOXQGL_LOAD_SURFSTOP(pSSM);

    return rc;
}

int VBoxGLWidget::vhwaSaveOverlayData(struct SSMHANDLE * pSSM, VBoxVHWASurfaceBase *pSurf, bool bVisible)
{
    VBOXQGL_SAVE_OVERLAYSTART(pSSM);

    uint32_t flags = 0;
    const VBoxVHWAColorKey * dstCKey = pSurf->dstOverlayCKey();
    const VBoxVHWAColorKey * defaultDstCKey = pSurf->defaultDstOverlayCKey();
    const VBoxVHWAColorKey * srcCKey = pSurf->srcOverlayCKey();;
    const VBoxVHWAColorKey * defaultSrcCKey = pSurf->defaultSrcOverlayCKey();
    bool bSaveDstCKey = false;
    bool bSaveSrcCKey = false;

    if(bVisible)
    {
        flags |= VBOXVHWA_OVER_SHOW;
    }
    else
    {
        flags |= VBOXVHWA_OVER_HIDE;
    }

    if(!dstCKey)
    {
        flags |= VBOXVHWA_OVER_KEYDEST;
    }
    else if(defaultDstCKey)
    {
        flags |= VBOXVHWA_OVER_KEYDESTOVERRIDE;
        bSaveDstCKey = true;
    }

    if(srcCKey == defaultSrcCKey)
    {
        flags |= VBOXVHWA_OVER_KEYSRC;
    }
    else if(srcCKey)
    {
        flags |= VBOXVHWA_OVER_KEYSRCOVERRIDE;
        bSaveSrcCKey = true;
    }

    int rc = SSMR3PutU32(pSSM, flags); AssertRC(rc);

    rc = SSMR3PutU32(pSSM, mDisplay.getPrimary()->handle()); AssertRC(rc);
    rc = SSMR3PutU32(pSSM, pSurf->handle()); AssertRC(rc);

    if(bSaveDstCKey)
    {
        rc = SSMR3PutU32(pSSM, dstCKey->lower()); AssertRC(rc);
        rc = SSMR3PutU32(pSSM, dstCKey->upper()); AssertRC(rc);
    }
    if(bSaveSrcCKey)
    {
        rc = SSMR3PutU32(pSSM, srcCKey->lower()); AssertRC(rc);
        rc = SSMR3PutU32(pSSM, srcCKey->upper()); AssertRC(rc);
    }

    int x1, x2, y1, y2;
    pSurf->targRect().getCoords(&x1, &y1, &x2, &y2);
    rc = SSMR3PutS32(pSSM, x1); AssertRC(rc);
    rc = SSMR3PutS32(pSSM, x2+1); AssertRC(rc);
    rc = SSMR3PutS32(pSSM, y1); AssertRC(rc);
    rc = SSMR3PutS32(pSSM, y2+1); AssertRC(rc);

    pSurf->srcRect().getCoords(&x1, &y1, &x2, &y2);
    rc = SSMR3PutS32(pSSM, x1); AssertRC(rc);
    rc = SSMR3PutS32(pSSM, x2+1); AssertRC(rc);
    rc = SSMR3PutS32(pSSM, y1); AssertRC(rc);
    rc = SSMR3PutS32(pSSM, y2+1); AssertRC(rc);

    VBOXQGL_SAVE_OVERLAYSTOP(pSSM);

    return rc;
}

int VBoxGLWidget::vhwaLoadOverlayData(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version)
{
    Q_UNUSED(u32Version);

    VBOXQGL_LOAD_OVERLAYSTART(pSSM);

//    char buf[VBOXVHWACMD_SIZE(VBOXVHWACMD_SURF_OVERLAY_UPDATE)];
    char *buf = new char[VBOXVHWACMD_SIZE(VBOXVHWACMD_SURF_CREATE)];
    memset(buf, 0, sizeof(buf));
    VBOXVHWACMD * pCmd = (VBOXVHWACMD*)buf;
    pCmd->enmCmd = VBOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE;
    pCmd->Flags = VBOXVHWACMD_FLAG_HH_CMD;

    VBOXVHWACMD_SURF_OVERLAY_UPDATE * pUpdateOverlay = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_OVERLAY_UPDATE);
    int rc;

    rc = SSMR3GetU32(pSSM, &pUpdateOverlay->u.in.flags); AssertRC(rc);
    uint32_t hSrc, hDst;
    rc = SSMR3GetU32(pSSM, &hDst); AssertRC(rc);
    rc = SSMR3GetU32(pSSM, &hSrc); AssertRC(rc);
    pUpdateOverlay->u.in.hSrcSurf = hSrc;
    pUpdateOverlay->u.in.hDstSurf = hDst;
//    Assert(hDst == mDisplay.getVGA()->handle());
//    VBoxVHWASurfaceBase *pDstSurf = handle2Surface(hDst);
//    VBoxVHWASurfaceBase *pSrcSurf = handle2Surface(hSrc);
//    Assert(pSrcSurf);
//    Assert(pDstSurf);
//    if(pSrcSurf && pDstSurf)
    {
        pUpdateOverlay->u.in.offDstSurface = VBOXVHWA_OFFSET64_VOID;
//        vboxVRAMOffset(pDstSurf);
        pUpdateOverlay->u.in.offSrcSurface = VBOXVHWA_OFFSET64_VOID;
//        vboxVRAMOffset(pSrcSurf);

        if(pUpdateOverlay->u.in.flags & VBOXVHWA_OVER_KEYDESTOVERRIDE)
        {
            rc = SSMR3GetU32(pSSM, &pUpdateOverlay->u.in.desc.DstCK.low); AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pUpdateOverlay->u.in.desc.DstCK.high); AssertRC(rc);
        }

        if(pUpdateOverlay->u.in.flags & VBOXVHWA_OVER_KEYSRCOVERRIDE)
        {
            rc = SSMR3GetU32(pSSM, &pUpdateOverlay->u.in.desc.SrcCK.low); AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pUpdateOverlay->u.in.desc.SrcCK.high); AssertRC(rc);
        }

        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.dstRect.left); AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.dstRect.right); AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.dstRect.top); AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.dstRect.bottom); AssertRC(rc);

        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.srcRect.left); AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.srcRect.right); AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.srcRect.top); AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.srcRect.bottom); AssertRC(rc);

        pCmdList->push_back(pCmd);
//        vboxExecOnResize(&VBoxGLWidget::vboxDoVHWACmdAndFree, pCmd); AssertRC(rc);
//        if(RT_SUCCESS(rc))
//        {
//            rc = pCmd->rc;
//            AssertRC(rc);
//        }
    }
//    else
//    {
//        rc = VERR_GENERAL_FAILURE;
//    }

    VBOXQGL_LOAD_OVERLAYSTOP(pSSM);

    return rc;
}

void VBoxGLWidget::vhwaSaveExec(struct SSMHANDLE * pSSM)
{
    VBOXQGL_SAVE_START(pSSM);

    /* the mechanism of restoring data is based on generating VHWA commands that restore the surfaces state
     * the following commands are generated:
     * I.    CreateSurface
     * II.   UpdateOverlay
     *
     * Data format is the following:
     * I.    u32 - Num primary surfaces - (the current frontbuffer is detected in the stored surf flags which are posted to the generated CreateSurface cmd)
     * II.   for each primary surf
     * II.1    generate & execute CreateSurface cmd (see below on the generation logic)
     * III.  u32 - Num overlays
     * IV.   for each overlay
     * IV.1    u32 - Num surfaces in overlay (the current frontbuffer is detected in the stored surf flags which are posted to the generated CreateSurface cmd)
     * IV.2    for each surface in overlay
     * IV.2.a    generate & execute CreateSurface cmd (see below on the generation logic)
     * IV.2.b    generate & execute UpdateOverlay cmd (see below on the generation logic)
     *
     */
    const SurfList & primaryList = mDisplay.primaries().surfaces();
    uint32_t cPrimary = (uint32_t)primaryList.size();
    Assert(cPrimary >= 1);
    if(mDisplay.getVGA() == NULL || mDisplay.getVGA()->handle() == VBOXVHWA_SURFHANDLE_INVALID)
    {
        cPrimary -= 1;
    }
    int rc = SSMR3PutU32(pSSM, cPrimary);         AssertRC(rc);

    for (SurfList::const_iterator pr = primaryList.begin();
         pr != primaryList.end(); ++ pr)
    {
        VBoxVHWASurfaceBase *pSurf = *pr;
//        bool bVga = (pSurf == mDisplay.getVGA());
        bool bVisible = (pSurf == mDisplay.getPrimary());
        uint32_t flags = VBOXVHWA_SCAPS_PRIMARYSURFACE;
        if(bVisible)
            flags |= VBOXVHWA_SCAPS_VISIBLE;

        if(pSurf->handle() != VBOXVHWA_SURFHANDLE_INVALID)
        {
            rc = vhwaSaveSurface(pSSM, *pr, flags);    AssertRC(rc);
#ifdef DEBUG
            --cPrimary;
            Assert(cPrimary < UINT32_MAX / 2);
#endif
        }
        else
        {
            Assert(pSurf == mDisplay.getVGA());
        }
    }

#ifdef DEBUG
    Assert(!cPrimary);
#endif

    const OverlayList & overlays = mDisplay.overlays();
    rc = SSMR3PutU32(pSSM, (uint32_t)overlays.size());         AssertRC(rc);

    for (OverlayList::const_iterator it = overlays.begin();
         it != overlays.end(); ++ it)
    {
        VBoxVHWASurfList * pSurfList = *it;
        const SurfList & surfaces = pSurfList->surfaces();
        uint32_t cSurfs = (uint32_t)surfaces.size();
        uint32_t flags = VBOXVHWA_SCAPS_OVERLAY;
        if(cSurfs > 1)
            flags |= VBOXVHWA_SCAPS_COMPLEX;
        rc = SSMR3PutU32(pSSM, cSurfs);         AssertRC(rc);
        for (SurfList::const_iterator sit = surfaces.begin();
             sit != surfaces.end(); ++ sit)
        {
            rc = vhwaSaveSurface(pSSM, *sit, flags);    AssertRC(rc);
        }

        bool bVisible = true;
        VBoxVHWASurfaceBase * pOverlayData = pSurfList->current();
        if(!pOverlayData)
        {
            pOverlayData = surfaces.front();
            bVisible = false;
        }

        rc = vhwaSaveOverlayData(pSSM, pOverlayData, bVisible);    AssertRC(rc);
    }

    VBOXQGL_SAVE_STOP(pSSM);
}

int VBoxGLWidget::vhwaLoadExec(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version)
{
    VBOXQGL_LOAD_START(pSSM);

    int rc;
    uint32_t u32;

    if(pCmdList == NULL)
    {
        /* use our own list */
        pCmdList = &mOnResizeCmdList;
    }

    rc = SSMR3GetU32(pSSM, &u32); AssertRC(rc);
    if(RT_SUCCESS(rc))
    {
        for(uint32_t i = 0; i < u32; ++i)
        {
            rc = vhwaLoadSurface(pCmdList, pSSM, u32Version);  AssertRC(rc);
            if(RT_FAILURE(rc))
                break;
        }

        if(RT_SUCCESS(rc))
        {
            rc = SSMR3GetU32(pSSM, &u32); AssertRC(rc);
            if(RT_SUCCESS(rc))
            {
                for(uint32_t i = 0; i < u32; ++i)
                {
                    uint32_t cSurfs;
                    rc = SSMR3GetU32(pSSM, &cSurfs); AssertRC(rc);
                    for(uint32_t j = 0; j < cSurfs; ++j)
                    {
                        rc = vhwaLoadSurface(pCmdList, pSSM, u32Version);  AssertRC(rc);
                        if(RT_FAILURE(rc))
                            break;
                    }

                    if(RT_SUCCESS(rc))
                    {
                        rc = vhwaLoadOverlayData(pCmdList, pSSM, u32Version);  AssertRC(rc);
                    }

                    if(RT_FAILURE(rc))
                    {
                        break;
                    }
                }
            }
        }
    }

    VBOXQGL_LOAD_STOP(pSSM);

    return rc;
}

int VBoxGLWidget::vhwaConstruct(struct _VBOXVHWACMD_HH_CONSTRUCT *pCmd)
{
    PVM pVM = (PVM)pCmd->pVM;
    uint32_t intsId = 0; /* @todo: set the proper id */

    char nameFuf[sizeof(VBOXQGL_STATE_NAMEBASE) + 8];

    char * pszName = nameFuf;
    sprintf(pszName, "%s%d", VBOXQGL_STATE_NAMEBASE, intsId);
    int rc = SSMR3RegisterExternal(
            pVM,                    /* The VM handle*/
            pszName,                /* Data unit name. */
            intsId,                 /* The instance identifier of the data unit.
                                     * This must together with the name be unique. */
            VBOXQGL_STATE_VERSION,   /* Data layout version number. */
            128,             /* The approximate amount of data in the unit.
                              * Only for progress indicators. */
            NULL, NULL, NULL, /* pfnLiveXxx */
            NULL,            /* Prepare save callback, optional. */
            vboxQGLSaveExec, /* Execute save callback, optional. */
            NULL,            /* Done save callback, optional. */
            NULL,            /* Prepare load callback, optional. */
            vboxQGLLoadExec, /* Execute load callback, optional. */
            NULL,            /* Done load callback, optional. */
            this             /* User argument. */
            );
    Assert(RT_SUCCESS(rc));
    return rc;
}

uchar * VBoxGLWidget::vboxVRAMAddressFromOffset(uint64_t offset)
{
    return ((offset != VBOXVHWA_OFFSET64_VOID) && vboxUsesGuestVRAM()) ? vboxAddress() + offset : NULL;
}

uint64_t VBoxGLWidget::vboxVRAMOffsetFromAddress(uchar* addr)
{
    return uint64_t(addr - vboxAddress());
}

uint64_t VBoxGLWidget::vboxVRAMOffset(VBoxVHWASurfaceBase * pSurf)
{
    return pSurf->addressAlocated() ? VBOXVHWA_OFFSET64_VOID : vboxVRAMOffsetFromAddress(pSurf->address());
}

#endif

void VBoxGLWidget::initializeGL()
{
    vboxVHWAGlInit(context());
    VBoxVHWASurfaceBase::globalInit();
}

#ifdef VBOXQGL_DBG_SURF

int g_iCur = 0;
VBoxVHWASurfaceBase * g_apSurf[] = {NULL, NULL, NULL};

void VBoxGLWidget::vboxDoTestSurfaces(void* context)
{
//    uint32_t width = 103;
//    uint32_t height = 47;
//    uint32_t rgbBitCount = 32;
//    uint32_t r = 0xff, g = 0xff00, b = 0xff0000;
//    QRect dstRect(10, 50, width, height);
//    QRect srcRect(0, 0, width, height);
////    Assert(0);
//    if(!pSurf1)
//    {
//
////        pSurf1 = new VBoxVHWASurfaceBase(this, width, height,
////                        VBoxVHWAColorFormat(rgbBitCount,
////                                r,
////                                g,
////                                b),
////                        NULL, NULL, NULL, NULL);
//        pSurf1 = new VBoxVHWASurfaceBase(this, &QSize(width, height),
////                        ((pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_OVERLAY) ? mDisplay.getPrimary()->rect().size() : &QSize(pCmd->SurfInfo.width, pCmd->SurfInfo.height)),
//                        &mDisplay.getPrimary()->rect().size(),
//                        VBoxVHWAColorFormat(rgbBitCount,
//                                r,
//                                g,
//                                b),
////                        pSrcBltCKey, pDstBltCKey, pSrcOverlayCKey, pDstOverlayCKey);
//                        NULL, NULL, NULL, NULL);
//
//        pSurf1->init(mDisplay.getVGA(), NULL);
//
//        VBoxVHWASurfList *pConstructingList = new VBoxVHWASurfList();
//        mDisplay.addOverlay(pConstructingList);
//        pConstructingList->add(pSurf1);
//        pConstructingList->setCurrentVisible(pSurf1);
////        pSurf1->performDisplay();
//    }
//
////    pDisplay->blt(&dstRect, pSurf1, &srcRect, NULL, NULL);
////    pDisplay->performDisplay();
    if(g_iCur >= RT_ELEMENTS(g_apSurf))
        g_iCur = 0;
    VBoxVHWASurfaceBase * pSurf1 = g_apSurf[g_iCur];
    if(pSurf1)
    {
        pSurf1->getComplexList()->setCurrentVisible(pSurf1);
    }
}
#endif

void VBoxGLWidget::vboxDoUpdateViewport(const QRect & aRect)
{
    adjustViewport(mDisplay.getPrimary()->size(), aRect);
    mViewport = aRect;

    const SurfList & primaryList = mDisplay.primaries().surfaces();

    for (SurfList::const_iterator pr = primaryList.begin();
         pr != primaryList.end(); ++ pr)
    {
        VBoxVHWASurfaceBase *pSurf = *pr;
        pSurf->updateVisibleTargRect(NULL, aRect);
    }

    const OverlayList & overlays = mDisplay.overlays();

    for (OverlayList::const_iterator it = overlays.begin();
         it != overlays.end(); ++ it)
    {
        VBoxVHWASurfList * pSurfList = *it;
        const SurfList & surfaces = pSurfList->surfaces();
        for (SurfList::const_iterator sit = surfaces.begin();
             sit != surfaces.end(); ++ sit)
        {
            VBoxVHWASurfaceBase *pSurf = *sit;
            pSurf->updateVisibleTargRect(mDisplay.getPrimary(), aRect);
        }
    }
}

bool VBoxGLWidget::hasSurfaces() const
{
    if(mDisplay.overlays().size() != 0)
        return true;
    if(mDisplay.primaries().size() > 1)
        return true;
    return mDisplay.getVGA()->handle() != VBOXVHWA_SURFHANDLE_INVALID;
}

bool VBoxGLWidget::hasVisibleOverlays()
{
    const OverlayList & overlays = mDisplay.overlays();
    for (OverlayList::const_iterator it = overlays.begin();
         it != overlays.end(); ++ it)
    {
        VBoxVHWASurfList * pSurfList = *it;
        if(pSurfList->current() != NULL)
            return true;
    }
    return false;
}

const QRect & VBoxGLWidget::overlaysRectUnion()
{
    const OverlayList & overlays = mDisplay.overlays();
    VBoxVHWADirtyRect un;
    for (OverlayList::const_iterator it = overlays.begin();
         it != overlays.end(); ++ it)
    {
        VBoxVHWASurfaceBase * pOverlay = (*it)->current();
        if(pOverlay != NULL)
        {
            un.add(pOverlay->targRect());
        }
    }
    return un.toRect();
}

//void VBoxGLWidget::vboxDoPaint(void *pe)
//{
//    Q_UNUSED(pe);
//
//#ifdef VBOXQGL_DBG_SURF
//    vboxDoTestSurfaces(NULL);
//#endif
////#ifdef VBOXQGL_PROF_BASE
////    vboxDoUpdateRect(&((QPaintEvent*)pe)->rect());
////#endif
////    mDisplay.performDisplay();
//}

void VBoxGLWidget::vboxDoUpdateRect(const QRect * pRect)
{
    mDisplay.getPrimary()->updatedMem(pRect);
}

void VBoxGLWidget::vboxDoResize(void *resize)
{
//    Assert(!format().accum());
//    Assert(format().alpha());
//    Assert(format().alphaBufferSize() == 8);
    Assert(format().blueBufferSize() == 8);
    Assert(format().greenBufferSize() == 8);
    Assert(format().redBufferSize() == 8);

//    Assert(!format().depth());
    Assert(format().directRendering());
    Assert(format().doubleBuffer());
    Assert(format().hasOpenGL());
    VBOXQGLLOG(("hasOpenGLOverlays(%d), hasOverlay(%d)\n", format().hasOpenGLOverlays(), format().hasOverlay()));
//    Assert(format().hasOpenGLOverlays());
//    Assert(format().hasOverlay());
    Assert(format().plane() == 0);
    Assert(format().rgba());
    Assert(!format().sampleBuffers());
//    Assert(!format().stencil());
    Assert(!format().stereo());
    VBOXQGLLOG(("swapInterval(%d)\n", format().swapInterval()));
//    Assert(format().swapInterval() == 0);


    VBOXQGL_CHECKERR(
            vboxglActiveTexture(GL_TEXTURE0);
        );

    VBoxResizeEvent *re = (VBoxResizeEvent*)resize;
    bool remind = false;
    bool fallback = false;

    VBOXQGLLOG(("resizing: fmt=%d, vram=%p, bpp=%d, bpl=%d, width=%d, height=%d\n",
                      re->pixelFormat(), re->VRAM(),
                      re->bitsPerPixel(), re->bytesPerLine(),
                      re->width(), re->height()));

    /* clean the old values first */

    ulong bytesPerLine;
    uint32_t bitsPerPixel;
    uint32_t b = 0xff, g = 0xff00, r = 0xff0000;

    /* check if we support the pixel format and can use the guest VRAM directly */
    if (re->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB)
    {

        bitsPerPixel = re->bitsPerPixel();
        bytesPerLine = re->bytesPerLine();
        ulong bitsPerLine = bytesPerLine * 8;

        switch (bitsPerPixel)
        {
            case 32:
//                format = VBoxVHWAColorFormat(bitsPerPixel, 0xff, 0xff00, 0xff0000);
                break;
            case 24:
#ifdef DEBUG_misha
                Assert(0);
#endif
//                format = VBoxVHWAColorFormat(bitsPerPixel, 0xff, 0xff00, 0xff0000);
//                remind = true;
                break;
//            case 16:
//                Assert(0);
////                r = 0xf800;
////                g = 0x7e0;
////                b = 0x1f;
//                break;
            case 8:
#ifdef DEBUG_misha
                Assert(0);
#endif
                g = b = 0;
                remind = true;
                break;
            case 1:
#ifdef DEBUG_misha
                Assert(0);
#endif
                r = 1;
                g = b = 0;
                remind = true;
                break;
            default:
#ifdef DEBUG_misha
                Assert(0);
#endif
                remind = true;
                fallback = true;
                break;
        }

        if (!fallback)
        {
            /* QImage only supports 32-bit aligned scan lines... */
            Assert ((re->bytesPerLine() & 3) == 0);
            fallback = ((re->bytesPerLine() & 3) != 0);
        }
        if (!fallback)
        {
            /* ...and the scan lines ought to be a whole number of pixels. */
            Assert ((bitsPerLine & (re->bitsPerPixel() - 1)) == 0);
            fallback = ((bitsPerLine & (re->bitsPerPixel() - 1)) != 0);
        }
        if (!fallback)
        {
            // ulong virtWdt = bitsPerLine / re->bitsPerPixel();
            mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
            mUsesGuestVRAM = true;
        }
    }
    else
    {
        fallback = true;
    }

    if (fallback)
    {
        /* we don't support either the pixel format or the color depth,
         * fallback to a self-provided 32bpp RGB buffer */
        bitsPerPixel = 32;
        b = 0xff;
        g = 0xff00;
        r = 0xff0000;
        bytesPerLine = re->width()*bitsPerPixel/8;
        mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
//        internalformat = 3;//GL_RGB;
//        format = GL_BGRA_EXT;//GL_RGBA;
//        type = GL_UNSIGNED_BYTE;
        mUsesGuestVRAM = false;
    }

    ulong bytesPerPixel = bitsPerPixel/8;
    ulong displayWidth = bytesPerLine/bytesPerPixel;
    ulong displayHeight = re->height();

#ifdef VBOXQGL_DBG_SURF
    for(int i = 0; i < RT_ELEMENTS(g_apSurf); i++)
    {
        VBoxVHWASurfaceBase * pSurf1 = g_apSurf[i];
        if(pSurf1)
        {
            VBoxVHWASurfList *pConstructingList = pSurf1->getComplexList();
            delete pSurf1;
            if(pConstructingList)
                delete pConstructingList;
    //
    //            mDisplay.addOverlay(pConstructingList);
    //            //            pConstructingList->add(pSurf1);
    //            //            pConstructingList->setCurrentVisible(pSurf1);
    //            ////            pConstructingList->setCurrentVisible(NULL);
        }
    }
#endif

    VBoxVHWASurfaceBase * pDisplay = mDisplay.setVGA(NULL);
    if(pDisplay)
        delete pDisplay;

    VBoxVHWAColorFormat format(bitsPerPixel, r,g,b);
    QSize dispSize(displayWidth, displayHeight);
    QRect dispRect(0, 0, displayWidth, displayHeight);
    pDisplay = new VBoxVHWASurfaceBase(this,
            dispSize,
            dispRect,
            dispRect,
            dispRect, /* we do not know viewport at the stage of recise, set as a disp rect, it will be updated on repaint */
            format,
            (VBoxVHWAColorKey*)NULL, (VBoxVHWAColorKey*)NULL, (VBoxVHWAColorKey*)NULL, (VBoxVHWAColorKey*)NULL, true);
    pDisplay->init(NULL, mUsesGuestVRAM ? re->VRAM() : NULL);
    mDisplay.setVGA(pDisplay);
//    VBOXQGLLOG(("\n\n*******\n\n     viewport size is: (%d):(%d)\n\n*******\n\n", size().width(), size().height()));
    mViewport = QRect(0,0,displayWidth, displayHeight);
    adjustViewport(dispSize, mViewport);
    setupMatricies(dispSize);

#ifdef VBOXQGL_DBG_SURF
    {
        uint32_t width = 100;
        uint32_t height = 60;
//        uint32_t rgbBitCount = 32;
//        uint32_t r = 0xff, g = 0xff00, b = 0xff0000;
//        QRect dstRect(150, 200, width, height);
//        QRect srcRect(0, 0, 720, 480);
    //    Assert(0);

        for(int i = 0; i < RT_ELEMENTS(g_apSurf); i++)
        {

    //        pSurf1 = new VBoxVHWASurfaceBase(this, width, height,
    //                        VBoxVHWAColorFormat(rgbBitCount,
    //                                r,
    //                                g,
    //                                b),
    //                        NULL, NULL, NULL, NULL);
            VBoxVHWASurfaceBase *pSurf1 = new VBoxVHWASurfaceBase(this, &QSize(width, height),
    //                        ((pCmd->SurfInfo.surfCaps & VBOXVHWA_SCAPS_OVERLAY) ? mDisplay.getPrimary()->rect().size() : &QSize(pCmd->SurfInfo.width, pCmd->SurfInfo.height)),
                            &mDisplay.getPrimary()->rect().size(),
//                            VBoxVHWAColorFormat(rgbBitCount,
//                                    r,
//                                    g,
//                                    b),
                            VBoxVHWAColorFormat(FOURCC_YV12),
    //                        pSrcBltCKey, pDstBltCKey, pSrcOverlayCKey, pDstOverlayCKey);
                            NULL, NULL, NULL, &VBoxVHWAColorKey(0,0), false);

            Assert(mDisplay.getVGA());
            pSurf1->init(mDisplay.getVGA(), NULL);
            uchar *addr = pSurf1->address();
            uchar cur = 0;
            for(uint32_t k = 0; k < width*height; k++)
            {
                addr[k] = cur;
                cur+=64;
            }
            pSurf1->updatedMem(&QRect(0,0,width, height));
//            VBOXQGL_CHECKERR(
//                    vboxglActiveTexture(GL_TEXTURE0);
//                );

            VBoxVHWASurfList *pConstructingList = new VBoxVHWASurfList();
            mDisplay.addOverlay(pConstructingList);
            pConstructingList->add(pSurf1);
//            pConstructingList->setCurrentVisible(pSurf1);
//            pConstructingList->setCurrentVisible(NULL);
            g_apSurf[i] = pSurf1;

//            VBoxVHWAGlProgramVHWA * pProgram = vboxVHWAGetGlProgramMngr()->getProgram(true, false, &pSurf1->colorFormat(), &pDisplay->colorFormat());
//            pProgram->start();
//            pProgram->setSrcTexImgWidth(pSurf1->texRect().width());
//            pProgram->stop();
        }
//        else
//        {
//            VBoxVHWASurfList *pConstructingList = pSurf1->getComplexList();
//            mDisplay.addOverlay(pConstructingList);
//            pConstructingList->add(pSurf1);
//            pConstructingList->setCurrentVisible(pSurf1);
////            pConstructingList->setCurrentVisible(NULL);
//        }

        VBOXVHWACMD_SURF_OVERLAY_UPDATE updateCmd;
        memset(&updateCmd, 0, sizeof(updateCmd));
        updateCmd.u.in.hSrcSurf = (VBOXVHWA_SURFHANDLE)g_apSurf[0];
        updateCmd.u.in.hDstSurf = (VBOXVHWA_SURFHANDLE)pDisplay;
        updateCmd.u.in.flags =
                VBOXVHWA_OVER_SHOW
                | VBOXVHWA_OVER_KEYDESTOVERRIDE;

        updateCmd.u.in.desc.DstCK.high = 1;
        updateCmd.u.in.desc.DstCK.low = 1;

        updateCmd.u.in.dstRect.left = 0;
        updateCmd.u.in.dstRect.right = pDisplay->width();
        updateCmd.u.in.dstRect.top = (pDisplay->height() - height)/2;
        updateCmd.u.in.dstRect.bottom = updateCmd.u.in.dstRect.top + height;

        updateCmd.u.in.srcRect.left = 0;
        updateCmd.u.in.srcRect.right = width;
        updateCmd.u.in.srcRect.top = 0;
        updateCmd.u.in.srcRect.bottom = height;

        updateCmd.u.in.offDstSurface = 0xffffffffffffffffL; /* just a magic to avoid surf mem buffer change  */
        updateCmd.u.in.offSrcSurface = 0xffffffffffffffffL; /* just a magic to avoid surf mem buffer change  */

        vhwaSurfaceOverlayUpdate(&updateCmd);
    }
#endif

    if(!mOnResizeCmdList.empty())
    {
        for (VHWACommandList::const_iterator it = mOnResizeCmdList.begin();
             it != mOnResizeCmdList.end(); ++ it)
        {
            VBOXVHWACMD * pCmd = (*it);
            vboxDoVHWACmdExec(pCmd);
            free(pCmd);
        }
        mOnResizeCmdList.clear();
    }


//    mDisplay.performDisplay();

    if (remind)
    {
        class RemindEvent : public VBoxAsyncEvent
        {
            ulong mRealBPP;
        public:
            RemindEvent (ulong aRealBPP)
                : mRealBPP (aRealBPP) {}
            void handle()
            {
                vboxProblem().remindAboutWrongColorDepth (mRealBPP, 32);
            }
        };
        (new RemindEvent (re->bitsPerPixel()))->post();
    }
}

VBoxVHWAColorFormat::VBoxVHWAColorFormat(uint32_t bitsPerPixel, uint32_t r, uint32_t g, uint32_t b) :
    mWidthCompression(1),
    mHeightCompression(1)
{
    init(bitsPerPixel, r, g, b);
}

VBoxVHWAColorFormat::VBoxVHWAColorFormat(uint32_t fourcc) :
    mWidthCompression(1),
    mHeightCompression(1)
{
    init(fourcc);
}

void VBoxVHWAColorFormat::init(uint32_t fourcc)
{
    mDataFormat = fourcc;
    mInternalFormat = GL_RGBA8;//GL_RGB;
    mFormat = GL_BGRA_EXT;//GL_RGBA;
    mType = GL_UNSIGNED_BYTE;
    mR = VBoxVHWAColorComponent(0xff);
    mG = VBoxVHWAColorComponent(0xff);
    mB = VBoxVHWAColorComponent(0xff);
    mA = VBoxVHWAColorComponent(0xff);
    mBitsPerPixelTex = 32;

    switch(fourcc)
    {
        case FOURCC_AYUV:
            mBitsPerPixel = 32;
            mWidthCompression = 1;
            break;
        case FOURCC_UYVY:
        case FOURCC_YUY2:
            mBitsPerPixel = 16;
            mWidthCompression = 2;
            break;
        case FOURCC_YV12:
            mBitsPerPixel = 8;
            mWidthCompression = 4;
            break;
        default:
            Assert(0);
            mBitsPerPixel = 0;
            mBitsPerPixelTex = 0;
            mWidthCompression = 0;
            break;
    }
}

void VBoxVHWAColorFormat::init(uint32_t bitsPerPixel, uint32_t r, uint32_t g, uint32_t b)
{
    mBitsPerPixel = bitsPerPixel;
    mBitsPerPixelTex = bitsPerPixel;
    mDataFormat = 0;
    switch (bitsPerPixel)
    {
        case 32:
            mInternalFormat = GL_RGB;//3;//GL_RGB;
            mFormat = GL_BGRA_EXT;//GL_RGBA;
            mType = GL_UNSIGNED_BYTE;
            mR = VBoxVHWAColorComponent(r);
            mG = VBoxVHWAColorComponent(g);
            mB = VBoxVHWAColorComponent(b);
            break;
        case 24:
#ifdef DEBUG_misha
            Assert(0);
#endif
            mInternalFormat = 3;//GL_RGB;
            mFormat = GL_BGR_EXT;
            mType = GL_UNSIGNED_BYTE;
            mR = VBoxVHWAColorComponent(r);
            mG = VBoxVHWAColorComponent(g);
            mB = VBoxVHWAColorComponent(b);
            break;
        case 16:
#ifdef DEBUG_misha
            Assert(0);
#endif
            mInternalFormat = GL_RGB5;
            mFormat = GL_BGR_EXT;
            mType = GL_UNSIGNED_BYTE; /* TODO" ??? */
            mR = VBoxVHWAColorComponent(r);
            mG = VBoxVHWAColorComponent(g);
            mB = VBoxVHWAColorComponent(b);
            break;
        case 8:
#ifdef DEBUG_misha
            Assert(0);
#endif
            mInternalFormat = 1;//GL_RGB;
            mFormat = GL_RED;//GL_RGB;
            mType = GL_UNSIGNED_BYTE;
            mR = VBoxVHWAColorComponent(0xff);
            break;
        case 1:
#ifdef DEBUG_misha
            Assert(0);
#endif
            mInternalFormat = 1;
            mFormat = GL_COLOR_INDEX;
            mType = GL_BITMAP;
            mR = VBoxVHWAColorComponent(0x1);
            break;
        default:
#ifdef DEBUG_misha
            Assert(0);
#endif
            mBitsPerPixel = 0;
            mBitsPerPixelTex = 0;
            break;
    }
}

bool VBoxVHWAColorFormat::equals (const VBoxVHWAColorFormat & other) const
{
    if(fourcc())
        return fourcc() == other.fourcc();
    if(other.fourcc())
        return false;

    return bitsPerPixel() == other.bitsPerPixel();
}

VBoxVHWAColorComponent::VBoxVHWAColorComponent(uint32_t aMask)
{
    unsigned f = ASMBitFirstSetU32(aMask);
    if(f)
    {
        mOffset = f - 1;
        f = ASMBitFirstSetU32(~(aMask >> mOffset));
        if(f)
        {
            mcBits = f - 1;
        }
        else
        {
            mcBits = 32 - mOffset;
        }

        Assert(mcBits);
        mMask = (((uint32_t)0xffffffff) >> (32 - mcBits)) << mOffset;
        Assert(mMask == aMask);

        mRange = (mMask >> mOffset) + 1;
    }
    else
    {
        mMask = 0;
        mRange = 0;
        mOffset = 32;
        mcBits = 0;
    }
}

void VBoxVHWAColorFormat::pixel2Normalized (uint32_t pix, float *r, float *g, float *b) const
{
    *r = mR.colorValNorm(pix);
    *g = mG.colorValNorm(pix);
    *b = mB.colorValNorm(pix);
}

VBoxQGLOverlay::VBoxQGLOverlay (VBoxConsoleView *aView, VBoxFrameBuffer * aContainer)
    : mView(aView),
      mContainer(aContainer),
      mGlOn(false),
      mOverlayWidgetVisible(false),
      mOverlayVisible(false),
      mGlCurrent(false),
      mProcessingCommands(false),
      mNeedOverlayRepaint(false),
      mCmdPipe(aView)
{
    mpOverlayWidget = new VBoxGLWidget (aView, aView->viewport());
    mOverlayWidgetVisible = true; /* to ensure it is set hidden with vboxShowOverlay */
    vboxShowOverlay(false);

//    resizeEvent (new VBoxResizeEvent (FramebufferPixelFormat_Opaque,
//                                      NULL, 0, 0, 640, 480));
}

int VBoxQGLOverlay::onVHWACommand(struct _VBOXVHWACMD * pCmd)
{
    uint32_t flags = 0;
    switch(pCmd->enmCmd)
    {
        case VBOXVHWACMD_TYPE_SURF_FLIP:
        case VBOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE:
        case VBOXVHWACMD_TYPE_SURF_OVERLAY_SETPOSITION:
            flags |= VBOXVHWACMDPIPEC_COMPLETEEVENT;
            break;
        default:
            break;
    }//    Assert(0);
    /* indicate that we process and complete the command asynchronously */
    pCmd->Flags |= VBOXVHWACMD_FLAG_HG_ASYNCH;

    mCmdPipe.postCmd(VBOXVHWA_PIPECMD_VHWA, pCmd, flags);
    return VINF_SUCCESS;

}

void VBoxQGLOverlay::onVHWACommandEvent(QEvent * pEvent)
{
    Q_UNUSED(pEvent);
    Assert(!mProcessingCommands);
    mProcessingCommands = true;
    Assert(!mGlCurrent);
    mGlCurrent = false; /* just a fall-back */
    VBoxVHWACommandElement * pFirst = mCmdPipe.detachCmdList(NULL, NULL);
    do
    {
        VBoxVHWACommandElement * pLast = processCmdList(pFirst);

        pFirst = mCmdPipe.detachCmdList(pFirst, pLast);
    } while(pFirst);

    mProcessingCommands = false;
    repaint();
//    vboxOpExit();
    mGlCurrent = false;
}

bool VBoxQGLOverlay::onNotifyUpdate(ULONG aX, ULONG aY,
                         ULONG aW, ULONG aH)
{
#if 1
    QRect r(aX, aY, aW, aH);
    mCmdPipe.postCmd(VBOXVHWA_PIPECMD_PAINT, &r, 0);
    return true;
#else
    /* We're not on the GUI thread and update() isn't thread safe in
     * Qt 4.3.x on the Win, Qt 3.3.x on the Mac (4.2.x is),
     * on Linux (didn't check Qt 4.x there) and probably on other
     * non-DOS platforms, so post the event instead. */
    QApplication::postEvent (mView,
                             new VBoxRepaintEvent (aX, aY, aW, aH));

    return S_OK;
#endif
}

//VBOXFBOVERLAY_RESUT VBoxQGLOverlay::onPaintEvent (const QPaintEvent *pe, QRect *pRect)
//{
//    Q_UNUSED(pe);
//    Q_UNUSED(pRect);
//
////    if(mOverlayWidgetVisible && !mProcessingCommands)
////    {
////        Assert(!mGlCurrent);
////        vboxDoCheckUpdateViewport();
////        vboxOpExit();
////    }
//    return VBOXFBOVERLAY_UNTOUCHED;
//}

void VBoxQGLOverlay::onResizeEvent (const VBoxResizeEvent *re)
{
    Q_UNUSED(re);
}

void VBoxQGLOverlay::onResizeEventPostprocess (const VBoxResizeEvent *re)
{
    Q_UNUSED(re);

    if(mGlOn)
    {
        Assert(!mGlCurrent);
        Assert(!mNeedOverlayRepaint);
        mGlCurrent = false;
        makeCurrent();
        /* need to ensure we're in synch */
        mNeedOverlayRepaint = vboxSynchGl();
    }

    if(!mOnResizeCmdList.empty())
    {
        for (VHWACommandList::const_iterator it = mOnResizeCmdList.begin();
             it != mOnResizeCmdList.end(); ++ it)
        {
            VBOXVHWACMD * pCmd = (*it);
            vboxDoVHWACmdExec(pCmd);
            free(pCmd);
        }
        mOnResizeCmdList.clear();
    }

    repaintOverlay();
    mGlCurrent = false;
}

void VBoxQGLOverlay::repaintMain()
{
    if(mMainDirtyRect.isClear())
        return;

    const QRect &rect = mMainDirtyRect.rect();
    if(mOverlayWidgetVisible)
    {
        if(mOverlayViewport.contains(rect))
            return;
    }

    mView->viewport()->repaint (rect.x() - mView->contentsX(),
            rect.y() - mView->contentsY(),
            rect.width(), rect.height());

    mMainDirtyRect.clear();
}

void VBoxQGLOverlay::vboxDoVHWACmd(void *cmd)
{
    vboxDoVHWACmdExec(cmd);

    CDisplay display = mView->console().GetDisplay();
    Assert (!display.isNull());

    display.CompleteVHWACommand((BYTE*)cmd);
}

//void VBoxQGLOverlay::vboxDoUpdateRect(const QRect * pRect)
//{
//    if(mGlOn)
//    {
//        makeCurrent();
//        mpOverlayWidget->vboxDoUpdateRect(pRect);
//        vboxOpExit();
//    }
//
//    mView->viewport()->repaint (pRect->x() - mView->contentsX(),
//            pRect->y() - mView->contentsY(),
//            pRect->width(), pRect->height());
//
//    /* translate to widget coords
//     * @todo: may eliminate this */
////    QPaintEvent pe(pRect->translated(-mView->contentsX(), -mView->contentsY()));
////    VBoxQImageFrameBuffer::paintEvent (&pe);
//}

bool VBoxQGLOverlay::vboxSynchGl()
{
    if(mpOverlayWidget->vboxIsInitialized()
            && mContainer->pixelFormat() == mpOverlayWidget->vboxPixelFormat()
            && mContainer->address() == mpOverlayWidget->vboxAddress()
            && mContainer->bitsPerPixel() == mpOverlayWidget->vboxBitsPerPixel()
            && mContainer->bytesPerLine() == mpOverlayWidget->vboxBytesPerLine()
            && mContainer->width() == mpOverlayWidget->vboxFbWidth()
            && mContainer->height() == mpOverlayWidget->vboxFbHeight())
    {
        return false;
    }
    /* create and issue a resize event to the gl widget to ensure we have all gl data initialized
     * and synchronized with the framebuffer */
    VBoxResizeEvent re(mContainer->pixelFormat(),
            mContainer->address(),
            mContainer->bitsPerPixel(),
            mContainer->bytesPerLine(),
            mContainer->width(),
            mContainer->height());

    mpOverlayWidget->vboxResizeEvent(&re);
    return true;
}

void VBoxQGLOverlay::vboxSetGlOn(bool on)
{
    if(on == mGlOn)
        return;

    mGlOn = on;

    if(on)
    {
        VBOXQGLLOGREL(("Switching Gl mode on\n"));
        Assert(!mpOverlayWidget->isVisible());
        /* just to ensure */
        vboxShowOverlay(false);
        mOverlayVisible = false;
        vboxSynchGl();
    }
    else
    {
        VBOXQGLLOGREL(("Switching Gl mode off\n"));
        mOverlayVisible = false;
        vboxShowOverlay(false);
        /* for now just set the flag w/o destroying anything */
    }
}

void VBoxQGLOverlay::vboxDoCheckUpdateViewport()
{
    if(!mOverlayVisible)
    {
        vboxShowOverlay(false);
        return;
    }

    int cX = mView->contentsX();
    int cY = mView->contentsY();
    QRect fbVp(cX, cY, mView->viewport()->width(), mView->viewport()->height());
    QRect overVp = fbVp.intersected(mOverlayViewport);

    if(overVp.isEmpty())
    {
        vboxShowOverlay(false);
    }
    else
    {
        if(overVp != mpOverlayWidget->vboxViewport())
        {
            makeCurrent();
            mpOverlayWidget->vboxDoUpdateViewport(overVp);
            mNeedOverlayRepaint = true;
        }

        QRect rect(overVp.x() - cX, overVp.y() - cY, overVp.width(), overVp.height());

        vboxCheckUpdateOverlay(rect);

        vboxShowOverlay(true);
    }
}

void VBoxQGLOverlay::vboxShowOverlay(bool show)
{
    if(mOverlayWidgetVisible != show)
    {
        mpOverlayWidget->setVisible(show);
        mOverlayWidgetVisible = show;
    }
}

//void VBoxQGLOverlayFrameBuffer::vboxUpdateOverlayPosition(const QPoint & pos)
//{
////    makeCurrent();
//
//    mpOverlayWidget->move(pos);
//
////    /* */
////    QRect rect = mpOverlayWidget->vboxViewport();
////    rect.moveTo(pos);
////    mpOverlayWidget->vboxDoUpdateViewport(rect);
//}

void VBoxQGLOverlay::vboxCheckUpdateOverlay(const QRect & rect)
{
    QRect overRect = mpOverlayWidget->rect();
    if(overRect.x() != rect.x() || overRect.y() != rect.y())
    {
        mpOverlayWidget->move(rect.x(), rect.y());
    }

    if(overRect.width() != rect.width() || overRect.height() != rect.height())
    {
        mpOverlayWidget->resize(rect.width(), rect.height());
    }

//    mpOverlayWidget->vboxDoUpdateViewport(rect);
//
//    vboxShowOverlay(show);
}

void VBoxQGLOverlay::addMainDirtyRect(const QRect & aRect)
{
    mMainDirtyRect.add(aRect);
    if(mGlOn)
    {
        mpOverlayWidget->vboxDoUpdateRect(&aRect);
        mNeedOverlayRepaint = true;
    }
}

int VBoxQGLOverlay::vhwaSurfaceUnlock(struct _VBOXVHWACMD_SURF_UNLOCK *pCmd)
{
    int rc = mpOverlayWidget->vhwaSurfaceUnlock(pCmd);
    VBoxVHWASurfaceBase * pVGA = mpOverlayWidget->vboxGetVGASurface();
    const VBoxVHWADirtyRect & rect = pVGA->getDirtyRect();
    mNeedOverlayRepaint = true;
    if(!rect.isClear())
    {
        mMainDirtyRect.add(rect);
    }
    return rc;
}

void VBoxQGLOverlay::vboxDoVHWACmdExec(void *cmd)
{
    struct _VBOXVHWACMD * pCmd = (struct _VBOXVHWACMD *)cmd;
    makeCurrent();
    switch(pCmd->enmCmd)
    {
        case VBOXVHWACMD_TYPE_SURF_CANCREATE:
        {
            VBOXVHWACMD_SURF_CANCREATE * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_CANCREATE);
            pCmd->rc = mpOverlayWidget->vhwaSurfaceCanCreate(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_CREATE:
        {
            VBOXVHWACMD_SURF_CREATE * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_CREATE);
            vboxSetGlOn(true);
            pCmd->rc = mpOverlayWidget->vhwaSurfaceCreate(pBody);
            if(!mpOverlayWidget->hasSurfaces())
            {
                vboxSetGlOn(false);
            }
            else
            {
                mOverlayVisible = mpOverlayWidget->hasVisibleOverlays();
                if(mOverlayVisible)
                {
                    mOverlayViewport = mpOverlayWidget->overlaysRectUnion();
                }
                vboxDoCheckUpdateViewport();
                mNeedOverlayRepaint = true;
            }
        } break;
        case VBOXVHWACMD_TYPE_SURF_DESTROY:
        {
            VBOXVHWACMD_SURF_DESTROY * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_DESTROY);
            pCmd->rc = mpOverlayWidget->vhwaSurfaceDestroy(pBody);
            if(!mpOverlayWidget->hasSurfaces())
            {
                vboxSetGlOn(false);
            }
            else
            {
                mOverlayVisible = mpOverlayWidget->hasVisibleOverlays();
                if(mOverlayVisible)
                {
                    mOverlayViewport = mpOverlayWidget->overlaysRectUnion();
                }
                vboxDoCheckUpdateViewport();
                mNeedOverlayRepaint = true;
            }
        } break;
        case VBOXVHWACMD_TYPE_SURF_LOCK:
        {
            VBOXVHWACMD_SURF_LOCK * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_LOCK);
            pCmd->rc = mpOverlayWidget->vhwaSurfaceLock(pBody);
        } break;
        case VBOXVHWACMD_TYPE_SURF_UNLOCK:
        {
            VBOXVHWACMD_SURF_UNLOCK * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_UNLOCK);
            pCmd->rc = vhwaSurfaceUnlock(pBody);
            /* mNeedOverlayRepaint is set inside the vhwaSurfaceUnlock */
        } break;
        case VBOXVHWACMD_TYPE_SURF_BLT:
        {
            VBOXVHWACMD_SURF_BLT * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_BLT);
            pCmd->rc = mpOverlayWidget->vhwaSurfaceBlt(pBody);
            mNeedOverlayRepaint = true;
        } break;
        case VBOXVHWACMD_TYPE_SURF_FLIP:
        {
            VBOXVHWACMD_SURF_FLIP * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_FLIP);
            pCmd->rc = mpOverlayWidget->vhwaSurfaceFlip(pBody);
            mNeedOverlayRepaint = true;
        } break;
        case VBOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE:
        {
            VBOXVHWACMD_SURF_OVERLAY_UPDATE * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_OVERLAY_UPDATE);
            pCmd->rc = mpOverlayWidget->vhwaSurfaceOverlayUpdate(pBody);
            mOverlayVisible = mpOverlayWidget->hasVisibleOverlays();
            if(mOverlayVisible)
            {
                mOverlayViewport = mpOverlayWidget->overlaysRectUnion();
            }
            vboxDoCheckUpdateViewport();
            mNeedOverlayRepaint = true;
        } break;
        case VBOXVHWACMD_TYPE_SURF_OVERLAY_SETPOSITION:
        {
            VBOXVHWACMD_SURF_OVERLAY_SETPOSITION * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_OVERLAY_SETPOSITION);
            pCmd->rc = mpOverlayWidget->vhwaSurfaceOverlaySetPosition(pBody);
            mOverlayVisible = mpOverlayWidget->hasVisibleOverlays();
            if(mOverlayVisible)
            {
                mOverlayViewport = mpOverlayWidget->overlaysRectUnion();
            }
            vboxDoCheckUpdateViewport();
            mNeedOverlayRepaint = true;
        } break;
        case VBOXVHWACMD_TYPE_SURF_COLORKEY_SET:
        {
            VBOXVHWACMD_SURF_COLORKEY_SET * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_SURF_COLORKEY_SET);
            pCmd->rc = mpOverlayWidget->vhwaSurfaceColorkeySet(pBody);
            mNeedOverlayRepaint = true;
        } break;
        case VBOXVHWACMD_TYPE_QUERY_INFO1:
        {
            VBOXVHWACMD_QUERYINFO1 * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO1);
            pCmd->rc = mpOverlayWidget->vhwaQueryInfo1(pBody);
        } break;
        case VBOXVHWACMD_TYPE_QUERY_INFO2:
        {
            VBOXVHWACMD_QUERYINFO2 * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO2);
            pCmd->rc = mpOverlayWidget->vhwaQueryInfo2(pBody);
        } break;
        case VBOXVHWACMD_TYPE_ENABLE:
        case VBOXVHWACMD_TYPE_DISABLE:
            pCmd->rc = VINF_SUCCESS;
            break;
        case VBOXVHWACMD_TYPE_HH_CONSTRUCT:
        {
            VBOXVHWACMD_HH_CONSTRUCT * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_HH_CONSTRUCT);
            pCmd->rc = vhwaConstruct(pBody);
        } break;
        default:
            Assert(0);
            pCmd->rc = VERR_NOT_IMPLEMENTED;
            break;
    }
}

static DECLCALLBACK(void) vboxQGLOverlaySaveExec(PSSMHANDLE pSSM, void *pvUser)
{
    VBoxQGLOverlay * fb = (VBoxQGLOverlay*)pvUser;
    fb->vhwaSaveExec(pSSM);
}

static DECLCALLBACK(int) vboxQGLOverlayLoadExec(PSSMHANDLE pSSM, void *pvUser, uint32_t u32Version, uint32_t uPass)
{
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);
    VBoxQGLOverlay * fb = (VBoxQGLOverlay*)pvUser;
    return fb->vhwaLoadExec(pSSM, u32Version);
}

int VBoxQGLOverlay::vhwaLoadExec(struct SSMHANDLE * pSSM, uint32_t u32Version)
{
//    bool bTmp;
//    int rc = SSMR3GetBool(pSSM, &bTmp /*&mGlOn*/);         AssertRC(rc);
//    rc = SSMR3GetBool(pSSM, &bTmp /*&mOverlayVisible*/);         AssertRC(rc);
//    if(RT_SUCCESS(rc))
    return mpOverlayWidget->vhwaLoadExec(&mOnResizeCmdList, pSSM, u32Version);
//    return rc;
}

void VBoxQGLOverlay::vhwaSaveExec(struct SSMHANDLE * pSSM)
{
//    int rc = SSMR3PutBool(pSSM, mGlOn);         AssertRC(rc);
//    rc = SSMR3PutBool(pSSM, mOverlayVisible);         AssertRC(rc);
//
    mpOverlayWidget->vhwaSaveExec(pSSM);
}

int VBoxQGLOverlay::vhwaConstruct(struct _VBOXVHWACMD_HH_CONSTRUCT *pCmd)
{
    PVM pVM = (PVM)pCmd->pVM;
    uint32_t intsId = 0; /* @todo: set the proper id */

    char nameFuf[sizeof(VBOXQGL_STATE_NAMEBASE) + 8];

    char * pszName = nameFuf;
    sprintf(pszName, "%s%d", VBOXQGL_STATE_NAMEBASE, intsId);
    int rc = SSMR3RegisterExternal(
            pVM,                    /* The VM handle*/
            pszName,                /* Data unit name. */
            intsId,                 /* The instance identifier of the data unit.
                                     * This must together with the name be unique. */
            VBOXQGL_STATE_VERSION,   /* Data layout version number. */
            128,             /* The approximate amount of data in the unit.
                              * Only for progress indicators. */
            NULL, NULL, NULL, /* pfnLiveXxx */
            NULL,            /* Prepare save callback, optional. */
            vboxQGLOverlaySaveExec, /* Execute save callback, optional. */
            NULL,            /* Done save callback, optional. */
            NULL,            /* Prepare load callback, optional. */
            vboxQGLOverlayLoadExec, /* Execute load callback, optional. */
            NULL,            /* Done load callback, optional. */
            this             /* User argument. */
            );
    Assert(RT_SUCCESS(rc));
    return rc;
}

/* static */
bool VBoxQGLOverlay::isAcceleration2DVideoAvailable()
{
    vboxVHWAGlInit(NULL);
    return vboxVHWASupportedInternal();
}

VBoxVHWACommandElement * VBoxQGLOverlay::processCmdList(VBoxVHWACommandElement * pCmd)
{
    VBoxVHWACommandElement * pCur;
    do
    {
        pCur = pCmd;
        switch(pCmd->type())
        {
        case VBOXVHWA_PIPECMD_PAINT:
            addMainDirtyRect(pCmd->rect());
            break;
#ifdef VBOX_WITH_VIDEOHWACCEL
        case VBOXVHWA_PIPECMD_VHWA:
            vboxDoVHWACmd(pCmd->vhwaCmd());
            break;
        case VBOXVHWA_PIPECMD_OP:
        {
            const VBOXVHWACALLBACKINFO & info = pCmd->op();
            (info.pThis->*(info.pfnCallback))(info.pContext);
            break;
        }
        case VBOXVHWA_PIPECMD_FUNC:
        {
            const VBOXVHWAFUNCCALLBACKINFO & info = pCmd->func();
            info.pfnCallback(info.pContext1, info.pContext2);
            break;
        }
#endif
        default:
            Assert(0);
        }
        pCmd = pCmd->mpNext;
    } while(pCmd);

    return pCur;
}


VBoxVHWACommandElementProcessor::VBoxVHWACommandElementProcessor(VBoxConsoleView *aView)
{
    int rc = RTCritSectInit(&mCritSect);
    AssertRC(rc);

    mpFirstEvent = NULL;
    mpLastEvent = NULL;
    mbNewEvent = false;
    mView = aView;
    for(int i = RT_ELEMENTS(mElementsBuffer) - 1; i >= 0; i--)
    {
        mFreeElements.push(&mElementsBuffer[i]);
    }
}

VBoxVHWACommandElementProcessor::~VBoxVHWACommandElementProcessor()
{
    RTCritSectDelete(&mCritSect);
}

void VBoxVHWACommandElementProcessor::completeCurrentEvent()
{
    RTCritSectEnter(&mCritSect);
    mbNewEvent = true;
    RTCritSectLeave(&mCritSect);
}

void VBoxVHWACommandElementProcessor::postCmd(VBOXVHWA_PIPECMD_TYPE aType, void * pvData, uint32_t flags)
{
    /* 1. lock*/
    RTCritSectEnter(&mCritSect);
    VBoxVHWACommandElement * pCmd = mFreeElements.pop();
    if(!pCmd)
    {
        VBOXQGLLOG(("!!!no more free elements!!!\n"));
#ifdef VBOXQGL_PROF_BASE
        RTCritSectLeave(&mCritSect);
        return;
#else
    //TODO:
#endif
    }
    pCmd->setData(aType, pvData);

    if((flags & VBOXVHWACMDPIPEC_NEWEVENT) != 0)
    {
        mbNewEvent = true;
    }

    /* 2. if can add to current*/
    if(!mbNewEvent)
    {
        /* 3. if event is being processed (event != 0) */
        if(mpLastEvent)
        {
            /* 3.a add cmd to event */
            mpLastEvent->pipe().put(pCmd);
            /* 3.b unlock and return */
            if((flags & VBOXVHWACMDPIPEC_COMPLETEEVENT) != 0)
            {
                mbNewEvent = true;
            }
            RTCritSectLeave(&mCritSect);
            return;
        }
    }

    /* we're here because the cmd was NOT be added to the current event queue */
    /* 4. unlock*/
    RTCritSectLeave(&mCritSect);
    /* 5. create & initialize new Event */
    VBoxVHWACommandProcessEvent *pCurrentEvent = new VBoxVHWACommandProcessEvent(pCmd);
    pCurrentEvent->mpNext = NULL;

    /* 6. lock */
    RTCritSectEnter(&mCritSect);
    /* 7. if no current event set event as current */
    if(!mpLastEvent)
    {
        Assert(!mpFirstEvent);
        mpFirstEvent = pCurrentEvent;
        mpLastEvent = pCurrentEvent;
    }
    else
    {
        mpLastEvent->mpNext = pCurrentEvent;
        mpLastEvent = pCurrentEvent;
    }
    /* 8. reset blocking events counter */
    mbNewEvent = ((flags & VBOXVHWACMDPIPEC_COMPLETEEVENT) != 0);
    /* 9. unlock */
    RTCritSectLeave(&mCritSect);
    /* 10. post event */
    QApplication::postEvent (mView, pCurrentEvent);
}

VBoxVHWACommandElement * VBoxVHWACommandElementProcessor::detachCmdList(VBoxVHWACommandElement * pFirst2Free, VBoxVHWACommandElement * pLast2Free)
{
    VBoxVHWACommandElement * pList = NULL;
    RTCritSectEnter(&mCritSect);
    if(pFirst2Free)
    {
        mFreeElements.pusha(pFirst2Free, pLast2Free);
    }
    if(mpFirstEvent)
    {
        pList = mpFirstEvent->pipe().detachList();
        if(!pList)
        {
            VBoxVHWACommandProcessEvent *pNext = mpFirstEvent->mpNext;
            if(pNext)
            {
                mpFirstEvent = pNext;
            }
            else
            {
                mpFirstEvent = NULL;
                mpLastEvent = NULL;
            }
        }
    }
    RTCritSectLeave(&mCritSect);

    return pList;
}


void VBoxVHWACommandsQueue::enqueue(PFNVBOXQGLFUNC pfn, void* pContext1, void* pContext2)
{
    VBoxVHWACommandElement *pCmd = new VBoxVHWACommandElement();
    VBOXVHWAFUNCCALLBACKINFO info;
    info.pfnCallback = pfn;
    info.pContext1 = pContext1;
    info.pContext2 = pContext2;
    pCmd->setFunc(info);
    mCmds.put(pCmd);
}

VBoxVHWACommandElement * VBoxVHWACommandsQueue::detachList()
{
    return mCmds.detachList();
}

void VBoxVHWACommandsQueue::freeList(VBoxVHWACommandElement * pList)
{
    while(pList)
    {
        VBoxVHWACommandElement * pCur = pList;
        pList = pCur->mpNext;
        delete pCur;
    }
}

#endif
