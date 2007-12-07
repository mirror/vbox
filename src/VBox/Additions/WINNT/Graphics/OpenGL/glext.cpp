/** @file
 *
 * VirtualBox Windows NT/2000/XP guest OpenGL ICD
 *
 * OpenGL extensions 
 *
 * References:  http://oss.sgi.com/projects/ogl-sample/registry/
 *              http://icps.u-strasbg.fr/~marchesin/perso/extensions/
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
#include "VBoxOGL.h"


/**
 * Initialize OpenGL extensions
 *
 * @returns VBox status code
 * @param pCtx  OpenGL thread context
 */
int vboxInitOpenGLExtensions(PVBOX_OGL_THREAD_CTX pCtx)
{
    char *pszExtensions;
    static bool    fInitialized  = false;
    VBoxOGLglGetString parms;
    int rc;

    if (fInitialized)
        return VINF_SUCCESS;

    memset(&parms, 0, sizeof(parms));
    VBOX_INIT_CALL(&parms.hdr, GLGETSTRING, pCtx);

    parms.name.type                      = VMMDevHGCMParmType_32bit;
    parms.name.u.value32                 = GL_EXTENSIONS;
    parms.pString.type                   = VMMDevHGCMParmType_LinAddr;
    parms.pString.u.Pointer.size         = sizeof(szOpenGLExtensions);
    parms.pString.u.Pointer.u.linearAddr = (vmmDevHypPtr)szOpenGLExtensions;

    rc = vboxHGCMCall(&parms, sizeof (parms));

    if (    VBOX_FAILURE(rc)
        ||  VBOX_FAILURE(parms.hdr.result))
    {
        DbgPrintf(("GL_EXTENSIONS failed with %x %x\n", rc, parms.hdr.result));
        return (rc == VINF_SUCCESS) ? parms.hdr.result : rc;
    }
    DbgPrintf(("GL_EXTENSIONS=%s\n\n", szOpenGLExtensions));

    pszExtensions = strdup(szOpenGLExtensions);
    szOpenGLExtensions[0] = 0;

    for (int i=0;i<RT_ELEMENTS(OpenGLExtensions);i++)
    {
        if (strstr((char *)pszExtensions, OpenGLExtensions[i].pszExtName))
            OpenGLExtensions[i].fAvailable = VBoxIsExtensionAvailable(OpenGLExtensions[i].pszExtFunctionName);
        
        if (    OpenGLExtensions[i].fAvailable
            &&  !strstr(szOpenGLExtensions, OpenGLExtensions[i].pszExtName))
        {
            strcat(szOpenGLExtensions, OpenGLExtensions[i].pszExtName);
            strcat(szOpenGLExtensions, " ");
        }
    }

    free(pszExtensions);

    fInitialized = true;
    return VINF_SUCCESS;
}


PROC APIENTRY DrvGetProcAddress(LPCSTR lpszProc)
{
    PROC pfnProc = NULL;

    for (int i=0;i<RT_ELEMENTS(OpenGLExtensions);i++)
    {
        if (    OpenGLExtensions[i].fAvailable
            && !strcmp(OpenGLExtensions[i].pszExtFunctionName, lpszProc))
        {
            pfnProc = (PROC)OpenGLExtensions[i].pfnFunction;
        }
    }
    if (pfnProc == NULL)
        DbgPrintf(("DrvGetProcAddress %s FAILED\n", lpszProc));
    else
        DbgPrintf(("DrvGetProcAddress %s\n", lpszProc));

    return pfnProc;
}

BOOL WINAPI wglSwapIntervalEXT(int interval)
{
    VBOX_OGL_GEN_SYNC_OP1_RET(BOOL, wglSwapIntervalEXT, interval);
    return retval;
}

int WINAPI wglGetSwapIntervalEXT(void)
{
    VBOX_OGL_GEN_SYNC_OP_RET(int, wglGetSwapIntervalEXT);
    return retval;
}





#if 0
 GL_ARB_multitexture
 GL_EXT_texture_env_add 
 GL_EXT_compiled_vertex_array 
 GL_S3_s3tc 
 GL_ARB_occlusion_query 
 GL_ARB_point_parameters 
 GL_ARB_texture_border_clamp 
 GL_ARB_texture_compression 
 GL_ARB_texture_cube_map 
 GL_ARB_texture_env_add 
 GL_ARB_texture_env_combine 
 GL_ARB_texture_env_crossbar 
 GL_ARB_texture_env_dot3 
 GL_ARB_texture_mirrored_repeat 
 GL_ARB_transpose_matrix 
 GL_ARB_vertex_blend 
 GL_ARB_vertex_buffer_object 
 GL_ARB_vertex_program 
 GL_ARB_window_pos 
 GL_ATI_element_array 
 GL_ATI_envmap_bumpmap 
 GL_ATI_fragment_shader 
 GL_ATI_map_object_buffer 
 GL_ATI_texture_env_combine3 
 GL_ATI_texture_mirror_once 
 GL_ATI_vertex_array_object 
 GL_ATI_vertex_attrib_array_object 
 GL_ATI_vertex_streams 
 GL_ATIX_texture_env_combine3 
 GL_ATIX_texture_env_route 
 GL_ATIX_vertex_shader_output_point_size 
 GL_EXT_abgr 
 GL_EXT_bgra 
 GL_EXT_blend_color 
 GL_EXT_blend_func_separate 
 GL_EXT_blend_minmax 
 GL_EXT_blend_subtract 
 GL_EXT_clip_volume_hint 
 GL_EXT_draw_range_elements 
 GL_EXT_fog_coord 
 GL_EXT_multi_draw_arrays 
 GL_EXT_packed_pixels 
 GL_EXT_point_parameters 
 GL_EXT_polygon_offset 
 GL_EXT_rescale_normal 
 GL_EXT_secondary_color 
 GL_EXT_separate_specular_color 
 GL_EXT_stencil_wrap 
 GL_EXT_texgen_reflection 
 GL_EXT_texture3D 
 GL_EXT_texture_compression_s3tc 
 GL_EXT_texture_cube_map 
 GL_EXT_texture_edge_clamp 
 GL_EXT_texture_env_combine 
 GL_EXT_texture_env_dot3 
 GL_EXT_texture_filter_anisotropic 
 GL_EXT_texture_lod_bias 
 GL_EXT_texture_mirror_clamp 
 GL_EXT_texture_object 
 GL_EXT_texture_rectangle 
 GL_EXT_vertex_array 
 GL_EXT_vertex_shader 
 GL_HP_occlusion_test 
 GL_KTX_buffer_region 
 GL_NV_blend_square 
 GL_NV_occlusion_query 
 GL_NV_texgen_reflection 
 GL_SGI_color_matrix 
 GL_SGIS_generate_mipmap 
 GL_SGIS_multitexture 
 GL_SGIS_texture_border_clamp 
 GL_SGIS_texture_edge_clamp 
 GL_SGIS_texture_lod 
 GL_SUN_multi_draw_arrays 
 GL_WIN_swap_hint 
 WGL_EXT_extensions_string 
#endif

