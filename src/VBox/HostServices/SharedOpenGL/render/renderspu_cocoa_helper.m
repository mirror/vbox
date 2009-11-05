/** @file
 *
 * VirtualBox OpenGL Cocoa Window System Helper implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "renderspu_cocoa_helper.h"

#include "chromium.h" /* For the visual bits of chromium */

#include <iprt/thread.h>
#include <iprt/string.h>

/* Debug macros */
#define FBO 1 /* Disable this to see how the output is without the FBO in the middle of the processing chain. */
//#define SHOW_WINDOW_BACKGROUND 1 /* Define this to see the window background even if the window is clipped */
//#define DEBUG_VERBOSE /* Define this could get some debug info about the messages flow. */

#ifdef DEBUG_poetzsch
#define DEBUG_MSG(text) \
    printf text
#else
#define DEBUG_MSG(text) \
    do {} while (0)
#endif

#ifdef DEBUG_VERBOSE
#define DEBUG_MSG_1(text) \
    DEBUG_MSG(text)
#else
#define DEBUG_MSG_1(text) \
    do {} while (0)
#endif

#ifdef DEBUG_poetzsch
#define CHECK_GL_ERROR()\
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
                case GL_INVALID_ENUM: errStr = RTStrDup("GL_INVALID_ENUM"); break;
                case GL_INVALID_VALUE: errStr = RTStrDup("GL_INVALID_VALUE"); break;
                case GL_INVALID_OPERATION: errStr = RTStrDup("GL_INVALID_OPERATION"); break;
                case GL_STACK_OVERFLOW: errStr = RTStrDup("GL_STACK_OVERFLOW"); break;
                case GL_STACK_UNDERFLOW: errStr = RTStrDup("GL_STACK_UNDERFLOW"); break;
                case GL_OUT_OF_MEMORY: errStr = RTStrDup("GL_OUT_OF_MEMORY"); break;
                case GL_TABLE_TOO_LARGE: errStr = RTStrDup("GL_TABLE_TOO_LARGE"); break;
                default: errStr = RTStrDup("UNKOWN"); break;
            }
            DEBUG_MSG(("%s:%d: glError %d (%s)\n", file, line, g, errStr));
            RTMemFree(errStr);
        }
    }
#else
#define CHECK_GL_ERROR()\
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

/* Custom OpenGL context class. This implementation doesn't allow to set a view
 * to the context, but save the view for later use. Also it saves a copy of the
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

/* The custom view class. This is the main class of the cocoa OpenGL
 * implementation. It manages an frame buffer object for the rendering of the
 * guest applications. The guest applications render in this frame buffer which
 * is bind to an OpenGL texture. To display the guest content, an secondary
 * shared OpenGL context of the main OpenGL context is created. The secondary
 * context is marked as non opaque & the texture is displayed on an object
 * which is composed out of the several visible region rectangles. */
@interface OverlayView: NSView
{
@private
    NSView          *m_pParentView;

    NSOpenGLContext *m_pGLCtx;
    NSOpenGLContext *m_pSharedGLCtx;
    RTTHREAD         mThread;

    /* FBO handling */
    GLuint           m_FBOId;
    GLuint           m_FBOTexId;
    NSSize           m_FBOTexSize;
    GLuint           m_FBODepthStencilPackedId;

    /* The corresponding dock tile view of this OpenGL view & all helper
     * members. */
    DockOverlayView *m_DockTileView;
    GLuint           m_FBOThumbId;
    GLuint           m_FBOThumbTexId;
    GLfloat          m_FBOThumbScaleX;
    GLfloat          m_FBOThumbScaleY;

    /* For clipping */
    GLint            m_cClipRects;
    GLint           *m_paClipRects;

    /* Position/Size tracking */
    NSPoint          m_Pos;
    NSSize           m_Size;

    /* This is necessary for clipping on the root window */
    NSPoint          m_RootShift;
}
- (id)initWithFrame:(NSRect)frame thread:(RTTHREAD)aThread parentView:(NSView*)pParentView;
- (void)setGLCtx:(NSOpenGLContext*)pCtx;
- (NSOpenGLContext*)glCtx;

- (void)setPos:(NSPoint)pos;
- (NSPoint)pos;
- (void)setSize:(NSSize)size;
- (NSSize)size;
- (void)updateViewport;
- (void)reshape;

- (void)createFBO;
- (void)deleteFBO;

- (void)updateFBO;
- (void)makeCurrentFBO;
- (void)swapFBO;
- (void)flushFBO;
- (void)finishFBO;
- (void)bindFBO;
- (void)renderFBOToView;

- (void)clearVisibleRegions;
- (void)setVisibleRegions:(GLint)cRects paRects:(GLint*)paRects;

- (NSView*)dockTileScreen;
- (void)reshapeDockTile;
@end

/* Helper view. This view is added as a sub view of the parent view to track
 * main window changes. Whenever the main window is changed (which happens on
 * fullscreen/seamless entry/exit) the overlay window is informed & can add
 * them self as a child window again. */
@class OverlayWindow;
@interface OverlayHelperView: NSView
{
@private
    OverlayWindow *m_pOverlayWindow;
}
-(id)initWithOverlayWindow:(OverlayWindow*)pOverlayWindow;
@end

/* Custom window class. This is the overlay window which contains our custom
 * NSView. Its a direct child of the Qt Main window. It marks its background
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

    /* Create a buffer for our thumbnail image. Its in the size of this view. */
    m_ThumbBitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
        pixelsWide:frame.size.width
        pixelsHigh:frame.size.height
        bitsPerSample:8
        samplesPerPixel:4
        hasAlpha:YES
        isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace
        bytesPerRow:frame.size.width * 4
        bitsPerPixel:8 * 4];
    m_ThumbImage = [[NSImage alloc] initWithSize:[m_ThumbBitmap size]];
    [m_ThumbImage addRepresentation:m_ThumbBitmap];
    [self unlock];
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)drawRect:(NSRect)aRect
{
    [self lock];
#ifdef SHOW_WINDOW_BACKGROUND
    [[NSColor colorWithCalibratedRed:1.0 green:0.0 blue:0.0 alpha:0.7] set];
    NSRect frame = [self frame];
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

    return self;
}
    
- (void)dealloc
{
    DEBUG_MSG(("Dealloc context %X\n", (uint)self));

    [m_pPixelFormat release];

    [super dealloc];
}

-(void)setView:(NSView*)view
{
#ifdef FBO
    m_pView = view;;
#else
    [super setView: view];
#endif
}

-(NSView*)view
{
#ifdef FBO
    return m_pView;
#else
    return [super view];
#endif
}

-(void)clearDrawable
{
    m_pView = NULL;;
    [super clearDrawable];
}

-(NSOpenGLPixelFormat*)openGLPixelFormat
{
    return m_pPixelFormat;
}

@end;

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

    return self;
}

-(void)viewDidMoveToWindow
{
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
    if(self = [super initWithContentRect:NSZeroRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO])
    {
        m_pParentView = pParentView;
        m_pOverlayView = pOverlayView;
        m_Thread = [NSThread currentThread];

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

        NSWindow *pParentWin = [m_pParentView window];

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
    return self;
}

- (void)dealloc
{
    DEBUG_MSG(("Dealloc window %X\n", (uint)self));

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [m_pOverlayHelperView removeFromSuperview];
    [m_pOverlayHelperView release];

    [super dealloc];
}

- (void)parentWindowFrameChanged:(NSNotification*)pNote
{
    /* Reposition this window with the help of the OverlayView. Perform the
     * call in the OpenGL thread. */
//    [m_pOverlayView performSelector:@selector(reshape) onThread:m_Thread withObject:nil waitUntilDone:YES];
    [m_pOverlayView reshape];
}

- (void)parentWindowChanged:(NSWindow*)pWindow
{
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
//        [m_pOverlayView performSelector:@selector(reshape) withObject:nil afterDelay:0.2];
//        [NSTimer scheduledTimerWithTimeInterval:0.2 target:m_pOverlayView selector:@selector(reshape) userInfo:nil repeats:NO];
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
    m_pParentView = pParentView;
    /* Make some reasonable defaults */
    m_pGLCtx = NULL;
    m_pSharedGLCtx = NULL;
    mThread = aThread;
    m_FBOId = 0;
    m_FBOTexId = 0;
    m_FBOTexSize = NSZeroSize;
    m_FBODepthStencilPackedId = 0;
    m_cClipRects = 0;
    m_paClipRects = NULL;
    m_Pos = NSZeroPoint;
    m_Size = NSZeroSize;
    m_RootShift = NSZeroPoint;

    DEBUG_MSG(("Init view %X (%X)\n", (uint)self, (uint)mThread));
    
    self = [super initWithFrame:frame];

    return self;
}

- (void)dealloc
{
    DEBUG_MSG(("Dealloc view %X\n", (uint)self));

    [self deleteFBO];

    if (m_pGLCtx)
    {
        if ([m_pGLCtx view] == self) 
            [m_pGLCtx clearDrawable];
    }
    if (m_pSharedGLCtx)
    {
        if ([m_pSharedGLCtx view] == self) 
            [m_pSharedGLCtx clearDrawable];

        [m_pSharedGLCtx release];
    }

    [self clearVisibleRegions];

    [super dealloc];
}

- (void)drawRect:(NSRect)aRect
{
//    NSGraphicsContext*pC = [NSGraphicsContext currentContext];
//    [[NSColor blueColor] set];
//    NSBezierPath *p = [[NSBezierPath alloc] bezierPathWithOvalInRect:[self frame]];
//    [p fill];
//    [[NSColor greenColor] set];
//    [p stroke];
//    if ([self lockFocusIfCanDraw])
//    {
//        [self renderFBOToView];
//        [self unlockFocus];
//    }
}

- (void)setGLCtx:(NSOpenGLContext*)pCtx
{
    m_pGLCtx = pCtx;
}

- (NSOpenGLContext*)glCtx
{
    return m_pGLCtx;
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
    [self updateFBO];
}

- (NSSize)size
{
    return m_Size;
}

- (void)updateViewport
{
    if (m_pSharedGLCtx)
    {
        /* Update the viewport for our OpenGL view */
        [m_pSharedGLCtx makeCurrentContext];
        [m_pSharedGLCtx update];

        NSRect r = [self frame];
        /* Setup all matrices */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glViewport(0, 0, r.size.width, r.size.height);
        glOrtho(0, r.size.width, 0, r.size.height, -1, 1);
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glTranslatef(0.0f, m_RootShift.y, 0.0f);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(-m_RootShift.x, 0.0f, 0.0f);

        /* Clear background to transparent */
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        glEnable(GL_TEXTURE_RECTANGLE_ARB);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_FBOTexId);

        [m_pGLCtx makeCurrentContext];
    }
}

- (void)reshape
{
    /* Getting the right screen coordinates of the parents frame is a little bit
     * complicated. */
    NSRect parentFrame = [m_pParentView frame];
    NSPoint parentPos = [[m_pParentView window] convertBaseToScreen:[[m_pParentView superview] convertPointToBase:NSMakePoint(parentFrame.origin.x, parentFrame.origin.y + parentFrame.size.height)]];
    parentFrame.origin.x = parentPos.x;
    parentFrame.origin.y = parentPos.y;

    /* Calculate the new screen coordinates of the overlay window. */
    NSPoint childPos = NSMakePoint(m_Pos.x, m_Pos.y + m_Size.height);
    childPos = [[m_pParentView window] convertBaseToScreen:[[m_pParentView superview] convertPointToBase:childPos]];

    /* Make a frame out of it. */
    NSRect childFrame = NSMakeRect(childPos.x, childPos.y, m_Size.width, m_Size.height);

    /* We have to make sure that the overlay window will not be displayed out
     * of the parent window. So intersect both frames & use the result as the new
     * frame for the window. */
    NSRect newFrame = NSIntersectionRect(parentFrame, childFrame);

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

//    NSScrollView *pScrollView = [[[m_pParentView window] contentView] enclosingScrollView];
//    if (pScrollView)
//    {
//        NSRect scrollRect = [pScrollView documentVisibleRect];
//        NSRect scrollRect = [m_pParentView visibleRect];
//        printf ("sc rect: %d %d %d %d\n", (int) scrollRect.origin.x,(int) scrollRect.origin.y,(int) scrollRect.size.width,(int) scrollRect.size.height);
//        NSRect b = [[m_pParentView superview] bounds];
//        printf ("bound rect: %d %d %d %d\n", (int) b.origin.x,(int) b.origin.y,(int) b.size.width,(int) b.size.height);
//        newFrame.origin.x += scrollRect.origin.x;
//        newFrame.origin.y += scrollRect.origin.y;
//    }

    /* Set the new frame. */
    [[self window] setFrame:newFrame display:YES];

    /* Inform the dock tile view as well */
    [self reshapeDockTile];

    /* Make sure the context is updated according */
    [self updateViewport];
}

- (void)createFBO
{    
    [self deleteFBO];

    GL_SAVE_STATE;

    /* If not previously setup generate IDs for FBO and its associated texture. */
    if (!m_FBOId)
    {
        /* Make sure the framebuffer extension is supported */
        const GLubyte* strExt;
        GLboolean isFBO;
        /* Get the extension name string. It is a space-delimited list of the
         * OpenGL extensions that are supported by the current renderer. */
        strExt = glGetString(GL_EXTENSIONS);
        isFBO = gluCheckExtension((const GLubyte*)"GL_EXT_framebuffer_object", strExt);
        if (!isFBO)
        {
            DEBUG_MSG(("Your system does not support framebuffer extension\n"));
        }
        
        /* Create FBO object */
        glGenFramebuffersEXT(1, &m_FBOId);
        /* & the texture as well the depth/stencil render buffer */
        glGenTextures(1, &m_FBOTexId);
        DEBUG_MSG_1(("Create FBO %d %d\n", m_FBOId, m_FBOTexId));

        glGenRenderbuffersEXT(1, &m_FBODepthStencilPackedId);
    }

    m_FBOTexSize = m_Size;
    /* Bind to FBO */
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBOId);

    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    
    GLfloat imageAspectRatio = m_FBOTexSize.width / m_FBOTexSize.height;

    /* Sanity check against maximum OpenGL texture size. If bigger adjust to
     * maximum possible size while maintain the aspect ratio. */
    GLint maxTexSize; 
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
//    maxTexSize = 150;
    GLint filter = GL_NEAREST;
    if (m_FBOTexSize.width > maxTexSize || m_FBOTexSize.height > maxTexSize) 
    {
        filter = GL_NICEST;
        if (imageAspectRatio > 1)
        {
            m_FBOTexSize.width = maxTexSize; 
            m_FBOTexSize.height = maxTexSize / imageAspectRatio;
        }
        else
        {
            m_FBOTexSize.width = maxTexSize * imageAspectRatio;
            m_FBOTexSize.height = maxTexSize; 
        }
    }
    
    /* Initialize FBO Texture */
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_FBOTexId);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
    
    /* The GPUs like the GL_BGRA / GL_UNSIGNED_INT_8_8_8_8_REV combination
     * others are also valid, but might incur a costly software translation. */
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGB, m_FBOTexSize.width, m_FBOTexSize.height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    
    /* Now attach texture to the FBO as its color destination */
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, m_FBOTexId, 0);

    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_FBODepthStencilPackedId);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_STENCIL_EXT, m_FBOTexSize.width, m_FBOTexSize.height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_FBODepthStencilPackedId);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_FBODepthStencilPackedId);

    /* Make sure the FBO was created succesfully. */
    if (GL_FRAMEBUFFER_COMPLETE_EXT != glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT))
        DEBUG_MSG(("Framebuffer Object creation or update failed!\n"));

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    /* Is there a dock tile preview enabled in the GUI? If so setup a
     * additional thumbnail view for the dock tile. */
    NSView *dockScreen = [self dockTileScreen];
    if (dockScreen)
    {
        if (!m_FBOThumbId)
        {
            glGenFramebuffersEXT(1, &m_FBOThumbId);
            glGenTextures(1, &m_FBOThumbTexId);
        }

        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBOThumbId);
        /* Initialize FBO Texture */
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_FBOThumbTexId);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NICEST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NICEST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
    
        /* The GPUs like the GL_BGRA / GL_UNSIGNED_INT_8_8_8_8_REV combination
         * others are also valid, but might incur a costly software translation. */
        glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, m_FBOTexSize.width * m_FBOThumbScaleX, m_FBOTexSize.height * m_FBOThumbScaleY, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

        /* Now attach texture to the FBO as its color destination */
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, m_FBOThumbTexId, 0);

        /* Make sure the FBO was created succesfully. */
        if (GL_FRAMEBUFFER_COMPLETE_EXT != glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT))
            DEBUG_MSG(("Framebuffer Thumb Object creation or update failed!\n"));

        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

        m_DockTileView = [[DockOverlayView alloc] init];
        [self reshapeDockTile];
        [dockScreen addSubview:m_DockTileView];
    }

    /* Initialize with one big visual region over the full size */
    [self clearVisibleRegions];
    m_cClipRects = 1;
    m_paClipRects = (GLint*)RTMemAlloc(sizeof(GLint) * 4);
    m_paClipRects[0] = 0;
    m_paClipRects[1] = 0;
    m_paClipRects[2] = m_FBOTexSize.width;
    m_paClipRects[3] = m_FBOTexSize.height;

    GL_RESTORE_STATE;
}

- (void)deleteFBO
{
    if ([NSOpenGLContext currentContext] != nil)
    {
        GL_SAVE_STATE;

        if (m_FBODepthStencilPackedId > 0)
        {
            glDeleteRenderbuffersEXT(1, &m_FBODepthStencilPackedId);
            m_FBODepthStencilPackedId = 0;
        }
        if (m_FBOTexId > 0)
        {
            glEnable(GL_TEXTURE_RECTANGLE_ARB);
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
            glDeleteTextures(1, &m_FBOTexId);
            m_FBOTexId = 0;
        }
        if (m_FBOId > 0)
        {
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

            glDeleteFramebuffersEXT(1, &m_FBOId);
            m_FBOId = 0;
        }

        GL_RESTORE_STATE;
    }
    if (m_DockTileView != nil)
    {
        [m_DockTileView removeFromSuperview];
        [m_DockTileView release];
        m_DockTileView = nil;
    }
}

- (void)updateFBO
{
    [self makeCurrentFBO];
    
    if (m_pGLCtx)
    {
#ifdef FBO
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        [self createFBO];
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBOId);
#endif
        [m_pGLCtx update];
    }
}

- (void)makeCurrentFBO
{
    DEBUG_MSG_1(("MakeCurrent called %X\n", self));

#ifdef FBO
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBOId);
#endif
    if (m_pGLCtx)
    {
        if ([m_pGLCtx view] != self) 
        {
            /* We change the active view, so flush first */
            glFlush();
            [m_pGLCtx setView: self];
            CHECK_GL_ERROR();
        }
//        if ([NSOpenGLContext currentContext] != m_pGLCtx)
        {
            [m_pGLCtx makeCurrentContext];
            CHECK_GL_ERROR();
//            [m_pGLCtx update];
        }
    }
#ifdef FBO
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBOId);
#endif
}

- (void)swapFBO
{
    DEBUG_MSG_1(("SwapCurrent called %X\n", self));

#ifdef FBO
    GLint tmpFB;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &tmpFB);
    DEBUG_MSG_1(("Swap GetINT %d\n", tmpFB));
    [m_pGLCtx flushBuffer];
//    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	if (tmpFB == m_FBOId)
    {
        if ([self lockFocusIfCanDraw])
        {
            [self renderFBOToView];
            [self unlockFocus];
        }
    }
//    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBOId);
#else
    [m_pGLCtx flushBuffer];
#endif
}

- (void)flushFBO
{
    GLint tmpFB;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &tmpFB);
    glFlush();
//    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    DEBUG_MSG_1 (("Flusj GetINT %d\n", tmpFB));
	if (tmpFB == m_FBOId)
    {
        if ([self lockFocusIfCanDraw])
        {
            [self renderFBOToView];
            [self unlockFocus];
        }
    }
//    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBOId);
}

- (void)finishFBO
{
    GLint tmpFB;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &tmpFB);
    glFinish();
        //    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    DEBUG_MSG_1 (("Finish GetINT %d\n", tmpFB));
	if (tmpFB == m_FBOId)
    {
        if ([self lockFocusIfCanDraw])
        {
            [self renderFBOToView];
            [self unlockFocus];
        }
    }
//    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBOId);
}

- (void)bindFBO
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBOId);
}

- (void)renderFBOToView
{
    if (!m_pSharedGLCtx)
    {
        /* Create a shared context out of the main context. Use the same pixel format. */
        m_pSharedGLCtx = [[NSOpenGLContext alloc] initWithFormat:[(OverlayOpenGLContext*)m_pGLCtx openGLPixelFormat] shareContext:m_pGLCtx];

        /* Set the new context as non opaque */
        GLint opaque = 0;
        [m_pSharedGLCtx setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];        
        /* Only swap on screen refresh */
//        GLint swap = 1;
//        [m_pSharedGLCtx setValues:&swap forParameter:NSOpenGLCPSwapInterval];        
        /* Set this view as the drawable for the new context */
        [m_pSharedGLCtx setView: self];
        [self updateViewport];
    }

    if (m_pSharedGLCtx)
    {
        NSRect r = [self frame];

        if (m_FBOTexId > 0)
        {
            [m_pSharedGLCtx makeCurrentContext];
        
            if (m_FBOThumbTexId > 0 &&
                [m_DockTileView thumbBitmap] != nil)
            {
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
                // Do other work processing here, using a double or triple buffer
                glGetTexImage(GL_TEXTURE_RECTANGLE_ARB, 0, GL_BGRA,
                              GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
#endif

                GL_SAVE_STATE;
                glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBOThumbId);

                /* We like to read from the primary color buffer */
                glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

                NSRect rr = [m_DockTileView frame];

                /* Setup all matrices */
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glViewport(0, 0, rr.size.width, rr.size.height);
                glOrtho(0, rr.size.width, 0, rr.size.height, -1, 1);
                glScalef(m_FBOThumbScaleX, m_FBOThumbScaleY, 1.0f);
                glMatrixMode(GL_TEXTURE);
                glLoadIdentity();
                glTranslatef(0.0f, m_RootShift.y, 0.0f);
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();

                /* Clear background to transparent */
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                glEnable(GL_TEXTURE_RECTANGLE_ARB);
                glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_FBOTexId);
                GLint i;
                for (i = 0; i < m_cClipRects; ++i)
                {
                    GLint x1 = m_paClipRects[4*i];
                    GLint y1 = (r.size.height - m_paClipRects[4*i+1]);
                    GLint x2 = m_paClipRects[4*i+2];
                    GLint y2 = (r.size.height - m_paClipRects[4*i+3]);
                    glBegin(GL_QUADS);
                    {
                        glTexCoord2i(x1, y1); glVertex2i(x1, y1);
                        glTexCoord2i(x1, y2); glVertex2i(x1, y2);
                        glTexCoord2i(x2, y2); glVertex2i(x2, y2);
                        glTexCoord2i(x2, y1); glVertex2i(x2, y1);
                    }
                    glEnd();
                }
                glFinish();
    
                /* Here the magic of reading the FBO content in our own buffer
                 * happens. We have to lock this access, in the case the dock
                 * is updated currently. */
                [m_DockTileView lock];
                glReadPixels(0, 0, rr.size.width, rr.size.height,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             [[m_DockTileView thumbBitmap] bitmapData]);
                [m_DockTileView unlock];

                NSDockTile *pDT = [[NSApplication sharedApplication] dockTile];

                /* Send a display message to the dock tile in the main thread */
                [[[NSApplication sharedApplication] dockTile] performSelectorOnMainThread:@selector(display) withObject:nil waitUntilDone:NO];

                glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
                GL_RESTORE_STATE;
            }

            /* Clear background to transparent */
            glClear(GL_COLOR_BUFFER_BIT);
        
            /* Blit the content of the FBO to the screen. todo: check for
             * optimization with display lists. */
            GLint i;
            for (i = 0; i < m_cClipRects; ++i)
            {
                GLint x1 = m_paClipRects[4*i];
                GLint y1 = r.size.height - m_paClipRects[4*i+1];
                GLint x2 = m_paClipRects[4*i+2];
                GLint y2 = r.size.height - m_paClipRects[4*i+3];
                glBegin(GL_QUADS);
                {
                    glTexCoord2i(x1, y1); glVertex2i(x1, y1);
                    glTexCoord2i(x1, y2); glVertex2i(x1, y2);
                    glTexCoord2i(x2, y2); glVertex2i(x2, y2);
                    glTexCoord2i(x2, y1); glVertex2i(x2, y1);
                }
                glEnd();
            }
            [m_pSharedGLCtx flushBuffer];
            [m_pGLCtx makeCurrentContext];
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
    DEBUG_MSG_1(("New region recieved\n"));

    [self clearVisibleRegions];

    if (cRects>0)
    {
        m_paClipRects = (GLint*)RTMemAlloc(sizeof(GLint) * 4 * cRects);
        m_cClipRects = cRects;
        memcpy(m_paClipRects, paRects, sizeof(GLint) * 4 * cRects);
    }
}

- (NSView*)dockTileScreen
{
    NSView *contentView = [[[NSApplication sharedApplication] dockTile] contentView];
    NSView *screenContent = nil;
    if ([contentView respondsToSelector:@selector(screenContent)])
         screenContent = [contentView performSelector:@selector(screenContent)];
    return screenContent;
}

- (void)reshapeDockTile
{
    NSRect dockFrame = [[self dockTileScreen] frame];
    NSRect parentFrame = [m_pParentView frame];

    m_FBOThumbScaleX = (float)dockFrame.size.width / parentFrame.size.width;
    m_FBOThumbScaleY = (float)dockFrame.size.height / parentFrame.size.height;
    NSRect newFrame = NSMakeRect ((int)(m_Pos.x * m_FBOThumbScaleX), (int)(dockFrame.size.height - (m_Pos.y + m_Size.height - m_RootShift.y) * m_FBOThumbScaleY), (int)(m_Size.width * m_FBOThumbScaleX), (int)(m_Size.height * m_FBOThumbScaleY));
//    NSRect newFrame = NSMakeRect ((int)roundf(m_Pos.x * m_FBOThumbScaleX), (int)roundf(dockFrame.size.height - (m_Pos.y + m_Size.height) * m_FBOThumbScaleY), (int)roundf(m_Size.width * m_FBOThumbScaleX), (int)roundf(m_Size.height * m_FBOThumbScaleY));
//      NSRect newFrame = NSMakeRect ((m_Pos.x * m_FBOThumbScaleX), (dockFrame.size.height - (m_Pos.y + m_Size.height) * m_FBOThumbScaleY), (m_Size.width * m_FBOThumbScaleX), (m_Size.height * m_FBOThumbScaleY));
//    printf ("%f %f %f %f - %f %f\n", newFrame.origin.x, newFrame.origin.y, newFrame.size.width, newFrame.size.height, m_Size.height, m_FBOThumbScaleY);
    [m_DockTileView setFrame: newFrame];
}

@end

/********************************************************************************
*
* OpenGL context management
*
********************************************************************************/
void cocoaGLCtxCreate(NativeGLCtxRef *ppCtx, GLbitfield fVisParams)
{
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
        DEBUG_MSG(("CR_DOUBLE_BIT requested\n"));
        attribs[i++] = NSOpenGLPFAStereo;
    }
    
    /* Mark the end */
    attribs[i++] = 0;

    /* Choose a pixel format */
    NSOpenGLPixelFormat* pFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
    
    if (pFmt)
    {
        *ppCtx = [[OverlayOpenGLContext alloc] initWithFormat:pFmt shareContext:nil];

        /* Enable multi threaded OpenGL engine */
//        CGLContextObj cglCtx = [*ppCtx CGLContextObj];
//        CGLError err = CGLEnable(cglCtx, kCGLCEMPEngine);
//        if (err != kCGLNoError)
//            printf ("Couldn't enable MT OpenGL engine!\n");
    
        DEBUG_MSG(("New context %X\n", (uint)*ppCtx));
    }

    [pPool release];
}

void cocoaGLCtxDestroy(NativeGLCtxRef pCtx)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

//    [pCtx release];

    [pPool release];
}

/********************************************************************************
*
* View management
*
********************************************************************************/
void cocoaViewCreate(NativeViewRef *ppView, NativeViewRef pParentView, GLbitfield fVisParams)
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

void cocoaViewDestroy(NativeViewRef pView)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    /* Hide the view early */
    [pView setHidden: YES];

    NSWindow *win = [pView window];
    [[NSNotificationCenter defaultCenter] removeObserver:win];
    [win setContentView: nil];
    [[win parentWindow] removeChildWindow: win];
    int b = [win retainCount];
    for (; b > 1; --b)
        [win release];

    /* There seems to be a bug in the performSelector method which is called in
     * parentWindowChanged above. The object is retained but not released. This
     * results in an unbalanced reference count, which is here manually
     * decremented. */
    int a = [pView retainCount];
    for (; a > 1; --a)
        [pView release];

    [pPool release];
}

void cocoaViewShow(NativeViewRef pView, GLboolean fShowIt)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [pView setHidden: fShowIt==GL_TRUE?NO:YES];

    [pPool release];
}

void cocoaViewDisplay(NativeViewRef pView)
{    
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView swapFBO];

    [pPool release];

}

void cocoaViewSetPosition(NativeViewRef pView, NativeViewRef pParentView, int x, int y)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView setPos:NSMakePoint(x, y)];

    [pPool release];
}

void cocoaViewSetSize(NativeViewRef pView, int w, int h)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView setSize:NSMakeSize(w, h)];

    [pPool release];
}

void cocoaViewGetGeometry(NativeViewRef pView, int *pX, int *pY, int *pW, int *pH)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    NSRect frame = [[pView window] frame];
    *pX = frame.origin.x;
    *pY = frame.origin.y;
    *pW = frame.size.width;
    *pH = frame.size.height;

    [pPool release];
}

void cocoaViewMakeCurrentContext(NativeViewRef pView, NativeGLCtxRef pCtx)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView setGLCtx:pCtx];
    [(OverlayView*)pView makeCurrentFBO];

    [pPool release];
}

void cocoaViewSetVisibleRegion(NativeViewRef pView, GLint cRects, GLint* paRects)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView setVisibleRegions:cRects paRects:paRects];

    [pPool release];
}

/********************************************************************************
*
* Additional OpenGL wrapper
*
********************************************************************************/
void cocoaFlush()
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

//    glFlush();
//    return;

    DEBUG_MSG_1(("glFlush called\n"));

#ifdef FBO
# if 0
    NSOpenGLContext *pCtx = [NSOpenGLContext currentContext];
    if (pCtx)
    {
        NSView *pView = [pCtx view];
        if (pView)
        {
            if ([pView respondsToSelector:@selector(flushFBO)])
                [pView performSelector:@selector(flushFBO)];
        }
    }
# endif
#else
    glFlush();
#endif

    [pPool release];
}

void cocoaFinish()
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    DEBUG_MSG_1(("glFinish called\n"));

#ifdef FBO
    NSOpenGLContext *pCtx = [NSOpenGLContext currentContext];
    if (pCtx)
    {
        NSView *pView = [pCtx view];
        if (pView)
        {
            if ([pView respondsToSelector:@selector(finishFBO)])
                [pView performSelector:@selector(finishFBO)];
        }
    }
#else
    glFinish();
#endif

    [pPool release];
}

void cocoaBindFramebufferEXT(GLenum target, GLuint framebuffer)
{
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    DEBUG_MSG_1(("glRenderspuBindFramebufferEXT called %d\n", framebuffer));

#ifdef FBO
    if (framebuffer != 0)
        glBindFramebufferEXT(target, framebuffer);
    else
    {
        NSOpenGLContext *pCtx = [NSOpenGLContext currentContext];
        if (pCtx)
        {
            NSView *pView = [pCtx view];
            if (pView)
            {
                if ([pView respondsToSelector:@selector(bindFBO)])
                    [pView performSelector:@selector(bindFBO)];
            }
        }
    }
#else
    glBindFramebufferEXT(target, framebuffer);
#endif

    [pPool release];
}

