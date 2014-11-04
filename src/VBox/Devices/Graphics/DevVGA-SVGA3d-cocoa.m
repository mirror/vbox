/** @file
 * VirtualBox OpenGL Cocoa Window System Helper Implementation.
 */

/*
 * Copyright (C) 2009-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "DevVGA-SVGA3d-cocoa.h"
#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>

#include <iprt/thread.h>

# define DEBUG_MSG(text) \
    do {} while (0)
#ifdef DEBUG_VERBOSE
# define DEBUG_MSG_1(text) \
    DEBUG_MSG(text)
#else
# define DEBUG_MSG_1(text) \
    do {} while (0)
#endif

# define CHECK_GL_ERROR()\
    do {} while (0)

/** Custom OpenGL context class.
 *
 * This implementation doesn't allow to set a view to the
 * context, but save the view for later use. Also it saves a copy of the
 * pixel format used to create that context for later use. */
@interface VMSVGA3DOpenGLContext: NSOpenGLContext
{
@private
    NSOpenGLPixelFormat *m_pPixelFormat;
    NSView              *m_pView;
}
- (NSOpenGLPixelFormat*)openGLPixelFormat;
@end

@interface VMSVGA3DOverlayView: NSView
{
@private
    NSView          *m_pParentView;
    NSWindow        *m_pOverlayWin;
    
    NSOpenGLContext *m_pGLCtx;
    
    /* position/size */
    NSPoint          m_Pos;
    NSSize           m_Size;
    
    /** This is necessary for clipping on the root window */
    NSRect           m_RootRect;
    float            m_yInvRootOffset;
}
- (id)initWithFrame:(NSRect)frame parentView:(NSView*)pparentView;
- (void)setGLCtx:(NSOpenGLContext*)pCtx;
- (NSOpenGLContext*)glCtx;
- (void)setOverlayWin: (NSWindow*)win;
- (NSWindow*)overlayWin;
- (void)setPos:(NSPoint)pos;
- (NSPoint)pos;
- (void)setSize:(NSSize)size;
- (NSSize) size;
- (void)updateViewportCS;
- (void)reshape;
@end

/** Helper view.
 *
 * This view is added as a sub view of the parent view to track
 * main window changes. Whenever the main window is changed
 * (which happens on fullscreen/seamless entry/exit) the overlay
 * window is informed & can add them self as a child window
 * again. */
@class VMSVGA3DOverlayWindow;
@interface VMSVGA3DOverlayHelperView: NSView
{
@private
    VMSVGA3DOverlayWindow *m_pOverlayWindow;
}
-(id)initWithOverlayWindow:(VMSVGA3DOverlayWindow*)pOverlayWindow;
@end

/** Custom window class.
 *
 * This is the overlay window which contains our custom NSView.
 * Its a direct child of the Qt Main window. It marks its background
 * transparent & non opaque to make clipping possible. It also disable mouse
 * events and handle frame change events of the parent view. */
@interface VMSVGA3DOverlayWindow: NSWindow
{
@private
    NSView                    *m_pParentView;
    VMSVGA3DOverlayView       *m_pOverlayView;
    VMSVGA3DOverlayHelperView *m_pOverlayHelperView;
    NSThread                  *m_Thread;
}
- (id)initWithParentView:(NSView*)pParentView overlayView:(VMSVGA3DOverlayView*)pOverlayView;
- (void)parentWindowFrameChanged:(NSNotification *)note;
- (void)parentWindowChanged:(NSWindow*)pWindow;
@end


/********************************************************************************
*
* VMSVGA3DOpenGLContext class implementation
*
********************************************************************************/
@implementation VMSVGA3DOpenGLContext

-(id)initWithFormat:(NSOpenGLPixelFormat*)format shareContext:(NSOpenGLContext*)share
{
    m_pPixelFormat = NULL;
    m_pView = NULL;

    self = [super initWithFormat:format shareContext:share];
    if (self)
        m_pPixelFormat = format;

    DEBUG_MSG(("OCTX(%p): init VMSVGA3DOpenGLContext\n", (void*)self));

    return self;
}

- (void)dealloc
{
    DEBUG_MSG(("OCTX(%p): dealloc VMSVGA3DOpenGLContext\n", (void*)self));

    [m_pPixelFormat release];

    [super dealloc];
}

-(bool)isDoubleBuffer
{
    GLint val;
    [m_pPixelFormat getValues:&val forAttribute:NSOpenGLPFADoubleBuffer forVirtualScreen:0];
    return val == 1 ? YES : NO;
}

-(void)setView:(NSView*)view
{
    DEBUG_MSG(("OCTX(%p): setView: new view: %p\n", (void*)self, (void*)view));

    m_pView = view;
}

-(NSView*)view
{
    return m_pView;
}

-(void)clearDrawable
{
    DEBUG_MSG(("OCTX(%p): clearDrawable\n", (void*)self));

    m_pView = NULL;;
    [super clearDrawable];
}

-(NSOpenGLPixelFormat*)openGLPixelFormat
{
    return m_pPixelFormat;
}

@end



@implementation VMSVGA3DOverlayView

- (id)initWithFrame:(NSRect) frame parentView:(NSView*)pParentView
{
    m_pParentView    = pParentView;
    m_pGLCtx         = nil;
    m_Pos            = NSZeroPoint;
    m_Size           = NSMakeSize(1, 1);
    m_RootRect       = NSMakeRect(0, 0, m_Size.width, m_Size.height);
    m_yInvRootOffset = 0;
    
    self = [super initWithFrame: frame];
    
    return self;
}

- (void)setGLCtx:(NSOpenGLContext*)pCtx
{
    if (m_pGLCtx == pCtx)
        return;
    if (m_pGLCtx)
        [m_pGLCtx clearDrawable];
    m_pGLCtx = pCtx;
}

- (NSOpenGLContext*)glCtx
{
    return m_pGLCtx;
}

- (void)setOverlayWin:(NSWindow*)pWin
{
    m_pOverlayWin = pWin;
}

- (NSWindow*)overlayWin
{
    return m_pOverlayWin;
}

- (void)setPos:(NSPoint)pos
{
    m_Pos = pos;
    [self reshape];
}

- (NSPoint)pos
{
    return m_Pos;
}

- (void)setSize:(NSSize)size
{
    m_Size = size;
    [self reshape];
}

- (NSSize)size
{
    return m_Size;
}

- (void)updateViewportCS
{
    DEBUG_MSG(("OVIW(%p): updateViewport\n", (void*)self));

    /* Update the viewport for our OpenGL view */
/*    [m_pSharedGLCtx update]; */
        
    /* Clear background to transparent */
//    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

- (void)reshape
{
    NSRect parentFrame = NSZeroRect;
    NSPoint parentPos  = NSZeroPoint;
    NSPoint childPos   = NSZeroPoint;
    NSRect childFrame  = NSZeroRect;
    NSRect newFrame    = NSZeroRect;

    DEBUG_MSG(("OVIW(%p): reshape\n", (void*)self));

    /* Getting the right screen coordinates of the parents frame is a little bit
     * complicated. */
    parentFrame = [m_pParentView frame];
    parentPos = [[m_pParentView window] convertBaseToScreen:[[m_pParentView superview] convertPointToBase:NSMakePoint(parentFrame.origin.x, parentFrame.origin.y + parentFrame.size.height)]];
    parentFrame.origin.x = parentPos.x;
    parentFrame.origin.y = parentPos.y;

    /* Calculate the new screen coordinates of the overlay window. */
    childPos = NSMakePoint(m_Pos.x, m_Pos.y + m_Size.height);
    childPos = [[m_pParentView window] convertBaseToScreen:[[m_pParentView superview] convertPointToBase:childPos]];

    /* Make a frame out of it. */
    childFrame = NSMakeRect(childPos.x, childPos.y, m_Size.width, m_Size.height);

    /* We have to make sure that the overlay window will not be displayed out
     * of the parent window. So intersect both frames & use the result as the new
     * frame for the window. */
    newFrame = NSIntersectionRect(parentFrame, childFrame);

    DEBUG_MSG(("[%#p]: parentFrame pos[%f : %f] size[%f : %f]\n",
          (void*)self, 
         parentFrame.origin.x, parentFrame.origin.y,
         parentFrame.size.width, parentFrame.size.height));
    DEBUG_MSG(("[%#p]: childFrame pos[%f : %f] size[%f : %f]\n",
          (void*)self, 
         childFrame.origin.x, childFrame.origin.y,
         childFrame.size.width, childFrame.size.height));
         
    DEBUG_MSG(("[%#p]: newFrame pos[%f : %f] size[%f : %f]\n",
          (void*)self, 
         newFrame.origin.x, newFrame.origin.y,
         newFrame.size.width, newFrame.size.height));
    
    /* Later we have to correct the texture position in the case the window is
     * out of the parents window frame. So save the shift values for later use. */ 
    m_RootRect.origin.x = newFrame.origin.x - childFrame.origin.x;
    m_RootRect.origin.y =  childFrame.size.height + childFrame.origin.y - (newFrame.size.height + newFrame.origin.y);
    m_RootRect.size = newFrame.size;
    m_yInvRootOffset = newFrame.origin.y - childFrame.origin.y;
    
    DEBUG_MSG(("[%#p]: m_RootRect pos[%f : %f] size[%f : %f]\n",
         (void*)self, 
         m_RootRect.origin.x, m_RootRect.origin.y,
         m_RootRect.size.width, m_RootRect.size.height));

    /* Set the new frame. */
    [[self window] setFrame:newFrame display:YES];

#if 0
    /* Make sure the context is updated according */
    /* [self updateViewport]; */
    if (m_pSharedGLCtx)
    {
        VBOX_CR_RENDER_CTX_INFO CtxInfo; 
        vboxCtxEnter(m_pSharedGLCtx, &CtxInfo);
    
        [self updateViewportCS];
    
        vboxCtxLeave(&CtxInfo);
    }
#endif
}

@end

/********************************************************************************
*
* VMSVGA3DOverlayHelperView class implementation
*
********************************************************************************/
@implementation VMSVGA3DOverlayHelperView

-(id)initWithOverlayWindow:(VMSVGA3DOverlayWindow*)pOverlayWindow
{
    self = [super initWithFrame:NSZeroRect];

    m_pOverlayWindow = pOverlayWindow;

    return self;
}

-(void)viewDidMoveToWindow
{
    DEBUG_MSG(("OHVW(%p): viewDidMoveToWindow: new win: %p\n", (void*)self, (void*)[self window]));

    [m_pOverlayWindow parentWindowChanged:[self window]];
}

@end


/********************************************************************************
*
* VMSVGA3DOverlayWindow class implementation
*
********************************************************************************/
@implementation VMSVGA3DOverlayWindow

- (id)initWithParentView:(NSView*)pParentView overlayView:(VMSVGA3DOverlayView*)pOverlayView
{
    NSWindow *pParentWin = nil;

    if((self = [super initWithContentRect:NSZeroRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]))
    {
        m_pParentView = pParentView;
        m_pOverlayView = pOverlayView;
        m_Thread = [NSThread currentThread];

        [m_pOverlayView setOverlayWin: self];

        m_pOverlayHelperView = [[VMSVGA3DOverlayHelperView alloc] initWithOverlayWindow:self];
        /* Add the helper view as a child of the parent view to get notifications */
        [pParentView addSubview:m_pOverlayHelperView];

        /* Make sure this window is transparent */
#ifdef SHOW_WINDOW_BACKGROUND
        /* For debugging */
        [self setBackgroundColor:[NSColor colorWithCalibratedRed:1.0 green:0.0 blue:0.0 alpha:0.7]];
#else
        [self setBackgroundColor:[NSColor clearColor]];
#endif
        [self setOpaque:NO];
        [self setAlphaValue:.999];
        /* Disable mouse events for this window */
        [self setIgnoresMouseEvents:YES];

        pParentWin = [m_pParentView window];

        /* Initial set the position to the parents view top/left (Compiz fix). */
        [self setFrameOrigin:
            [pParentWin convertBaseToScreen:
                [m_pParentView convertPoint:NSZeroPoint toView:nil]]];

        /* Set the overlay view as our content view */
        [self setContentView:m_pOverlayView];

        /* Add ourself as a child to the parent views window. Note: this has to
         * be done last so that everything else is setup in
         * parentWindowChanged. */
        [pParentWin addChildWindow:self ordered:NSWindowAbove];
    }
    DEBUG_MSG(("OWIN(%p): init OverlayWindow\n", (void*)self));

    return self;
}

- (void)dealloc
{
    DEBUG_MSG(("OWIN(%p): dealloc OverlayWindow\n", (void*)self));

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [m_pOverlayHelperView removeFromSuperview];
    [m_pOverlayHelperView release];

    [super dealloc];
}

- (void)parentWindowFrameChanged:(NSNotification*)pNote
{
    DEBUG_MSG(("OWIN(%p): parentWindowFrameChanged\n", (void*)self));

    [m_pOverlayView reshape];
}

- (void)parentWindowChanged:(NSWindow*)pWindow
{
    DEBUG_MSG(("OWIN(%p): parentWindowChanged\n", (void*)self));

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    if(pWindow != nil)
    {
        /* Ask to get notifications when our parent window frame changes. */
        [[NSNotificationCenter defaultCenter]
            addObserver:self
            selector:@selector(parentWindowFrameChanged:)
            name:NSWindowDidResizeNotification
            object:pWindow];
        /* Add us self as child window */
        [pWindow addChildWindow:self ordered:NSWindowAbove];
        [m_pOverlayView reshape];
    }
}

@end


void vmsvga3dCocoaCreateContext(NativeNSOpenGLContextRef *ppCtx, NativeNSOpenGLContextRef pShareCtx)
{
    NSOpenGLPixelFormat *pFmt = nil;

    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

#if 1
    NSOpenGLPixelFormatAttribute attribs[] =
    {
        NSOpenGLPFAWindow,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24,
        NSOpenGLPFAAlphaSize, (NSOpenGLPixelFormatAttribute)8,
        NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)24,
        0
    };
#else
    NSOpenGLPixelFormatAttribute attribs[] =
    {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        0
    };
#endif

    /* Choose a pixel format */
    pFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];

    if (pFmt)
    {
        *ppCtx = [[VMSVGA3DOpenGLContext alloc] initWithFormat:pFmt shareContext:pShareCtx];
        DEBUG_MSG(("New context %X\n", (uint)*ppCtx));
    }

    [pPool release];
}

void vmsvga3dCocoaDestroyContext(NativeNSOpenGLContextRef pCtx)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    [pCtx release];
    [pPool release];
}

void vmsvga3dCocoaCreateView(NativeNSViewRef *ppView, NativeNSViewRef pParentView)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    
    /* Create our worker view */
    VMSVGA3DOverlayView* pView = [[VMSVGA3DOverlayView alloc] initWithFrame:NSZeroRect parentView:pParentView];

    if (pView)
    {
        /* We need a real window as container for the view */
        [[VMSVGA3DOverlayWindow alloc] initWithParentView:pParentView overlayView:pView];
        /* Return the freshly created overlay view */
        *ppView = pView;
    }
    
    [pPool release];
}

void vmsvga3dCocoaDestroyView(NativeNSViewRef pView)
{
    NSWindow *pWin = nil;
    
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    [pView setHidden: YES];
    
    [pWin release];
    [pView release];
    [pPool release];
}

void vmsvga3dCocoaViewSetPosition(NativeNSViewRef pView, NativeNSViewRef pParentView, int x, int y)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(VMSVGA3DOverlayView*)pView setPos:NSMakePoint(x, y)];

    [pPool release];
}

void vmsvga3dCocoaViewSetSize(NativeNSViewRef pView, int w, int h)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(VMSVGA3DOverlayView*)pView setSize:NSMakeSize(w, h)];

    [pPool release];
}

void vmsvga3dCocoaViewMakeCurrentContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    DEBUG_MSG(("cocoaViewMakeCurrentContext(%p, %p)\n", (void*)pView, (void*)pCtx));

    if (pView)
    {
        [(VMSVGA3DOverlayView*)pView setGLCtx:pCtx];
//        [(VMSVGA3DOverlayView*)pView makeCurrentFBO];
        [pCtx makeCurrentContext];
    }
    else
    {
    	[NSOpenGLContext clearCurrentContext];
    }

    [pPool release];
}

void vmsvga3dCocoaSwapBuffers(NativeNSOpenGLContextRef pCtx)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    
    [pCtx flushBuffer];
    
    [pPool release];
}
