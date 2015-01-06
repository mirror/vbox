/** @file
 * VirtualBox OpenGL Cocoa Window System Helper Implementation. 
 *  
 * @remarks Inspired by HostServices/SharedOpenGL/render/renderspu_cocoa_helper.m.
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

/* Debug macros */
#if 0 /*def DEBUG_VERBOSE*/
/*# error "should be disabled!"*/
# define DEBUG_INFO(text) do { \
        crWarning text ; \
        Assert(0); \
    } while (0)

# define DEBUG_MSG(text) \
    printf text

# define DEBUG_WARN(text) do { \
        crWarning text ; \
        Assert(0); \
    } while (0)

# define DEBUG_MSG_1(text) \
    DEBUG_MSG(text)

# define DEBUG_FUNC_ENTER() \
    int cchDebugFuncEnter = printf("==>%s\n", __PRETTY_FUNCTION__)

#define DEBUG_FUNC_LEAVE() do { \
        DEBUG_MSG(("<==%s\n", __PRETTY_FUNCTION__)); \
    } while (0)

#define DEBUG_FUNC_RET(valuefmtnl) do { \
        DEBUG_MSG(("<==%s returns", __PRETTY_FUNCTION__)); \
        DEBUG_MSG(valuefmtnl); \
    } while (0)

#else

# define DEBUG_INFO(text) do { \
        crInfo text ; \
    } while (0)

# define DEBUG_MSG(text) \
    do {} while (0)

# define DEBUG_WARN(text) do { \
        crWarning text ; \
    } while (0)

# define DEBUG_MSG_1(text) \
    do {} while (0)

# define DEBUG_FUNC_ENTER() int cchDebugFuncEnter = 0
# define DEBUG_FUNC_LEAVE() NOREF(cchDebugFuncEnter)
# define DEBUG_FUNC_RET(valuefmtnl) DEBUG_FUNC_LEAVE()

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
    
    /* Position/Size tracking */
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
    DEBUG_FUNC_ENTER();

    m_pPixelFormat = NULL;
    m_pView = NULL;

    self = [super initWithFormat:format shareContext:share];
    if (self)
        m_pPixelFormat = format;

    DEBUG_MSG(("OCTX(%p): init VMSVGA3DOpenGLContext\n", (void*)self));
    DEBUG_FUNC_RET(("%p\n", (void *)self));
    return self;
}

- (void)dealloc
{
    DEBUG_FUNC_ENTER();
    DEBUG_MSG(("OCTX(%p): dealloc VMSVGA3DOpenGLContext\n", (void*)self));

    [m_pPixelFormat release];

m_pPixelFormat = NULL;
m_pView = NULL;

    [super dealloc];
    DEBUG_FUNC_LEAVE();
}

-(bool)isDoubleBuffer
{
    DEBUG_FUNC_ENTER();
    GLint val;
    [m_pPixelFormat getValues:&val forAttribute:NSOpenGLPFADoubleBuffer forVirtualScreen:0];
    DEBUG_FUNC_RET(("%d\n", val == 1 ? YES : NO));
    return val == 1 ? YES : NO;
}

-(void)setView:(NSView*)view
{
    DEBUG_FUNC_ENTER();
    DEBUG_MSG(("OCTX(%p): setView: new view: %p\n", (void*)self, (void*)view));

    m_pView = view;

    DEBUG_FUNC_LEAVE();
}

-(NSView*)view
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_RET(("%p\n", (void *)m_pView));
    return m_pView;
}

-(void)clearDrawable
{
    DEBUG_FUNC_ENTER();
    DEBUG_MSG(("OCTX(%p): clearDrawable\n", (void*)self));

    m_pView = NULL;
    [super clearDrawable];

    DEBUG_FUNC_LEAVE();
}

-(NSOpenGLPixelFormat*)openGLPixelFormat
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_RET(("%p\n", (void *)m_pPixelFormat));
    return m_pPixelFormat;
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
    DEBUG_FUNC_ENTER();

    self = [super initWithFrame:NSZeroRect];

    m_pOverlayWindow = pOverlayWindow;

    DEBUG_MSG(("OHVW(%p): init OverlayHelperView\n", (void*)self));
    DEBUG_FUNC_RET(("%p\n", (void *)self));
    return self;
}

-(void)viewDidMoveToWindow
{
    DEBUG_FUNC_ENTER();
    DEBUG_MSG(("OHVW(%p): viewDidMoveToWindow: new win: %p\n", (void*)self, (void*)[self window]));

    [m_pOverlayWindow parentWindowChanged:[self window]];

    DEBUG_FUNC_LEAVE();
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
    DEBUG_FUNC_ENTER();
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
    DEBUG_FUNC_RET(("%p\n", (void *)self));
    return self;
}

- (void)dealloc
{
    DEBUG_FUNC_ENTER();
    DEBUG_MSG(("OWIN(%p): dealloc OverlayWindow\n", (void*)self));

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [m_pOverlayHelperView removeFromSuperview];
    [m_pOverlayHelperView release];

    [super dealloc];
    DEBUG_FUNC_LEAVE();
}

- (void)parentWindowFrameChanged:(NSNotification*)pNote
{
    DEBUG_FUNC_ENTER();
    DEBUG_MSG(("OWIN(%p): parentWindowFrameChanged\n", (void*)self));

    [m_pOverlayView reshape];

    DEBUG_FUNC_LEAVE();
}

- (void)parentWindowChanged:(NSWindow*)pWindow
{
    DEBUG_FUNC_ENTER();
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

    DEBUG_FUNC_LEAVE();
}

@end

/********************************************************************************
*
* VMSVGA3DOverlayView class implementation
*
********************************************************************************/
@implementation VMSVGA3DOverlayView

- (id)initWithFrame:(NSRect) frame parentView:(NSView*)pParentView
{
    DEBUG_FUNC_ENTER();

    m_pParentView    = pParentView;
    /* Make some reasonable defaults */
    m_pGLCtx         = nil;
    m_Pos            = NSZeroPoint;
    m_Size           = NSMakeSize(1, 1);
    m_RootRect       = NSMakeRect(0, 0, m_Size.width, m_Size.height);
    m_yInvRootOffset = 0;
    
    self = [super initWithFrame: frame];
    
    DEBUG_MSG(("OVIW(%p): init VMSVGA3DOverlayView\n", (void*)self));
    DEBUG_FUNC_RET(("%p\n", (void *)self));
    return self;
}

- (void)cleanupData
{
    DEBUG_FUNC_ENTER();

    /*[self deleteDockTile];*/
    
    [self setGLCtx:nil];
    
#if 0
    if (m_pSharedGLCtx)
    {
        if ([m_pSharedGLCtx view] == self)
            [m_pSharedGLCtx clearDrawable];

        [m_pSharedGLCtx release];

        m_pSharedGLCtx = nil;
        
        CrBltTerm(m_pBlitter);
        
        RTMemFree(m_pBlitter);
        
        m_pBlitter = nil;
    }
#endif

    /*[self clearVisibleRegions];*/
    
    DEBUG_FUNC_LEAVE();
}

- (void)dealloc
{
    DEBUG_FUNC_ENTER();
    DEBUG_MSG(("OVIW(%p): dealloc OverlayView\n", (void*)self));

    [self cleanupData];

    [super dealloc];

    DEBUG_FUNC_LEAVE();
}


- (void)setGLCtx:(NSOpenGLContext*)pCtx
{
    DEBUG_FUNC_ENTER();

    DEBUG_MSG(("OVIW(%p): setGLCtx: new ctx: %p (old: %p)\n", (void*)self, (void*)pCtx, (void *)m_pGLCtx));
    if (m_pGLCtx == pCtx)
    {
        DEBUG_FUNC_LEAVE();
        return;
    }

    /* ensure the context drawable is cleared to avoid holding a reference to inexistent view */
    if (m_pGLCtx)
    {
        [m_pGLCtx clearDrawable];
        [m_pGLCtx release];
        /*[m_pGLCtx performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];*/
    }
    m_pGLCtx = pCtx;
    if (pCtx)
        [pCtx retain];

    DEBUG_FUNC_LEAVE();
}

- (NSOpenGLContext*)glCtx
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_RET(("%p\n", (void *)m_pGLCtx));
    return m_pGLCtx;
}

- (void)setOverlayWin:(NSWindow*)pWin
{
    DEBUG_FUNC_ENTER();
    DEBUG_MSG(("OVIW(%p): setOverlayWin: new win: %p\n", (void*)self, (void*)pWin));
    m_pOverlayWin = pWin;
    DEBUG_FUNC_LEAVE();
}

- (NSWindow*)overlayWin
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_RET(("%p\n", (void *)m_pOverlayWin));
    return m_pOverlayWin;
}

- (void)setPos:(NSPoint)pos
{
    DEBUG_FUNC_ENTER();

    m_Pos = pos;
    [self reshape];
    DEBUG_FUNC_LEAVE();
}

- (NSPoint)pos
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_RET(("%f,%f\n", m_Pos.x, m_Pos.y));
    return m_Pos;
}

- (void)setSize:(NSSize)size
{
    DEBUG_FUNC_ENTER();
    m_Size = size;
    [self reshape];
    DEBUG_FUNC_LEAVE();
}

- (NSSize)size
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_RET(("%f,%f\n", m_Size.width, m_Size.height));
    return m_Size;
}

- (void)updateViewportCS
{
    DEBUG_FUNC_ENTER();
    DEBUG_MSG(("OVIW(%p): updateViewport\n", (void*)self));

    /* Update the viewport for our OpenGL view */
/*    [m_pSharedGLCtx update]; */
        
    /* Clear background to transparent */
/*    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);*/
    DEBUG_FUNC_LEAVE();
}

- (void)reshape
{
    DEBUG_FUNC_ENTER();
    NSRect parentFrame = NSZeroRect;
    NSPoint parentPos  = NSZeroPoint;
    NSPoint childPos   = NSZeroPoint;
    NSRect childFrame  = NSZeroRect;
    NSRect newFrame    = NSZeroRect;

    DEBUG_MSG(("OVIW(%p): reshape\n", (void*)self));

    /* Getting the right screen coordinates of the parents frame is a little bit
     * complicated. */
    parentFrame = [m_pParentView frame];
    DEBUG_MSG(("FIXED parentFrame [%f:%f], [%f:%f]\n", parentFrame.origin.x, parentFrame.origin.y, parentFrame.size.width, parentFrame.size.height));    
    parentPos = [[m_pParentView window] convertBaseToScreen:[[m_pParentView superview] convertPointToBase:NSMakePoint(parentFrame.origin.x, parentFrame.origin.y + parentFrame.size.height)]];
    parentFrame.origin.x = parentPos.x;
    parentFrame.origin.y = parentPos.y;

    /* Calculate the new screen coordinates of the overlay window. */
    childPos = NSMakePoint(m_Pos.x, m_Pos.y + m_Size.height);
    childPos = [[m_pParentView window] convertBaseToScreen:[[m_pParentView superview] convertPointToBase:childPos]];
    DEBUG_MSG(("FIXED childPos(screen) [%f:%f]\n", childPos.x, childPos.y));

    /* Make a frame out of it. */
    childFrame = NSMakeRect(childPos.x, childPos.y, m_Size.width, m_Size.height);
    DEBUG_MSG(("FIXED childFrame [%f:%f], [%f:%f]\n", childFrame.origin.x, childFrame.origin.y, childFrame.size.width, childFrame.size.height));

    /* We have to make sure that the overlay window will not be displayed out
     * of the parent window. So intersect both frames & use the result as the new
     * frame for the window. */
    newFrame = NSIntersectionRect(parentFrame, childFrame);

    DEBUG_MSG(("[%p]: parentFrame pos[%f : %f] size[%f : %f]\n",
          (void*)self, 
         parentFrame.origin.x, parentFrame.origin.y,
         parentFrame.size.width, parentFrame.size.height));
    DEBUG_MSG(("[%p]: childFrame pos[%f : %f] size[%f : %f]\n",
          (void*)self, 
         childFrame.origin.x, childFrame.origin.y,
         childFrame.size.width, childFrame.size.height));
         
    DEBUG_MSG(("[%p]: newFrame pos[%f : %f] size[%f : %f]\n",
          (void*)self, 
         newFrame.origin.x, newFrame.origin.y,
         newFrame.size.width, newFrame.size.height));
    
    /* Later we have to correct the texture position in the case the window is
     * out of the parents window frame. So save the shift values for later use. */ 
    m_RootRect.origin.x = newFrame.origin.x - childFrame.origin.x;
    m_RootRect.origin.y =  childFrame.size.height + childFrame.origin.y - (newFrame.size.height + newFrame.origin.y);
    m_RootRect.size = newFrame.size;
    m_yInvRootOffset = newFrame.origin.y - childFrame.origin.y;
    
    DEBUG_MSG(("[%p]: m_RootRect pos[%f : %f] size[%f : %f]\n",
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
    DEBUG_FUNC_LEAVE();
}

@end


void vmsvga3dCocoaCreateContext(NativeNSOpenGLContextRef *ppCtx, NativeNSOpenGLContextRef pShareCtx, bool fLegacy)
{
    DEBUG_FUNC_ENTER();
    NSOpenGLPixelFormat *pFmt = nil;
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

#if 1
    // @todo galitsyn: NSOpenGLPFAWindow was deprecated starting from OSX 10.9.
    // Consider to remove it and check if it's harmless.
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
        DEBUG_MSG(("New context %p\n", (void *)*ppCtx));
    }

    [pPool release];

    DEBUG_FUNC_LEAVE();
}

void vmsvga3dCocoaDestroyContext(NativeNSOpenGLContextRef pCtx)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    [pCtx release];
    [pPool release];
    DEBUG_FUNC_LEAVE();
}

void vmsvga3dCocoaCreateView(NativeNSViewRef *ppView, NativeNSViewRef pParentView)
{
    DEBUG_FUNC_ENTER();
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
    DEBUG_FUNC_LEAVE();
}

void vmsvga3dCocoaDestroyView(NativeNSViewRef pView)
{
    DEBUG_FUNC_ENTER();
    NSWindow *pWin = nil;
    
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    [pView setHidden: YES];
    
    [pWin release];
    [pView release];

    [pPool release];
    DEBUG_FUNC_LEAVE();
}

void vmsvga3dCocoaViewSetPosition(NativeNSViewRef pView, NativeNSViewRef pParentView, int x, int y)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(VMSVGA3DOverlayView*)pView setPos:NSMakePoint(x, y)];

    [pPool release];
}

void vmsvga3dCocoaViewSetSize(NativeNSViewRef pView, int w, int h)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(VMSVGA3DOverlayView*)pView setSize:NSMakeSize(w, h)];

    [pPool release];
    DEBUG_FUNC_LEAVE();
}

void vmsvga3dCocoaViewMakeCurrentContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    DEBUG_MSG(("cocoaViewMakeCurrentContext(%p, %p)\n", (void*)pView, (void*)pCtx));

    if (pView)
    {
        [(VMSVGA3DOverlayView*)pView setGLCtx:pCtx];
/*        [(VMSVGA3DOverlayView*)pView makeCurrentFBO];*/
        [pCtx makeCurrentContext];
    }
    else
    {
    	[NSOpenGLContext clearCurrentContext];
    }

    [pPool release];
    DEBUG_FUNC_LEAVE();
}

void vmsvga3dCocoaSwapBuffers(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    
    [pCtx flushBuffer];
    
    [pPool release];
    DEBUG_FUNC_LEAVE();
}
