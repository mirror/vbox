/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "packspu.h"
#include "cr_packfunctions.h"
#include "state/cr_statefuncs.h"
#include "cr_string.h"
#include "packspu_proto.h"

static GLubyte gpszExtensions[10000];
#ifdef CR_OPENGL_VERSION_2_0
static GLubyte gpszShadingVersion[255]="";
#endif

static const GLubyte *
GetExtensions(void)
{
    GLubyte return_value[10*1000];
    const GLubyte *extensions, *ext;
    GET_THREAD(thread);
    int writeback = 1;

    if (pack_spu.swap)
    {
        crPackGetStringSWAP( GL_EXTENSIONS, return_value, &writeback );
    }
    else
    {
        crPackGetString( GL_EXTENSIONS, return_value, &writeback );
    }
    packspuFlush( (void *) thread );

    while (writeback)
        crNetRecv();

    CRASSERT(crStrlen((char *)return_value) < 10*1000);

    /* OK, we got the result from the server.  Now we have to
     * intersect is with the set of extensions that Chromium understands
     * and tack on the Chromium-specific extensions.
     */
    extensions = return_value;
    ext = crStateMergeExtensions(1, &extensions);

#ifdef CR_OPENGL_VERSION_2_0
    /* @todo, check if host supports opengl 2.0 or this extension 
     * before exporting it.
     */
    sprintf(gpszExtensions, "%sGL_ARB_shading_language_100 GL_ARB_shader_objects GL_ARB_vertex_shader GL_ARB_fragment_shader", ext);
#else
    sprintf(gpszExtensions, "%s", ext);
#endif

    return gpszExtensions;
}

static void GetString(GLenum name, char *pszStr)
{
    GET_THREAD(thread);
    int writeback = 1;

    if (pack_spu.swap)
        crPackGetStringSWAP(name, pszStr, &writeback);
    else
        crPackGetString(name, pszStr, &writeback);
    packspuFlush( (void *) thread );

    while (writeback)
        crNetRecv();
}

static GLfloat
GetVersionString(void)
{
    GLubyte return_value[100];
    int writeback = 1;
    GLfloat version;

    GetString(GL_VERSION, return_value);
    CRASSERT(crStrlen((char *)return_value) < 100);

    version = crStrToFloat((char *) return_value);
    version = crStateComputeVersion(version);

    return version;
}

#ifdef WINDOWS
static bool packspuRunningUnderWine(void)
{
    return NULL != GetModuleHandle("wined3d.dll");
}
#endif

const GLubyte * PACKSPU_APIENTRY packspu_GetString( GLenum name )
{
    GET_CONTEXT(ctx);

    switch(name)
    {
        case GL_EXTENSIONS:
            return GetExtensions();
        case GL_VERSION:
#ifdef WINDOWS
            if (packspuRunningUnderWine())
            {
                GetString(GL_REAL_VERSION, ctx->pszRealVersion);
                return (const GLubyte*) ctx->pszRealVersion;               
            }
            else
#endif
            {
                float version = GetVersionString();
                sprintf(ctx->glVersion, "%.1f Chromium %s", version, CR_VERSION_STRING);
                return (const GLubyte *) ctx->glVersion;
            }
        case GL_VENDOR:
#ifdef WINDOWS
            if (packspuRunningUnderWine())
            {
                GetString(GL_REAL_VENDOR, ctx->pszRealVendor);
                return (const GLubyte*) ctx->pszRealVendor;
            }
            else
#endif
            {
                return crStateGetString(name);
            }
        case GL_RENDERER:
#ifdef WINDOWS
            if (packspuRunningUnderWine())
            {
                GetString(GL_REAL_RENDERER, ctx->pszRealRenderer);
                return (const GLubyte*) ctx->pszRealRenderer;
            }
            else
#endif
            {
                return crStateGetString(name);
            }

#ifdef CR_OPENGL_VERSION_2_0
        case GL_SHADING_LANGUAGE_VERSION:
            GetString(GL_SHADING_LANGUAGE_VERSION, gpszShadingVersion);
            return gpszShadingVersion;
#endif
#ifdef GL_CR_real_vendor_strings
        case GL_REAL_VENDOR:
            GetString(GL_REAL_VENDOR, ctx->pszRealVendor);
            return (const GLubyte*) ctx->pszRealVendor;
        case GL_REAL_VERSION:
            GetString(GL_REAL_VERSION, ctx->pszRealVersion);
            return (const GLubyte*) ctx->pszRealVersion;
        case GL_REAL_RENDERER:
            GetString(GL_REAL_RENDERER, ctx->pszRealRenderer);
            return (const GLubyte*) ctx->pszRealRenderer;
#endif
        default:
            return crStateGetString(name);
    }
}
