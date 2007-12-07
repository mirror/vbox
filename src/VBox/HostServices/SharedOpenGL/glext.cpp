/** @file
 *
 * VBox OpenGL helper functions
 *
 * OpenGL extensions
 *
 * References:  http://oss.sgi.com/projects/ogl-sample/registry/
 *              http://icps.u-strasbg.fr/~marchesin/perso/extensions/
 */

/*
 *
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

#define VBOX_OGL_WITH_EXTENSION_ARRAY
#include "vboxgl.h"
#include <VBox/HostServices/wglext.h>

#define LOG_GROUP LOG_GROUP_SHARED_OPENGL
#include <VBox/log.h>
#include <iprt/string.h>

/**
 * Initialize OpenGL extensions
 *
 * @returns VBox error code
 */
int vboxInitOpenGLExtensions()
{
    const GLubyte *pszExtensions = glGetString(GL_EXTENSIONS);
    static bool    fInitialized  = false;

    if (fInitialized)
        return VINF_SUCCESS;

    for (unsigned int i=0;i<RT_ELEMENTS(OpenGLExtensions);i++)
    {
        if (strstr((char *)pszExtensions, OpenGLExtensions[i].pszExtName))
        {
            *OpenGLExtensions[i].ppfnFunction = vboxDrvIsExtensionAvailable((char *)OpenGLExtensions[i].pszExtFunctionName);
            OpenGLExtensions[i].fAvailable   = !!*OpenGLExtensions[i].ppfnFunction;
        }
    }
    fInitialized = true;
    return VINF_SUCCESS;
}

/**
 * Check if an opengl extension function is available
 *
 * @returns VBox error code
 * @param   pszFunctionName     OpenGL extension function name
 */
bool vboxwglGetProcAddress(char *pszFunctionName)
{
    for (unsigned int i=0;i<RT_ELEMENTS(OpenGLExtensions);i++)
    {
        if (    OpenGLExtensions[i].fAvailable
            && !strcmp(OpenGLExtensions[i].pszExtFunctionName, pszFunctionName))
        {
            Log(("vboxwglGetProcAddress: found %s\n", pszFunctionName));
            return true;
        }
    }
    Log(("vboxwglGetProcAddress: didn't find %s\n", pszFunctionName));
    return false;
}

#ifdef RT_OS_WINDOWS
void vboxwglSwapIntervalEXT (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    Assert(pfnwglSwapIntervalEXT);

    OGL_CMD(wglSwapIntervalEXT, 1);
    OGL_PARAM(int, interval);
    pClient->lastretval = pfnwglSwapIntervalEXT(interval);
    if (!pClient->lastretval)
        Log(("wglSwapIntervalEXT failed with %d\n", GetLastError()));

    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxwglGetSwapIntervalEXT (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    Assert(pfnwglGetSwapIntervalEXT);

    OGL_CMD(wglGetSwapIntervalEXT, 0);
    pClient->lastretval = pfnwglGetSwapIntervalEXT();
    if (!pClient->lastretval)
        Log(("wglGetSwapIntervalEXT failed with %d\n", GetLastError()));

    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}
#endif /* RT_OS_WINDOWS */
