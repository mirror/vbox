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

static void GetString(GLenum name, GLubyte *pszStr)
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
    GLfloat version;

    GetString(GL_VERSION, return_value);
    CRASSERT(crStrlen((char *)return_value) < 100);

    version = crStrToFloat((char *) return_value);
    version = crStateComputeVersion(version);

    return version;
}

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

#if 1
    {
        const char *addext = "GL_EXT_compiled_vertex_array GL_S3_s3tc ";
/*"GL_AMDX_vertex_shader_tessellator GL_AMD_draw_buffers_blend GL_AMD_performance_monitor "
"GL_AMD_texture_texture4 GL_ARB_color_buffer_float GL_ARB_copy_buffer GL_ARB_depth_buffer_float "
"GL_ARB_depth_texture GL_ARB_draw_buffers GL_ARB_draw_instanced GL_ARB_fragment_program "
"GL_ARB_fragment_program_shadow GL_ARB_fragment_shader GL_ARB_framebuffer_object "
"GL_ARB_framebuffer_sRGB GL_ARB_half_float_pixel GL_ARB_half_float_vertex GL_ARB_instanced_arrays "
"GL_ARB_map_buffer_range GL_ARB_multisample GL_ARB_multitexture GL_ARB_occlusion_query "
"GL_ARB_pixel_buffer_object GL_ARB_point_parameters GL_ARB_point_sprite GL_ARB_shader_objects "
"GL_ARB_shader_texture_lod GL_ARB_shading_language_100 GL_ARB_shadow GL_ARB_shadow_ambient "
"GL_ARB_texture_border_clamp GL_ARB_texture_buffer_object GL_ARB_texture_compression "
"GL_ARB_texture_compression_rgtc GL_ARB_texture_cube_map GL_ARB_texture_env_add "
"GL_ARB_texture_env_combine GL_ARB_texture_env_crossbar GL_ARB_texture_env_dot3 "
"GL_ARB_texture_float GL_ARB_texture_mirrored_repeat GL_ARB_texture_non_power_of_two "
"GL_ARB_texture_rectangle GL_ARB_texture_rg GL_ARB_texture_snorm GL_ARB_transpose_matrix "
"GL_ARB_vertex_array_object GL_ARB_vertex_buffer_object GL_ARB_vertex_program GL_ARB_vertex_shader "
"GL_ARB_window_pos GL_ATI_draw_buffers GL_ATI_envmap_bumpmap GL_ATI_fragment_shader GL_ATI_meminfo "
"GL_ATI_separate_stencil GL_ATI_texture_compression_3dc GL_ATI_texture_env_combine3 "
"GL_ATI_texture_float GL_ATI_texture_mirror_once GL_EXT_abgr GL_EXT_bgra GL_EXT_bindable_uniform "
"GL_EXT_blend_color GL_EXT_blend_equation_separate GL_EXT_blend_func_separate GL_EXT_blend_minmax "
"GL_EXT_blend_subtract GL_EXT_compiled_vertex_array GL_EXT_copy_buffer GL_EXT_copy_texture "
"GL_EXT_draw_buffers2 GL_EXT_draw_instanced GL_EXT_draw_range_elements GL_EXT_fog_coord "
"GL_EXT_framebuffer_blit GL_EXT_framebuffer_multisample GL_EXT_framebuffer_object "
"GL_EXT_framebuffer_sRGB GL_EXT_gpu_program_parameters GL_EXT_gpu_shader4 GL_EXT_multi_draw_arrays "
"GL_EXT_packed_depth_stencil GL_EXT_packed_float GL_EXT_packed_pixels GL_EXT_point_parameters "
"GL_EXT_rescale_normal GL_EXT_secondary_color GL_EXT_separate_specular_color "
"GL_EXT_shadow_funcs GL_EXT_stencil_wrap GL_EXT_subtexture GL_EXT_texgen_reflection "
"GL_EXT_texture3D GL_EXT_texture_array GL_EXT_texture_buffer_object "
"GL_EXT_texture_compression_latc GL_EXT_texture_compression_rgtc GL_EXT_texture_compression_s3tc "
"GL_EXT_texture_cube_map GL_EXT_texture_edge_clamp GL_EXT_texture_env_add "
"GL_EXT_texture_env_combine GL_EXT_texture_env_dot3 GL_EXT_texture_filter_anisotropic "
"GL_EXT_texture_integer GL_EXT_texture_lod GL_EXT_texture_lod_bias GL_EXT_texture_mirror_clamp "
"GL_EXT_texture_object GL_EXT_texture_rectangle GL_EXT_texture_sRGB GL_EXT_texture_shared_exponent "
"GL_EXT_texture_snorm GL_EXT_texture_swizzle GL_EXT_transform_feedback GL_EXT_vertex_array "
"GL_IBM_texture_mirrored_repeat GL_KTX_buffer_region GL_NV_blend_square GL_NV_conditional_render "
"GL_NV_copy_depth_to_color GL_NV_primitive_restart GL_NV_texgen_reflection GL_SGIS_generate_mipmap "
"GL_SGIS_texture_edge_clamp GL_SGIS_texture_lod GL_SUN_multi_draw_arrays GL_WIN_swap_hint "
"WGL_EXT_swap_control";*/

        sprintf(gpszExtensions, "%s%s", addext, ext);
    }
#else
    sprintf(gpszExtensions, "%s", ext);
#endif

    return gpszExtensions;
}

#ifdef WINDOWS
static bool packspuRunningUnderWine(void)
{
    return 1;//NULL != GetModuleHandle("wined3d.dll");
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
                return ctx->pszRealVersion;               
            }
            else
#endif
            {
                float version = GetVersionString();
                sprintf((char*)ctx->glVersion, "%.1f Chromium %s", version, CR_VERSION_STRING);
                return ctx->glVersion;
            }
        case GL_VENDOR:
#ifdef WINDOWS
            if (packspuRunningUnderWine())
            {
                GetString(GL_REAL_VENDOR, ctx->pszRealVendor);
                return ctx->pszRealVendor;
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
                return ctx->pszRealRenderer;
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
            return ctx->pszRealVendor;
        case GL_REAL_VERSION:
            GetString(GL_REAL_VERSION, ctx->pszRealVersion);
            return ctx->pszRealVersion;
        case GL_REAL_RENDERER:
            GetString(GL_REAL_RENDERER, ctx->pszRealRenderer);
            return ctx->pszRealRenderer;
#endif
        default:
            return crStateGetString(name);
    }
}
