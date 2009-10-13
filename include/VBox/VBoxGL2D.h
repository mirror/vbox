/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * OpenGL support info used for 2D support detection
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
#ifndef __VBoxGLSupportInfo_h__
#define __VBoxGLSupportInfo_h__

#include <iprt/types.h>

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

/* @todo: move those to VBoxGLInfo class instance members ??? */
extern PFNVBOXVHWA_ACTIVE_TEXTURE vboxglActiveTexture;
extern PFNVBOXVHWA_MULTI_TEX_COORD2I vboxglMultiTexCoord2i;
extern PFNVBOXVHWA_MULTI_TEX_COORD2D vboxglMultiTexCoord2d;
extern PFNVBOXVHWA_MULTI_TEX_COORD2F vboxglMultiTexCoord2f;


extern PFNVBOXVHWA_CREATE_SHADER   vboxglCreateShader;
extern PFNVBOXVHWA_SHADER_SOURCE   vboxglShaderSource;
extern PFNVBOXVHWA_COMPILE_SHADER  vboxglCompileShader;
extern PFNVBOXVHWA_DELETE_SHADER   vboxglDeleteShader;

extern PFNVBOXVHWA_CREATE_PROGRAM  vboxglCreateProgram;
extern PFNVBOXVHWA_ATTACH_SHADER   vboxglAttachShader;
extern PFNVBOXVHWA_DETACH_SHADER   vboxglDetachShader;
extern PFNVBOXVHWA_LINK_PROGRAM    vboxglLinkProgram;
extern PFNVBOXVHWA_USE_PROGRAM     vboxglUseProgram;
extern PFNVBOXVHWA_DELETE_PROGRAM  vboxglDeleteProgram;

extern PFNVBOXVHWA_IS_SHADER       vboxglIsShader;
extern PFNVBOXVHWA_GET_SHADERIV    vboxglGetShaderiv;
extern PFNVBOXVHWA_IS_PROGRAM      vboxglIsProgram;
extern PFNVBOXVHWA_GET_PROGRAMIV   vboxglGetProgramiv;
extern PFNVBOXVHWA_GET_ATTACHED_SHADERS vboxglGetAttachedShaders;
extern PFNVBOXVHWA_GET_SHADER_INFO_LOG  vboxglGetShaderInfoLog;
extern PFNVBOXVHWA_GET_PROGRAM_INFO_LOG vboxglGetProgramInfoLog;

extern PFNVBOXVHWA_GET_UNIFORM_LOCATION vboxglGetUniformLocation;

extern PFNVBOXVHWA_UNIFORM1F vboxglUniform1f;
extern PFNVBOXVHWA_UNIFORM2F vboxglUniform2f;
extern PFNVBOXVHWA_UNIFORM3F vboxglUniform3f;
extern PFNVBOXVHWA_UNIFORM4F vboxglUniform4f;

extern PFNVBOXVHWA_UNIFORM1I vboxglUniform1i;
extern PFNVBOXVHWA_UNIFORM2I vboxglUniform2i;
extern PFNVBOXVHWA_UNIFORM3I vboxglUniform3i;
extern PFNVBOXVHWA_UNIFORM4I vboxglUniform4i;

extern PFNVBOXVHWA_GEN_BUFFERS vboxglGenBuffers;
extern PFNVBOXVHWA_DELETE_BUFFERS vboxglDeleteBuffers;
extern PFNVBOXVHWA_BIND_BUFFER vboxglBindBuffer;
extern PFNVBOXVHWA_BUFFER_DATA vboxglBufferData;
extern PFNVBOXVHWA_MAP_BUFFER vboxglMapBuffer;
extern PFNVBOXVHWA_UNMAP_BUFFER vboxglUnmapBuffer;

class VBoxGLInfo
{
public:
    VBoxGLInfo() :
        mGLVersion(0),
        mFragmentShaderSupported(false),
        mTextureRectangleSupported(false),
        mTextureNP2Supported(false),
        mPBOSupported(false),
        mMultiTexNumSupported(1), /* 1 would mean it is not supported */
        m_GL_ARB_multitexture(false),
        m_GL_ARB_shader_objects(false),
        m_GL_ARB_fragment_shader(false),
        m_GL_ARB_pixel_buffer_object(false),
        m_GL_ARB_texture_rectangle(false),
        m_GL_EXT_texture_rectangle(false),
        m_GL_NV_texture_rectangle(false),
        m_GL_ARB_texture_non_power_of_two(false),
        mInitialized(false)
    {}

    void init(const class QGLContext * pContext);

    bool isInitialized() const { return mInitialized; }

    int getGLVersion() const { return mGLVersion; }
    bool isFragmentShaderSupported() const { return mFragmentShaderSupported; }
    bool isTextureRectangleSupported() const { return mTextureRectangleSupported; }
    bool isTextureNP2Supported() const { return mTextureNP2Supported; }
    bool isPBOSupported() const { return mPBOSupported; }
    /* 1 would mean it is not supported */
    int getMultiTexNumSupported() const { return mMultiTexNumSupported; }

    static int parseVersion(const GLubyte * ver);
private:
    void initExtSupport(const class QGLContext & context);

    int mGLVersion;
    bool mFragmentShaderSupported;
    bool mTextureRectangleSupported;
    bool mTextureNP2Supported;
    bool mPBOSupported;
    int mMultiTexNumSupported; /* 1 would mean it is not supported */

    bool m_GL_ARB_multitexture;
    bool m_GL_ARB_shader_objects;
    bool m_GL_ARB_fragment_shader;
    bool m_GL_ARB_pixel_buffer_object;
    bool m_GL_ARB_texture_rectangle;
    bool m_GL_EXT_texture_rectangle;
    bool m_GL_NV_texture_rectangle;
    bool m_GL_ARB_texture_non_power_of_two;

    bool mInitialized;
};

class VBoxGLTmpContext
{
public:
    VBoxGLTmpContext();
    ~VBoxGLTmpContext();

    const class QGLContext * makeCurrent();
private:
    class QGLWidget * mWidget;
};


#define VBOXQGL_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
                ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))

#define FOURCC_AYUV VBOXQGL_MAKEFOURCC('A', 'Y', 'U', 'V')
#define FOURCC_UYVY VBOXQGL_MAKEFOURCC('U', 'Y', 'V', 'Y')
#define FOURCC_YUY2 VBOXQGL_MAKEFOURCC('Y', 'U', 'Y', '2')
#define FOURCC_YV12 VBOXQGL_MAKEFOURCC('Y', 'V', '1', '2')
#define VBOXVHWA_NUMFOURCC 4

class VBoxVHWAInfo
{
public:
    VBoxVHWAInfo() :
        mFourccSupportedCount(0),
        mInitialized(false)
    {}

    VBoxVHWAInfo(const VBoxGLInfo & glInfo) :
        mglInfo(glInfo),
        mFourccSupportedCount(0),
        mInitialized(false)
    {}

    void init(const class QGLContext * pContext);

    bool isInitialized() const { return mInitialized; }

    const VBoxGLInfo & getGlInfo() const { return mglInfo; }

    bool isVHWASupported() const;

    int getFourccSupportedCount() const { return mFourccSupportedCount; }
    const uint32_t * getFourccSupportedList() const { return mFourccSupportedList; }

    static bool checkVHWASupport();
private:
    VBoxGLInfo mglInfo;
    uint32_t mFourccSupportedList[VBOXVHWA_NUMFOURCC];
    int mFourccSupportedCount;

    bool mInitialized;
};

#endif /* #ifndef __VBoxGLSupportInfo_h__ */
