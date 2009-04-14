/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

/* opengl_stub/glx.c */
#include "chromium.h"
#include "cr_error.h"
#include "cr_spu.h"
#include "cr_mem.h"
#include "cr_string.h"
#include "stub.h"
#include "dri_glx.h"

#include <X11/Xregion.h>

//#define VBOX_NO_NATIVEGL

/* Force full pixmap update if there're more damaged regions than this number*/
#define CR_MAX_DAMAGE_REGIONS_TRACKED 50

/* Force "bigger" update (full or clip) if it's reducing number of regions updated 
 * but doesn't increase updated area more than given number
 */
#define CR_MIN_DAMAGE_PROFIT_SIZE 64*64

/*@todo combine it in some other place*/
/* Size of pack spu buffer - some delta for commands packing, see pack/packspu_config.c*/

/** Ramshankar: Solaris compiz fix */
#ifdef RT_OS_SOLARIS
# define CR_MAX_TRANSFER_SIZE 20*1024*1024
#else
# define CR_MAX_TRANSFER_SIZE 4*1024*1024
#endif

/** For optimizing glXMakeCurrent */
static Display *currentDisplay = NULL;
static GLXDrawable currentDrawable = 0;
static GLXDrawable currentReadDrawable = 0;

/**
 * Keep a list of structures which associates X visual IDs with
 * Chromium visual bitmasks.
 */
struct VisualInfo {
    Display *dpy;
    int screen;
    VisualID visualid;
    int visBits;
    struct VisualInfo *next;
};

static struct VisualInfo *VisualInfoList = NULL;

static void stubXshmUpdateImageRect(Display *dpy, GLXDrawable draw, GLX_Pixmap_t *pGlxPixmap, XRectangle *pRect);

static void
AddVisualInfo(Display *dpy, int screen, VisualID visualid, int visBits)
{
    struct VisualInfo *v;
    for (v = VisualInfoList; v; v = v->next) {
        if (v->dpy == dpy && v->screen == screen && v->visualid == visualid) {
            v->visBits |= visBits;
            return;
        }
    }
    v = (struct VisualInfo *) crAlloc(sizeof(struct VisualInfo));
    v->dpy = dpy;
    v->screen = screen;
    v->visualid = visualid;
    v->visBits = visBits;
    v->next = VisualInfoList;
    VisualInfoList = v;
}

static struct VisualInfo *
FindVisualInfo(Display *dpy, int screen, VisualID visualid)
{
    struct VisualInfo *v;
    for (v = VisualInfoList; v; v = v->next) {
        if (v->dpy == dpy && v->screen == screen && v->visualid == visualid)
            return v;
    }
    return NULL;
}

/**
 * Return string for a GLX error code
 */
static const char *glx_error_string(int err)
{
    static const char *glxErrors[] = {
        "none",
        "GLX_BAD_SCREEN",
        "GLX_BAD_ATTRIBUTE",
        "GLX_NO_EXTENSION",
        "GLX_BAD_VISUAL",
        "GLX_BAD_CONTEXT",
        "GLX_BAD_VALUE",
        "GLX_BAD_ENUM"
    };
    if (err > 0 && err < 8) {
        return glxErrors[err];
    }
    else {
        static char tmp[100];
        sprintf(tmp, "0x%x", err);
        return tmp;
    }
}

/* Given an XVisualInfo structure, try to figure out what its 
 * OpenGL capabilities are, if we have a native OpenGL.
 * Returns 0 if no information is available.
 */
static struct {
    int gl_attrib;
    char *attrib_name;
    enum {TEST_TRUE, TEST_GREATER_0} test;
    int match_vis_bits;
} attrib_map[] = {
    {GLX_RGBA, "GLX_RGBA", TEST_TRUE, CR_RGB_BIT},
    {GLX_DOUBLEBUFFER, "GLX_DOUBLEBUFFER", TEST_TRUE, CR_DOUBLE_BIT},
    {GLX_STEREO, "GLX_STEREO", TEST_TRUE, CR_STEREO_BIT},
    {GLX_LEVEL, "GLX_LEVEL", TEST_GREATER_0, CR_OVERLAY_BIT},
    {GLX_ALPHA_SIZE, "GLX_ALPHA_SIZE", TEST_GREATER_0, CR_ALPHA_BIT},
    {GLX_DEPTH_SIZE, "GLX_DEPTH_SIZE", TEST_GREATER_0, CR_DEPTH_BIT},
    {GLX_STENCIL_SIZE, "GLX_STENCIL_SIZE", TEST_GREATER_0, CR_STENCIL_BIT},
    {GLX_ACCUM_RED_SIZE, "GLX_ACCUM_RED_SIZE", TEST_GREATER_0, CR_ACCUM_BIT},
    {GLX_SAMPLE_BUFFERS_SGIS, "GLX_SAMPLE_BUFFERS_SGIS", TEST_GREATER_0, CR_MULTISAMPLE_BIT},
};
#ifndef VBOX_NO_NATIVEGL  /* Currently not used */
static int QueryVisBits(Display *dpy, XVisualInfo *vis)
{
    int visBits = 0;
    int foo, bar, return_val, value;
    unsigned int i;

    /* We can only query the OpenGL capabilities if we actually
     * have a native OpenGL underneath us.  Without it, we can't
     * get at all the actual OpenGL characteristics.
     */
    if (!stub.haveNativeOpenGL) return 0;

    if (!stub.wsInterface.glXQueryExtension(dpy, &foo, &bar)) return 0;

    /* If we don't have the GLX_USE_GL attribute, we've failed. */
    return_val = stub.wsInterface.glXGetConfig(dpy, vis, GLX_USE_GL, &value);
    if (return_val) {
        crDebug("native glXGetConfig returned %d (%s) at %s line %d",
            return_val, glx_error_string(return_val), __FILE__, __LINE__);
        return 0;
    }
    if (value == 0) {
        crDebug("visual ID 0x%x doesn't support OpenGL at %s line %d",
            (int) vis->visual->visualid, __FILE__, __LINE__);
        return 0;
    }

    for (i = 0; i < sizeof(attrib_map)/sizeof(attrib_map[0]); i++) {
        return_val = stub.wsInterface.glXGetConfig(dpy, vis, attrib_map[i].gl_attrib, &value);
        if (return_val) {
            crDebug("native glXGetConfig(%s) returned %d (%s) at %s line %d",
                attrib_map[i].attrib_name, return_val, glx_error_string(return_val), __FILE__, __LINE__);
            return 0;
        }

        switch(attrib_map[i].test) {
            case TEST_TRUE:
                if (value)
                    visBits |= attrib_map[i].match_vis_bits;
                break;

            case TEST_GREATER_0:
                if (value > 0)
                    visBits |= attrib_map[i].match_vis_bits;
                break;

            default:
                crWarning("illegal attribute map test for %s at %s line %d", 
                    attrib_map[i].attrib_name, __FILE__, __LINE__);
                return 0;
        }
    }

    return visBits;
}
#endif /* not 0 */



#ifndef VBOX_NO_NATIVEGL /* Old code */
DECLEXPORT(XVisualInfo *)
VBOXGLXTAG(glXChooseVisual)( Display *dpy, int screen, int *attribList )
{
    XVisualInfo *vis;
    int *attrib;
    int visBits = 0;

    stubInit();

    for (attrib = attribList; *attrib != None; attrib++)
    {
        switch (*attrib)
        {
            case GLX_USE_GL:
                /* ignored, this is mandatory */
                break;

            case GLX_BUFFER_SIZE:
                /* this is for color-index visuals, which we don't support */
                attrib++;
                break;

            case GLX_LEVEL:
                if (attrib[1] > 0)
                    visBits |= CR_OVERLAY_BIT;
                attrib++;
                break;

            case GLX_RGBA:
                visBits |= CR_RGB_BIT;
                break;

            case GLX_DOUBLEBUFFER:
                visBits |= CR_DOUBLE_BIT;
                break;

            case GLX_STEREO:
                visBits |= CR_STEREO_BIT;
                /*
                crWarning( "glXChooseVisual: stereo unsupported" );
                return NULL;
                */
                break;

            case GLX_AUX_BUFFERS:
                {
                    int aux_buffers = attrib[1];
                    if (aux_buffers != 0)
                    {
                        crWarning("glXChooseVisual: aux_buffers=%d unsupported",
                                            aux_buffers);
                        return NULL;
                    }
                }
                attrib++;
                break;

            case GLX_RED_SIZE:
            case GLX_GREEN_SIZE:
            case GLX_BLUE_SIZE:
                if (attrib[1] > 0)
                    visBits |= CR_RGB_BIT;
                attrib++;
                break;

            case GLX_ALPHA_SIZE:
                if (attrib[1] > 0)
                    visBits |= CR_ALPHA_BIT;
                attrib++;
                break;

            case GLX_DEPTH_SIZE:
                if (attrib[1] > 0)
                    visBits |= CR_DEPTH_BIT;
                attrib++;
                break;

            case GLX_STENCIL_SIZE:
                if (attrib[1] > 0)
                    visBits |= CR_STENCIL_BIT;
                attrib++;
                break;

            case GLX_ACCUM_RED_SIZE:
            case GLX_ACCUM_GREEN_SIZE:
            case GLX_ACCUM_BLUE_SIZE:
            case GLX_ACCUM_ALPHA_SIZE:
                if (attrib[1] > 0)
                    visBits |= CR_ACCUM_BIT;
                attrib++;
                break;

            case GLX_SAMPLE_BUFFERS_SGIS: /* aka GLX_SAMPLES_ARB */
                if (attrib[1] > 0)
                    visBits |= CR_MULTISAMPLE_BIT;
                attrib++;
                break;
            case GLX_SAMPLES_SGIS: /* aka GLX_SAMPLES_ARB */
                /* just ignore value for now, we'll try to get 4 samples/pixel */
                if (attrib[1] > 4)
                    return NULL;
                visBits |= CR_MULTISAMPLE_BIT;
                attrib++;
                break;

#ifdef GLX_VERSION_1_3
            case GLX_X_VISUAL_TYPE:
            case GLX_TRANSPARENT_TYPE_EXT:
            case GLX_TRANSPARENT_INDEX_VALUE_EXT:
            case GLX_TRANSPARENT_RED_VALUE_EXT:
            case GLX_TRANSPARENT_GREEN_VALUE_EXT:
            case GLX_TRANSPARENT_BLUE_VALUE_EXT:
            case GLX_TRANSPARENT_ALPHA_VALUE_EXT:
                /* ignore */
                crWarning("glXChooseVisual: ignoring attribute 0x%x", *attrib);
                attrib++;
                break;
#endif

            default:
                crWarning( "glXChooseVisual: bad attrib=0x%x", *attrib );
                return NULL;
        }
    }

    if ((visBits & CR_RGB_BIT) == 0 && (visBits & CR_OVERLAY_BIT) == 0)
    {
        /* normal layer, color index mode not supported */
        crWarning( "glXChooseVisual: didn't request RGB visual?" );
        return NULL;
    }

    vis = crChooseVisual(&stub.wsInterface, dpy, screen, GL_FALSE, visBits);
    if (!vis && (visBits & CR_STEREO_BIT)) {
        /* try non-stereo */
        visBits &= ~CR_STEREO_BIT;
        vis = crChooseVisual(&stub.wsInterface, dpy, screen, GL_FALSE, visBits);
    }

    if (vis) {
        AddVisualInfo(dpy, screen, vis->visual->visualid, visBits);
    }
    return vis;
}
#else  /* not 0 */
DECLEXPORT(XVisualInfo *)
VBOXGLXTAG(glXChooseVisual)( Display *dpy, int screen, int *attribList )
{
    bool useRGBA = false;
    int *attrib;
    XVisualInfo searchvis, *pret;
    int nvisuals;
    stubInit();

    for (attrib = attribList; *attrib != None; attrib++)
    {
        switch (*attrib)
        {
            case GLX_USE_GL:
                /* ignored, this is mandatory */
                break;

            case GLX_BUFFER_SIZE:
                /* this is for color-index visuals, which we don't support */
                attrib++;
                break;

            case GLX_LEVEL:
                if (attrib[1] != 0)
                    goto err_exit;
                attrib++;
                break;

            case GLX_RGBA:
                useRGBA = true;
                break;

            case GLX_STEREO:
                goto err_exit;
                /*
                crWarning( "glXChooseVisual: stereo unsupported" );
                return NULL;
                */
                break;

            case GLX_AUX_BUFFERS:
                if (attrib[1] != 0)
                    goto err_exit;
                attrib++;
                break;

            case GLX_RED_SIZE:
            case GLX_GREEN_SIZE:
            case GLX_BLUE_SIZE:
                if (attrib[1] > 8)
                    goto err_exit;
                attrib++;
                break;

            case GLX_ALPHA_SIZE:
                if (attrib[1] > 8)
                    goto err_exit;
                attrib++;
                break;

            case GLX_DEPTH_SIZE:
                if (attrib[1] > 16)
                    goto err_exit;
                attrib++;
                break;

            case GLX_STENCIL_SIZE:
                if (attrib[1] > 8)
                    goto err_exit;
                attrib++;
                break;

            case GLX_ACCUM_RED_SIZE:
            case GLX_ACCUM_GREEN_SIZE:
            case GLX_ACCUM_BLUE_SIZE:
            case GLX_ACCUM_ALPHA_SIZE:
                if (attrib[1] > 16)
                    goto err_exit;
                attrib++;
                break;

            case GLX_SAMPLE_BUFFERS_SGIS: /* aka GLX_SAMPLES_ARB */
                if (attrib[1] > 0)
                    goto err_exit;
                attrib++;
                break;
            case GLX_SAMPLES_SGIS: /* aka GLX_SAMPLES_ARB */
                if (attrib[1] > 0)
                    goto err_exit;
                attrib++;
                break;

            case GLX_DOUBLEBUFFER: /* @todo, check if we support it */
                break;

#ifdef GLX_VERSION_1_3
            case GLX_X_VISUAL_TYPE:
            case GLX_TRANSPARENT_TYPE_EXT:
            case GLX_TRANSPARENT_INDEX_VALUE_EXT:
            case GLX_TRANSPARENT_RED_VALUE_EXT:
            case GLX_TRANSPARENT_GREEN_VALUE_EXT:
            case GLX_TRANSPARENT_BLUE_VALUE_EXT:
            case GLX_TRANSPARENT_ALPHA_VALUE_EXT:
                /* ignore */
                crWarning("glXChooseVisual: ignoring attribute 0x%x", *attrib);
                attrib++;
                break;
#endif

            default:
                crWarning( "glXChooseVisual: bad attrib=0x%x, ignoring", *attrib );
                attrib++;
                //return NULL;
        }
    }

    if (!useRGBA)
        return NULL;

    searchvis.visualid = XVisualIDFromVisual(DefaultVisual(dpy, screen));
    pret = XGetVisualInfo(dpy, VisualIDMask, &searchvis, &nvisuals);
      
    if (nvisuals!=1) crWarning("glXChooseVisual: XGetVisualInfo returned %i visuals for %x", nvisuals, (unsigned int) searchvis.visualid);
    if (pret)
      crDebug("glXChooseVisual returned %x depth=%i", (unsigned int)pret->visualid, pret->depth);
    return pret;

err_exit:
    crDebug("glXChooseVisual returning NULL, due to attrib=0x%x, next=0x%x", attrib[0], attrib[1]);
    return NULL;
}
#endif

/**
 **  There is a problem with glXCopyContext.
 ** IRIX and Mesa both define glXCopyContext
 ** to have the mask argument being a 
 ** GLuint.  XFree 4 and oss.sgi.com
 ** define it to be an unsigned long.
 ** Solution: We don't support
 ** glXCopyContext anyway so we'll just
 ** #ifdef out the code.
 */
DECLEXPORT(void)
VBOXGLXTAG(glXCopyContext)( Display *dpy, GLXContext src, GLXContext dst, 
#if defined(AIX) || defined(PLAYSTATION2)
GLuint mask )
#elif defined(SunOS)
unsigned long mask )
#else
unsigned long mask )
#endif
{
    (void) dpy;
    (void) src;
    (void) dst;
    (void) mask;
    crWarning( "Unsupported GLX Call: glXCopyContext()" );
}


/**
 * Get the display string for the given display pointer.
 * Never return just ":0.0".  In that case, prefix with our host name.
 */
static void
stubGetDisplayString( Display *dpy, char *nameResult, int maxResult )
{
    const char *dpyName = DisplayString(dpy);
    char host[1000];
#ifndef VBOX_NO_NATIVEGL
    if (dpyName[0] == ':')
    {
        crGetHostname(host, 1000);
    }
    else
#endif
    {
      host[0] = 0;
    }
    if (crStrlen(host) + crStrlen(dpyName) >= maxResult - 1)
    {
        /* return null string */
        crWarning("Very long host / display name string in stubDisplayString!");
        nameResult[0] = 0;
    }
    else
    {
        /* return host concatenated with dpyName */
        crStrcpy(nameResult, host);
        crStrcat(nameResult, dpyName);
    }
}



DECLEXPORT(GLXContext)
VBOXGLXTAG(glXCreateContext)(Display *dpy, XVisualInfo *vis, GLXContext share, Bool direct)
{
    char dpyName[MAX_DPY_NAME];
    ContextInfo *context;
    int visBits = CR_RGB_BIT | CR_DOUBLE_BIT | CR_DEPTH_BIT; /* default vis */
    int i, numExt;

    stubInit();

    CRASSERT(stub.contextTable);

    /*
    {
        char **list;

        list = XListExtensions(dpy, &numExt);
        crDebug("X extensions [%i]:", numExt);
        for (i=0; i<numExt; ++i)
        {
            crDebug("%s", list[i]);
        }
        XFreeExtensionList(list);
    }
    */

    stubGetDisplayString(dpy, dpyName, MAX_DPY_NAME);
#ifndef VBOX_NO_NATIVEGL  /* We only care about the host capabilities, not the guest. */
    if (stub.haveNativeOpenGL) {
        int foo, bar;
        if (stub.wsInterface.glXQueryExtension(dpy, &foo, &bar)) {
            /* If we have real GLX, compute the Chromium visual bitmask now.
             * otherwise, we'll use the default desiredVisual bitmask.
             */
            struct VisualInfo *v = FindVisualInfo(dpy, DefaultScreen(dpy),
                                                  vis->visual->visualid);
            if (v) {
                visBits = v->visBits;
                /*crDebug("%s visBits=0x%x", __FUNCTION__, visBits);*/
            }
            else {
                /* For some reason, we haven't tested this visual
                 * before.  This could be because the visual was found 
                 * through a different display connection to the same
                 * display (as happens in GeoProbe), or through a
                 * connection to an external daemon that queries
                 * visuals.  If we can query it directly, we can still
                 * find the proper visBits.
                 */
                int newVisBits = QueryVisBits(dpy, vis);
                if (newVisBits > 0) {
                    AddVisualInfo(dpy, DefaultScreen(dpy), vis->visual->visualid, newVisBits);
                    crDebug("Application used unexpected but queryable visual id 0x%x", (int) vis->visual->visualid);
                    visBits = newVisBits;
                }
                else {
                    crWarning("Application used unexpected and unqueryable visual id 0x%x; using default visbits", (int) vis->visual->visualid);
                }
            }

            /*crDebug("ComputeVisBits(0x%x) = 0x%x", (int)vis->visual->visualid, visBits);*/
            if (stub.force_pbuffers) {
                crDebug("App faker: Forcing use of Pbuffers");
                visBits |= CR_PBUFFER_BIT;
            }

            if (!v) {
                 AddVisualInfo(dpy, DefaultScreen(dpy),
                               vis->visual->visualid, visBits);
            }

        }
    }
    else {
        crDebug("No native OpenGL; cannot compute visbits");
    }
#endif

    context = stubNewContext(dpyName, visBits, UNDECIDED, (unsigned long) share);
    if (!context)
        return 0;

    context->dpy = dpy;
    context->visual = vis;
    context->direct = direct;

    return (GLXContext) context->id;
}


DECLEXPORT(void) VBOXGLXTAG(glXDestroyContext)( Display *dpy, GLXContext ctx )
{
    (void) dpy;
    stubDestroyContext( (unsigned long) ctx );
}


DECLEXPORT(Bool) VBOXGLXTAG(glXMakeCurrent)( Display *dpy, GLXDrawable drawable, GLXContext ctx )
{
    ContextInfo *context;
    WindowInfo *window;
    Bool retVal;

    /*crDebug("glXMakeCurrent(%p, 0x%x, 0x%x)", (void *) dpy, (int) drawable, (int) ctx);*/

    if (ctx && drawable) {
        context = (ContextInfo *) crHashtableSearch(stub.contextTable, (unsigned long) ctx);
        window = stubGetWindowInfo(dpy, drawable);

        if (context && context->type == UNDECIDED) {
            XSync(dpy, 0); /* sync to force window creation on the server */
        }
    }
    else {
        dpy = NULL;
        window = NULL;
        context = NULL;
    }

    currentDisplay = dpy;
    currentDrawable = drawable;

    retVal = stubMakeCurrent(window, context);
    return retVal;
}


DECLEXPORT(GLXPixmap) VBOXGLXTAG(glXCreateGLXPixmap)( Display *dpy, XVisualInfo *vis, Pixmap pixmap )
{
    (void) dpy;
    (void) vis;
    (void) pixmap;

    stubInit();
    crWarning( "Unsupported GLX Call: glXCreateGLXPixmap()" );
    return (GLXPixmap) 0;
}

DECLEXPORT(void) VBOXGLXTAG(glXDestroyGLXPixmap)( Display *dpy, GLXPixmap pix )
{
    VBOXGLXTAG(glXDestroyPixmap)(dpy, pix);
}

#ifndef VBOX_NO_NATIVEGL  /* old code */
DECLEXPORT(int) VBOXGLXTAG(glXGetConfig)( Display *dpy, XVisualInfo *vis, int attrib, int *value )
{
    struct VisualInfo *v;
    int visBits;

    if (!vis) {
        /* SGI OpenGL Performer hits this */
        crWarning("glXGetConfig called with NULL XVisualInfo");
        return GLX_BAD_VISUAL;
    }

    v = FindVisualInfo(dpy, DefaultScreen(dpy), vis->visual->visualid);
    if (v) {
        visBits = v->visBits;
    }
    else {
        visBits = 0;
    }

    stubInit();

    /* try to satisfy this request with the native glXGetConfig() */
    if (stub.haveNativeOpenGL)
    {
        int foo, bar;
        int return_val;

        if (stub.wsInterface.glXQueryExtension(dpy, &foo, &bar))
        {
            return_val = stub.wsInterface.glXGetConfig( dpy, vis, attrib, value );
            if (return_val)
            {
                crDebug("faker native glXGetConfig returned %s",
                                glx_error_string(return_val));
            }
            return return_val;
        }
    }

    /*
     * If the GLX application chooses its visual via a bunch of calls to
     * glXGetConfig, instead of by calling glXChooseVisual, we need to keep
     * track of which attributes are queried to help satisfy context creation
     * later.
     */
    switch ( attrib ) {

        case GLX_USE_GL:
            *value = 1;
            break;

        case GLX_BUFFER_SIZE:
            *value = 32;
            break;

        case GLX_LEVEL:
            visBits |= CR_OVERLAY_BIT;
            *value = (visBits & CR_OVERLAY_BIT) ? 1 : 0;
            break;

        case GLX_RGBA:
            visBits |= CR_RGB_BIT;
            *value = 1;
            break;

        case GLX_DOUBLEBUFFER:
            *value = 1;
            break;

        case GLX_STEREO:
            *value = 1;
            break;

        case GLX_AUX_BUFFERS:
            *value = 0;
            break;

        case GLX_RED_SIZE:
            *value = 8;
            break;

        case GLX_GREEN_SIZE:
            *value = 8;
            break;

        case GLX_BLUE_SIZE:
            *value = 8;
            break;

        case GLX_ALPHA_SIZE:
            visBits |= CR_ALPHA_BIT;
            *value = (visBits & CR_ALPHA_BIT) ? 8 : 0;
            break;

        case GLX_DEPTH_SIZE:
            visBits |= CR_DEPTH_BIT;
            *value = 16;
            break;

        case GLX_STENCIL_SIZE:
            visBits |= CR_STENCIL_BIT;
            *value = 8;
            break;

        case GLX_ACCUM_RED_SIZE:
            visBits |= CR_ACCUM_BIT;
            *value = 16;
            break;

        case GLX_ACCUM_GREEN_SIZE:
            visBits |= CR_ACCUM_BIT;
            *value = 16;
            break;

        case GLX_ACCUM_BLUE_SIZE:
            visBits |= CR_ACCUM_BIT;
            *value = 16;
            break;

        case GLX_ACCUM_ALPHA_SIZE:
            visBits |= CR_ACCUM_BIT;
            *value = 16;
            break;

        case GLX_SAMPLE_BUFFERS_SGIS:
            visBits |= CR_MULTISAMPLE_BIT;
            *value = 0;  /* fix someday */
            break;

        case GLX_SAMPLES_SGIS:
            visBits |= CR_MULTISAMPLE_BIT;
            *value = 0;  /* fix someday */
            break;

        case GLX_VISUAL_CAVEAT_EXT:
            *value = GLX_NONE_EXT;
            break;
#if  defined(SunOS)
        /*
          I don't think this is even a valid attribute for glxGetConfig. 
          No idea why this gets called under SunOS but we simply ignore it
          -- jw
        */
        case GLX_X_VISUAL_TYPE:
          crWarning ("Ignoring Unsuported GLX Call: glxGetConfig with attrib 0x%x", attrib);
          break;
#endif 

        case GLX_TRANSPARENT_TYPE:
            *value = GLX_NONE_EXT;
            break;
        case GLX_TRANSPARENT_INDEX_VALUE:
            *value = 0;
            break;
        case GLX_TRANSPARENT_RED_VALUE:
            *value = 0;
            break;
        case GLX_TRANSPARENT_GREEN_VALUE:
            *value = 0;
            break;
        case GLX_TRANSPARENT_BLUE_VALUE:
            *value = 0;
            break;
        case GLX_TRANSPARENT_ALPHA_VALUE:
            *value = 0;
            break;
        default:
            crWarning( "Unsupported GLX Call: glXGetConfig with attrib 0x%x", attrib );
            return GLX_BAD_ATTRIBUTE;
    }

    AddVisualInfo(dpy, DefaultScreen(dpy), vis->visual->visualid, visBits);

    return 0;
}
#else  /* not 0 */
DECLEXPORT(int) VBOXGLXTAG(glXGetConfig)( Display *dpy, XVisualInfo *vis, int attrib, int *value )
{
    if (!vis) {
        /* SGI OpenGL Performer hits this */
        crWarning("glXGetConfig called with NULL XVisualInfo");
        return GLX_BAD_VISUAL;
    }

    stubInit();

    *value = 0;  /* For sanity */

    switch ( attrib ) {

        case GLX_USE_GL:
            *value = 1;
            break;

        case GLX_BUFFER_SIZE:
            *value = 32;
            break;

        case GLX_LEVEL:
            *value = 0;  /* for now */
            break;

        case GLX_RGBA:
            *value = 1;
            break;

        case GLX_DOUBLEBUFFER:
            *value = 1;
            break;

        case GLX_STEREO:
            *value = 1;
            break;

        case GLX_AUX_BUFFERS:
            *value = 0;
            break;

        case GLX_RED_SIZE:
            *value = 8;
            break;

        case GLX_GREEN_SIZE:
            *value = 8;
            break;

        case GLX_BLUE_SIZE:
            *value = 8;
            break;

        case GLX_ALPHA_SIZE:
            *value = 8;
            break;

        case GLX_DEPTH_SIZE:
            *value = 16;
            break;

        case GLX_STENCIL_SIZE:
            *value = 8;
            break;

        case GLX_ACCUM_RED_SIZE:
            *value = 16;
            break;

        case GLX_ACCUM_GREEN_SIZE:
            *value = 16;
            break;

        case GLX_ACCUM_BLUE_SIZE:
            *value = 16;
            break;

        case GLX_ACCUM_ALPHA_SIZE:
            *value = 16;
            break;

        case GLX_SAMPLE_BUFFERS_SGIS:
            *value = 0;  /* fix someday */
            break;

        case GLX_SAMPLES_SGIS:
            *value = 0;  /* fix someday */
            break;

        case GLX_VISUAL_CAVEAT_EXT:
            *value = GLX_NONE_EXT;
            break;
#if defined(SunOS) || 1
        /*
          I don't think this is even a valid attribute for glxGetConfig. 
          No idea why this gets called under SunOS but we simply ignore it
          -- jw
        */
        case GLX_X_VISUAL_TYPE:
          crWarning ("Ignoring Unsuported GLX Call: glxGetConfig with attrib 0x%x", attrib);
          break;
#endif 

        case GLX_TRANSPARENT_TYPE:
            *value = GLX_NONE_EXT;
            break;
        case GLX_TRANSPARENT_INDEX_VALUE:
            *value = 0;
            break;
        case GLX_TRANSPARENT_RED_VALUE:
            *value = 0;
            break;
        case GLX_TRANSPARENT_GREEN_VALUE:
            *value = 0;
            break;
        case GLX_TRANSPARENT_BLUE_VALUE:
            *value = 0;
            break;
        case GLX_TRANSPARENT_ALPHA_VALUE:
            *value = 0;
            break;
        default:
            crWarning( "Unsupported GLX Call: glXGetConfig with attrib 0x%x, ignoring...", attrib );
            //return GLX_BAD_ATTRIBUTE;
            *value = 0;
    }

    return 0;
}
#endif

DECLEXPORT(GLXContext) VBOXGLXTAG(glXGetCurrentContext)( void )
{
    if (stub.currentContext)
        return (GLXContext) stub.currentContext->id;
    else
        return (GLXContext) NULL;
}

DECLEXPORT(GLXDrawable) VBOXGLXTAG(glXGetCurrentDrawable)(void)
{
    return currentDrawable;
}

DECLEXPORT(Display *) VBOXGLXTAG(glXGetCurrentDisplay)(void)
{
    return currentDisplay;
}

DECLEXPORT(Bool) VBOXGLXTAG(glXIsDirect)(Display *dpy, GLXContext ctx)
{
    (void) dpy;
    (void) ctx;
    crDebug("->glXIsDirect");
    return True;
}

DECLEXPORT(Bool) VBOXGLXTAG(glXQueryExtension)(Display *dpy, int *errorBase, int *eventBase)
{
    (void) dpy;
    (void) errorBase;
    (void) eventBase;
    return 1; /* You BET we do... */
}

DECLEXPORT(Bool) VBOXGLXTAG(glXQueryVersion)( Display *dpy, int *major, int *minor )
{
    (void) dpy;
    *major = 1;
    *minor = 3;
    return 1;
}

DECLEXPORT(void) VBOXGLXTAG(glXSwapBuffers)( Display *dpy, GLXDrawable drawable )
{
    const WindowInfo *window = stubGetWindowInfo(dpy, drawable);
    stubSwapBuffers( window, 0 );
}

#ifndef VBOX_NO_NATIVEGL
DECLEXPORT(void) VBOXGLXTAG(glXUseXFont)( Font font, int first, int count, int listBase )
{
    if (stub.currentContext->type == CHROMIUM)
    {
        Display *dpy = stub.wsInterface.glXGetCurrentDisplay();
        if (dpy) {
            stubUseXFont( dpy, font, first, count, listBase );
        }
        else {
            dpy = XOpenDisplay(NULL);
            if (!dpy)
                return;
            stubUseXFont( dpy, font, first, count, listBase );
            XCloseDisplay(dpy);
        }
    } else
        stub.wsInterface.glXUseXFont( font, first, count, listBase );
}
#else /* not 0 */
DECLEXPORT(void) VBOXGLXTAG(glXUseXFont)( Font font, int first, int count, int listBase )
{
    Display *dpy = stub.currentContext->dpy;
    if (dpy) {
        stubUseXFont( dpy, font, first, count, listBase );
    }
    else {
        dpy = XOpenDisplay(NULL);
        if (!dpy)
            return;
        stubUseXFont( dpy, font, first, count, listBase );
        XCloseDisplay(dpy);
    }
}
#endif

DECLEXPORT(void) VBOXGLXTAG(glXWaitGL)( void )
{
    static int first_call = 1;

    if ( first_call )
    {
        crDebug( "Ignoring unsupported GLX call: glXWaitGL()" );
        first_call = 0;
    }
}

DECLEXPORT(void) VBOXGLXTAG(glXWaitX)( void )
{
    static int first_call = 1;

    if ( first_call )
    {
        crDebug( "Ignoring unsupported GLX call: glXWaitX()" );
        first_call = 0;
    }
}

DECLEXPORT(const char *) VBOXGLXTAG(glXQueryExtensionsString)( Display *dpy, int screen )
{
    /* XXX maybe also advertise GLX_SGIS_multisample? */

    static const char *retval = "GLX_ARB_multisample GLX_EXT_texture_from_pixmap GLX_SGIX_fbconfig";

    (void) dpy;
    (void) screen;

    crDebug("->glXQueryExtensionsString");
    return retval;
}

DECLEXPORT(const char *) VBOXGLXTAG(glXGetClientString)( Display *dpy, int name )
{
    const char *retval;
    (void) dpy;
    (void) name;

    switch ( name ) {

        case GLX_VENDOR:
            retval  = "Chromium";
            break;

        case GLX_VERSION:
            retval  = "1.2 Chromium";
            break;

        case GLX_EXTENSIONS:
            /*@todo should be a screen not a name...but it's not used anyway*/
            retval  = glXQueryExtensionsString(dpy, name);
            break;

        default:
            retval  = NULL;
    }

    return retval;
}

DECLEXPORT(const char *) VBOXGLXTAG(glXQueryServerString)( Display *dpy, int screen, int name )
{
    const char *retval;
    (void) dpy;
    (void) screen;

    switch ( name ) {

        case GLX_VENDOR:
            retval  = "Chromium";
            break;

        case GLX_VERSION:
            retval  = "1.2 Chromium";
            break;

        case GLX_EXTENSIONS:
            retval  = glXQueryExtensionsString(dpy, screen);
            break;

        default:
            retval  = NULL;
    }

    return retval;
}

DECLEXPORT(CR_GLXFuncPtr) VBOXGLXTAG(glXGetProcAddressARB)( const GLubyte *name )
{
    return (CR_GLXFuncPtr) crGetProcAddress( (const char *) name );
}

DECLEXPORT(CR_GLXFuncPtr) VBOXGLXTAG(glXGetProcAddress)( const GLubyte *name )
{
    return (CR_GLXFuncPtr) crGetProcAddress( (const char *) name );
}


#if GLX_EXTRAS

DECLEXPORT(GLXPbufferSGIX) 
VBOXGLXTAG(glXCreateGLXPbufferSGIX)(Display *dpy, GLXFBConfigSGIX config,
                                    unsigned int width, unsigned int height,
                                    int *attrib_list)
{
    (void) dpy;
    (void) config;
    (void) width;
    (void) height;
    (void) attrib_list;
    crWarning("glXCreateGLXPbufferSGIX not implemented by Chromium");
    return 0;
}

DECLEXPORT(void) VBOXGLXTAG(glXDestroyGLXPbufferSGIX)(Display *dpy, GLXPbuffer pbuf)
{
    (void) dpy;
    (void) pbuf;
    crWarning("glXDestroyGLXPbufferSGIX not implemented by Chromium");
}

DECLEXPORT(void) VBOXGLXTAG(glXSelectEventSGIX)(Display *dpy, GLXDrawable drawable, unsigned long mask)
{
    (void) dpy;
    (void) drawable;
    (void) mask;
}

DECLEXPORT(void) VBOXGLXTAG(glXGetSelectedEventSGIX)(Display *dpy, GLXDrawable drawable, unsigned long *mask)
{
    (void) dpy;
    (void) drawable;
    (void) mask;
}

DECLEXPORT(int) VBOXGLXTAG(glXQueryGLXPbufferSGIX)(Display *dpy, GLXPbuffer pbuf,
                                                   int attribute, unsigned int *value)
{
    (void) dpy;
    (void) pbuf;
    (void) attribute;
    (void) value;
    crWarning("glXQueryGLXPbufferSGIX not implemented by Chromium");
    return 0;
}

DECLEXPORT(int) VBOXGLXTAG(glXGetFBConfigAttribSGIX)(Display *dpy, GLXFBConfig config,
                                                     int attribute, int *value)
{
    return VBOXGLXTAG(glXGetFBConfigAttrib)(dpy, config, attribute, value);
}

DECLEXPORT(GLXFBConfigSGIX *) 
VBOXGLXTAG(glXChooseFBConfigSGIX)(Display *dpy, int screen,
                                  int *attrib_list, int *nelements)
{
    (void) dpy;
    (void) screen;
    (void) attrib_list;
    (void) nelements;
    crWarning("glXChooseFBConfigSGIX not implemented by Chromium");
    return NULL;
}

DECLEXPORT(GLXPixmap) 
VBOXGLXTAG(glXCreateGLXPixmapWithConfigSGIX)(Display *dpy,
                                             GLXFBConfig config,
                                             Pixmap pixmap)
{
    (void) dpy;
    (void) config;
    (void) pixmap;
    crWarning("glXCreateGLXPixmapWithConfigSGIX not implemented by Chromium");
    return 0;    }

DECLEXPORT(GLXContext) 
VBOXGLXTAG(glXCreateContextWithConfigSGIX)(Display *dpy, GLXFBConfig config,
                                           int render_type,
                                           GLXContext share_list,
                                           Bool direct)
{
    (void) dpy;
    (void) config;
    (void) render_type;
    (void) share_list;
    (void) direct;
    crWarning("glXCreateContextWithConfigSGIX not implemented by Chromium");
    return NULL;
}

DECLEXPORT(XVisualInfo *) 
VBOXGLXTAG(glXGetVisualFromFBConfigSGIX)(Display *dpy,
                                         GLXFBConfig config)
{
    (void) dpy;
    (void) config;
    crWarning("glXGetVisualFromFBConfigSGIX not implemented by Chromium");
    return NULL;
}

DECLEXPORT(GLXFBConfigSGIX)
VBOXGLXTAG(glXGetFBConfigFromVisualSGIX)(Display *dpy, XVisualInfo *vis)
{
    (void) dpy;
    (void) vis;
    crWarning("glXGetFBConfigFromVisualSGIX not implemented by Chromium");
    return NULL;
}

/*
 * GLX 1.3 functions
 */
DECLEXPORT(GLXFBConfig *) 
VBOXGLXTAG(glXChooseFBConfig)(Display *dpy, int screen, ATTRIB_TYPE *attrib_list, int *nelements)
{
    (void) dpy;
    (void) screen;
    (void) attrib_list;
    (void) nelements;
    crWarning("glXChooseFBConfig not implemented by Chromium");
    return NULL;
}

DECLEXPORT(GLXContext) 
VBOXGLXTAG(glXCreateNewContext)(Display *dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct)
{
    XVisualInfo *vis;

    (void) dpy;
    (void) config;
    (void) render_type;
    (void) share_list;
    (void) direct;

    if (render_type != GLX_RGBA_TYPE)
    {
        crWarning("glXCreateNewContext, unsupported render_type %x", render_type);
        return NULL;
    }

    vis = VBOXGLXTAG(glXGetVisualFromFBConfig)(dpy, config);
    return VBOXGLXTAG(glXCreateContext)(dpy, vis, share_list, direct);
}

DECLEXPORT(GLXPbuffer) 
VBOXGLXTAG(glXCreatePbuffer)(Display *dpy, GLXFBConfig config, ATTRIB_TYPE *attrib_list)
{
    (void) dpy;
    (void) config;
    (void) attrib_list;
    crWarning("glXCreatePbuffer not implemented by Chromium");
    return 0;
}

/* Note: there're examples where glxpixmaps are created without current context, so can't do much of the work here.
 * Instead we'd do necessary initialization on first use of those pixmaps.
 */
DECLEXPORT(GLXPixmap) 
VBOXGLXTAG(glXCreatePixmap)(Display *dpy, GLXFBConfig config, Pixmap pixmap, const ATTRIB_TYPE *attrib_list)
{
    (void) dpy;
    (void) config;
    (void) attrib_list;

    return (GLXPixmap) pixmap;
}

DECLEXPORT(GLXWindow) 
VBOXGLXTAG(glXCreateWindow)(Display *dpy, GLXFBConfig config, Window win, ATTRIB_TYPE *attrib_list)
{
    GLXFBConfig *realcfg;
    int nconfigs;
    //XVisualInfo *vis;
    (void) dpy;
    (void) config;
    (void) win;
    (void) attrib_list;
    //crWarning("glXCreateWindow not implemented by Chromium");
    //vis = VBOXGLXTAG(glXGetVisualFromFBConfig)(config);
    if (stub.wsInterface.glXGetFBConfigs)
    {
        realcfg = stub.wsInterface.glXGetFBConfigs(dpy, 0, &nconfigs);
        if (!realcfg || nconfigs<1)
        {
            crWarning("glXCreateWindow !realcfg || nconfigs<1");
            return 0;
        }
        else
        {
            return stub.wsInterface.glXCreateWindow(dpy, realcfg[0], win, attrib_list);
        }
    }
    else
    {
        crWarning("glXCreateWindow stub.wsInterface.glXChooseFBConfig==NULL");
        return 0;
    }
}

DECLEXPORT(void) VBOXGLXTAG(glXDestroyPbuffer)(Display *dpy, GLXPbuffer pbuf)
{
    (void) dpy;
    (void) pbuf;
    crWarning("glXDestroyPbuffer not implemented by Chromium");
}

DECLEXPORT(void) VBOXGLXTAG(glXDestroyPixmap)(Display *dpy, GLXPixmap pixmap)
{
    GLX_Pixmap_t *pGlxPixmap;

    if (!stub.currentContext)
    {
        crWarning("glXDestroyPixmap failed, no current context");
        return;
    }

    pGlxPixmap = (GLX_Pixmap_t *) crHashtableSearch(stub.currentContext->pGLXPixmapsHash, (unsigned int) pixmap);

    if (pGlxPixmap)
    {
        if (pGlxPixmap->gc)
        {
            XFreeGC(dpy, pGlxPixmap->gc);
        }

        if (pGlxPixmap->hShmPixmap>0)
        {
            XFreePixmap(dpy, pGlxPixmap->hShmPixmap);
        }

        if (pGlxPixmap->hDamage>0)
        {
            //crDebug("Destroy: Damage for drawable 0x%x, handle 0x%x", (unsigned int) pixmap, (unsigned int) pGlxPixmap->damage);
            XDamageDestroy(stub.currentContext->damageDpy, pGlxPixmap->hDamage);
        }

        if (pGlxPixmap->pDamageRegion)
        {
            XDestroyRegion(pGlxPixmap->pDamageRegion);
        }

        crHashtableDelete(stub.currentContext->pGLXPixmapsHash, (unsigned int) pixmap, crFree);
    }
    /*else
    {
        crWarning("glXDestroyPixmap called for unknown glxpixmap 0x%x", (unsigned int) pixmap);
    }*/
}

DECLEXPORT(void) VBOXGLXTAG(glXDestroyWindow)(Display *dpy, GLXWindow win)
{
    (void) dpy;
    (void) win;
    crWarning("glXDestroyWindow not implemented by Chromium");
}

DECLEXPORT(GLXDrawable) VBOXGLXTAG(glXGetCurrentReadDrawable)(void)
{
    //crWarning("glXGetCurrentReadDrawable not implemented by Chromium");
    return currentReadDrawable;
}

DECLEXPORT(int) VBOXGLXTAG(glXGetFBConfigAttrib)(Display *dpy, GLXFBConfig config, int attribute, int *value)
{
    XVisualInfo * pVisual;

    pVisual =  glXGetVisualFromFBConfig(dpy, config);
    if (!pVisual)
    {
        crWarning("glXGetFBConfigAttrib for 0x%x, failed to get XVisualInfo", (int)config);
        return GLX_BAD_ATTRIBUTE;
    }
    //crDebug("glXGetFBConfigAttrib 0x%x for 0x%x, visualid=0x%x, depth=%i", attribute, (int)config, (int)pVisual->visualid, pVisual->depth);
    

    switch (attribute)
    {
        case GLX_DRAWABLE_TYPE:
            *value = GLX_PIXMAP_BIT;
            break;
        case GLX_BIND_TO_TEXTURE_TARGETS_EXT:
            *value = GLX_TEXTURE_2D_BIT_EXT;
            break;
        case GLX_BIND_TO_TEXTURE_RGBA_EXT:
            //crDebug("attribute=GLX_BIND_TO_TEXTURE_RGBA_EXT");
            *value = True;
            break;
        case GLX_BIND_TO_TEXTURE_RGB_EXT:
            *value = True;
            break;
        case GLX_DOUBLEBUFFER:
            //crDebug("attribute=GLX_DOUBLEBUFFER");
            *value = True;
            break;
        case GLX_Y_INVERTED_EXT:
            *value = True;
            break;
        case GLX_ALPHA_SIZE:
            //crDebug("attribute=GLX_ALPHA_SIZE");
            *value = pVisual->depth==32 ? 8:0;
            break;
        case GLX_BUFFER_SIZE:
            //crDebug("attribute=GLX_BUFFER_SIZE");
            *value = pVisual->depth;
            break;
        case GLX_STENCIL_SIZE:
            //crDebug("attribute=GLX_STENCIL_SIZE");
            *value = 8;
            break;
        case GLX_DEPTH_SIZE:
            *value = 16;
            //crDebug("attribute=GLX_DEPTH_SIZE");
            break;
        case GLX_BIND_TO_MIPMAP_TEXTURE_EXT:
            *value = 0;
            break;
        case GLX_RENDER_TYPE:
            //crDebug("attribute=GLX_RENDER_TYPE");
            *value = GLX_RGBA_BIT;
            break;
        case GLX_CONFIG_CAVEAT:
            //crDebug("attribute=GLX_CONFIG_CAVEAT");
            *value = GLX_NONE;
            break;
        case GLX_VISUAL_ID:
            //crDebug("attribute=GLX_VISUAL_ID");
            *value = pVisual->visualid;
            break;
        case GLX_FBCONFIG_ID:
            *value = pVisual->visualid; /*or config, though those are the same at the moment but this could change one day?*/
            break;
        case GLX_RED_SIZE:
        case GLX_GREEN_SIZE:
        case GLX_BLUE_SIZE:
            *value = 8;
            break;
        case GLX_LEVEL:
            *value = 0;
            break;
        case GLX_STEREO:
            *value = false;
            break;
        case GLX_AUX_BUFFERS:
            *value = 0;
            break;
        case GLX_ACCUM_RED_SIZE:
        case GLX_ACCUM_GREEN_SIZE:
        case GLX_ACCUM_BLUE_SIZE:
        case GLX_ACCUM_ALPHA_SIZE:
            *value = 0;
            break;
        case GLX_X_VISUAL_TYPE:
            *value = GLX_TRUE_COLOR;
            break;
        case GLX_TRANSPARENT_TYPE:
            *value = GLX_NONE;
            break;
        case GLX_SAMPLE_BUFFERS:
        case GLX_SAMPLES:
            *value = 1;
            break;
        default:
            crDebug("glXGetFBConfigAttrib: unknown attribute=0x%x", attribute); 
            XFree(pVisual);
            return GLX_BAD_ATTRIBUTE;
    }

    XFree(pVisual);
    return Success;
}

#if !defined(VBOX_NO_NATIVEGL) || 1 /* need fbconfigs atleast for depths 24 and 32 */
DECLEXPORT(GLXFBConfig *) VBOXGLXTAG(glXGetFBConfigs)(Display *dpy, int screen, int *nelements)
{
    GLXFBConfig *pGLXFBConfigs = NULL;
    struct VisualInfo *v;
    int i=0, cVisuals;
    XVisualInfo searchvis, *pVisuals;

    *nelements = 0;

    /*
    for (v = VisualInfoList; v; v = v->next) {
        if (v->dpy == dpy && v->screen == screen)
            ++*nelements;
    }

    if (*nelements)
        pGLXFBConfigs = crAlloc(*nelements * sizeof(GLXFBConfig));

    for (v = VisualInfoList; v && i<*nelements; v = v->next) {
        if (v->dpy == dpy && v->screen == screen)
            pGLXFBConfigs[i++] = (GLXFBConfig) v->visualid;
    }
    */

    /*@todo doesn't really list all the common visuals, have to use some static list*/
    searchvis.screen = screen;
    pVisuals = XGetVisualInfo(dpy, VisualScreenMask, &searchvis, nelements);

    if (*nelements)
        pGLXFBConfigs = crAlloc(*nelements * sizeof(GLXFBConfig));

    for (i=0; i<*nelements; ++i)
    {
        pGLXFBConfigs[i] = (GLXFBConfig) pVisuals[i].visualid;
    }

    XFree(pVisuals);

    crDebug("glXGetFBConfigs returned %i configs", *nelements);
    for (i=0; i<*nelements; ++i)
    {
        crDebug("glXGetFBConfigs[%i]=0x%x", i, (unsigned int) pGLXFBConfigs[i]);
    }
    return pGLXFBConfigs;
}
#else /* not 0 */
DECLEXPORT(GLXFBConfig *) VBOXGLXTAG(glXGetFBConfigs)(Display *dpy, int screen, int *nelements)
{
    int i;

    GLXFBConfig *pGLXFBConfigs = crAlloc(sizeof(GLXFBConfig));

    *nelements = 1;
    *pGLXFBConfigs = (GLXFBConfig) XVisualIDFromVisual(DefaultVisual(dpy, screen));

    crDebug("glXGetFBConfigs returned %i configs", *nelements);
    for (i=0; i<*nelements; ++i)
    {
        crDebug("glXGetFBConfigs[%i]=0x%x", i, (unsigned int) pGLXFBConfigs[i]);
    }
    return pGLXFBConfigs;
}
#endif

DECLEXPORT(void) VBOXGLXTAG(glXGetSelectedEvent)(Display *dpy, GLXDrawable draw, unsigned long *event_mask)
{
    (void) dpy;
    (void) draw;
    (void) event_mask;
    crWarning("glXGetSelectedEvent not implemented by Chromium");
}

DECLEXPORT(XVisualInfo *) VBOXGLXTAG(glXGetVisualFromFBConfig)(Display *dpy, GLXFBConfig config)
{
    (void) dpy;
    (void) config;
    
    struct VisualInfo *v;

    /*
    for (v = VisualInfoList; v; v = v->next) {
        if (v->dpy == dpy && v->visualid == (VisualID)config)
        {
            XVisualInfo temp, *pret;
            int nret;

            temp.visualid = v->visualid;
            pret = XGetVisualInfo(dpy, VisualIDMask, &temp, &nret);
            
            if (nret!=1) crWarning("XGetVisualInfo returned %i visuals", nret);
            crDebug("glXGetVisualFromFBConfig(cfg/visid==0x%x): depth=%i", (int) config, pret->depth);
            return pret;
        }
    }
    */

    {
        XVisualInfo temp, *pret;
        int nret;

        temp.visualid = (VisualID)config;
        pret = XGetVisualInfo(dpy, VisualIDMask, &temp, &nret);
        
        if (nret!=1) crWarning("XGetVisualInfo returned %i visuals for %x", nret, (unsigned int) config);
        //crDebug("glXGetVisualFromFBConfig(cfg/visid==0x%x): depth=%i", (int) config, pret->depth);
        return pret;
    }

    crDebug("glXGetVisualFromFBConfig unknown fbconfig 0x%x", (int) config);
    return NULL;
}

DECLEXPORT(Bool) VBOXGLXTAG(glXMakeContextCurrent)(Display *display, GLXDrawable draw, GLXDrawable read, GLXContext ctx)
{
    (void) display;
    (void) draw;
    (void) read;
    (void) ctx;
    //crWarning("glXMakeContextCurrent not implemented by Chromium");
    currentReadDrawable = read;
    return VBOXGLXTAG(glXMakeCurrent)(display, draw, ctx);
}

DECLEXPORT(int) VBOXGLXTAG(glXQueryContext)(Display *dpy, GLXContext ctx, int attribute, int *value)
{
    (void) dpy;
    (void) ctx;
    (void) attribute;
    (void) value;
    crWarning("glXQueryContext not implemented by Chromium");
    return 0;
}

DECLEXPORT(void) VBOXGLXTAG(glXQueryDrawable)(Display *dpy, GLXDrawable draw, int attribute, unsigned int *value)
{
    (void) dpy;
    (void) draw;
    (void) attribute;
    (void) value;
    crWarning("glXQueryDrawable not implemented by Chromium");
}

DECLEXPORT(void) VBOXGLXTAG(glXSelectEvent)(Display *dpy, GLXDrawable draw, unsigned long event_mask)
{
    (void) dpy;
    (void) draw;
    (void) event_mask;
    crWarning("glXSelectEvent not implemented by Chromium");
}

#ifdef CR_EXT_texture_from_pixmap
/*typedef struct
{
    int x, y;
    unsigned int w, h, border, depth;
    Window root;
    void *data;
} pminfo;*/

static void stubInitXSharedMemory(Display *dpy)
{
    int vma, vmi;
    Bool pixmaps;

    if (stub.bShmInitFailed || stub.xshmSI.shmid>=0)
        return;

    stub.bShmInitFailed = GL_TRUE;

    /* Check for extension and pixmaps format */

    if (!XShmQueryExtension(dpy))
    {
        crWarning("No XSHM extension");
        return;
    }

    if (!XShmQueryVersion(dpy, &vma, &vmi, &pixmaps) || !pixmaps)
    {
        crWarning("XSHM extension doesn't support pixmaps");
        return;
    }

    if (XShmPixmapFormat(dpy)!=ZPixmap)
    {
        crWarning("XSHM extension doesn't support ZPixmap format");
        return;
    }

    /* Alloc shared memory, so far using hardcoded value...could fail for bigger displays one day */
    stub.xshmSI.readOnly = false;
    stub.xshmSI.shmid = shmget(IPC_PRIVATE, 4*4096*2048, IPC_CREAT | 0600);
    if (stub.xshmSI.shmid<0)
    {
        crWarning("XSHM Failed to create shared segment");
        return;
    }

    stub.xshmSI.shmaddr = (char*) shmat(stub.xshmSI.shmid, NULL, 0);
    if (stub.xshmSI.shmaddr==(void*)-1)
    {
        crWarning("XSHM Failed to attach shared segment");
        shmctl(stub.xshmSI.shmid, IPC_RMID, 0);
        return;
    }


    if (!XShmAttach(dpy, &stub.xshmSI))
    {
        crWarning("XSHM Failed to attach shared segment to XServer");
        shmctl(stub.xshmSI.shmid, IPC_RMID, 0);
        shmdt(stub.xshmSI.shmaddr);
        return;
    }

    stub.bShmInitFailed = GL_FALSE;
    crInfo("Using XSHM for GLX_EXT_texture_from_pixmap");

    /*Anyway mark to be deleted when our process detaches it, in case of segfault etc*/   

/* Ramshankar: Solaris compiz fix */
#ifndef RT_OS_SOLARIS
    shmctl(stub.xshmSI.shmid, IPC_RMID, 0);
#endif
}

static void stubInitXDamageExtension(ContextInfo *pContext)
{
    int erb, vma, vmi;

    CRASSERT(pContext);
    
    if (pContext->damageInitFailed || pContext->damageDpy)
        return;

    pContext->damageInitFailed = True;

    /* Open second xserver connection to make sure we'd recieve all the xdamage messages 
     * and those wouldn't be eaten by application even queue */
    pContext->damageDpy = XOpenDisplay(DisplayString(pContext->dpy));

    if (!pContext->damageDpy)
    {
        crWarning("XDamage: Can't connect to display %s", DisplayString(pContext->dpy));
        return;
    }

    if (!XDamageQueryExtension(pContext->damageDpy, &pContext->damageEventsBase, &erb)
        || !XDamageQueryVersion(pContext->damageDpy, &vma, &vmi))
    {
        crWarning("XDamage not found or old version (%i.%i), going to run *very* slow", vma, vmi);
        XCloseDisplay(pContext->damageDpy);
        pContext->damageDpy = NULL;
        return;
    }

    crDebug("XDamage %i.%i", vma, vmi);
    pContext->damageInitFailed = False;
}

static void stubCheckXDamageCB(unsigned long key, void *data1, void *data2)
{
    GLX_Pixmap_t *pGlxPixmap = (GLX_Pixmap_t *) data1;
    XDamageNotifyEvent *e = (XDamageNotifyEvent *) data2;

    if (pGlxPixmap->hDamage==e->damage)
    {
        /*crDebug("Event: Damage for pixmap 0x%lx(drawable 0x%x), handle 0x%x (level=%i) [%i,%i,%i,%i]",
                key, (unsigned int) e->drawable, (unsigned int) e->damage, (int) e->level,
                e->area.x, e->area.y, e->area.width, e->area.height);*/

        if (pGlxPixmap->pDamageRegion)
        {
            /* If it's dirty and regions are empty, it marked for full update, so do nothing.*/
            if (!pGlxPixmap->bPixmapImageDirty || !XEmptyRegion(pGlxPixmap->pDamageRegion))
            {
                if (CR_MAX_DAMAGE_REGIONS_TRACKED <= pGlxPixmap->pDamageRegion->numRects)
                {
                    /* Mark for full update */
                    EMPTY_REGION(pGlxPixmap->pDamageRegion);
                }
                else
                {
                    /* Add to damage regions */
                    XUnionRectWithRegion(&e->area, pGlxPixmap->pDamageRegion, pGlxPixmap->pDamageRegion);
                }
            }
        }

        pGlxPixmap->bPixmapImageDirty = True;
    }
}

static GLX_Pixmap_t* stubInitGlxPixmap(Display *dpy, GLXDrawable draw, ContextInfo *pContext)
{
    int x, y;
    unsigned int w, h;
    unsigned int border;
    unsigned int depth;
    Window root;
    GLX_Pixmap_t *pGlxPixmap;

    CRASSERT(pContext);

    if (!XGetGeometry(dpy, (Pixmap)draw, &root, &x, &y, &w, &h, &border, &depth))
    {
        crWarning("stubInitGlxPixmap failed in call to XGetGeometry for 0x%x", (int) draw);
        return NULL;
    }

    pGlxPixmap = crAlloc(sizeof(GLX_Pixmap_t));
    if (!pGlxPixmap)
    {
        crWarning("stubInitGlxPixmap failed to allocate memory");
        return NULL;
    }

    pGlxPixmap->x = x;
    pGlxPixmap->y = y;
    pGlxPixmap->w = w;
    pGlxPixmap->h = h;
    pGlxPixmap->border = border;
    pGlxPixmap->depth = depth;
    pGlxPixmap->root = root;

    /* Try to allocate shared memory
     * As we're allocating huge chunk of memory, do it in this function, only if this extension is really used
     */
    if (!stub.bShmInitFailed && stub.xshmSI.shmid<0)
    {
        stubInitXSharedMemory(dpy);
    }

    if (stub.xshmSI.shmid>=0)
    {
        XGCValues xgcv;
        xgcv.graphics_exposures = False;
        xgcv.subwindow_mode = IncludeInferiors;
        pGlxPixmap->gc = XCreateGC(dpy, (Pixmap)draw, GCGraphicsExposures|GCSubwindowMode, &xgcv);

        pGlxPixmap->hShmPixmap = XShmCreatePixmap(dpy, pGlxPixmap->root, stub.xshmSI.shmaddr, &stub.xshmSI, 
                                                  pGlxPixmap->w, pGlxPixmap->h, pGlxPixmap->depth);
    }
    else
    {
        pGlxPixmap->gc = NULL;
        pGlxPixmap->hShmPixmap = 0;
    }

    stubInitXDamageExtension(pContext);

    /* If there's damage extension, then get handle for damage events related to this pixmap */
    if (pContext->damageDpy)
    {
        pGlxPixmap->hDamage = XDamageCreate(pContext->damageDpy, (Pixmap)draw, XDamageReportRawRectangles);
        /*crDebug("Create: Damage for drawable 0x%x, handle 0x%x (level=%i)",
                 (unsigned int) draw, (unsigned int) pGlxPixmap->damage, (int) XDamageReportRawRectangles);*/
        pGlxPixmap->pDamageRegion = XCreateRegion();
        if (!pGlxPixmap->pDamageRegion)
        {
            crWarning("stubInitGlxPixmap failed to create empy damage region for drawable 0x%x", (unsigned int) draw);
        }

        /*We have never seen this pixmap before, so mark it as dirty for first use*/
        pGlxPixmap->bPixmapImageDirty = True;
    }
    else
    {
        pGlxPixmap->hDamage = 0;
        pGlxPixmap->pDamageRegion = NULL;
    }

    /* glTexSubImage2D generates GL_INVALID_OP if texture array hasn't been defined by a call to glTexImage2D first. 
     * It's fine for small textures which would be updated in stubXshmUpdateWholeImage, but we'd never call glTexImage2D for big ones.
     * Note that we're making empty texture by passing NULL as pixels pointer, so there's no overhead transferring data to host.*/
    if (CR_MAX_TRANSFER_SIZE < 4*pGlxPixmap->w*pGlxPixmap->h)
    {
        stub.spu->dispatch_table.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pGlxPixmap->w, pGlxPixmap->h, 0, 
                                            GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }

    crHashtableAdd(pContext->pGLXPixmapsHash, (unsigned int) draw, pGlxPixmap);

    return pGlxPixmap;
}

static void stubXshmUpdateWholeImage(Display *dpy, GLXDrawable draw, GLX_Pixmap_t *pGlxPixmap)
{
    /* To limit the size of transferring buffer, split bigger texture into regions
     * which fit into connection buffer. Could be done in hgcm or packspu but implementation in this place allows to avoid
     * unnecessary memcpy. 
     * This also workarounds guest driver failures when sending 6+mb texture buffers on linux.
     */
    if (CR_MAX_TRANSFER_SIZE < 4*pGlxPixmap->w*pGlxPixmap->h)
    {
        XRectangle rect;

        rect.x = pGlxPixmap->x;
        rect.y = pGlxPixmap->y;
        rect.width = pGlxPixmap->w;
        rect.height = CR_MAX_TRANSFER_SIZE/(4*pGlxPixmap->w);

        /*crDebug("Texture size too big, splitting in lower sized chunks. [%i,%i,%i,%i] (%i)",
                pGlxPixmap->x, pGlxPixmap->y, pGlxPixmap->w, pGlxPixmap->h, rect.height);*/

        for (; (rect.y+rect.height)<=(pGlxPixmap->y+pGlxPixmap->h); rect.y+=rect.height)
        {
            stubXshmUpdateImageRect(dpy, draw, pGlxPixmap, &rect);
        }

        if (rect.y!=(pGlxPixmap->y+pGlxPixmap->h))
        {
            rect.height=pGlxPixmap->h-rect.y;
            stubXshmUpdateImageRect(dpy, draw, pGlxPixmap, &rect);
        }
    }
    else
    {
        XCopyArea(dpy, (Pixmap)draw, pGlxPixmap->hShmPixmap, pGlxPixmap->gc, 
                  pGlxPixmap->x, pGlxPixmap->y, pGlxPixmap->w, pGlxPixmap->h, 0, 0);
        /* Have to make sure XCopyArea is processed */
        XSync(dpy, False);
        stub.spu->dispatch_table.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pGlxPixmap->w, pGlxPixmap->h, 0, 
                                            GL_BGRA, GL_UNSIGNED_BYTE, stub.xshmSI.shmaddr);
        /*crDebug("Sync texture for drawable 0x%x(dmg handle 0x%x) [%i,%i,%i,%i]", 
                  (unsigned int) draw, (unsigned int)pGlxPixmap->hDamage, 
                  pGlxPixmap->x, pGlxPixmap->y, pGlxPixmap->w, pGlxPixmap->h);*/
    }
}

static void stubXshmUpdateImageRect(Display *dpy, GLXDrawable draw, GLX_Pixmap_t *pGlxPixmap, XRectangle *pRect)
{
    /* See comment in stubXshmUpdateWholeImage */
    if (CR_MAX_TRANSFER_SIZE < 4*pRect->width*pRect->height)
    {
        XRectangle rect;

        rect.x = pRect->x;
        rect.y = pRect->y;
        rect.width = pRect->width;
        rect.height = CR_MAX_TRANSFER_SIZE/(4*pRect->width);

        /*crDebug("Region size too big, splitting in lower sized chunks. [%i,%i,%i,%i] (%i)",
                pRect->x, pRect->y, pRect->width, pRect->height, rect.height);*/

        for (; (rect.y+rect.height)<=(pRect->y+pRect->height); rect.y+=rect.height)
        {
            stubXshmUpdateImageRect(dpy, draw, pGlxPixmap, &rect);
        }

        if (rect.y!=(pRect->y+pRect->height))
        {
            rect.height=pRect->y+pRect->height-rect.y;
            stubXshmUpdateImageRect(dpy, draw, pGlxPixmap, &rect);
        }
    }
    else
    {
        GLint origUnpackRowLength;

        XCopyArea(dpy, (Pixmap)draw, pGlxPixmap->hShmPixmap, pGlxPixmap->gc, 
                  pRect->x, pRect->y, pRect->width, pRect->height, 0, 0);
        /* Have to make sure XCopyArea is processed */
        XSync(dpy, False);

        /* Save original value, doesn't cause sync as it's reported by state tracker*/
        if (pRect->width!=pGlxPixmap->w)
        {
            stub.spu->dispatch_table.GetIntegerv(GL_UNPACK_ROW_LENGTH, &origUnpackRowLength);
            stub.spu->dispatch_table.PixelStorei(GL_UNPACK_ROW_LENGTH, pGlxPixmap->w);
        }
        stub.spu->dispatch_table.TexSubImage2D(GL_TEXTURE_2D, 0, pRect->x, pRect->y, pRect->width, pRect->height, 
                                               GL_BGRA, GL_UNSIGNED_BYTE, stub.xshmSI.shmaddr);
        if (pRect->width!=pGlxPixmap->w)
        {
            /* Restore original value*/
            stub.spu->dispatch_table.PixelStorei(GL_UNPACK_ROW_LENGTH, origUnpackRowLength);
        }

        /*crDebug("Region sync texture for drawable 0x%x(dmg handle 0x%x) [%i,%i,%i,%i]", 
                (unsigned int) draw, (unsigned int)pGlxPixmap->hDamage, 
                pRect->x, pRect->y, pRect->width, pRect->height);*/
    }
}

#if 0
Bool checkevents(Display *display, XEvent *event, XPointer arg)
{
    //crDebug("got type: 0x%x", event->type);
    if (event->type==damage_evb+XDamageNotify)
    {
        XDamageNotifyEvent *e = (XDamageNotifyEvent *) event;
        /* we're interested in pixmaps only...and those have e->drawable set to 0 or other strange value for some odd reason 
         * so have to walk glxpixmaps hashtable to find if we have damage event handle assigned to some pixmap
         */
        /*crDebug("Event: Damage for drawable 0x%x, handle 0x%x (level=%i) [%i,%i,%i,%i]",
                (unsigned int) e->drawable, (unsigned int) e->damage, (int) e->level,
                e->area.x, e->area.y, e->area.width, e->area.height);*/
        CRASSERT(stub.currentContext);
        crHashtableWalk(stub.currentContext->pGLXPixmapsHash, checkdamageCB, e);
    }
    return False;
}
#endif

/*@todo check what error codes could we throw for failures here*/
DECLEXPORT(void) VBOXGLXTAG(glXBindTexImageEXT)(Display *dpy, GLXDrawable draw, int buffer, const int *attrib_list)
{
    static int cnt=0;
    XImage dummyimg;

    GLX_Pixmap_t *pGlxPixmap;

    crDebug("->glXBindTexImageEXT");

    if (!stub.currentContext)
    {
        crWarning("glXBindTexImageEXT called without current context");
        return;
    }

    pGlxPixmap = (GLX_Pixmap_t *) crHashtableSearch(stub.currentContext->pGLXPixmapsHash, (unsigned int) draw);
    if (!pGlxPixmap)
    {
        /*Not a fault, just setup the desired information, see comment for glXCreatePixmap function*/
        pGlxPixmap = stubInitGlxPixmap(dpy, draw, stub.currentContext);

        if (!pGlxPixmap) return;
    }

    /* If there's damage extension, then process incoming events as we need the information right now */
    if (stub.currentContext->damageDpy)
    {
        /* Sync connections, note that order of syncs is important here.
         * First make sure client commands are finished, then make sure we get all the damage events back*/
        XSync(dpy, False);
        XSync(stub.currentContext->damageDpy, False);

        while (XPending(stub.currentContext->damageDpy))
        {
            XEvent event;
            XNextEvent(stub.currentContext->damageDpy, &event);
            if (event.type==stub.currentContext->damageEventsBase+XDamageNotify)
            {
                crHashtableWalk(stub.currentContext->pGLXPixmapsHash, stubCheckXDamageCB, &event);
            }
        }
    }

    /* No shared memory? Rollback to use slow x protocol then */
    if (stub.xshmSI.shmid<0)
    {
        /*@todo add damage support here too*/
        XImage *pxim;

        pxim = XGetImage(dpy, (Pixmap)draw, pGlxPixmap->x, pGlxPixmap->y, pGlxPixmap->w, pGlxPixmap->h, AllPlanes, ZPixmap);
        /*if (pxim)
        {
            if (!ptextable)
            {
                ptextable = crAllocHashtable();
            }
            pm = crHashtableSearch(ptextable, (unsigned int) draw);
            if (!pm)
            {
                pm = crCalloc(sizeof(pminfo));
                crHashtableAdd(ptextable, (unsigned int) draw, pm);
            }
            pm->w = w;
            pm->h = h;
            if (pm->data) crFree(pm->data);
            pm->data = crAlloc(4*w*h);
            crMemcpy(pm->data, (void*)(&(pxim->data[0])), 4*w*h);
        }*/

        if (NULL==pxim)
        {
            crWarning("Failed, to get pixmap data for 0x%x", (unsigned int) draw);
            return;
        }

        stub.spu->dispatch_table.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pxim->width, pxim->height, 0, 
                                            GL_BGRA, GL_UNSIGNED_BYTE, (void*)(&(pxim->data[0])));
        XDestroyImage(pxim);
    }
    else /* Use shm to get pixmap data */
    {
        /* Check if we have damage extension */
        if (stub.currentContext->damageDpy)
        {
            if (pGlxPixmap->bPixmapImageDirty)
            {
                /* Either we failed to allocate damage region or this pixmap is marked for full update */
                if (!pGlxPixmap->pDamageRegion || XEmptyRegion(pGlxPixmap->pDamageRegion))
                {
                    /*crDebug("**FULL** update for 0x%x", (unsigned int)draw);*/
                    stubXshmUpdateWholeImage(dpy, draw, pGlxPixmap);
                }
                else
                {
                    long fullArea, damageArea=0, clipdamageArea, i;
                    XRectangle damageClipBox;

                    fullArea = pGlxPixmap->w * pGlxPixmap->h;
                    XClipBox(pGlxPixmap->pDamageRegion, &damageClipBox);
                    clipdamageArea = damageClipBox.width * damageClipBox.height;

                    //crDebug("FullSize [%i,%i,%i,%i]", pGlxPixmap->x, pGlxPixmap->y, pGlxPixmap->w, pGlxPixmap->h);
                    //crDebug("Clip [%i,%i,%i,%i]", damageClipBox.x, damageClipBox.y, damageClipBox.width, damageClipBox.height);

                    for (i=0; i<pGlxPixmap->pDamageRegion->numRects; ++i)
                    {
                        BoxPtr pBox = &pGlxPixmap->pDamageRegion->rects[i];
                        damageArea += (pBox->x2-pBox->x1)*(pBox->y2-pBox->y1);
                        //crDebug("Damage rect [%i,%i,%i,%i]", pBox->x1, pBox->y1, pBox->x2, pBox->y2);
                    }

                    if (damageArea>clipdamageArea || clipdamageArea>fullArea)
                    {
                        crWarning("glXBindTexImageEXT, damage regions seems to be broken, forcing full update");
                        /*crDebug("**FULL** update for 0x%x, numRect=%li, *FS*=%li, CS=%li, DS=%li", 
                                (unsigned int)draw, pGlxPixmap->pDamageRegion->numRects, fullArea, clipdamageArea, damageArea);*/
                        stubXshmUpdateWholeImage(dpy, draw, pGlxPixmap);
                    }
                    else /*We have corect damage info*/
                    {
                        if (CR_MIN_DAMAGE_PROFIT_SIZE > (fullArea-damageArea))
                        {
                            /*crDebug("**FULL** update for 0x%x, numRect=%li, *FS*=%li, CS=%li, DS=%li", 
                                    (unsigned int)draw, pGlxPixmap->pDamageRegion->numRects, fullArea, clipdamageArea, damageArea);*/
                            stubXshmUpdateWholeImage(dpy, draw, pGlxPixmap);
                        }
                        else if (CR_MIN_DAMAGE_PROFIT_SIZE > (clipdamageArea-damageArea))
                        {
                            /*crDebug("**PARTIAL** update for 0x%x, numRect=%li, FS=%li, *CS*=%li, DS=%li", 
                                    (unsigned int)draw, pGlxPixmap->pDamageRegion->numRects, fullArea, clipdamageArea, damageArea);*/
                            stubXshmUpdateImageRect(dpy, draw, pGlxPixmap, &damageClipBox);
                        }
                        else
                        {
                            /*crDebug("**PARTIAL** update for 0x%x, numRect=*%li*, FS=%li, CS=%li, *DS*=%li", 
                                    (unsigned int)draw, pGlxPixmap->pDamageRegion->numRects, fullArea, clipdamageArea, damageArea);*/
                            for (i=0; i<pGlxPixmap->pDamageRegion->numRects; ++i)
                            {
                                XRectangle rect;
                                BoxPtr pBox = &pGlxPixmap->pDamageRegion->rects[i];
                                
                                rect.x = pBox->x1;
                                rect.y = pBox->y1;
                                rect.width = pBox->x2-pBox->x1;
                                rect.height = pBox->y2-pBox->y1;

                                stubXshmUpdateImageRect(dpy, draw, pGlxPixmap, &rect);
                            }
                        }
                    }
                }

                /* Clean dirty flag and damage region */
                pGlxPixmap->bPixmapImageDirty = False;
                if (pGlxPixmap->pDamageRegion)
                    EMPTY_REGION(pGlxPixmap->pDamageRegion);
            }
        }
        else
        {
            stubXshmUpdateWholeImage(dpy, draw, pGlxPixmap);
        }
    }
}

DECLEXPORT(void) VBOXGLXTAG(glXReleaseTexImageEXT)(Display *dpy, GLXDrawable draw, int buffer)
{
    (void) dpy;
    (void) draw;
    (void) buffer;
    //crDebug("glXReleaseTexImageEXT 0x%x", (unsigned int)draw);
}
#endif

#endif /* GLX_EXTRAS */


#ifdef GLX_SGIX_video_resize
/* more dummy funcs.  These help when linking with older GLUTs */

DECLEXPORT(int) VBOXGLXTAG(glXBindChannelToWindowSGIX)(Display *dpy, int scrn, int chan, Window w)
{
    (void) dpy;
    (void) scrn;
    (void) chan;
    (void) w;
    crDebug("glXBindChannelToWindowSGIX");
    return 0;
}

DECLEXPORT(int) VBOXGLXTAG(glXChannelRectSGIX)(Display *dpy, int scrn, int chan, int x , int y, int w, int h)
{
    (void) dpy;
    (void) scrn;
    (void) chan;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    crDebug("glXChannelRectSGIX");
    return 0;
}

DECLEXPORT(int) VBOXGLXTAG(glXQueryChannelRectSGIX)(Display *dpy, int scrn, int chan, int *x, int *y, int *w, int *h)
{
    (void) dpy;
    (void) scrn;
    (void) chan;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    crDebug("glXQueryChannelRectSGIX");
    return 0;
}

DECLEXPORT(int) VBOXGLXTAG(glXQueryChannelDeltasSGIX)(Display *dpy, int scrn, int chan, int *dx, int *dy, int *dw, int *dh)
{
    (void) dpy;
    (void) scrn;
    (void) chan;
    (void) dx;
    (void) dy;
    (void) dw;
    (void) dh;
    crDebug("glXQueryChannelDeltasSGIX");
    return 0;
}

DECLEXPORT(int) VBOXGLXTAG(glXChannelRectSyncSGIX)(Display *dpy, int scrn, int chan, GLenum synctype)
{
    (void) dpy;
    (void) scrn;
    (void) chan;
    (void) synctype;
    crDebug("glXChannelRectSyncSGIX");
    return 0;
}

#endif /* GLX_SGIX_video_resize */

#ifdef VBOXOGL_FAKEDRI
DECLEXPORT(const char *) VBOXGLXTAG(glXGetDriverConfig)(const char *driverName)
{
    return NULL;
}

DECLEXPORT(void) VBOXGLXTAG(glXFreeMemoryMESA)(Display *dpy, int scrn, void *pointer)
{
    (void) dpy;
    (void) scrn;
    (void) pointer;
}

DECLEXPORT(GLXContext) VBOXGLXTAG(glXImportContextEXT)(Display *dpy, GLXContextID contextID)
{
    (void) dpy;
    (void) contextID;
    return NULL;
}

DECLEXPORT(GLXContextID) VBOXGLXTAG(glXGetContextIDEXT)(const GLXContext ctx)
{
    (void) ctx;
    return 0;
}

DECLEXPORT(Bool) VBOXGLXTAG(glXMakeCurrentReadSGI)(Display *display, GLXDrawable draw, GLXDrawable read, GLXContext ctx)
{
    return VBOXGLXTAG(glXMakeContextCurrent)(display, draw, read, ctx);
}

DECLEXPORT(const char *) VBOXGLXTAG(glXGetScreenDriver)(Display *dpy, int scrNum)
{
    static char *screendriver = "vboxvideo";
    return screendriver;
}

DECLEXPORT(Display *) VBOXGLXTAG(glXGetCurrentDisplayEXT)(void)
{
    return VBOXGLXTAG(glXGetCurrentDisplay());
}

DECLEXPORT(void) VBOXGLXTAG(glXFreeContextEXT)(Display *dpy, GLXContext ctx)
{
    VBOXGLXTAG(glXDestroyContext(dpy, ctx));
}

/*Mesa internal*/
DECLEXPORT(int) VBOXGLXTAG(glXQueryContextInfoEXT)(Display *dpy, GLXContext ctx)
{
    (void) dpy;
    (void) ctx;
    return 0;
}

DECLEXPORT(void *) VBOXGLXTAG(glXAllocateMemoryMESA)(Display *dpy, int scrn,
                                                     size_t size, float readFreq,
                                                     float writeFreq, float priority)
{
    (void) dpy;
    (void) scrn;
    (void) size;
    (void) readFreq;
    (void) writeFreq;
    (void) priority;
    return NULL;
}

DECLEXPORT(GLuint) VBOXGLXTAG(glXGetMemoryOffsetMESA)(Display *dpy, int scrn, const void *pointer)
{
    (void) dpy;
    (void) scrn;
    (void) pointer;
    return 0;
}

DECLEXPORT(GLXPixmap) VBOXGLXTAG(glXCreateGLXPixmapMESA)(Display *dpy, XVisualInfo *visual, Pixmap pixmap, Colormap cmap)
{
    (void) dpy;
    (void) visual;
    (void) pixmap;
    (void) cmap;
    return 0;
}

#endif /*VBOXOGL_FAKEDRI*/
