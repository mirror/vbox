/* $Id$ */
/** @file
 * VirtualBox OpenGL Cocoa Window System Helper Implementation.
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "renderspu_cocoa_helper.h"

#import <Cocoa/Cocoa.h>

#include "chromium.h" /* For the visual bits of chromium */

#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/time.h>

#include <cr_vreg.h>

/** @page pg_opengl_cocoa  OpenGL - Cocoa Window System Helper
 *
 * How this works:
 * In general it is not so easy like on the other platforms, cause Cocoa
 * doesn't support any clipping of already painted stuff. In Mac OS X there is
 * the concept of translucent canvas's e.g. windows and there it is just
 * painted what should be visible to the user. Unfortunately this isn't the
 * concept of chromium. Therefor I reroute all OpenGL operation from the guest
 * to a frame buffer object (FBO). This is a OpenGL extension, which is
 * supported by all OS X versions we support (AFAIC tell). Of course the guest
 * doesn't know that and we have to make sure that the OpenGL state always is
 * in the right state to paint into the FBO and not to the front/back buffer.
 * Several functions below (like cocoaBindFramebufferEXT, cocoaGetIntegerv,
 * ...) doing this. When a swap or finish is triggered by the guest, the
 * content (which is already bound to an texture) is painted on the screen
 * within a separate OpenGL context. This allows the usage of the same
 * resources (texture ids, buffers ...) but at the same time having an
 * different internal OpenGL state. Another advantage is that we can paint a
 * thumbnail of the current output in a much more smaller (GPU accelerated
 * scale) version on a third context and use glReadPixels to get the actual
 * data. glReadPixels is a very slow operation, but as we just use a much more
 * smaller image, we can handle it (anyway this is only done 5 times per
 * second).
 *
 * Other things to know:
 * - If the guest request double buffering, we have to make sure there are two
 *   buffers. We use the same FBO with 2 color attachments. Also glDrawBuffer
 *   and glReadBuffer is intercepted to make sure it is painted/read to/from
 *   the correct buffers. On swap our buffers are swapped and not the
 *   front/back buffer.
 * - If the guest request a depth/stencil buffer, a combined render buffer for
 *   this is created.
 * - If the size of the guest OpenGL window changes, all FBO's, textures, ...
 *   need to be recreated.
 * - We need to track any changes to the parent window
 *   (create/destroy/move/resize). The various classes like OverlayHelperView,
 *   OverlayWindow, ... are there for.
 * - The HGCM service runs on a other thread than the Main GUI. Keeps this
 *   always in mind (see e.g. performSelectorOnMainThread in renderFBOToView)
 * - We make heavy use of late binding. We can not be sure that the GUI (or any
 *   other third party GUI), overwrite our NSOpenGLContext. So we always ask if
 *   this is our own one, before use. Really neat concept of Objective-C/Cocoa
 *   ;)
 */

/* Debug macros */
#define FBO 1 /* Disable this to see how the output is without the FBO in the middle of the processing chain. */
#if 0
# define SHOW_WINDOW_BACKGROUND 1 /* Define this to see the window background even if the window is clipped */
# define DEBUG_VERBOSE /* Define this to get some debug info about the messages flow. */
#endif

#ifdef DEBUG_misha
# define DEBUG_MSG(text) \
    printf text
#else
# define DEBUG_MSG(text) \
    do {} while (0)
#endif

#ifdef DEBUG_VERBOSE
# define DEBUG_MSG_1(text) \
    DEBUG_MSG(text)
#else
# define DEBUG_MSG_1(text) \
    do {} while (0)
#endif

#ifdef DEBUG_poetzsch
# define CHECK_GL_ERROR()\
    do \
    { \
        checkGLError(__FILE__, __LINE__); \
    }while (0);

    static void checkGLError(char *file, int line)
    {
        GLenum g = glGetError();
        if (g != GL_NO_ERROR)
        {
            char *errStr;
            switch (g)
            {
                case GL_INVALID_ENUM:      errStr = RTStrDup("GL_INVALID_ENUM"); break;
                case GL_INVALID_VALUE:     errStr = RTStrDup("GL_INVALID_VALUE"); break;
                case GL_INVALID_OPERATION: errStr = RTStrDup("GL_INVALID_OPERATION"); break;
                case GL_STACK_OVERFLOW:    errStr = RTStrDup("GL_STACK_OVERFLOW"); break;
                case GL_STACK_UNDERFLOW:   errStr = RTStrDup("GL_STACK_UNDERFLOW"); break;
                case GL_OUT_OF_MEMORY:     errStr = RTStrDup("GL_OUT_OF_MEMORY"); break;
                case GL_TABLE_TOO_LARGE:   errStr = RTStrDup("GL_TABLE_TOO_LARGE"); break;
                default:                   errStr = RTStrDup("UNKNOWN"); break;
            }
            DEBUG_MSG(("%s:%d: glError %d (%s)\n", file, line, g, errStr));
            RTMemFree(errStr);
        }
    }
#else
# define CHECK_GL_ERROR()\
    do {} while (0)
#endif

#define GL_SAVE_STATE \
    do \
    { \
        glPushAttrib(GL_ALL_ATTRIB_BITS); \
        glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS); \
        glMatrixMode(GL_PROJECTION); \
        glPushMatrix(); \
        glMatrixMode(GL_TEXTURE); \
        glPushMatrix(); \
        glMatrixMode(GL_COLOR); \
        glPushMatrix(); \
        glMatrixMode(GL_MODELVIEW); \
        glPushMatrix(); \
    } \
    while(0);

#define GL_RESTORE_STATE \
    do \
    { \
        glMatrixMode(GL_MODELVIEW); \
        glPopMatrix(); \
        glMatrixMode(GL_COLOR); \
        glPopMatrix(); \
        glMatrixMode(GL_TEXTURE); \
        glPopMatrix(); \
        glMatrixMode(GL_PROJECTION); \
        glPopMatrix(); \
        glPopClientAttrib(); \
        glPopAttrib(); \
    } \
    while(0);
    
static NSOpenGLContext *g_pVBoxCurrentContext = NULL;
#ifdef DEBUG
static NSView *g_pVBoxCurrentView = NULL;
#endif

static NSOpenGLContext * vboxCtxGetCurrent()
{
#ifdef DEBUG
    if (g_pVBoxCurrentContext)
    {
    	Assert(g_pVBoxCurrentView == [g_pVBoxCurrentContext view]);
    }
#endif
    return g_pVBoxCurrentContext;
}

static void vboxCtxSetCurrent(NSOpenGLContext * pCtx)
{
	g_pVBoxCurrentContext = pCtx;
	if (pCtx)
	{
#ifdef DEBUG
		g_pVBoxCurrentView = [g_pVBoxCurrentContext view];
#endif
		[pCtx makeCurrentContext];
	}
	else
	{
#ifdef DEBUG
		g_pVBoxCurrentView = NULL;
#endif
		[NSOpenGLContext clearCurrentContext];
	}
}

/** Custom OpenGL context class.
 *
 * This implementation doesn't allow to set a view to the
 * context, but save the view for later use. Also it saves a copy of the
 * pixel format used to create that context for later use. */
@interface OverlayOpenGLContext: NSOpenGLContext
{
@private
    NSOpenGLPixelFormat *m_pPixelFormat;
    NSView              *m_pView;
}
- (NSOpenGLPixelFormat*)openGLPixelFormat;
@end

@class DockOverlayView;

/** The custom view class.
 * This is the main class of the cocoa OpenGL implementation. It
 * manages an frame buffer object for the rendering of the guest
 * applications. The guest applications render in this frame buffer which
 * is bind to an OpenGL texture. To display the guest content, an secondary
 * shared OpenGL context of the main OpenGL context is created. The secondary
 * context is marked as non opaque & the texture is displayed on an object
 * which is composed out of the several visible region rectangles. */
@interface OverlayView: NSView
{
@private
    NSView          *m_pParentView;
    NSWindow        *m_pOverlayWin;

    NSOpenGLContext *m_pGLCtx;
    NSOpenGLContext *m_pSharedGLCtx;
    RTTHREAD         mThread;

    GLuint           m_FBOId;

    /** The corresponding dock tile view of this OpenGL view & all helper
     * members. */
    DockOverlayView *m_DockTileView;

    GLfloat          m_FBOThumbScaleX;
    GLfloat          m_FBOThumbScaleY;
    uint64_t         m_uiDockUpdateTime;

    /* For clipping */
    GLint            m_cClipRects;
    GLint           *m_paClipRects;

    /* Position/Size tracking */
    NSPoint          m_Pos;
    NSSize           m_Size;

    /** This is necessary for clipping on the root window */
    NSPoint          m_RootShift;
    
    PVBOXVR_SCR_COMPOSITOR m_pCompositor;
    bool m_fNeedViewportUpdate;
    bool m_fNeedCtxUpdate;
}
- (id)initWithFrame:(NSRect)frame thread:(RTTHREAD)aThread parentView:(NSView*)pParentView;
- (void)setGLCtx:(NSOpenGLContext*)pCtx;
- (NSOpenGLContext*)glCtx;

- (void)setParentView: (NSView*)view;
- (NSView*)parentView;
- (void)setOverlayWin: (NSWindow*)win;
- (NSWindow*)overlayWin;

- (void)setPos:(NSPoint)pos;
- (NSPoint)pos;
- (void)setSize:(NSSize)size;
- (NSSize)size;
- (void)updateViewportCS;
- (void)reshape;

- (void)createDockTile;
- (void)deleteDockTile;

- (void)makeCurrentFBO;
- (void)swapFBO;
- (void)tryDraw;
- (void)vboxPresent;
- (void)vboxPresentCS;
- (void)vboxPresentToDockTileCS;
- (void)vboxPresentToViewCS;
- (void)presentComposition:(PVBOXVR_SCR_COMPOSITOR)pCompositor withChangedEntry:(PVBOXVR_SCR_COMPOSITOR_ENTRY)pChangedEntry;
- (void)performSelectorMakingCurrent:(NSOpenGLContext*)pCtx idSel:(SEL)selector;

- (void)clearVisibleRegions;
- (void)setVisibleRegions:(GLint)cRects paRects:(GLint*)paRects;

- (NSView*)dockTileScreen;
- (void)reshapeDockTile;
- (void)cleanupData;
@end

/** Helper view.
 *
 * This view is added as a sub view of the parent view to track
 * main window changes. Whenever the main window is changed
 * (which happens on fullscreen/seamless entry/exit) the overlay
 * window is informed & can add them self as a child window
 * again. */
@class OverlayWindow;
@interface OverlayHelperView: NSView
{
@private
    OverlayWindow *m_pOverlayWindow;
}
-(id)initWithOverlayWindow:(OverlayWindow*)pOverlayWindow;
@end

/** Custom window class.
 *
 * This is the overlay window which contains our custom NSView.
 * Its a direct child of the Qt Main window. It marks its background
 * transparent & non opaque to make clipping possible. It also disable mouse
 * events and handle frame change events of the parent view. */
@interface OverlayWindow: NSWindow
{
@private
    NSView            *m_pParentView;
    OverlayView       *m_pOverlayView;
    OverlayHelperView *m_pOverlayHelperView;
    NSThread          *m_Thread;
}
- (id)initWithParentView:(NSView*)pParentView overlayView:(OverlayView*)pOverlayView;
- (void)parentWindowFrameChanged:(NSNotification *)note;
- (void)parentWindowChanged:(NSWindow*)pWindow;
@end

@interface DockOverlayView: NSView
{
    NSBitmapImageRep *m_ThumbBitmap;
    NSImage          *m_ThumbImage;
    NSLock           *m_Lock;
}
- (void)dealloc;
- (void)cleanup;
- (void)lock;
- (void)unlock;
- (void)setFrame:(NSRect)frame;
- (void)drawRect:(NSRect)aRect;
- (NSBitmapImageRep*)thumbBitmap;
- (NSImage*)thumbImage;
@end

@implementation DockOverlayView
- (id)init
{
    self = [super init];

    if (self)
    {
        /* We need a lock cause the thumb image could be accessed from the main
         * thread when someone is calling display on the dock tile & from the
         * OpenGL thread when the thumbnail is updated. */
        m_Lock = [[NSLock alloc] init];
    }

    return self;
}

- (void)dealloc
{
    [self cleanup];
    [m_Lock release];

    [super dealloc];
}

- (void)cleanup
{
    if (m_ThumbImage != nil)
    {
        [m_ThumbImage release];
        m_ThumbImage = nil;
    }
    if (m_ThumbBitmap != nil)
    {
        [m_ThumbBitmap release];
        m_ThumbBitmap = nil;
    }
}

- (void)lock
{
    [m_Lock lock];
}

- (void)unlock
{
    [m_Lock unlock];
}

- (void)setFrame:(NSRect)frame
{
    [super setFrame:frame];

    [self lock];
    [self cleanup];

    if (   frame.size.width > 0
        && frame.size.height > 0)
    {
        /* Create a buffer for our thumbnail image. Its in the size of this view. */
        m_ThumbBitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
            pixelsWide:frame.size.width
            pixelsHigh:frame.size.height
            bitsPerSample:8
            samplesPerPixel:4
            hasAlpha:YES
            isPlanar:NO
            colorSpaceName:NSDeviceRGBColorSpace
            bitmapFormat:NSAlphaFirstBitmapFormat
            bytesPerRow:frame.size.width * 4
            bitsPerPixel:8 * 4];
        m_ThumbImage = [[NSImage alloc] initWithSize:[m_ThumbBitmap size]];
        [m_ThumbImage addRepresentation:m_ThumbBitmap];
    }
    [self unlock];
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)drawRect:(NSRect)aRect
{
    NSRect frame;

    [self lock];
#ifdef SHOW_WINDOW_BACKGROUND
    [[NSColor colorWithCalibratedRed:1.0 green:0.0 blue:0.0 alpha:0.7] set];
    frame = [self frame];
    [NSBezierPath fillRect:NSMakeRect(0, 0, frame.size.width, frame.size.height)];
#endif /* SHOW_WINDOW_BACKGROUND */
    if (m_ThumbImage != nil)
        [m_ThumbImage drawAtPoint:NSMakePoint(0, 0) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];
    [self unlock];
}

- (NSBitmapImageRep*)thumbBitmap
{
    return m_ThumbBitmap;
}

- (NSImage*)thumbImage
{
    return m_ThumbImage;
}
@end

/********************************************************************************
*
* OverlayOpenGLContext class implementation
*
********************************************************************************/
@implementation OverlayOpenGLContext

-(id)initWithFormat:(NSOpenGLPixelFormat*)format shareContext:(NSOpenGLContext*)share
{
    m_pPixelFormat = NULL;
    m_pView = NULL;

    self = [super initWithFormat:format shareContext:share];
    if (self)
        m_pPixelFormat = format;

    DEBUG_MSG(("OCTX(%p): init OverlayOpenGLContext\n", (void*)self));

    return self;
}

- (void)dealloc
{
    DEBUG_MSG(("OCTX(%p): dealloc OverlayOpenGLContext\n", (void*)self));

    [m_pPixelFormat release];

    [super dealloc];
}

-(bool)isDoubleBuffer
{
    GLint val;
    [m_pPixelFormat getValues:&val forAttribute:NSOpenGLPFADoubleBuffer forVirtualScreen:0];
    return val == GL_TRUE ? YES : NO;
}

-(void)setView:(NSView*)view
{
    DEBUG_MSG(("OCTX(%p): setView: new view: %p\n", (void*)self, (void*)view));

#if 1 /* def FBO */
    m_pView = view;;
#else
    [super setView: view];
#endif
}

-(NSView*)view
{
#if 1 /* def FBO */
    return m_pView;
#else
    return [super view];
#endif
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

/********************************************************************************
*
* OverlayHelperView class implementation
*
********************************************************************************/
@implementation OverlayHelperView

-(id)initWithOverlayWindow:(OverlayWindow*)pOverlayWindow
{
    self = [super initWithFrame:NSZeroRect];

    m_pOverlayWindow = pOverlayWindow;

    DEBUG_MSG(("OHVW(%p): init OverlayHelperView\n", (void*)self));

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
* OverlayWindow class implementation
*
********************************************************************************/
@implementation OverlayWindow

- (id)initWithParentView:(NSView*)pParentView overlayView:(OverlayView*)pOverlayView
{
    NSWindow *pParentWin = nil;

    if((self = [super initWithContentRect:NSZeroRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]))
    {
        m_pParentView = pParentView;
        m_pOverlayView = pOverlayView;
        m_Thread = [NSThread currentThread];

        [m_pOverlayView setOverlayWin: self];

        m_pOverlayHelperView = [[OverlayHelperView alloc] initWithOverlayWindow:self];
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

    /* Reposition this window with the help of the OverlayView. Perform the
     * call in the OpenGL thread. */
    /*
    [m_pOverlayView performSelector:@selector(reshape) onThread:m_Thread withObject:nil waitUntilDone:YES];
    */

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
        /* Reshape the overlay view after a short waiting time to let the main
         * window resize itself properly. */
        /*
        [m_pOverlayView performSelector:@selector(reshape) withObject:nil afterDelay:0.2];
        [NSTimer scheduledTimerWithTimeInterval:0.2 target:m_pOverlayView selector:@selector(reshape) userInfo:nil repeats:NO];
        */
        [m_pOverlayView reshape];
    }
}

@end

/********************************************************************************
*
* OverlayView class implementation
*
********************************************************************************/
@implementation OverlayView

- (id)initWithFrame:(NSRect)frame thread:(RTTHREAD)aThread parentView:(NSView*)pParentView
{
    m_pParentView             = pParentView;
    /* Make some reasonable defaults */
    m_pGLCtx                  = nil;
    m_pSharedGLCtx            = nil;
    mThread                   = aThread;
    m_FBOId                   = 0;
    m_cClipRects              = 0;
    m_paClipRects             = NULL;
    m_Pos                     = NSZeroPoint;
    m_Size                    = NSMakeSize(1, 1);
    m_RootShift               = NSZeroPoint;
    m_pCompositor             = NULL;
    m_fNeedViewportUpdate     = true;        
    m_fNeedCtxUpdate          = true;
    
    self = [super initWithFrame:frame];

    DEBUG_MSG(("OVIW(%p): init OverlayView\n", (void*)self));

    return self;
}

- (void)cleanupData
{
    [self deleteDockTile];
    
    if (m_pGLCtx)
    {
        if ([m_pGLCtx view] == self)
            [m_pGLCtx clearDrawable];

        m_pGLCtx = nil;
    }
    if (m_pSharedGLCtx)
    {
        if ([m_pSharedGLCtx view] == self)
            [m_pSharedGLCtx clearDrawable];

        [m_pSharedGLCtx release];

        m_pSharedGLCtx = nil;
    }

    [self clearVisibleRegions];
}

- (void)dealloc
{
    DEBUG_MSG(("OVIW(%p): dealloc OverlayView\n", (void*)self));

    [self cleanupData];

    [super dealloc];
}

- (void)drawRect:(NSRect)aRect
{
    /* Do nothing */
}

- (void)setGLCtx:(NSOpenGLContext*)pCtx
{
    DEBUG_MSG(("OVIW(%p): setGLCtx: new ctx: %p\n", (void*)self, (void*)pCtx));
    if (m_pGLCtx == pCtx)
        return;

    /* ensure the context drawable is cleared to avoid holding a reference to inexistent view */
    if (m_pGLCtx)
        [m_pGLCtx clearDrawable];

    m_pGLCtx = pCtx;
}

- (NSOpenGLContext*)glCtx
{
    return m_pGLCtx;
}

- (NSView*)parentView
{
    return m_pParentView;
}

- (void)setParentView:(NSView*)pView
{
    DEBUG_MSG(("OVIW(%p): setParentView: new view: %p\n", (void*)self, (void*)pView));

    m_pParentView = pView;
}

- (void)setOverlayWin:(NSWindow*)pWin
{
    DEBUG_MSG(("OVIW(%p): setOverlayWin: new win: %p\n", (void*)self, (void*)pWin));

    m_pOverlayWin = pWin;
}

- (NSWindow*)overlayWin
{
    return m_pOverlayWin;
}

- (void)setPos:(NSPoint)pos
{
    DEBUG_MSG(("OVIW(%p): setPos: new pos: %d, %d\n", (void*)self, (int)pos.x, (int)pos.y));

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

    {
        DEBUG_MSG(("OVIW(%p): setSize: new size: %dx%d\n", (void*)self, (int)size.width, (int)size.height));
        [self reshape];
        [self createDockTile];
        /* have to rebind GL_TEXTURE_RECTANGLE_ARB as m_FBOTexId could be changed in updateFBO call */
        m_fNeedViewportUpdate = true;
        m_fNeedCtxUpdate = true;
    }
}

- (NSSize)size
{
    return m_Size;
}

- (void)updateViewportCS
{
    NSRect r;

    DEBUG_MSG(("OVIW(%p): updateViewport\n", (void*)self));

    {
        /* Update the viewport for our OpenGL view */
        [m_pSharedGLCtx update];

        r = [self frame];
        /* Setup all matrices */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glViewport(0, 0, r.size.width, r.size.height);
        glOrtho(0, r.size.width, 0, r.size.height, -1, 1);
        DEBUG_MSG_1(("OVIW(%p): frame[%i, %i, %i, %i]\n", (void*)self, (int)r.origin.x, (int)r.origin.x, (int)r.size.width, (int)r.size.height));
        DEBUG_MSG_1(("OVIW(%p): m_Pos(%i,%i) m_Size(%i,%i)\n", (void*)self, (int)m_Pos.x, (int)m_Pos.y, (int)m_Size.width, (int)m_Size.height));
        DEBUG_MSG_1(("OVIW(%p): m_RootShift(%i, %i)\n", (void*)self, (int)m_RootShift.x, (int)m_RootShift.y));
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        /* Clear background to transparent */
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    }
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

    /* Later we have to correct the texture position in the case the window is
     * out of the parents window frame. So save the shift values for later use. */
    if (parentFrame.origin.x > childFrame.origin.x)
        m_RootShift.x = parentFrame.origin.x - childFrame.origin.x;
    else
        m_RootShift.x = 0;
    if (parentFrame.origin.y > childFrame.origin.y)
        m_RootShift.y = parentFrame.origin.y - childFrame.origin.y;
    else
        m_RootShift.y = 0;

    /*
    NSScrollView *pScrollView = [[[m_pParentView window] contentView] enclosingScrollView];
    if (pScrollView)
    {
        NSRect scrollRect = [pScrollView documentVisibleRect];
        NSRect scrollRect = [m_pParentView visibleRect];
        printf ("sc rect: %d %d %d %d\n", (int) scrollRect.origin.x,(int) scrollRect.origin.y,(int) scrollRect.size.width,(int) scrollRect.size.height);
        NSRect b = [[m_pParentView superview] bounds];
        printf ("bound rect: %d %d %d %d\n", (int) b.origin.x,(int) b.origin.y,(int) b.size.width,(int) b.size.height);
        newFrame.origin.x += scrollRect.origin.x;
        newFrame.origin.y += scrollRect.origin.y;
    }
    */

    /* Set the new frame. */
    [[self window] setFrame:newFrame display:YES];

    /* Inform the dock tile view as well */
    [self reshapeDockTile];

    /* Make sure the context is updated according */
    /* [self updateViewport]; */
    if (m_pSharedGLCtx)
    {
    	[self performSelectorMakingCurrent:m_pSharedGLCtx idSel:@selector(updateViewportCS)];
    }
}

- (void)createDockTile
{
	NSView *pDockScreen      = nil;
	[self deleteDockTile];
	
	/* Is there a dock tile preview enabled in the GUI? If so setup a
     * additional thumbnail view for the dock tile. */
	pDockScreen = [self dockTileScreen];
	if (pDockScreen)
    {
        m_DockTileView = [[DockOverlayView alloc] init];
        [self reshapeDockTile];
        [pDockScreen addSubview:m_DockTileView];
    }
}

- (void)deleteDockTile
{
	if (m_DockTileView != nil)
    {
        [m_DockTileView removeFromSuperview];
        [m_DockTileView release];
        m_DockTileView = nil;
    }
}

- (void)makeCurrentFBO
{
    DEBUG_MSG(("OVIW(%p): makeCurrentFBO\n", (void*)self));

    if (m_pGLCtx)
    {
        if ([m_pGLCtx view] != self)
        {
            /* We change the active view, so flush first */
            if([NSOpenGLContext currentContext] != 0)
                glFlush();
            [m_pGLCtx setView: self];
            CHECK_GL_ERROR();
        }
        /*
        if ([NSOpenGLContext currentContext] != m_pGLCtx)
        */
        {
            vboxCtxSetCurrent(m_pGLCtx);
            CHECK_GL_ERROR();
            if (m_fNeedCtxUpdate == true)
            {
                [m_pGLCtx update];
                m_fNeedCtxUpdate = false;
            }
        }
        
        if (!m_FBOId)
        {
            glGenFramebuffersEXT(1, &m_FBOId);
            Assert(m_FBOId);
        }
        
    }
}

- (void)tryDraw
{
    if ([self lockFocusIfCanDraw])
    {
        [self vboxPresent];
        [self unlockFocus];
    }
}

- (void)swapFBO
{
    [m_pGLCtx flushBuffer];
}

- (void)vboxPresent
{
    GLint opaque       = 0;

    DEBUG_MSG(("OVIW(%p): renderFBOToView\n", (void*)self));
    
    if (!m_pCompositor)
        return;

#ifdef DEBUG
            {
            	NSOpenGLContext *pTstOldCtx = [NSOpenGLContext currentContext];
            	NSView *pTstOldView = (pTstOldCtx ? [pTstOldCtx view] : nil);
            	Assert(pTstOldCtx == m_pGLCtx);
            	Assert(pTstOldView == self);
            }
#endif

    if (!m_pSharedGLCtx)
    {
        /* Create a shared context out of the main context. Use the same pixel format. */
        m_pSharedGLCtx = [[NSOpenGLContext alloc] initWithFormat:[(OverlayOpenGLContext*)m_pGLCtx openGLPixelFormat] shareContext:m_pGLCtx];

        /* Set the new context as non opaque */
        [m_pSharedGLCtx setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];
        /* Set this view as the drawable for the new context */
        [m_pSharedGLCtx setView: self];
        m_fNeedViewportUpdate = true;
    }
    
#ifdef DEBUG
            {
            	NSOpenGLContext *pTstOldCtx = [NSOpenGLContext currentContext];
            	NSView *pTstOldView = (pTstOldCtx ? [pTstOldCtx view] : nil);
            	Assert(pTstOldCtx == m_pGLCtx);
            	Assert(pTstOldView == self);
            }
#endif
    
    [self performSelectorMakingCurrent:m_pSharedGLCtx idSel:@selector(vboxPresentCS)];
}

- (void)vboxPresentCS
{
        NSRect r = [self frame];
        DEBUG_MSG(("OVIW(%p): rF2V frame: [%i, %i, %i, %i]\n", (void*)self, (int)r.origin.x, (int)r.origin.y, (int)r.size.width, (int)r.size.height));

        {
            if ([m_pSharedGLCtx view] != self)
            {
                DEBUG_MSG(("OVIW(%p): not current view of shared ctx! Switching ...\n", (void*)self));
                [m_pSharedGLCtx setView: self];
                m_fNeedViewportUpdate = true;
            }
            
            if (m_fNeedViewportUpdate)
            {
                [self updateViewportCS];
                m_fNeedViewportUpdate = false;
            }
            
            /* Render FBO content to the dock tile when necessary. */
            [self vboxPresentToDockTileCS];
            
            [self vboxPresentToViewCS];
        }
}

- (void)vboxPresentToViewCS
{
#if 1 /* Set to 0 to see the docktile instead of the real output */
    VBOXVR_SCR_COMPOSITOR_ITERATOR CIter;
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry;
        
    CrVrScrCompositorIterInit(m_pCompositor, &CIter);
        
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
    glDrawBuffer(GL_BACK);

    /* Clear background to transparent */
    glClear(GL_COLOR_BUFFER_BIT);
        
    while ((pEntry = CrVrScrCompositorIterNext(&CIter)) != NULL)
    {
        uint32_t cRegions;
        const RTRECT *paSrcRegions, *paDstRegions;
        int rc = CrVrScrCompositorEntryRegionsGet(m_pCompositor, pEntry, &cRegions, &paSrcRegions, &paDstRegions);
        if (RT_SUCCESS(rc))
        {
            uint32_t i;
                    
            glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_FBOId);
            glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pEntry->Tex.target, pEntry->Tex.hwid, 0);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
                    
            for (i = 0; i < pEntry->cRects; ++i)
            {
                const RTRECT * pSrcRect = &paSrcRegions[i];
                const RTRECT * pDstRect = &paDstRegions[i];
                       
                glBlitFramebufferEXT(pSrcRect->xLeft, pSrcRect->yTop, pSrcRect->xRight, pSrcRect->yBottom,
                                            pDstRect->xLeft, pDstRect->yTop, pDstRect->xRight, pDstRect->yBottom,
                                            GL_COLOR_BUFFER_BIT, GL_LINEAR);
            }
        }
        else
        {
            Assert(0);
            DEBUG_MSG_1(("BlitStretched: CrVrScrCompositorEntryRegionsGet failed rc %d\n", rc));
        }
    }
#endif
            /*
            glFinish();
            */
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            [m_pSharedGLCtx flushBuffer];
}

- (void)performSelectorMakingCurrent:(NSOpenGLContext*)pCtx idSel:(SEL)selector
{
    NSOpenGLContext *pOldCtx = vboxCtxGetCurrent();
    NSView *pOldView = (pOldCtx ? [pOldCtx view] : nil);
    NSView *pView = [pCtx view];
    bool fNeedCtxSwitch = (pOldCtx != pCtx || pOldView != pView);
    Assert(pCtx);
    Assert(pOldCtx == m_pGLCtx);
    Assert(pOldView == self);
    Assert(fNeedCtxSwitch);
    if (fNeedCtxSwitch)
    {
        if(pOldCtx != nil)
            glFlush();
        
        vboxCtxSetCurrent(pCtx);
    }
    
    [self performSelector:selector];
    
    if (fNeedCtxSwitch)
    {
        glFlush();
        if (pOldCtx != nil)
        {
            if ([pOldCtx view] != pOldView)
            {
                [pOldCtx setView: pOldView];
            }
        
        	vboxCtxSetCurrent(pOldCtx);
            
#ifdef DEBUG
            {
            	NSOpenGLContext *pTstOldCtx = [NSOpenGLContext currentContext];
            	NSView *pTstOldView = (pTstOldCtx ? [pTstOldCtx view] : nil);
            	Assert(pTstOldCtx == pOldCtx);
            	Assert(pTstOldView == pOldView);
            }
#endif
        }
        else
        {
            [NSOpenGLContext clearCurrentContext];
        }
    }
}

- (void)presentComposition:(PVBOXVR_SCR_COMPOSITOR)pCompositor withChangedEntry:(PVBOXVR_SCR_COMPOSITOR_ENTRY)pChangedEntry
{
    m_pCompositor = pCompositor;
    
    [self tryDraw];
}

- (void)vboxPresentToDockTileCS
{
    NSRect r        = [self frame];
    NSRect rr       = NSZeroRect;
    GLint i         = 0;
    NSDockTile *pDT = nil;

    if ([m_DockTileView thumbBitmap] != nil)
    {
        /* Only update after at least 200 ms, cause glReadPixels is
         * heavy performance wise. */
        uint64_t uiNewTime = RTTimeMilliTS();
        VBOXVR_SCR_COMPOSITOR_ITERATOR CIter;
        PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry;
        
        if (uiNewTime - m_uiDockUpdateTime > 200)
        {
            m_uiDockUpdateTime = uiNewTime;
#if 0
            /* todo: check this for optimization */
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, myTextureName);
            glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE,
                            GL_STORAGE_SHARED_APPLE);
            glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
            glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA,
                         sizex, sizey, 0, GL_BGRA,
                         GL_UNSIGNED_INT_8_8_8_8_REV, myImagePtr);
            glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,
                                0, 0, 0, 0, 0, image_width, image_height);
            glFlush();
            /* Do other work processing here, using a double or triple buffer */
            glGetTexImage(GL_TEXTURE_RECTANGLE_ARB, 0, GL_BGRA,
                          GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
#endif
            glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
            glDrawBuffer(GL_BACK);

            /* Clear background to transparent */
            glClear(GL_COLOR_BUFFER_BIT);

            rr = [m_DockTileView frame];
            
            CrVrScrCompositorIterInit(m_pCompositor, &CIter);
            while ((pEntry = CrVrScrCompositorIterNext(&CIter)) != NULL)
            {
                uint32_t cRegions;
                const RTRECT *paSrcRegions, *paDstRegions;
                int rc = CrVrScrCompositorEntryRegionsGet(m_pCompositor, pEntry, &cRegions, &paSrcRegions, &paDstRegions);
                if (RT_SUCCESS(rc))
                {
                    uint32_t i;
                    
                    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_FBOId);
                    glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pEntry->Tex.target, pEntry->Tex.hwid, 0);
                    glReadBuffer(GL_COLOR_ATTACHMENT0);
                    
                    for (i = 0; i < pEntry->cRects; ++i)
                    {
                        const RTRECT * pSrcRect = &paSrcRegions[i];
                        const RTRECT * pDstRect = &paDstRegions[i];
                       
                        glBlitFramebufferEXT(pSrcRect->xLeft, pSrcRect->yTop, pSrcRect->xRight, pSrcRect->yBottom,
                                            pDstRect->xLeft * m_FBOThumbScaleX, pDstRect->yTop * m_FBOThumbScaleY, pDstRect->xRight * m_FBOThumbScaleX, pDstRect->yBottom * m_FBOThumbScaleY,
                                            GL_COLOR_BUFFER_BIT, GL_LINEAR);
                    }
                }
                else
                {
                    Assert(0);
                    DEBUG_MSG_1(("BlitStretched: CrVrScrCompositorEntryRegionsGet failed rc %d\n", rc));
                }
            }
            
            glFinish();

            glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
            glReadBuffer(GL_BACK);
            /* Here the magic of reading the FBO content in our own buffer
             * happens. We have to lock this access, in the case the dock
             * is updated currently. */
            [m_DockTileView lock];
            glReadPixels(0, 0, rr.size.width, rr.size.height,
                         GL_BGRA,
                         GL_UNSIGNED_INT_8_8_8_8,
                         [[m_DockTileView thumbBitmap] bitmapData]);
            [m_DockTileView unlock];

            pDT = [[NSApplication sharedApplication] dockTile];

            /* Send a display message to the dock tile in the main thread */
            [[[NSApplication sharedApplication] dockTile] performSelectorOnMainThread:@selector(display) withObject:nil waitUntilDone:NO];
        }
    }
}

- (void)clearVisibleRegions
{
    if(m_paClipRects)
    {
        RTMemFree(m_paClipRects);
        m_paClipRects = NULL;
    }
    m_cClipRects = 0;
}

- (void)setVisibleRegions:(GLint)cRects paRects:(GLint*)paRects
{
    GLint cOldRects = m_cClipRects;

    DEBUG_MSG_1(("OVIW(%p): setVisibleRegions: cRects=%d\n", (void*)self, cRects));

    [self clearVisibleRegions];

    if (cRects > 0)
    {
#ifdef DEBUG_poetzsch
        int i =0;
        for (i = 0; i < cRects; ++i)
            DEBUG_MSG_1(("OVIW(%p): setVisibleRegions: %d - %d %d %d %d\n", (void*)self, i, paRects[i * 4], paRects[i * 4 + 1], paRects[i * 4 + 2], paRects[i * 4 + 3]));
#endif

        m_paClipRects = (GLint*)RTMemAlloc(sizeof(GLint) * 4 * cRects);
        m_cClipRects = cRects;
        memcpy(m_paClipRects, paRects, sizeof(GLint) * 4 * cRects);
    }
#if 0
    /* todo: handle pending m_fNeedCtxUpdate */
    else if (cOldRects)
        [self tryDraw];
#endif
}

- (NSView*)dockTileScreen
{
    NSView *contentView = [[[NSApplication sharedApplication] dockTile] contentView];
    NSView *screenContent = nil;
    /* First try the new variant which checks if this window is within the
       screen which is previewed in the dock. */
    if ([contentView respondsToSelector:@selector(screenContentWithParentView:)])
         screenContent = [contentView performSelector:@selector(screenContentWithParentView:) withObject:(id)m_pParentView];
    /* If it fails, fall back to the old variant (VBox...) */
    else if ([contentView respondsToSelector:@selector(screenContent)])
         screenContent = [contentView performSelector:@selector(screenContent)];
    return screenContent;
}

- (void)reshapeDockTile
{
    NSRect newFrame = NSZeroRect;

    NSView *pView = [self dockTileScreen];
    if (pView != nil)
    {
        NSRect dockFrame = [pView frame];
        NSRect parentFrame = [m_pParentView frame];

        m_FBOThumbScaleX = (float)dockFrame.size.width / parentFrame.size.width;
        m_FBOThumbScaleY = (float)dockFrame.size.height / parentFrame.size.height;
        newFrame = NSMakeRect((int)(m_Pos.x * m_FBOThumbScaleX), (int)(dockFrame.size.height - (m_Pos.y + m_Size.height - m_RootShift.y) * m_FBOThumbScaleY), (int)(m_Size.width * m_FBOThumbScaleX), (int)(m_Size.height * m_FBOThumbScaleY));
        /*
        NSRect newFrame = NSMakeRect ((int)roundf(m_Pos.x * m_FBOThumbScaleX), (int)roundf(dockFrame.size.height - (m_Pos.y + m_Size.height) * m_FBOThumbScaleY), (int)roundf(m_Size.width * m_FBOThumbScaleX), (int)roundf(m_Size.height * m_FBOThumbScaleY));
        NSRect newFrame = NSMakeRect ((m_Pos.x * m_FBOThumbScaleX), (dockFrame.size.height - (m_Pos.y + m_Size.height) * m_FBOThumbScaleY), (m_Size.width * m_FBOThumbScaleX), (m_Size.height * m_FBOThumbScaleY));
        printf ("%f %f %f %f - %f %f\n", newFrame.origin.x, newFrame.origin.y, newFrame.size.width, newFrame.size.height, m_Size.height, m_FBOThumbScaleY);
        */
        [m_DockTileView setFrame: newFrame];
    }
}

@end

/********************************************************************************
*
* OpenGL context management
*
********************************************************************************/
void cocoaGLCtxCreate(NativeNSOpenGLContextRef *ppCtx, GLbitfield fVisParams, NativeNSOpenGLContextRef pSharedCtx)
{
    NSOpenGLPixelFormat *pFmt = nil;

    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    NSOpenGLPixelFormatAttribute attribs[24] =
    {
        NSOpenGLPFAWindow,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24
    };

    int i = 4;

    if (fVisParams & CR_ALPHA_BIT)
    {
        DEBUG_MSG(("CR_ALPHA_BIT requested\n"));
        attribs[i++] = NSOpenGLPFAAlphaSize;
        attribs[i++] = 8;
    }
    if (fVisParams & CR_DEPTH_BIT)
    {
        DEBUG_MSG(("CR_DEPTH_BIT requested\n"));
        attribs[i++] = NSOpenGLPFADepthSize;
        attribs[i++] = 24;
    }
    if (fVisParams & CR_STENCIL_BIT)
    {
        DEBUG_MSG(("CR_STENCIL_BIT requested\n"));
        attribs[i++] = NSOpenGLPFAStencilSize;
        attribs[i++] = 8;
    }
    if (fVisParams & CR_ACCUM_BIT)
    {
        DEBUG_MSG(("CR_ACCUM_BIT requested\n"));
        attribs[i++] = NSOpenGLPFAAccumSize;
        if (fVisParams & CR_ALPHA_BIT)
            attribs[i++] = 32;
        else
            attribs[i++] = 24;
    }
    if (fVisParams & CR_MULTISAMPLE_BIT)
    {
        DEBUG_MSG(("CR_MULTISAMPLE_BIT requested\n"));
        attribs[i++] = NSOpenGLPFASampleBuffers;
        attribs[i++] = 1;
        attribs[i++] = NSOpenGLPFASamples;
        attribs[i++] = 4;
    }
    if (fVisParams & CR_DOUBLE_BIT)
    {
        DEBUG_MSG(("CR_DOUBLE_BIT requested\n"));
        attribs[i++] = NSOpenGLPFADoubleBuffer;
    }
    if (fVisParams & CR_STEREO_BIT)
    {
        /* We don't support that.
        DEBUG_MSG(("CR_STEREO_BIT requested\n"));
        attribs[i++] = NSOpenGLPFAStereo;
        */
    }

    /* Mark the end */
    attribs[i++] = 0;

    /* Choose a pixel format */
    pFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];

    if (pFmt)
    {
        *ppCtx = [[OverlayOpenGLContext alloc] initWithFormat:pFmt shareContext:pSharedCtx];

        /* Enable multi threaded OpenGL engine */
        /*
        CGLContextObj cglCtx = [*ppCtx CGLContextObj];
        CGLError err = CGLEnable(cglCtx, kCGLCEMPEngine);
        if (err != kCGLNoError)
            printf ("Couldn't enable MT OpenGL engine!\n");
        */

        DEBUG_MSG(("New context %X\n", (uint)*ppCtx));
    }

    [pPool release];
}

void cocoaGLCtxDestroy(NativeNSOpenGLContextRef pCtx)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    /*
    [pCtx release];
    */

    [pPool release];
}

/********************************************************************************
*
* View management
*
********************************************************************************/
void cocoaViewCreate(NativeNSViewRef *ppView, NativeNSViewRef pParentView, GLbitfield fVisParams)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    /* Create our worker view */
    OverlayView* pView = [[OverlayView alloc] initWithFrame:NSZeroRect thread:RTThreadSelf() parentView:pParentView];

    if (pView)
    {
        /* We need a real window as container for the view */
        [[OverlayWindow alloc] initWithParentView:pParentView overlayView:pView];
        /* Return the freshly created overlay view */
        *ppView = pView;
    }

    [pPool release];
}

void cocoaViewReparent(NativeNSViewRef pView, NativeNSViewRef pParentView)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    OverlayView* pOView = (OverlayView*)pView;

    if (pOView)
    {
        /* Make sure the window is removed from any previous parent window. */
        if ([[pOView overlayWin] parentWindow] != nil)
        {
            [[[pOView overlayWin] parentWindow] removeChildWindow:[pOView overlayWin]];
        }

        /* Set the new parent view */
        [pOView setParentView: pParentView];

        /* Add the overlay window as a child to the new parent window */
        if (pParentView != nil)
        {
            [[pParentView window] addChildWindow:[pOView overlayWin] ordered:NSWindowAbove];
            [pOView createDockTile];
        }
    }

    [pPool release];
}

void cocoaViewDestroy(NativeNSViewRef pView)
{
    NSWindow *pWin = nil;

    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    /* Hide the view early */
    [pView setHidden: YES];

    pWin = [pView window];
    [[NSNotificationCenter defaultCenter] removeObserver:pWin];
    [pWin setContentView: nil];
    [[pWin parentWindow] removeChildWindow: pWin];
    
    /*
    a = [pWin retainCount];
    for (; a > 1; --a)
        [pWin performSelector:@selector(release)]
    */
    /* We can NOT run synchronously with the main thread since this may lead to a deadlock,
       caused by main thread waiting xpcom thread, xpcom thread waiting to main hgcm thread,
       and main hgcm thread waiting for us, this is why use waitUntilDone:NO, 
       which should cause no harm */ 
    [pWin performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    /*
    [pWin release];
    */

    /* We can NOT run synchronously with the main thread since this may lead to a deadlock,
       caused by main thread waiting xpcom thread, xpcom thread waiting to main hgcm thread,
       and main hgcm thread waiting for us, this is why use waitUntilDone:NO. 
       We need to avoid concurrency though, so we cleanup some data right away via a cleanupData call */
    [(OverlayView*)pView cleanupData];

    /* There seems to be a bug in the performSelector method which is called in
     * parentWindowChanged above. The object is retained but not released. This
     * results in an unbalanced reference count, which is here manually
     * decremented. */
    /*
    a = [pView retainCount];
    for (; a > 1; --a)
    */
    [pView performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    /*
    [pView release];
    */

    [pPool release];
}

void cocoaViewShow(NativeNSViewRef pView, GLboolean fShowIt)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [pView setHidden: fShowIt==GL_TRUE?NO:YES];

    [pPool release];
}

void cocoaViewDisplay(NativeNSViewRef pView)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    DEBUG_MSG_1(("cocoaViewDisplay %p\n", (void*)pView));
    [(OverlayView*)pView swapFBO];

    [pPool release];

}

void cocoaViewSetPosition(NativeNSViewRef pView, NativeNSViewRef pParentView, int x, int y)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView setPos:NSMakePoint(x, y)];

    [pPool release];
}

void cocoaViewSetSize(NativeNSViewRef pView, int w, int h)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView setSize:NSMakeSize(w, h)];

    [pPool release];
}

void cocoaViewGetGeometry(NativeNSViewRef pView, int *pX, int *pY, int *pW, int *pH)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    NSRect frame = [[pView window] frame];
    *pX = frame.origin.x;
    *pY = frame.origin.y;
    *pW = frame.size.width;
    *pH = frame.size.height;

    [pPool release];
}

void cocoaViewPresentComposition(NativeNSViewRef pView, struct VBOXVR_SCR_COMPOSITOR * pCompositor, struct VBOXVR_SCR_COMPOSITOR_ENTRY *pChangedEntry)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView presentComposition:pCompositor withChangedEntry:pChangedEntry];

    [pPool release];
}

void cocoaViewMakeCurrentContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    DEBUG_MSG(("cocoaViewMakeCurrentContext(%p, %p)\n", (void*)pView, (void*)pCtx));

    [(OverlayView*)pView setGLCtx:pCtx];
    [(OverlayView*)pView makeCurrentFBO];

    [pPool release];
}

void cocoaViewSetVisibleRegion(NativeNSViewRef pView, GLint cRects, GLint* paRects)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView setVisibleRegions:cRects paRects:paRects];

    [pPool release];
}
