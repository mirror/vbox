/* $Id$ */

/** @file
 * VBox OpenGL DRI driver functions
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#define _GNU_SOURCE 1

#include "cr_error.h"
#include "cr_gl.h"
#include "cr_mem.h"
#include "stub.h"
#include "fakedri_drv.h"
#include "dri_glx.h"
#include "iprt/mem.h"
#include "iprt/err.h"
#include <dlfcn.h>
#include <elf.h>
#include <unistd.h>
#include "xf86.h"

#define VBOX_NO_MESA_PATCH_REPORTS

//#define DEBUG_DRI_CALLS

//@todo this could be different...
#ifdef RT_ARCH_AMD64
# define DRI_DEFAULT_DRIVER_DIR "/usr/lib64/dri:/usr/lib/dri"
# define DRI_XORG_DRV_DIR "/usr/lib/xorg/modules/drivers/"
#else
# define DRI_DEFAULT_DRIVER_DIR "/usr/lib/dri"
# define DRI_XORG_DRV_DIR "/usr/lib/xorg/modules/drivers/"
#endif

#ifdef DEBUG_DRI_CALLS
 #define SWDRI_SHOWNAME(pext, func) \
   crDebug("SWDRI: sc %s->%s", #pext, #func)
#else
 #define SWDRI_SHOWNAME(pext, func)
#endif

#define SWDRI_SAFECALL(pext, func, ...)        \
    SWDRI_SHOWNAME(pext, func);                \
    if (pext && pext->func){                   \
        (*pext->func)(__VA_ARGS__);            \
    } else {                                   \
        crDebug("swcore_call NULL for "#func); \
    }

#define SWDRI_SAFERET(pext, func, ...)         \
    SWDRI_SHOWNAME(pext, func);                \
    if (pext && pext->func){                   \
        return (*pext->func)(__VA_ARGS__);     \
    } else {                                   \
        crDebug("swcore_call NULL for "#func); \
        return 0;                              \
    }

#define SWDRI_SAFERET_CORE(func, ...) SWDRI_SAFERET(gpSwDriCoreExternsion, func, __VA_ARGS__)
#define SWDRI_SAFECALL_CORE(func, ...) SWDRI_SAFECALL(gpSwDriCoreExternsion, func, __VA_ARGS__)
#define SWDRI_SAFERET_SWRAST(func, ...) SWDRI_SAFERET(gpSwDriSwrastExtension, func, __VA_ARGS__)
#define SWDRI_SAFECALL_SWRAST(func, ...) SWDRI_SAFECALL(gpSwDriSwrastExtension, func, __VA_ARGS__)

#ifndef PAGESIZE
#define PAGESIZE 4096
#endif

static struct _glapi_table vbox_glapi_table;
fakedri_glxapi_table glxim;

static const __DRIextension **gppSwDriExternsion = NULL;
static const __DRIcoreExtension *gpSwDriCoreExternsion = NULL;
static const __DRIswrastExtension *gpSwDriSwrastExtension = NULL;

extern const __DRIextension * __driDriverExtensions[];

#define GLAPI_ENTRY(Func) pGLTable->Func = cr_gl##Func;
static void
vboxFillMesaGLAPITable(struct _glapi_table *pGLTable)
{
    #include "fakedri_glfuncsList.h"

    pGLTable->SampleMaskSGIS = cr_glSampleMaskEXT;
    pGLTable->SamplePatternSGIS = cr_glSamplePatternEXT;
    pGLTable->WindowPos2dMESA = cr_glWindowPos2d;
    pGLTable->WindowPos2dvMESA = cr_glWindowPos2dv;
    pGLTable->WindowPos2fMESA = cr_glWindowPos2f;
    pGLTable->WindowPos2fvMESA = cr_glWindowPos2fv;
    pGLTable->WindowPos2iMESA = cr_glWindowPos2i;
    pGLTable->WindowPos2ivMESA = cr_glWindowPos2iv;
    pGLTable->WindowPos2sMESA = cr_glWindowPos2s;
    pGLTable->WindowPos2svMESA = cr_glWindowPos2sv;
    pGLTable->WindowPos3dMESA = cr_glWindowPos3d;
    pGLTable->WindowPos3dvMESA = cr_glWindowPos3dv;
    pGLTable->WindowPos3fMESA = cr_glWindowPos3f;
    pGLTable->WindowPos3fvMESA = cr_glWindowPos3fv;
    pGLTable->WindowPos3iMESA = cr_glWindowPos3i;
    pGLTable->WindowPos3ivMESA = cr_glWindowPos3iv;
    pGLTable->WindowPos3sMESA = cr_glWindowPos3s;
    pGLTable->WindowPos3svMESA = cr_glWindowPos3sv;
};
#undef GLAPI_ENTRY

#define GLXAPI_ENTRY(Func) pGLXTable->Func = VBOXGLXTAG(glX##Func);
static void
vboxFillGLXAPITable(fakedri_glxapi_table *pGLXTable)
{
    #include "fakedri_glxfuncsList.h"
}
#undef GLXAPI_ENTRY

static void
vboxPatchMesaExport(const char* psFuncName, const void *pStart, const void *pEnd)
{
    Dl_info dlip;
    Elf32_Sym* sym=0;
    int rv;
    void *alPatch;
    void *pMesaEntry;

#ifndef VBOX_NO_MESA_PATCH_REPORTS
    crDebug("vboxPatchMesaExport: %s", psFuncName);
#endif

    pMesaEntry = dlsym(RTLD_DEFAULT, psFuncName);

    if (!pMesaEntry)
    {
        crDebug("%s not defined in current scope, are we being loaded by mesa's libGL.so?", psFuncName);
        return;
    }

    rv = dladdr1(pMesaEntry, &dlip, (void**)&sym, RTLD_DL_SYMENT);
    if (!rv || !sym)
    {
        crError("Failed to get size for %p(%s)", pMesaEntry, psFuncName);
        return;
    }

#if VBOX_OGL_GLX_USE_CSTUBS
    {
        Dl_info dlip1;
        Elf32_Sym* sym1=0;
        int rv;

        rv = dladdr1(pStart, &dlip1, (void**)&sym1, RTLD_DL_SYMENT);
        if (!rv || !sym)
        {
            crError("Failed to get size for %p", pStart);
            return;
        }

        pEnd = pStart + sym1->st_size;
        crDebug("VBox Entry: %p, start: %p(%s:%s), size: %i", pStart, dlip1.dli_saddr, dlip1.dli_fname, dlip1.dli_sname, sym1->st_size);
    }
#endif

#ifndef VBOX_NO_MESA_PATCH_REPORTS
    crDebug("Mesa Entry: %p, start: %p(%s:%s), size: %i", pMesaEntry, dlip.dli_saddr, dlip.dli_fname, dlip.dli_sname, sym->st_size);
    crDebug("Vbox code: start: %p, end %p, size: %i", pStart, pEnd, pEnd-pStart);
#endif

    if (sym->st_size<(pEnd-pStart))
    {
        crDebug("Can't patch size too small.(%s)", psFuncName);
        return;
    }

    /* Get aligned start adress we're going to patch*/
    alPatch = (void*) ((uintptr_t)dlip.dli_saddr & ~(uintptr_t)(PAGESIZE-1));

#ifndef VBOX_NO_MESA_PATCH_REPORTS
    crDebug("MProtecting: %p, %i", alPatch, dlip.dli_saddr-alPatch+pEnd-pStart);
#endif

    /* Get write access to mesa functions */
    rv = RTMemProtect(alPatch, dlip.dli_saddr-alPatch+pEnd-pStart, 
                      RTMEM_PROT_READ|RTMEM_PROT_WRITE|RTMEM_PROT_EXEC);
    if (RT_FAILURE(rv))
    {
        crError("mprotect failed with %x (%s)", rv, psFuncName);
    }

#ifndef VBOX_NO_MESA_PATCH_REPORTS
    crDebug("Writing %i bytes to %p from %p", pEnd-pStart, dlip.dli_saddr, pStart);
#endif

    crMemcpy(dlip.dli_saddr, pStart, pEnd-pStart);

    /*@todo Restore the protection, probably have to check what was it before us...*/
    rv = RTMemProtect(alPatch, dlip.dli_saddr-alPatch+pEnd-pStart, 
                      RTMEM_PROT_READ|RTMEM_PROT_EXEC);
    if (RT_FAILURE(rv))
    {
        crError("mprotect2 failed with %x (%s)", rv, psFuncName);
    }
}

#ifdef VBOX_OGL_GLX_USE_CSTUBS
static void
# define GLXAPI_ENTRY(Func) vboxPatchMesaExport("glX"#Func, &vbox_glX##Func, NULL);
vboxPatchMesaExports()
#else
static void
# define GLXAPI_ENTRY(Func) vboxPatchMesaExport("glX"#Func, &vbox_glX##Func, &vbox_glX##Func##_EndProc);
vboxPatchMesaExports()
#endif
{
    crDebug("Patching mesa glx entries");
    #include "fakedri_glxfuncsList.h"
}
#undef GLXAPI_ENTRY

bool vbox_load_sw_dri()
{
    const char *libPaths, *p, *next;;
    char realDriverName[200];
    void *handle;
    int len, i;

    /*code from Mesa-7.2/src/glx/x11/dri_common.c:driOpenDriver*/

    libPaths = NULL;
    if (geteuid() == getuid()) {
        /* don't allow setuid apps to use LIBGL_DRIVERS_PATH */
        libPaths = getenv("LIBGL_DRIVERS_PATH");
        if (!libPaths)
            libPaths = getenv("LIBGL_DRIVERS_DIR"); /* deprecated */
    }
    if (libPaths == NULL)
        libPaths = DRI_DEFAULT_DRIVER_DIR;

    handle = NULL;
    for (p = libPaths; *p; p = next) 
    {
        next = strchr(p, ':');
        if (next == NULL) 
        {
            len = strlen(p);
            next = p + len;
        } 
        else 
        {
            len = next - p;
            next++;
        }

        snprintf(realDriverName, sizeof realDriverName, "%.*s/%s_dri.so", len, p, "swrast");
        crDebug("trying %s", realDriverName);
        handle = dlopen(realDriverName, RTLD_NOW | RTLD_LOCAL);
        if (handle) break;
    }

    /*end code*/

    if (handle) gppSwDriExternsion = dlsym(handle, "__driDriverExtensions");

    if (!gppSwDriExternsion)
    {
        crDebug("%s doesn't export __driDriverExtensions", realDriverName);
        return false;
    }
    crDebug("loaded %s", realDriverName);

    for (i = 0; gppSwDriExternsion[i]; i++) 
    {
        if (strcmp(gppSwDriExternsion[i]->name, __DRI_CORE) == 0)
            gpSwDriCoreExternsion = (__DRIcoreExtension *) gppSwDriExternsion[i];
        if (strcmp(gppSwDriExternsion[i]->name, __DRI_SWRAST) == 0)
            gpSwDriSwrastExtension = (__DRIswrastExtension *) gppSwDriExternsion[i];
    }

    return gpSwDriCoreExternsion && gpSwDriSwrastExtension;
}

void __attribute__ ((constructor)) vbox_install_into_mesa(void)
{
    if (!stubInit())
    {
        crDebug("vboxdriInitScreen: stubInit failed");
        return;
    }

    {
        void (*pxf86Msg)(MessageType type, const char *format, ...) _printf_attribute(2,3);

        pxf86Msg = dlsym(RTLD_DEFAULT, "xf86Msg");
        if (pxf86Msg)
        {
            pxf86Msg(X_INFO, "Next line is added to allow vboxvideo_drv.so to appear as whitelisted driver\n");
            pxf86Msg(X_INFO, "The file referenced, is *NOT* loaded\n");
            pxf86Msg(X_INFO, "Loading %s/ati_drv.so\n", DRI_XORG_DRV_DIR);

            /* we're failing to proxy software dri driver calls for certain xservers, so just make sure we're unloaded for now */
            __driDriverExtensions[0] = NULL;
            return;
        }
    }

    /* Load swrast_dri.so to proxy dri related calls there. */
    if (!vbox_load_sw_dri())
    {
        crDebug("vboxdriInitScreen: vbox_load_sw_dri failed...going to fail badly");
        return;
    }

    /* Handle gl api.
     * In the end application call would look like this:
     * app call glFoo->(mesa asm dispatch stub)->cr_glFoo(vbox asm dispatch stub)->SPU Foo function(packspuFoo or alike)
     * Note, we don't need to install extension functions via _glapi_add_dispatch, because we'd override glXGetProcAddress.
     */    
    /* We don't support all mesa's functions. Initialize our table to mesa dispatch first*/
    crMemcpy(&vbox_glapi_table, _glapi_get_dispatch(), sizeof(struct _glapi_table));
    /* Now install our assembly dispatch entries into table */
    vboxFillMesaGLAPITable(&vbox_glapi_table);
    /* Install our dispatch table into mesa */
    _glapi_set_dispatch(&vbox_glapi_table);

    /* Handle glx api.
     * In the end application call would look like this:
     * app call glxFoo->(mesa asm dispatch stub patched with vbox_glXFoo:jmp glxim[Foo's index])->VBOXGLXTAG(glxFoo)
     */
    /* Fill structure used by our assembly stubs */
    vboxFillGLXAPITable(&glxim);
    /* Now patch functions exported by libGL.so */
    vboxPatchMesaExports();
}

/*
 * @todo we're missing first glx related call from the client application.
 * Luckily, this doesn't add much problems, except for some cases.
 */

/* __DRIcoreExtension */

static __DRIscreen *
vboxdriCreateNewScreen(int screen, int fd, unsigned int sarea_handle,
                       const __DRIextension **extensions, const __DRIconfig ***driverConfigs,
                       void *loaderPrivate)
{
    (void) fd;
    (void) sarea_handle;
    SWDRI_SAFERET_SWRAST(createNewScreen, screen, extensions, driverConfigs, loaderPrivate);
}

static void 
vboxdriDestroyScreen(__DRIscreen *screen)
{
    SWDRI_SAFECALL_CORE(destroyScreen, screen);
}

static const __DRIextension **
vboxdriGetExtensions(__DRIscreen *screen)
{
    SWDRI_SAFERET_CORE(getExtensions, screen);
}

static int
vboxdriGetConfigAttrib(const __DRIconfig *config,
                       unsigned int attrib,
                       unsigned int *value)
{
    SWDRI_SAFERET_CORE(getConfigAttrib, config, attrib, value);
}

static int
vboxdriIndexConfigAttrib(const __DRIconfig *config, int index,
                         unsigned int *attrib, unsigned int *value)
{
    SWDRI_SAFERET_CORE(indexConfigAttrib, config, index, attrib, value);
}

static __DRIdrawable *
vboxdriCreateNewDrawable(__DRIscreen *screen,
                         const __DRIconfig *config,
                         unsigned int drawable_id,
                         unsigned int head,
                         void *loaderPrivate)
{
    (void) drawable_id;
    (void) head;
    SWDRI_SAFERET_SWRAST(createNewDrawable, screen, config, loaderPrivate);
}

static void 
vboxdriDestroyDrawable(__DRIdrawable *drawable)
{
    SWDRI_SAFECALL_CORE(destroyDrawable, drawable);
}

static void
vboxdriSwapBuffers(__DRIdrawable *drawable)
{
    SWDRI_SAFECALL_CORE(swapBuffers, drawable);
}

static __DRIcontext *
vboxdriCreateNewContext(__DRIscreen *screen,
                        const __DRIconfig *config,
                        __DRIcontext *shared,
                        void *loaderPrivate)
{
    SWDRI_SAFERET_CORE(createNewContext, screen, config, shared, loaderPrivate);
}

static int 
vboxdriCopyContext(__DRIcontext *dest,
                   __DRIcontext *src,
                   unsigned long mask)
{
    SWDRI_SAFERET_CORE(copyContext, dest, src, mask);
}

static void 
vboxdriDestroyContext(__DRIcontext *context)
{
    SWDRI_SAFECALL_CORE(destroyContext, context);
}

static int 
vboxdriBindContext(__DRIcontext *ctx,
                   __DRIdrawable *pdraw,
                   __DRIdrawable *pread)
{
    SWDRI_SAFERET_CORE(bindContext, ctx, pdraw, pread);
}

static int 
vboxdriUnbindContext(__DRIcontext *ctx)
{
    SWDRI_SAFERET_CORE(unbindContext, ctx)
}

/* __DRIlegacyExtension */

static __DRIscreen *
vboxdriCreateNewScreen_Legacy(int scrn,
                              const __DRIversion *ddx_version,
                              const __DRIversion *dri_version,
                              const __DRIversion *drm_version,
                              const __DRIframebuffer *frame_buffer,
                              drmAddress pSAREA, int fd, 
                              const __DRIextension **extensions,
                              const __DRIconfig ***driver_modes,
                              void *loaderPrivate)
{
    (void) ddx_version;
    (void) dri_version;
    (void) frame_buffer;
    (void) pSAREA;
    (void) fd;
    SWDRI_SAFERET_SWRAST(createNewScreen, scrn, extensions, driver_modes, loaderPrivate);
}

static __DRIdrawable *
vboxdriCreateNewDrawable_Legacy(__DRIscreen *psp, const __DRIconfig *config,
                                drm_drawable_t hwDrawable, int renderType,
                                const int *attrs, void *data)
{
    (void) hwDrawable;
    (void) renderType;
    (void) attrs;
    (void) data;
    SWDRI_SAFERET_SWRAST(createNewDrawable, psp, config, data);
}

static __DRIcontext *
vboxdriCreateNewContext_Legacy(__DRIscreen *psp, const __DRIconfig *config,
                               int render_type, __DRIcontext *shared, 
                               drm_context_t hwContext, void *data)
{
    (void) render_type;
    (void) hwContext;
    return vboxdriCreateNewContext(psp, config, shared, data);
}


static const __DRIlegacyExtension vboxdriLegacyExtension = {
    { __DRI_LEGACY, __DRI_LEGACY_VERSION },
    vboxdriCreateNewScreen_Legacy,
    vboxdriCreateNewDrawable_Legacy,
    vboxdriCreateNewContext_Legacy
};

static const __DRIcoreExtension vboxdriCoreExtension = {
    { __DRI_CORE, __DRI_CORE_VERSION },
    vboxdriCreateNewScreen, /* driCreateNewScreen */
    vboxdriDestroyScreen,
    vboxdriGetExtensions,
    vboxdriGetConfigAttrib,
    vboxdriIndexConfigAttrib,
    vboxdriCreateNewDrawable, /* driCreateNewDrawable */
    vboxdriDestroyDrawable,
    vboxdriSwapBuffers,
    vboxdriCreateNewContext,
    vboxdriCopyContext,
    vboxdriDestroyContext,
    vboxdriBindContext,
    vboxdriUnbindContext
};

/* This structure is used by dri_util from mesa, don't rename it! */
DECLEXPORT(const __DRIextension *) __driDriverExtensions[] = {
    &vboxdriLegacyExtension.base,
    &vboxdriCoreExtension.base,
    NULL
};
