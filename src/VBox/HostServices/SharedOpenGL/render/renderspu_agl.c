/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include <Carbon/Carbon.h>
#include <AGL/agl.h>
#include <OpenGL/OpenGL.h>

#include <iprt/time.h>
#include <iprt/assert.h>

#include <stdio.h>

#include "cr_environment.h"
#include "cr_error.h"
#include "cr_string.h"
#include "cr_mem.h"
#include "renderspu.h"

/* Some necessary global defines */
WindowGroupRef gParentGroup = NULL;
WindowGroupRef gMasterGroup = NULL;
GLint gCurrentBufferName = 1;
uint64_t gDockUpdateTS = 0;
static EventHandlerUPP gParentEventHandler = NULL;

enum
{
    /* Event classes */
    kEventClassVBox         = 'vbox',
    /* Event kinds */
    kEventVBoxShowWindow    = 'swin',
    kEventVBoxHideWindow    = 'hwin',
    kEventVBoxMoveWindow    = 'mwin',
    kEventVBoxResizeWindow  = 'rwin',
    kEventVBoxDisposeWindow = 'dwin',
    kEventVBoxUpdateDock    = 'udck',
    kEventVBoxUpdateContext = 'uctx',
    kEventVBoxBoundsChanged = 'bchg'
};

#ifdef __LP64__ /** @todo port to 64-bit darwin. */
# define renderspuSetWindowContext(w, c) \
    AssertFailed()
# define renderspuGetWindowContext(w) \
    ( (ContextInfo *) GetWRefCon( ((w)->nativeWindow ? (w)->nativeWindow : (w)->window) ) )
#else
# define renderspuSetWindowContext(w, c) \
    ( SetWRefCon( (w), (unsigned long) (c) ) )
# define renderspuGetWindowContext(w) \
    ( (ContextInfo *) GetWRefCon( ((w)->nativeWindow ? (w)->nativeWindow : (w)->window) ) )
#endif

/* Debug macros */
#ifdef DEBUG_poetzsch
#define DEBUG_MSG_POETZSCH(text) \
    printf text
#else
#define DEBUG_MSG_POETZSCH(text) \
    do {} while (0)
#endif

#define DEBUG_MSG_RESULT(result, text) \
        crDebug(text" (%d; %s:%d)", (int)(result), __FILE__, __LINE__)

#define CHECK_CARBON_RC(result, text) \
    if((result) != noErr) \
        DEBUG_MSG_RESULT(result, text);

#define CHECK_CARBON_RC_RETURN(result, text, ret) \
    if((result) != noErr) \
    { \
        DEBUG_MSG_RESULT(result, text); \
        return ret; \
    }

#define CHECK_CARBON_RC_RETURN_VOID(result, text) \
    CHECK_CARBON_RC_RETURN(result, text,)

#define CHECK_AGL_RC(result, text) \
    if(!(result)) \
    { \
        GLenum error = render_spu.ws.aglGetError(); \
        DEBUG_MSG_RESULT(result, text); \
    }

/* In some case (like compiz which doesn't provide us with clipping regions) we
 * have to make sure that *all* open OpenGL windows are clipped to the main
 * application window. This is done here when called from the event handler
 * which monitor bounding changes of the main window. */
static void crClipRootHelper(unsigned long key, void *data1, void *data2)
{
    /* The window with id zero is the base window, which isn't displayed at
     * all. So ignore it. */
    if (key > 0)
    {
        /* Fetch the actually window info & the user data */
        WindowInfo *pWin = (WindowInfo *) data1;
        /* We need to assign the context with this window */
        ContextInfo *context = renderspuGetWindowContext(pWin);
        if (context &&
            context->context)
        {
            OSStatus result = render_spu.ws.aglSetCurrentContext(context->context);
            CHECK_AGL_RC (result, "Render SPU (renderspuWindowAttachContext): SetCurrentContext Failed");
            result = render_spu.ws.aglUpdateContext(context->context);
            CHECK_AGL_RC (result, "Render SPU (renderspuWindowAttachContext): UpdateContext Failed");
            /* Update the clipping region */
            renderspu_SystemWindowApplyVisibleRegion(pWin);
            /* Make sure that the position is updated relative to the Qt main
             * view */
            renderspu_SystemWindowPosition(pWin, pWin->x, pWin->y);
        }
    }
}

/* Window event handler */
static pascal OSStatus
windowEvtHndlr(EventHandlerCallRef myHandler, EventRef event, void* userData)
{
    WindowRef   window = NULL;
    OSStatus    result = eventNotHandledErr;
    UInt32      class = GetEventClass (event);
    UInt32      kind = GetEventKind (event);

    GetEventParameter(event, kEventParamDirectObject, typeWindowRef,
                      NULL, sizeof(WindowRef), NULL, &window);
    switch (class)
    {
        case kEventClassVBox:
        {
            switch (kind)
            {
                case kEventVBoxUpdateContext:
                {
#ifndef __LP64__ /** @todo port to 64-bit darwin! Need to cehck if this event is generated or not (it probably isn't). */
                    WindowInfo *wi1;
                    GetEventParameter(event, kEventParamUserData, typeVoidPtr,
                                      NULL, sizeof(wi1), NULL, &wi1);
                    ContextInfo *context = renderspuGetWindowContext(wi1);
                    GLboolean result1 = true;
                    if (context &&
                        context->context)
                    {
                        DEBUG_MSG_POETZSCH (("kEventVBoxUpdateContext %x %x\n", window, context->context));
                        result1 = render_spu.ws.aglSetCurrentContext(context->context);
                        result1 = render_spu.ws.aglUpdateContext(context->context);
                        CHECK_AGL_RC (result, "Render SPU (windowEvtHndlr): UpdateContext Failed");
                        //glFlush();
                    }
                    result = noErr;
#endif
                    break;
                }
                case kEventVBoxBoundsChanged:
                {
#ifndef __LP64__ /** @todo port to 64-bit darwin! Need to cehck if this event is generated or not (it probably isn't). */
                    HIPoint p;
                    GetEventParameter(event, kEventParamOrigin, typeHIPoint,
                                      NULL, sizeof(p), NULL, &p);
                    HISize s;
                    GetEventParameter(event, kEventParamDimensions, typeHISize,
                                      NULL, sizeof(s), NULL, &s);
                    HIRect r = CGRectMake (0, 0, s.width, s.height);
                    DEBUG_MSG_POETZSCH (("kEventVBoxBoundsChanged %f %f %f %f\n", p.x, p.y, s.width, s.height));
                    GLint l[4] = { 0,
                                   0,
                                   r.size.width,
                                   r.size.height };
                    renderspu_SystemSetRootVisibleRegion(1, &l);
                    crHashtableWalk(render_spu.windowTable, crClipRootHelper, NULL);
#endif
                    break;
                }
            };
            break;
        }
        break;
    };

    return result;
}

GLboolean
renderspu_SystemInitVisual(VisualInfo *visual)
{
    if(visual->visAttribs & CR_PBUFFER_BIT)
        crWarning("Render SPU (renderspu_SystemInitVisual): PBuffers not support on Darwin/AGL yet.");

    return GL_TRUE;
}

GLboolean
renderspuChoosePixelFormat(ContextInfo *context, AGLPixelFormat *pix)
{
    GLbitfield  visAttribs = context->visual->visAttribs;
    GLint       attribs[32];
    GLint       ind = 0;

#define ATTR_ADD(s)     ( attribs[ind++] = (s) )
#define ATTR_ADDV(s,v)  ( ATTR_ADD((s)), ATTR_ADD((v)) )

    CRASSERT(render_spu.ws.aglChoosePixelFormat);

    ATTR_ADD(AGL_RGBA);
/*  ATTR_ADDV(AGL_RED_SIZE, 1);
    ATTR_ADDV(AGL_GREEN_SIZE, 1);
    ATTR_ADDV(AGL_BLUE_SIZE, 1); */

/*  if( render_spu.fullscreen )*/
/*      ATTR_ADD(AGL_FULLSCREEN);*/

    if( visAttribs & CR_ALPHA_BIT )
        ATTR_ADDV(AGL_ALPHA_SIZE, 1);

    if( visAttribs & CR_DOUBLE_BIT )
        ATTR_ADD(AGL_DOUBLEBUFFER);

    if( visAttribs & CR_STEREO_BIT )
        ATTR_ADD(AGL_STEREO);

    if( visAttribs & CR_DEPTH_BIT )
        ATTR_ADDV(AGL_DEPTH_SIZE, 1);

    if( visAttribs & CR_STENCIL_BIT )
        ATTR_ADDV(AGL_STENCIL_SIZE, 1);

    if( visAttribs & CR_ACCUM_BIT ) {
        ATTR_ADDV(AGL_ACCUM_RED_SIZE, 1);
        ATTR_ADDV(AGL_ACCUM_GREEN_SIZE, 1);
        ATTR_ADDV(AGL_ACCUM_BLUE_SIZE, 1);
        if( visAttribs & CR_ALPHA_BIT )
            ATTR_ADDV(AGL_ACCUM_ALPHA_SIZE, 1);
    }

    if( visAttribs & CR_MULTISAMPLE_BIT ) {
        ATTR_ADDV(AGL_SAMPLE_BUFFERS_ARB, 1);
        ATTR_ADDV(AGL_SAMPLES_ARB, 4);
    }

    if( visAttribs & CR_OVERLAY_BIT )
        ATTR_ADDV(AGL_LEVEL, 1);

    ATTR_ADD(AGL_NONE);

    *pix = render_spu.ws.aglChoosePixelFormat( NULL, 0, attribs );

    return (*pix != NULL);
}

void
renderspuDestroyPixelFormat(ContextInfo *context, AGLPixelFormat *pix)
{
    render_spu.ws.aglDestroyPixelFormat( *pix );
    *pix = NULL;
}

GLboolean
renderspu_SystemCreateContext(VisualInfo *visual, ContextInfo *context, ContextInfo *sharedContext)
{
    AGLPixelFormat pix;

    (void) sharedContext;
    CRASSERT(visual);
    CRASSERT(context);

    context->visual = visual;

    if( !renderspuChoosePixelFormat(context, &pix) ) {
        crError( "Render SPU (renderspu_SystemCreateContext): Unable to create pixel format" );
        return GL_FALSE;
    }

    context->context = render_spu.ws.aglCreateContext( pix, NULL );
    renderspuDestroyPixelFormat( context, &pix );

    if( !context->context ) {
        crError( "Render SPU (renderspu_SystemCreateContext): Could not create rendering context" );
        return GL_FALSE;
    }

    return GL_TRUE;
}

void
renderspu_SystemDestroyContext(ContextInfo *context)
{
    if(!context)
        return;

    render_spu.ws.aglSetDrawable(context->context, NULL);
    render_spu.ws.aglSetCurrentContext(NULL);
    if(context->context)
    {
        render_spu.ws.aglDestroyContext(context->context);
        context->context = NULL;
    }

    context->visual = NULL;
}

void
renderspuFullscreen(WindowInfo *window, GLboolean fullscreen)
{
    /* Real fullscreen isn't supported by VirtualBox */
}

GLboolean
renderspuWindowAttachContext(WindowInfo *wi, WindowRef window,
                             ContextInfo *context)
{
    GLboolean result;

    if(!context || !wi)
        return render_spu.ws.aglSetCurrentContext( NULL );

/*    DEBUG_MSG_POETZSCH (("WindowAttachContext %d\n", wi->id));*/

    /* Flush old context first */
    if (context->currentWindow->window != window)
        render_spu.self.Flush();
    /* If the window buffer name is uninitialized we have to create a new
     * dummy context. */
    if (wi->bufferName == -1)
    {
        DEBUG_MSG_POETZSCH (("WindowAttachContext: create context %d\n", wi->id));
        /* Use the same visual bits as those in the context structure */
        AGLPixelFormat pix;
        if( !renderspuChoosePixelFormat(context, &pix) )
        {
            crError( "Render SPU (renderspuWindowAttachContext): Unable to create pixel format" );
            return GL_FALSE;
        }
        /* Create the dummy context */
        wi->dummyContext = render_spu.ws.aglCreateContext( pix, NULL );
        renderspuDestroyPixelFormat( context, &pix );
        if( !wi->dummyContext )
        {
            crError( "Render SPU (renderspuWindowAttachContext): Could not create rendering context" );
            return GL_FALSE;
        }
        AGLDrawable drawable;
#ifdef __LP64__ /** @todo port to 64-bit darwin. */
        drawable = NULL;
#else
        drawable = (AGLDrawable) GetWindowPort(window);
#endif
        /* New global buffer name */
        wi->bufferName = gCurrentBufferName++;
        /* Set the new buffer name to the dummy context. This enable the
         * sharing of the same hardware buffer afterwards. */
        result = render_spu.ws.aglSetInteger(wi->dummyContext, AGL_BUFFER_NAME, &wi->bufferName);
        CHECK_AGL_RC (result, "Render SPU (renderspuWindowAttachContext): SetInteger Failed");
        /* Assign the dummy context to the window */
        result = render_spu.ws.aglSetDrawable(wi->dummyContext, drawable);
        CHECK_AGL_RC (result, "Render SPU (renderspuWindowAttachContext): SetDrawable Failed");
    }

    AGLDrawable oldDrawable;
    AGLDrawable newDrawable;

    oldDrawable = render_spu.ws.aglGetDrawable(context->context);
#ifdef __LP64__ /** @todo port to 64-bit darwin. */
    newDrawable = oldDrawable;
#else
    newDrawable = (AGLDrawable) GetWindowPort(window);
#endif
    /* Only switch the context if the drawable has changed */
    if (oldDrawable != newDrawable)
    {
        /* Reset the current context */
        result = render_spu.ws.aglSetDrawable(context->context, NULL);
        CHECK_AGL_RC (result, "Render SPU (renderspuWindowAttachContext): SetDrawable Failed");
        /* Set the buffer name of the dummy context to the current context
         * also. After that both share the same hardware buffer. */
        render_spu.ws.aglSetInteger (context->context, AGL_BUFFER_NAME, &wi->bufferName);
        CHECK_AGL_RC (result, "Render SPU (renderspuWindowAttachContext): SetInteger Failed");
        /* Set the new drawable */
#ifdef __LP64__ /** @todo port to 64-bit darwin. */
        result = -1;
#else
        result = render_spu.ws.aglSetDrawable(context->context, newDrawable);
#endif
        CHECK_AGL_RC (result, "Render SPU (renderspuWindowAttachContext): SetDrawable Failed");
        renderspuSetWindowContext( window, context );
    }
    result = render_spu.ws.aglSetCurrentContext(context->context);
    CHECK_AGL_RC (result, "Render SPU (renderspuWindowAttachContext): SetCurrentContext Failed");
    result = render_spu.ws.aglUpdateContext(context->context);
    CHECK_AGL_RC (result, "Render SPU (renderspuWindowAttachContext): UpdateContext Failed");

    return result;
}

GLboolean
renderspu_SystemCreateWindow(VisualInfo *visual, GLboolean showIt,
                             WindowInfo *window)
{
    return GL_TRUE;
}

void
renderspu_SystemDestroyWindow(WindowInfo *window)
{
    CRASSERT(window);
    CRASSERT(window->visual);

    if(!window->nativeWindow)
    {
        EventRef evt;
        OSStatus status = CreateEvent(NULL, kEventClassVBox, kEventVBoxDisposeWindow, 0, kEventAttributeNone, &evt);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemDestroyWindow): CreateEvent Failed");
        status = SetEventParameter(evt, kEventParamWindowRef, typeWindowRef, sizeof (window->window), &window->window);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemDestroyWindow): SetEventParameter Failed");
        status = PostEventToQueue(GetMainEventQueue(), evt, kEventPriorityStandard);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemDestroyWindow): PostEventToQueue Failed");
    }

    /* Delete the dummy context */
    if(window->dummyContext)
    {
        render_spu.ws.aglSetDrawable(window->dummyContext, NULL);
        render_spu.ws.aglDestroyContext(window->dummyContext);
        window->dummyContext = NULL;
    }

    /* Reset some values */
    window->bufferName = -1;
    window->visual = NULL;
    window->window = NULL;

    if (window->hVisibleRegion)
    {
        DisposeRgn(window->hVisibleRegion);
        window->hVisibleRegion = 0;
    }
}

void
renderspu_SystemWindowSize(WindowInfo *window, GLint w, GLint h)
{
    CRASSERT(window);
    CRASSERT(window->window);

    OSStatus status = noErr;
    /* Send a event to the main thread, cause some function of Carbon aren't
     * thread safe */
    EventRef evt;
    status = CreateEvent(NULL, kEventClassVBox, kEventVBoxResizeWindow, 0, kEventAttributeNone, &evt);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowSize): CreateEvent Failed ");
    status = SetEventParameter(evt, kEventParamWindowRef, typeWindowRef, sizeof(window->window), &window->window);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowSize): SetEventParameter Failed");
    HISize s = CGSizeMake (w, h);
    status = SetEventParameter(evt, kEventParamDimensions, typeHISize, sizeof (s), &s);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowSize): SetEventParameter Failed");
    status = SetEventParameter(evt, kEventParamUserData, typeVoidPtr, sizeof (window), &window);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowSize): SetEventParameter Failed");
    status = PostEventToQueue(GetMainEventQueue(), evt, kEventPriorityStandard);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowSize): SendEventToEventTarget Failed");

    DEBUG_MSG_POETZSCH (("Size %d visible %d\n", window->id, IsWindowVisible (window->window)));
    /* save the new size */
    window->width = w;
    window->height = h;
}

void
renderspu_SystemGetWindowGeometry(WindowInfo *window,
                                  GLint *x, GLint *y,
                                  GLint *w, GLint *h)
{
    CRASSERT(window);
    CRASSERT(window->window);

    OSStatus status = noErr;
    Rect r;
    status = GetWindowBounds(window->window, kWindowStructureRgn, &r);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemGetWindowGeometry): GetWindowBounds Failed");

    *x = (int) r.left;
    *y = (int) r.top;
    *w = (int) (r.right - r.left);
    *h = (int) (r.bottom - r.top);
}

void
renderspu_SystemGetMaxWindowSize(WindowInfo *window,
                                 GLint *w, GLint *h)
{
    CRASSERT(window);
    CRASSERT(window->window);

    OSStatus status = noErr;
    HISize s;
#ifdef __LP64__ /** @todo port to 64-bit darwin. */
    status = -1;
#else
    status = GetWindowResizeLimits (window->window, NULL, &s);
#endif
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemGetMaxWindowSize): GetWindowResizeLimits Failed");

    *w = s.width;
    *h = s.height;
}

void
renderspu_SystemWindowPosition(WindowInfo *window,
                               GLint x, GLint y)
{
    CRASSERT(window);
    CRASSERT(window->window);

    OSStatus status = noErr;
    /* Send a event to the main thread, cause some function of Carbon aren't
     * thread safe */
    EventRef evt;
    status = CreateEvent(NULL, kEventClassVBox, kEventVBoxMoveWindow, 0, kEventAttributeNone, &evt);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowPosition): CreateEvent Failed");
    status = SetEventParameter(evt, kEventParamWindowRef, typeWindowRef, sizeof(window->window), &window->window);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowPosition): SetEventParameter Failed");
    HIPoint p = CGPointMake (x, y);
    status = SetEventParameter(evt, kEventParamOrigin, typeHIPoint, sizeof (p), &p);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowPosition): SetEventParameter Failed");
    status = SetEventParameter(evt, kEventParamUserData, typeVoidPtr, sizeof (window), &window);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowPosition): SetEventParameter Failed");
    status = PostEventToQueue(GetMainEventQueue(), evt, kEventPriorityStandard);
    CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowPosition): PostEventToQueue Failed");

    /* save the new pos */
    window->x = x;
    window->y = y;
}

/* Either show or hide the render SPU's window. */
void
renderspu_SystemShowWindow(WindowInfo *window, GLboolean showIt)
{
    CRASSERT(window);
    CRASSERT(window->window);

    if (!IsValidWindowPtr(window->window))
        return;

    if(showIt)
    {
        /* Force moving the win to the right position before we show it */
        renderspu_SystemWindowPosition (window, window->x, window->y);
        OSStatus status = noErr;
        /* Send a event to the main thread, cause some function of Carbon
         * aren't thread safe */
        EventRef evt;
        status = CreateEvent(NULL, kEventClassVBox, kEventVBoxShowWindow, 0, kEventAttributeNone, &evt);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemShowWindow): CreateEvent Failed");
        status = SetEventParameter(evt, kEventParamWindowRef, typeWindowRef, sizeof (window->window), &window->window);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemShowWindow): SetEventParameter Failed");
        status = SetEventParameter(evt, kEventParamUserData, typeVoidPtr, sizeof (window), &window);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemWindowShow): SetEventParameter Failed");
        //status = SendEventToEventTarget (evt, GetWindowEventTarget (HIViewGetWindow ((HIViewRef)render_spu_parent_window_id)));
        status = PostEventToQueue(GetMainEventQueue(), evt, kEventPriorityStandard);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemShowWindow): PostEventToQueue Failed");
    }
    else
    {
        EventRef evt;
        OSStatus status = CreateEvent(NULL, kEventClassVBox, kEventVBoxHideWindow, 0, kEventAttributeNone, &evt);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemShowWindow): CreateEvent Failed");
        status = SetEventParameter(evt, kEventParamWindowRef, typeWindowRef, sizeof (window->window), &window->window);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemShowWindow): SetEventParameter Failed");
        status = PostEventToQueue(GetMainEventQueue(), evt, kEventPriorityStandard);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemShowWindow): PostEventToQueue Failed");
    }

    window->visible = showIt;
}

void
renderspu_SystemMakeCurrent(WindowInfo *window, GLint nativeWindow,
                            ContextInfo *context)
{
    Boolean result;
/*    DEBUG_MSG_POETZSCH (("makecurrent %d: \n", window->id));*/

    CRASSERT(render_spu.ws.aglSetCurrentContext);
    //crDebug( "renderspu_SystemMakeCurrent( %x, %i, %x )", window, nativeWindow, context );

    if(window && context)
    {
        CRASSERT(window->window);
        CRASSERT(context->context);

        if(window->visual != context->visual)
        {
            crDebug("Render SPU (renderspu_SystemMakeCurrent): MakeCurrent visual mismatch (0x%x != 0x%x); remaking window.",
                    (uint)window->visual->visAttribs, (uint)context->visual->visAttribs);
            /*
             * XXX have to revisit this issue!!!
             *
             * But for now we destroy the current window
             * and re-create it with the context's visual abilities
             */
            renderspu_SystemDestroyWindow(window);
            renderspu_SystemCreateWindow(context->visual, window->visible,
                                         window);
        }

        /* This is the normal case: rendering to the render SPU's own window */
        result = renderspuWindowAttachContext(window, window->window,
                                              context);
        /* XXX this is a total hack to work around an NVIDIA driver bug */
        if(render_spu.self.GetFloatv && context->haveWindowPosARB)
        {
            GLfloat f[4];
            render_spu.self.GetFloatv(GL_CURRENT_RASTER_POSITION, f);
            if (!window->everCurrent || f[1] < 0.0)
            {
                crDebug("Render SPU (renderspu_SystemMakeCurrent): Resetting raster pos");
                render_spu.self.WindowPos2iARB(0, 0);
            }
        }
    }
    else
        renderspuWindowAttachContext (0, 0, 0);
}

void
renderspu_SystemSwapBuffers(WindowInfo *window, GLint flags)
{
    CRASSERT(window);
    CRASSERT(window->window);

    ContextInfo *context = renderspuGetWindowContext(window);

    if(!context)
        crError("Render SPU (renderspu_SystemSwapBuffers): SwapBuffers got a null context from the window");

//    DEBUG_MSG_POETZSCH (("Swapped %d context %x visible: %d\n", window->id, context->context, IsWindowVisible (window->window)));
    if (context->visual->visAttribs & CR_DOUBLE_BIT)
        render_spu.ws.aglSwapBuffers(context->context);
    else
        glFlush();

    /* This method seems called very often. To prevent the dock using all free
     * resources we update the dock only two times per second. */
    uint64_t curTS = RTTimeMilliTS();
    if ((curTS - gDockUpdateTS) > 500)
    {
        OSStatus status = noErr;
        /* Send a event to the main thread, cause some function of Carbon aren't
         * thread safe */
        EventRef evt;
        status = CreateEvent(NULL, kEventClassVBox, kEventVBoxUpdateDock, 0, kEventAttributeNone, &evt);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemSwapBuffers): CreateEvent Failed");
        status = PostEventToQueue(GetMainEventQueue(), evt, kEventPriorityStandard);
        CHECK_CARBON_RC_RETURN_VOID (status, "Render SPU (renderspu_SystemSwapBuffers): PostEventToQueue Failed");

        gDockUpdateTS = curTS;
    }
}

void renderspu_SystemWindowVisibleRegion(WindowInfo *window, GLint cRects, GLint* pRects)
{
    CRASSERT(window);
    CRASSERT(window->window);

    DEBUG_MSG_POETZSCH (("Visible region \n"));

    if (window->hVisibleRegion)
    {
        DisposeRgn(window->hVisibleRegion);
        window->hVisibleRegion = 0;
    }

    if (cRects>0)
    {
        int i;
        /* Create some temporary regions */
        RgnHandle rgn = NewRgn();
        SetEmptyRgn (rgn);
        RgnHandle tmpRgn = NewRgn();
        for (i=0; i<cRects; ++i)
        {
            SetRectRgn (tmpRgn,
                        pRects[4*i]  , pRects[4*i+1],
                        pRects[4*i+2], pRects[4*i+3]);
            DEBUG_MSG_POETZSCH (("visible rect %d %d %d %d\n", pRects[4*i]  , pRects[4*i+1],
                                 pRects[4*i+2], pRects[4*i+3]));
            UnionRgn (rgn, tmpRgn, rgn);
        }
        DisposeRgn (tmpRgn);
        window->hVisibleRegion = rgn;
    }

    renderspu_SystemWindowApplyVisibleRegion(window);
}

void renderspu_SystemSetRootVisibleRegion(GLint cRects, GLint *pRects)
{
    if (render_spu.hRootVisibleRegion)
    {
        DisposeRgn(render_spu.hRootVisibleRegion);
        render_spu.hRootVisibleRegion = 0;
    }

    if (cRects>0)
    {
        int i;
        render_spu.hRootVisibleRegion = NewRgn();
        SetEmptyRgn (render_spu.hRootVisibleRegion);
        RgnHandle tmpRgn = NewRgn();
        for (i=0; i<cRects; ++i)
        {
            SetRectRgn (tmpRgn,
                        pRects[4*i]  , pRects[4*i+1],
                        pRects[4*i+2], pRects[4*i+3]);
            UnionRgn (render_spu.hRootVisibleRegion, tmpRgn, render_spu.hRootVisibleRegion);
        }
        DisposeRgn (tmpRgn);
    }
}

/*Assumes that all regions are in the guest coordinates system*/
void renderspu_SystemWindowApplyVisibleRegion(WindowInfo *window)
{
    ContextInfo *c = renderspuGetWindowContext(window);
    RgnHandle rgn;
    GLboolean result = true;

    if (!c || !c->context) return;

    rgn = NewRgn();
    SetEmptyRgn (rgn);

    if (render_spu.hRootVisibleRegion)
    {
        CopyRgn(render_spu.hRootVisibleRegion, rgn);
    }
    else /*@todo create tmp region rect with size of underlying framebuffer */
    {
        /* SetRectRgn(0,0,fb->width,fb->height); */
        SetRectRgn(rgn,0,0,4000,4000);
    }

    if (window->hVisibleRegion)
    {
        SectRgn(rgn, window->hVisibleRegion, rgn);
    }

    /* If we'd need to set clip region in host screen coordinates, than shift it*/
    /* OffsetRgn(rgn, fb->hostleft, fb->hosttop); */

    /* Set the clip region to the context */
    result = render_spu.ws.aglSetInteger(c->context, AGL_CLIP_REGION, (const GLint*)rgn);
    CHECK_AGL_RC (result, "Render SPU (renderspu_SystemWindowVisibleRegion): SetInteger Failed");
    result = render_spu.ws.aglEnable(c->context, AGL_CLIP_REGION);
    CHECK_AGL_RC (result, "Render SPU (renderspu_SystemWindowVisibleRegion): Enable Failed");
    /* Clear the region structure */
    DisposeRgn (rgn);
}

GLboolean
renderspu_SystemVBoxCreateWindow(VisualInfo *visual, GLboolean showIt,
                                 WindowInfo *window)
{
    CRASSERT(visual);
    CRASSERT(window);

    WindowAttributes winAttr = kWindowNoShadowAttribute | kWindowCompositingAttribute | kWindowIgnoreClicksAttribute | kWindowStandardHandlerAttribute | kWindowLiveResizeAttribute;
    WindowClass winClass = kOverlayWindowClass;
    Rect windowRect;
    OSStatus status = noErr;

    window->visual = visual;
    window->nativeWindow = NULL;

    if(window->window && IsValidWindowPtr(window->window))
    {
        EventRef evt;
        status = CreateEvent(NULL, kEventClassVBox, kEventVBoxDisposeWindow, 0, kEventAttributeNone, &evt);
        CHECK_CARBON_RC_RETURN (status, "Render SPU (renderspu_SystemVBoxCreateWindow): CreateEvent Failed", false);
        status = SetEventParameter(evt, kEventParamWindowRef, typeWindowRef, sizeof (window->window), &window->window);
        CHECK_CARBON_RC_RETURN (status, "Render SPU (renderspu_SystemVBoxCreateWindow): SetEventParameter Failed", false);
        status = PostEventToQueue(GetMainEventQueue(), evt, kEventPriorityStandard);
        CHECK_CARBON_RC_RETURN (status, "Render SPU (renderspu_SystemVBoxCreateWindow): PostEventToQueue Failed", false);
    }

    windowRect.left = window->x;
    windowRect.top = window->y;
    windowRect.right = window->x + window->width;
    windowRect.bottom = window->y + window->height;

    status = CreateNewWindow(winClass, winAttr, &windowRect, &window->window);
    CHECK_CARBON_RC_RETURN (status, "Render SPU (renderspu_SystemVBoxCreateWindow): CreateNewWindow Failed", GL_FALSE);

    /* We set a title for debugging purposes */
    CFStringRef title_string;
    title_string = CFStringCreateWithCStringNoCopy(NULL, window->title,
                                                   kCFStringEncodingMacRoman, NULL);
    SetWindowTitleWithCFString(window->window, title_string);
    CFRelease(title_string);

    /* We need grouping so create a master group for this & all following
     * windows & one group for the parent. */
    if(!gMasterGroup || !gParentGroup)
    {
        status = CreateWindowGroup(kWindowGroupAttrMoveTogether | kWindowGroupAttrLayerTogether | kWindowGroupAttrSharedActivation | kWindowGroupAttrHideOnCollapse | kWindowGroupAttrFixedLevel, &gMasterGroup);
        CHECK_CARBON_RC_RETURN (status, "Render SPU (renderspu_SystemVBoxCreateWindow): CreateWindowGroup Failed", GL_FALSE);
        status = CreateWindowGroup(kWindowGroupAttrMoveTogether | kWindowGroupAttrLayerTogether | kWindowGroupAttrSharedActivation | kWindowGroupAttrHideOnCollapse | kWindowGroupAttrFixedLevel, &gParentGroup);
        CHECK_CARBON_RC_RETURN (status, "Render SPU (renderspu_SystemVBoxCreateWindow): CreateWindowGroup Failed", GL_FALSE);
        /* Make the correct z-layering */
        SendWindowGroupBehind (gParentGroup, gMasterGroup);
        /* and set the gParentGroup as parent for gMasterGroup. */
#ifdef __LP64__ /** @todo port to 64-bit darwin. */
#else
        SetWindowGroupParent (gMasterGroup, gParentGroup);
#endif
    }

    /* The parent has to be in its own group */
    WindowRef parent = NULL;
    if (render_spu_parent_window_id)
    {
        parent = HIViewGetWindow ((HIViewRef)render_spu_parent_window_id);
        SetWindowGroup (parent, gParentGroup);

        /* We need to process events from our main window */
        if(!gParentEventHandler)
        {
            /* Install the event handlers */
            EventTypeSpec eventList[] =
            {
                {kEventClassVBox, kEventVBoxUpdateContext}, /* Update the context after show/size/move events */
                {kEventClassVBox, kEventVBoxBoundsChanged}  /* Clip/Pos the OpenGL windows when the main window is changed in pos/size */
            };

            gParentEventHandler = NewEventHandlerUPP(windowEvtHndlr);
            InstallApplicationEventHandler (gParentEventHandler,
                                            GetEventTypeCount(eventList), eventList,
                                            NULL, NULL);
        }
    }
    /* Add the new window to the master group */
    SetWindowGroup(window->window, gMasterGroup);

    /* This will be initialized on the first attempt to attach the global
     * context to this new window */
    window->bufferName = -1;
    window->dummyContext = NULL;

    if(showIt)
        renderspu_SystemShowWindow(window, GL_TRUE);

    crDebug("Render SPU (renderspu_SystemVBoxCreateWindow): actual window (x, y, width, height): %d, %d, %d, %d",
            window->x, window->y, window->width, window->height);

    return GL_TRUE;
}

