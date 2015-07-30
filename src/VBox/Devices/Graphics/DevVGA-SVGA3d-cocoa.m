/* $Id$ */
/** @file
 * VirtualBox OpenGL Cocoa Window System Helper Implementation.
 *
 * @remarks Inspired by HostServices/SharedOpenGL/render/renderspu_cocoa_helper.m.
 */

/*
 * Copyright (C) 2009-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VMSVGA
#include "DevVGA-SVGA3d-cocoa.h"
#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>

#include <iprt/thread.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <VBox/log.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def USE_NSOPENGLVIEW
 * Define this to experiment with using NSOpenGLView instead
 * of NSView.  There are transparency issues with the former,
 * so for the time being we're using the latter.  */
#if 0
#define USE_NSOPENGLVIEW
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Argument package for doing this on the main thread.
 */
@interface VMSVGA3DCreateViewAndContext : NSObject
{
@public
    /* in */
    NativeNSViewRef             pParentView;
    uint32_t                    cx;
    uint32_t                    cy;
    NativeNSOpenGLContextRef    pSharedCtx;
    bool                        fOtherProfile;

    /* out */
    NativeNSViewRef             pView;
    NativeNSOpenGLContextRef    pCtx;
}
@end


/**
 * The overlay view.
 */
@interface VMSVGA3DOverlayView
#ifdef USE_NSOPENGLVIEW
    : NSOpenGLView
#else
    : NSView
#endif
{
@private
    /** This points to the parent view, if there is one.  If there isn't a parent
     * the view will be hidden and never used for displaying stuff.  We only have 
     * one visible context per guest screen that is visible to the user and 
     * subject to buffer swapping. */ 
    NSView         *m_pParentView;
    /** Indicates that buffers (back+front) needs clearing before use because
     * the view changed size.  There are two buffers, so this is set to two 
     * each time when the view area increases. */ 
    uint32_t        m_cClears;
    /** Set if the OpenGL context needs updating after a resize. */
    bool            m_fUpdateCtx;

#ifndef USE_NSOPENGLVIEW
    /** The OpenGL context associated with this view. */
    NSOpenGLContext *m_pCtx;
#endif

    /** The desired view position relative to super. */
    NSPoint         m_Pos;
    /** The desired view size. */
    NSSize          m_Size;
}
+ (void)createViewAndContext:(VMSVGA3DCreateViewAndContext *)pParams;
- (id)initWithFrameAndFormat:(NSRect)frame parentView:(NSView*)pparentView pixelFormat:(NSOpenGLPixelFormat *)pFmt;
- (void)vboxSetPos:(NSPoint)pos;
- (void)vboxSetSize:(NSSize)size;
- (void)vboxReshapePerform;
- (void)vboxReshape;
#if 0 // doesn't work or isn't needed :/
- (void)vboxFrameDidChange;
- (BOOL)postsFrameChangedNotifications;
#endif
- (void)vboxRemoveFromSuperviewAndHide;
- (void)vboxClearBuffers;
- (void)vboxUpdateCtxIfNecessary;
- (void)vboxClearBackBufferIfNecessary;
- (NSOpenGLContext *)makeCurrentGLContext;
- (void)restoreSavedGLContext:(NSOpenGLContext *)pSavedCtx;

#ifndef USE_NSOPENGLVIEW
/* NSOpenGLView fakes: */
- (void)setOpenGLContext:(NSOpenGLContext *)pCtx;
- (NSOpenGLContext *)openGLContext;
- (void)prepareOpenGL;

#endif
/* Overridden: */
- (void)viewDidMoveToWindow;
- (void)viewDidMoveToSuperview;
- (void)resizeWithOldSuperviewSize:(NSSize)oldBoundsSize;
- (void)drawRect:(NSRect)rect;

@end


/********************************************************************************
*
* VMSVGA3DOverlayView class implementation
*
********************************************************************************/
@implementation VMSVGA3DOverlayView


+ (void)createViewAndContext:(VMSVGA3DCreateViewAndContext *)pParams
{
    LogFlow(("OvlView createViewAndContext:\n"));

    /*
     * Create a pixel format.
     */
    NSOpenGLPixelFormat *pFmt = nil;

    // Consider to remove it and check if it's harmless.
    NSOpenGLPixelFormatAttribute attribs[] =
    {
        NSOpenGLPFAOpenGLProfile, (NSOpenGLPixelFormatAttribute)0,
        //NSOpenGLPFAWindow, - obsolete/deprecated, try work without it...
        NSOpenGLPFAAccelerated,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFABackingStore,
        NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24,
        NSOpenGLPFAAlphaSize, (NSOpenGLPixelFormatAttribute)8,
        NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)24,
        0
    };
    attribs[1] = pParams->fOtherProfile ? NSOpenGLProfileVersion3_2Core : NSOpenGLProfileVersionLegacy;
    pFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
    if (pFmt)
    {
        /*
         * Create a new view.
         */
        NSRect Frame;
        Frame.origin.x    = 0;
        Frame.origin.y    = 0;
        Frame.size.width  = pParams->cx < _1M ? pParams->cx : 0;
        Frame.size.height = pParams->cy < _1M ? pParams->cy : 0;
        VMSVGA3DOverlayView *pView = [[VMSVGA3DOverlayView alloc] initWithFrameAndFormat:Frame
                                                                              parentView:pParams->pParentView
                                                                             pixelFormat:pFmt];
        if (pView)
        {
            /*
             * If we have no shared GL context, we use the one that NSOpenGLView create. Otherwise,
             * we replace it.  (If we don't call openGLContext, it won't yet have been instantiated,
             * so there is no unecessary contexts created here when pSharedCtx != NULL.)
             */
            NSOpenGLContext *pCtx;
#ifdef USE_NSOPENGLVIEW
            if (!pParams->pSharedCtx)
                pCtx = [pView openGLContext];
            else
#endif
            {
                pCtx = [[NSOpenGLContext alloc] initWithFormat:pFmt shareContext: pParams->pSharedCtx];
                if (pCtx)
                {
                    [pView setOpenGLContext:pCtx];
                    [pCtx setView:pView];
#ifdef USE_NSOPENGLVIEW
                    Assert([pCtx view] == pView);
#endif
                }
            }
            if (pCtx)
            {
                /*
                 * Attach the view to the parent if we have one.  Otherwise make sure its invisible.
                 */
                if (pParams->pParentView)
                    [pParams->pParentView addSubview:pView];
                else
                    [pView setHidden:YES];

                /*
                 * Resize and return.
                 */
                //[pView vboxSetSize:Frame.size];

                NSOpenGLContext *pSavedCtx = [pView makeCurrentGLContext];

                [pView prepareOpenGL];
                GLint x;
                //x = 0; [pCtx setValues:&x forParameter:NSOpenGLCPSwapInterval];
                //x = 1; [pCtx setValues:&x forParameter:NSOpenGLCPSurfaceOrder];
                x = 0; [pCtx setValues:&x forParameter:NSOpenGLCPSurfaceOpacity];

                if (pParams->pParentView)
                    [pView setHidden:NO];
                else
                    [pView setHidden:YES];

                [pView restoreSavedGLContext:pSavedCtx];

                pParams->pView = pView;
                pParams->pCtx  = pCtx;
                [pCtx retain]; //??

                [pFmt release];

#if 0 // doesn't work or isn't needed :/
                /*
                 * Get notifications when we're moved...
                 */
                if (pParams->pParentView)
                {
                    [[NSNotificationCenter defaultCenter] addObserver:self 
                                                             selector:@selector(vboxFrameDidChange) 
                                                                 name:NSViewFrameDidChangeNotification
                                                               object:self];
                }
#endif

                LogFlow(("OvlView createViewAndContext: returns successfully\n"));
                return;
            }
            [pView release];
        }
        [pFmt release];
    }
    else
        AssertFailed();

    LogFlow(("OvlView createViewAndContext: returns failure\n"));
    return;
}

- (id)initWithFrameAndFormat:(NSRect) frame parentView:(NSView*)pParentView pixelFormat:(NSOpenGLPixelFormat *)pFmt
{
    LogFlow(("OvlView(%p) initWithFrameAndFormat:\n", (void *)self));

    m_pParentView    = pParentView;
    /* Make some reasonable defaults */
    m_Pos            = NSZeroPoint;
    m_Size           = frame.size;
    m_cClears        = 2;
    m_fUpdateCtx     = true;

#ifdef USE_NSOPENGLVIEW
    self = [super initWithFrame:frame pixelFormat:pFmt];
#else
    self = [super initWithFrame:frame];
#endif
    if (self)
    {
        //self.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin | NSViewMaxYMargin;
        self.autoresizingMask = NSViewNotSizable;
    }
    LogFlow(("OvlView(%p) initWithFrameAndFormat: returns %p\n", (void *)self, (void *)self));
    return self;
}

- (void)dealloc
{
    LogFlow(("OvlView(%p) dealloc:\n", (void *)self));

#ifdef USE_NSOPENGLVIEW
    [[self openGLContext] clearDrawable];
#else
    if (m_pCtx)
    {
        [m_pCtx clearDrawable];
        [m_pCtx release];
        m_pCtx = nil;
    }
#endif

    [super dealloc];

    LogFlow(("OvlView(%p) dealloc: returns\n", (void *)self));
}


- (void)vboxSetPos:(NSPoint)pos
{
    Log(("OvlView(%p) vboxSetPos: (%d,%d)\n", (void *)self, (int)pos.x, (int)pos.y));

    m_Pos = pos;
    [self vboxReshape];

    LogFlow(("OvlView(%p) vboxSetPos: returns\n", (void *)self));
}


- (void)vboxSetSize:(NSSize)size
{
    Log(("OvlView(%p) vboxSetSize: (%d,%d):\n", (void *)self, (int)size.width, (int)size.height));
    m_Size = size;
    [self vboxReshape];
    LogFlow(("OvlView(%p) vboxSetSize: returns\n", (void *)self));
}


- (void)vboxClearBuffers
{
#if 1 /* experiment */
    if ([NSThread isMainThread])
        Log(("OvlView(%p) vboxClearBuffers: skip, main thread\n", (void *)self));
    else
    {
        Log(("OvlView(%p) vboxClearBuffers: clears\n", (void *)self));
        NSOpenGLContext *pSavedCtx = [self makeCurrentGLContext];

        /* Clear errors. */
        GLenum rc;
        while ((rc = glGetError()) != GL_NO_ERROR)
            continue;

        /* Save the old buffer setting and make it GL_BACK (shall be GL_BACK already actually). */
        GLint iOldDrawBuf = GL_BACK;
        glGetIntegerv(GL_DRAW_BUFFER, &iOldDrawBuf);
        glDrawBuffer(GL_BACK);
        while ((rc = glGetError()) != GL_NO_ERROR)
            AssertMsgFailed(("rc=%x\n", rc));

        /* Clear the current GL_BACK. */
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT /*|GL_DEPTH_BUFFER_BIT*/ );
        [[self openGLContext] flushBuffer];
        while ((rc = glGetError()) != GL_NO_ERROR)
            AssertMsgFailed(("rc=%x\n", rc));

        /* Clear the previous GL_FRONT. */
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT /*|GL_DEPTH_BUFFER_BIT*/ );
        [[self openGLContext] flushBuffer];
        while ((rc = glGetError()) != GL_NO_ERROR)
            AssertMsgFailed(("rc=%x\n", rc));

        /* We're back to the orignal back buffer now. Just restore GL_DRAW_BUFFER. */
        glDrawBuffer(iOldDrawBuf);

        while ((rc = glGetError()) != GL_NO_ERROR)
            AssertMsgFailed(("rc=%x\n", rc));

        [self restoreSavedGLContext:pSavedCtx];
    }
#endif
}

- (void)vboxUpdateCtxIfNecessary
{
    if (m_fUpdateCtx)
    {
        Log(("OvlView(%p) vboxUpdateCtxIfNecessary: m_fUpdateCtx\n", (void *)self));
        [[self openGLContext] update];
        m_fUpdateCtx = false;
    }
}


- (void)vboxClearBackBufferIfNecessary
{
#if 1 /* experiment */
    if (m_cClears > 0)
    {
        Assert(![NSThread isMainThread]);
        Assert([self openGLContext] == [NSOpenGLContext currentContext]);
        Log(("OvlView(%p) vboxClearBackBufferIfNecessary: m_cClears=%d\n", (void *)self, m_cClears));
        m_cClears--;

        /* Clear errors. */
        GLenum rc;
        while ((rc = glGetError()) != GL_NO_ERROR)
            continue;

        /* Save the old buffer setting and make it GL_BACK (shall be GL_BACK already actually). */
        GLint iOldDrawBuf = GL_BACK;
        glGetIntegerv(GL_DRAW_BUFFER, &iOldDrawBuf);
        if (iOldDrawBuf != GL_BACK)
            glDrawBuffer(GL_BACK);
        while ((rc = glGetError()) != GL_NO_ERROR)
            AssertMsgFailed(("rc=%x\n", rc));

        /* Clear the current GL_BACK. */
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT /*|GL_DEPTH_BUFFER_BIT*/ );
        while ((rc = glGetError()) != GL_NO_ERROR)
            AssertMsgFailed(("rc=%x\n", rc));

        /* We're back to the orignal back buffer now. Just restore GL_DRAW_BUFFER. */
        if (iOldDrawBuf != GL_BACK)
            glDrawBuffer(iOldDrawBuf);

        while ((rc = glGetError()) != GL_NO_ERROR)
            AssertMsgFailed(("rc=%x\n", rc));
    }
#endif
}



- (void)vboxReshapePerform
{
    /*
     * Change the size and position if necessary.
     */
    NSRect CurFrameRect = [self frame];
    /** @todo conversions?   */
    if (   m_Pos.x != CurFrameRect.origin.x
        || m_Pos.y != CurFrameRect.origin.y)
    {
        LogFlow(("OvlView(%p) vboxReshapePerform: moving (%d,%d) -> (%d,%d)\n", 
                 (void *)self,  CurFrameRect.origin.x, CurFrameRect.origin.y, m_Pos.x, m_Pos.y));
        [self setFrameOrigin:m_Pos];
    }

    if (   CurFrameRect.size.width != m_Size.width
        || CurFrameRect.size.height != m_Size.height)
    {                    
        LogFlow(("OvlView(%p) vboxReshapePerform: resizing (%d,%d) -> (%d,%d)\n", 
                 (void *)self,  CurFrameRect.size.width, CurFrameRect.size.height, m_Size.width, m_Size.height));
        [self setFrameSize:m_Size];

        /* 
         * Schedule two clears and a context update for now. 
         * Really though, we should just clear any new surface area.
         */
        m_cClears = 2;
    }
    m_fUpdateCtx = true;
    LogFlow(("OvlView(%p) vboxReshapePerform: returns\n", self));
}


- (void)vboxReshape
{
    LogFlow(("OvlView(%p) vboxReshape:\n", (void *)self));

    /*
     * Resize the view.
     */
    if ([NSThread isMainThread])
        [self vboxReshapePerform];
    else
    {
        [self performSelectorOnMainThread:@selector(vboxReshapePerform) withObject:nil waitUntilDone:NO];
        vmsvga3dCocoaServiceRunLoop();

        /*
         * Try update the opengl context.
         */
        [[self openGLContext] update];
    }

    LogFlow(("OvlView(%p) vboxReshape: returns\n", (void *)self));
}

#if 0 // doesn't work or isn't needed :/
- (void)vboxFrameDidChange
{
    LogFlow(("OvlView(%p) vboxFrameDidChange:\n", (void *)self));
}

- (BOOL)postsFrameChangedNotifications
{
    LogFlow(("OvlView(%p) postsFrameChangedNotifications:\n", (void *)self));
    return YES;
}
#endif

/**
 * Removes the view from the parent, if it has one, and makes sure it's hidden. 
 *  
 * This is callbed before destroying it. 
 */
- (void)vboxRemoveFromSuperviewAndHide
{
    LogFlow(("OvlView(%p) vboxRemoveFromSuperviewAndHide:\n", (void *)self));
    if (m_pParentView)
    {
        [self removeFromSuperview];
        [self setHidden:YES];
        [[NSNotificationCenter defaultCenter] removeObserver:self];
    }
}


/**
 * Changes to the OpenGL context associated with the view.
 * @returns Previous OpenGL context.
 */
- (NSOpenGLContext *)makeCurrentGLContext
{
    NSOpenGLContext *pSavedCtx = [NSOpenGLContext currentContext];

    /* Always flush before changing. glXMakeCurrent and wglMakeCurrent does this
       implicitly, seemingly NSOpenGLContext::makeCurrentContext doesn't. */
    if (pSavedCtx != nil)
        glFlush();

    [[self openGLContext] makeCurrentContext];
    return pSavedCtx;
}


/**
 * Restores the previous OpenGL context after
 * makeCurrentGLContext.
 *
 * @param pSavedCtx     The makeCurrentGLContext return value.
 */
- (void)restoreSavedGLContext:(NSOpenGLContext *)pSavedCtx
{
    /* Always flush before changing. glXMakeCurrent and wglMakeCurrent does this
       implicitly, seemingly NSOpenGLContext::makeCurrentContext doesn't. */
    glFlush();

    if (pSavedCtx)
        [pSavedCtx makeCurrentContext];
    else
        [NSOpenGLContext clearCurrentContext];
}

#ifndef USE_NSOPENGLVIEW
/*
 * Faking NSOpenGLView interface.
 */
- (void)setOpenGLContext:(NSOpenGLContext *)pCtx
{
    if (pCtx != m_pCtx)
    {
        if (pCtx)
        {
            [pCtx retain];
            [pCtx setView:self];
            /*Assert([pCtx view] == self); - setView fails early on, works later... */
        }

        if (m_pCtx)
            [m_pCtx release];

        m_pCtx = pCtx;

        if (pCtx)
            [pCtx update];
    }
}

- (NSOpenGLContext *)openGLContext
{
    /* Stupid hack to work around setView failing early. */
    if (m_pCtx && [m_pCtx view] != self)
        [m_pCtx setView:self];
    return m_pCtx;
}

- (void)prepareOpenGL
{
    //[m_pCtx prepareOpenGL];
}
#endif /* USE_NSOPENGLVIEW */

/*
 * Overridden NSOpenGLView / NSView methods:
 */

/** @todo do we need this?  */
-(void)viewDidMoveToWindow
{
    LogFlow(("OvlView(%p) viewDidMoveToWindow: new win: %p\n", (void *)self, (void *)[self window]));
    [super viewDidMoveToWindow];
    [self vboxReshape];
}

-(void)viewDidMoveToSuperview
{
    LogFlow(("OvlView(%p) viewDidMoveToSuperview: new view: %p\n", (void *)self, (void *)[self superview]));
    [super viewDidMoveToSuperview];
    [self vboxReshape];
}

-(void)resizeWithOldSuperviewSize:(NSSize)oldBoundsSize
{
    LogFlow(("OvlView(%p) resizeWithOldSuperviewSize: %d,%d -> %d,%d\n", (void *)self,
             (int)oldBoundsSize.width, (int)oldBoundsSize.height, (int)[self bounds].size.width, (int)[self bounds].size.height));
    [super resizeWithOldSuperviewSize:oldBoundsSize];
    [self vboxReshape];
}

- (void)drawRect:(NSRect)rect
{
//    if (m_fClear)
//    {
//        m_fClear = false;
//        [self vboxClearBuffers];
//    }
}

@end /* VMSVGA3DOverlayView */

@implementation VMSVGA3DCreateViewAndContext
@end


VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaServiceRunLoop(void)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    NSRunLoop *pRunLoop = [NSRunLoop currentRunLoop];

    if ([NSRunLoop mainRunLoop] != pRunLoop)
    {
        [pRunLoop runUntilDate:[NSDate distantPast]];
    }

    [pPool release];
}


/**
 * Document me later.
 * 
 * @param   pParentView     The parent view if this is a context we'll be 
 *                          presenting to.
 */
VMSVGA3DCOCOA_DECL(bool) vmsvga3dCocoaCreateViewAndContext(NativeNSViewRef *ppView, NativeNSOpenGLContextRef *ppCtx,
                                                           NativeNSViewRef pParentView, uint32_t cx, uint32_t cy,
                                                           NativeNSOpenGLContextRef pSharedCtx, bool fOtherProfile)
{
    LogFlow(("vmsvga3dCocoaCreateViewAndContext: pParentView=%d size=%d,%d pSharedCtx=%p fOtherProfile=%RTbool\n",
             (void *)pParentView, cx, cy, (void *)pSharedCtx, fOtherProfile));
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    vmsvga3dCocoaServiceRunLoop();


    VMSVGA3DCreateViewAndContext *pParams = [VMSVGA3DCreateViewAndContext alloc];
    pParams->pParentView = pParentView;
    pParams->cx = cx;
    pParams->cy = cy;
    pParams->pSharedCtx = pSharedCtx;
    pParams->fOtherProfile = fOtherProfile;
    pParams->pView = NULL;
    pParams->pCtx = NULL;

    [VMSVGA3DOverlayView performSelectorOnMainThread:@selector(createViewAndContext:)
                                          withObject:pParams
                                       waitUntilDone:YES];

    vmsvga3dCocoaServiceRunLoop();

    *ppCtx  = pParams->pCtx;
    *ppView = pParams->pView;
    bool fRet = *ppCtx != NULL && *ppView != NULL;

    [pParams release];

    [pPool release];
    LogFlow(("vmsvga3dCocoaDestroyContext: returns %RTbool\n", fRet));
    return fRet;
}


VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaDestroyViewAndContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx)
{
    LogFlow(("vmsvga3dCocoaDestroyViewAndContext: pView=%p pCtx=%p\n", (void *)pView, (void *)pCtx));
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    /* The view */
    [(VMSVGA3DOverlayView *)pView vboxRemoveFromSuperviewAndHide];

    Log(("vmsvga3dCocoaDestroyViewAndContext: view %p ref count=%d\n", (void *)pView, [pView retainCount]));
    [pView release];

    /* The OpenGL context. */
    Log(("vmsvga3dCocoaDestroyViewAndContext: ctx  %p ref count=%d\n", (void *)pCtx, [pCtx retainCount]));
    [pCtx release];

    [pPool release];
    LogFlow(("vmsvga3dCocoaDestroyViewAndContext: returns\n"));
}


/** @note Not currently used. */
VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaViewSetPosition(NativeNSViewRef pView, NativeNSViewRef pParentView, int x, int y)
{
    LogFlow(("vmsvga3dCocoaViewSetPosition: pView=%p pParentView=%p (%d,%d)\n", (void *)pView, (void *)pParentView, x, y));
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(VMSVGA3DOverlayView *)pView vboxSetPos:NSMakePoint(x, y)];

    [pPool release];
    LogFlow(("vmsvga3dCocoaViewSetPosition: returns\n"));
}


VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaViewSetSize(NativeNSViewRef pView, int cx, int cy)
{
    LogFlow(("vmsvga3dCocoaViewSetSize: pView=%p (%d,%d)\n", (void *)pView, cx, cy));
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    VMSVGA3DOverlayView *pOverlayView = (VMSVGA3DOverlayView *)pView;

    [pOverlayView vboxSetSize:NSMakeSize(cx, cy)];

    [pPool release];
    LogFlow(("vmsvga3dCocoaViewSetSize: returns\n"));
}


void vmsvga3dCocoaViewMakeCurrentContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx)
{
    LogFlow(("vmsvga3dCocoaViewMakeCurrentContext: pView=%p, pCtx=%p\n", (void*)pView, (void*)pCtx));
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    VMSVGA3DOverlayView *pOverlayView = (VMSVGA3DOverlayView *)pView;

    /* Always flush before flush. glXMakeCurrent and wglMakeCurrent does this
       implicitly, seemingly NSOpenGLContext::makeCurrentContext doesn't. */
    if ([NSOpenGLContext currentContext] != 0)
        glFlush();

    if (pOverlayView)
    {
        /* This must be a release assertion as we depend on the setView
           sideeffect of the openGLContext method call. (hack alert!) */
        AssertRelease([pOverlayView openGLContext] == pCtx);
        [pCtx makeCurrentContext];
        [pOverlayView vboxUpdateCtxIfNecessary];
    }
    else
        [NSOpenGLContext clearCurrentContext];

    [pPool release];
    LogFlow(("vmsvga3dCocoaSwapBuffers: returns\n"));
}


void vmsvga3dCocoaSwapBuffers(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx)
{
    LogFlow(("vmsvga3dCocoaSwapBuffers: pView=%p, pCtx=%p\n", (void*)pView, (void*)pCtx));
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    VMSVGA3DOverlayView *pMyView = (VMSVGA3DOverlayView *)pView;

#ifndef USE_NSOPENGLVIEW
    /* Hack alert! setView fails early on so call openGLContext to try again. */
    if ([pCtx view] == NULL)
        [pMyView openGLContext];
#endif

    Assert(pCtx == [NSOpenGLContext currentContext]);
    Assert(pCtx == [pMyView openGLContext]);
    AssertMsg([pCtx view] == pMyView, ("%p != %p\n", (void *)[pCtx view], (void *)pMyView));

    [pCtx flushBuffer];
    //[pView setNeedsDisplay:YES];
    vmsvga3dCocoaServiceRunLoop();

    /* If buffer clearing or/and context updates are pending, execute that now. */
    [pMyView vboxUpdateCtxIfNecessary];
    [pMyView vboxClearBackBufferIfNecessary];

    [pPool release];
    LogFlow(("vmsvga3dCocoaSwapBuffers: returns\n"));
}

