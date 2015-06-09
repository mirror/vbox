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
    NSView          *m_pParentView;
    /** Set if the buffers needs clearing. */
    bool            m_fClear;

#ifndef USE_NSOPENGLVIEW
    /** The OpenGL context associated with this view. */
    NSOpenGLContext *m_pCtx;
#endif

    /* Position/Size tracking */
    NSPoint          m_Pos;
    NSSize           m_Size;
    
    /** This is necessary for clipping on the root window */
    NSRect           m_RootRect;
}
+ (void)createViewAndContext:(VMSVGA3DCreateViewAndContext *)pParams;
- (id)initWithFrameAndFormat:(NSRect)frame parentView:(NSView*)pparentView pixelFormat:(NSOpenGLPixelFormat *)pFmt;
- (void)setPos:(NSPoint)pos;
- (void)setSize:(NSSize)size;
- (void)vboxReshape;
- (void)vboxClearBuffers;
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
    LogFlow(("OvlWin createViewAndContext:\n"));

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
                 * Attach the view to the parent.
                 */
                [pParams->pParentView addSubview:pView];

                /*
                 * Resize and return.
                 */
                //[pView setSize:Frame.size];

                NSOpenGLContext *pSavedCtx = [pView makeCurrentGLContext];

                [pView prepareOpenGL];
                GLint x;
                //x = 0; [pCtx setValues:&x forParameter:NSOpenGLCPSwapInterval];
                //x = 1; [pCtx setValues:&x forParameter:NSOpenGLCPSurfaceOrder];
                x = 0; [pCtx setValues:&x forParameter:NSOpenGLCPSurfaceOpacity];

                [pView setHidden:NO];

                [pView restoreSavedGLContext:pSavedCtx];

                pParams->pView = pView;
                pParams->pCtx  = pCtx;
                [pCtx retain]; //??

                [pFmt release];
                LogFlow(("OvlWin createViewAndContext: returns successfully\n"));
                return;
            }
            [pView release];
        }
        [pFmt release];
    }
    else
        AssertFailed();

    LogFlow(("OvlWin createViewAndContext: returns failure\n"));
    return;
}

- (id)initWithFrameAndFormat:(NSRect) frame parentView:(NSView*)pParentView pixelFormat:(NSOpenGLPixelFormat *)pFmt
{
    LogFlow(("OvlWin(%p) initWithFrameAndFormat:\n", (void *)self));

    m_pParentView    = pParentView;
    /* Make some reasonable defaults */
    m_Pos            = NSZeroPoint;
    m_Size           = frame.size;
    m_RootRect       = NSMakeRect(0, 0, m_Size.width, m_Size.height);
    
#ifdef USE_NSOPENGLVIEW
    self = [super initWithFrame:frame pixelFormat:pFmt];
#else
    self = [super initWithFrame:frame];
#endif
    if (self)
    {
        self.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin | NSViewMaxYMargin;
    }
    LogFlow(("OvlWin(%p) initWithFrameAndFormat: returns %p\n", (void *)self, (void *)self));
    return self;
}

- (void)dealloc
{
    LogFlow(("OvlWin(%p) dealloc:\n", (void *)self));

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

    LogFlow(("OvlWin(%p) dealloc: returns\n", (void *)self));
}


- (void)setPos:(NSPoint)pos
{
    Log(("OvlWin(%p) setPos: (%d,%d)\n", (void *)self, (int)pos.x, (int)pos.y));

    m_Pos = pos;
    [self vboxReshape];

    LogFlow(("OvlWin(%p) setPos: returns\n", (void *)self));
}


- (void)setSize:(NSSize)size
{
    Log(("OvlWin(%p) setSize: (%d,%d):\n", (void *)self, (int)size.width, (int)size.height));
    m_Size = size;
    [self vboxReshape];
    LogFlow(("OvlWin(%p) setSize: returns\n", (void *)self));
}


- (void)vboxClearBuffers
{
#if 1 /* experiment */
    if ([NSThread isMainThread])
        Log(("OvlWin(%p) vboxClearBuffers: skip, main thread\n", (void *)self));
    else
    {
        Log(("OvlWin(%p) vboxClearBuffers: clears\n", (void *)self));
        NSOpenGLContext *pSavedCtx = [self makeCurrentGLContext];

        GLint iOldDrawBuf = GL_BACK;
        glGetIntegerv(GL_DRAW_BUFFER, &iOldDrawBuf);
        glDrawBuffer(GL_FRONT_AND_BACK);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT /*|GL_DEPTH_BUFFER_BIT*/ );
        [[self openGLContext] flushBuffer];
        glDrawBuffer(iOldDrawBuf);

        [self restoreSavedGLContext:pSavedCtx];
    }
#endif
}


- (void)vboxReshape
{
    LogFlow(("OvlWin(%p) vboxReshape:\n", (void *)self));

    /*
     * Not doing any complicate stuff here yet, hoping that we'll get correctly 
     * resized when the parent view changes...
     */

    /*
     * Tell the GL context.
     */
    //[[self openGLContext] setView:self];
    [[self openGLContext] update];

    [self vboxClearBuffers];

    LogFlow(("OvlWin(%p) vboxReshape: returns\n", (void *)self));
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
    if (m_fClear)
    {
        m_fClear = false;
        [self vboxClearBuffers];
    }
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
    [pView removeFromSuperview];
    [pView setHidden: YES];
    Log(("vmsvga3dCocoaDestroyViewAndContext: view %p ref count=%d\n", (void *)pView, [pView retainCount]));
    [pView release];

    /* The OpenGL context. */
    Log(("vmsvga3dCocoaDestroyViewAndContext: ctx  %p ref count=%d\n", (void *)pCtx, [pView retainCount]));
    [pCtx release];

    [pPool release];
    LogFlow(("vmsvga3dCocoaDestroyViewAndContext: returns\n"));
}


VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaViewSetPosition(NativeNSViewRef pView, NativeNSViewRef pParentView, int x, int y)
{
    LogFlow(("vmsvga3dCocoaViewSetPosition: pView=%p pParentView=%p (%d,%d)\n", (void *)pView, (void *)pParentView, x, y));
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(VMSVGA3DOverlayView *)pView setPos:NSMakePoint(x, y)];

    [pPool release];
    LogFlow(("vmsvga3dCocoaViewSetPosition: returns\n"));
}


VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaViewSetSize(NativeNSViewRef pView, int cx, int cy)
{
    LogFlow(("vmsvga3dCocoaViewSetSize: pView=%p (%d,%d)\n", (void *)pView, cx, cy));
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    VMSVGA3DOverlayView *pOverlayView = (VMSVGA3DOverlayView *)pView;

    [pOverlayView setSize:NSMakeSize(cx, cy)];

    [pPool release];
    LogFlow(("vmsvga3dCocoaViewSetSize: returns\n"));
}


void vmsvga3dCocoaViewMakeCurrentContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx)
{
    LogFlow(("vmsvga3dCocoaViewSetSize: pView=%p, pCtx=%p\n", (void*)pView, (void*)pCtx));
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

#ifndef USE_NSOPENGLVIEW
    /* Hack alert! setView fails early on so call openGLContext to try again. */
    VMSVGA3DOverlayView *pMyView = (VMSVGA3DOverlayView *)pView;
    if ([pCtx view] == NULL)
        [pMyView openGLContext];
#elif defined(RT_STRICT)
    NSOpenGLView *pMyView = (NSOpenGLView *)pView;
#endif

    Assert(pCtx == [NSOpenGLContext currentContext]);
    Assert(pCtx == [pMyView openGLContext]);
    AssertMsg([pCtx view] == pMyView, ("%p != %p\n", (void *)[pCtx view], (void *)pMyView));

    [pCtx flushBuffer];
    //[pView setNeedsDisplay:YES];
    vmsvga3dCocoaServiceRunLoop();
    
    [pPool release];
    LogFlow(("vmsvga3dCocoaSwapBuffers: returns\n"));
}

