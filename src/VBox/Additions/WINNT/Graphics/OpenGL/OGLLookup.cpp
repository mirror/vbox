/** @file
 *
 * VirtualBox Windows NT/2000/XP guest OpenGL ICD
 *
 * Parameter size helpers
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#include "VBoxOGL.h"
#include <iprt/cdefs.h>
#include <iprt/assert.h>

/* DataType */
static GLuint glDataTypeSize[11] = 
{
/* GL_BYTE */                           sizeof(GLbyte),
/* GL_UNSIGNED_BYTE */                  sizeof(GLubyte),
/* GL_SHORT */                          sizeof(GLshort),
/* GL_UNSIGNED_SHORT */                 sizeof(GLushort),
/* GL_INT */                            sizeof(GLint),
/* GL_UNSIGNED_INT */                   sizeof(GLuint),
/* GL_FLOAT */                          sizeof(GLfloat),
/* GL_2_BYTES */                        2,
/* GL_3_BYTES */                        3,
/* GL_4_BYTES */                        4,
/* GL_DOUBLE */                         sizeof(GLdouble),
};

/**
 * Query the size of the specified data type
 *
 * @returns type size or 0 if unknown type
 * @param   type        data type
 */
GLint glVBoxGetDataTypeSize(GLenum type)
{
    if (type - GL_BYTE >= RT_ELEMENTS(glDataTypeSize))
    {
        return 0;
    }
    return glDataTypeSize[type-GL_BYTE];
}

/**
 * Query the specified cached parameter
 * 
 * @returns requested cached value
 * @param   type        Parameter type (Note: minimal checks only!)
 */
GLint glInternalGetIntegerv(GLenum type)
{
    AssertFailed();
    return 0;
}

/**
 * Query the specified cached texture level parameter
 * 
 * @returns requested cached value
 * @param   type        Parameter type (Note: minimal checks only!)
 */
GLint glInternalGetTexLevelParameteriv(GLenum type)
{
    AssertFailed();
    return 0;
}


uint32_t glInternalLightvElem(GLenum pname)
{
    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_POSITION:
        return 4;

    case GL_SPOT_DIRECTION:
        return 3;

    case GL_SPOT_EXPONENT:
    case GL_SPOT_CUTOFF:
    case GL_CONSTANT_ATTENUATION:
    case GL_LINEAR_ATTENUATION:
    case GL_QUADRATIC_ATTENUATION:
        return 1;

    default:
        AssertMsgFailed(("%s Unknown element %x\n", __FUNCTION__, pname));
        return 0;
    }
}


uint32_t glInternalMaterialvElem(GLenum pname)
{
    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_AMBIENT_AND_DIFFUSE:
        return 4;

    case GL_SHININESS:
        return 1;

    case GL_COLOR_INDEXES:
        return 3;

    default:
        AssertMsgFailed(("%s Unknown element %x\n", __FUNCTION__, pname));
        return 0;
    }
}

uint32_t glInternalTexEnvvElem(GLenum pname)
{
    switch (pname)
    {
    case GL_TEXTURE_ENV_MODE:
        return 1;

    case GL_TEXTURE_ENV_COLOR:
        return 4;

    default:
        AssertMsgFailed(("%s Unknown element %x\n", __FUNCTION__, pname));
        return 0;
    }
}

uint32_t glInternalTexGenvElem(GLenum pname)
{
    switch (pname)
    {
    case GL_TEXTURE_GEN_MODE:
        return 1;

    case GL_OBJECT_PLANE:
    case GL_EYE_PLANE:
        return 4;

    default:
        AssertMsgFailed(("%s Unknown element %x\n", __FUNCTION__, pname));
        return 0;
    }
}

uint32_t glInternalTexParametervElem(GLenum pname)
{
    switch (pname)
    {
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
    case GL_TEXTURE_PRIORITY:
        return 1;

    case GL_TEXTURE_BORDER_COLOR:
        return 4;

    default:
        AssertMsgFailed(("%s Unknown element %x\n", __FUNCTION__, pname));
        return 0;
    }
}

/**
 * Query the number of bytes required for a pixel in the specified format
 * 
 * @returns requested pixel size
 * @param   type        Parameter type
 */
GLint glInternalGetPixelFormatElements(GLenum format)
{
    switch (format)
    {
    case GL_COLOR_INDEX:
    case GL_STENCIL_INDEX:
    case GL_DEPTH_COMPONENT:
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_LUMINANCE_ALPHA:
        return 1;

    case GL_RGB:
    case GL_BGR_EXT:
        return 3;

    case GL_RGBA:
    case GL_BGRA_EXT:
        return 4;
    default:
        AssertMsgFailed(("%s Unknown format %x\n", __FUNCTION__, format));
        return 0;
    }
}
