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
#undef PVM

#include "chromium.h" /* For the visual bits of chromium */

#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/time.h>
#include <iprt/assert.h>

#include <cr_vreg.h>
#include <cr_error.h>
#include <cr_blitter.h>
#ifdef VBOX_WITH_CRDUMPER_THUMBNAIL
# include <cr_pixeldata.h>
#endif


#include "renderspu.h"

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
# define CR_RENDER_FORCE_PRESENT_MAIN_THREAD /* force present schedule to main thread */
# define SHOW_WINDOW_BACKGROUND 1 /* Define this to see the window background even if the window is clipped */
# define DEBUG_VERBOSE /* Define this to get some debug info about the messages flow. */
#endif

#ifdef DEBUG_misha
#define DEBUG_INFO(text) do { \
        crWarning text ; \
        Assert(0); \
    } while (0)
# define DEBUG_MSG(text) \
    printf text
# define DEBUG_WARN(text) do { \
        crWarning text ; \
        Assert(0); \
    } while (0)
#else
#define DEBUG_INFO(text) do { \
        crInfo text ; \
    } while (0)
# define DEBUG_MSG(text) \
    do {} while (0)
# define DEBUG_WARN(text) do { \
        crWarning text ; \
    } while (0)
#endif

#ifdef DEBUG_VERBOSE
# define DEBUG_MSG_1(text) \
    DEBUG_MSG(text)
#else
# define DEBUG_MSG_1(text) \
    do {} while (0)
#endif

#define DEBUG_FUNC_ENTER() do { \
        DEBUG_MSG(("==>%s\n", __PRETTY_FUNCTION__)); \
    } while (0)
    
#define DEBUG_FUNC_LEAVE() do { \
        DEBUG_MSG(("<==%s\n", __PRETTY_FUNCTION__)); \
    } while (0)


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

static NSOpenGLContext * vboxCtxGetCurrent()
{
    GET_CONTEXT(pCtxInfo);
    if (pCtxInfo)
    {
        Assert(pCtxInfo->context);
        return pCtxInfo->context;
    }

    return nil;
}

static bool vboxCtxSyncCurrentInfo()
{
    GET_CONTEXT(pCtxInfo);
    NSOpenGLContext *pCtx = [NSOpenGLContext currentContext];
    NSView *pView = pCtx ? [pCtx view] : nil;
    bool fAdjusted = false;
    if (pCtxInfo)
    {
        WindowInfo *pWinInfo = pCtxInfo->currentWindow;
        Assert(pWinInfo);
        if (pCtxInfo->context != pCtx
            || pWinInfo->window != pView)
        {
            renderspu_SystemMakeCurrent(pWinInfo, 0, pCtxInfo);
            fAdjusted = true;
        }
    }
    else
    {
        if (pCtx)
        {
            [NSOpenGLContext clearCurrentContext];
            fAdjusted = true;
        }
    }
    
    return fAdjusted;
}

typedef struct VBOX_CR_RENDER_CTX_INFO
{
    bool fIsValid;
    NSOpenGLContext *pCtx;
    NSView *pView;
} VBOX_CR_RENDER_CTX_INFO, *PVBOX_CR_RENDER_CTX_INFO;

static void vboxCtxEnter(NSOpenGLContext*pCtx, PVBOX_CR_RENDER_CTX_INFO pCtxInfo)
{
    NSOpenGLContext *pOldCtx = vboxCtxGetCurrent(); 
    NSView *pOldView = (pOldCtx ? [pOldCtx view] : nil);
    NSView *pView = [pCtx view];
    bool fNeedCtxSwitch = (pOldCtx != pCtx || pOldView != pView);
    Assert(pCtx);
 //   Assert(pOldCtx == m_pGLCtx);
 //   Assert(pOldView == self);
 //   Assert(fNeedCtxSwitch);
    if (fNeedCtxSwitch)
    {
        if(pOldCtx != nil)
            glFlush();
        
        [pCtx makeCurrentContext];
        
        pCtxInfo->fIsValid = true;
        pCtxInfo->pCtx = pOldCtx;
        pCtxInfo->pView = pView;
    }
    else
    {
        pCtxInfo->fIsValid = false;
    }
}    
    
static void vboxCtxLeave(PVBOX_CR_RENDER_CTX_INFO pCtxInfo)
{
    if (pCtxInfo->fIsValid)
    {
        NSOpenGLContext *pOldCtx = pCtxInfo->pCtx;
        NSView *pOldView = pCtxInfo->pView;
    
        glFlush();
        if (pOldCtx != nil)
        {
            if ([pOldCtx view] != pOldView)
            {
                [pOldCtx setView: pOldView];
            }
        
            [pOldCtx makeCurrentContext];
            
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

@interface VBoxTask : NSObject
{
}
- (void)run;
@end

@interface VBoxTaskPerformSelector : VBoxTask
{
@private
    id m_Object;
    SEL m_Selector;
    id m_Arg;
}
- (id)initWithObject:(id)aObject selector:(SEL)aSelector arg:(id)aArg;
- (void)run;
- (void)dealloc;
@end

#if 0
typedef DECLCALLBACKPTR(void, PFNVBOXTASKCALLBACK)(void *pvCb);

@interface VBoxTaskCallback: VBoxTask
{
@private
    PFNVBOXTASKCALLBACK m_pfnCb;
    void *m_pvCb;
}
- (id)initWithCb:(PFNVBOXTASKCALLBACK)pfnCb arg:(void*)pvCb;
- (void)run;
@end
#endif

@interface VBoxTaskComposite: VBoxTask
{
@private
    NSUInteger m_CurIndex;
    RTCRITSECT m_Lock;
    NSMutableArray *m_pArray;
}
- (id)init;
- (void)add:(VBoxTask*)pTask;
- (void)run;
- (void)dealloc;
@end

@implementation VBoxTask
@end

@implementation VBoxTaskPerformSelector
- (id)initWithObject:(id)aObject selector:(SEL)aSelector arg:(id)aArg
{
    [aObject retain];
    m_Object = aObject;
    m_Selector = aSelector;
    if (aArg != nil)
        [aArg retain];
    m_Arg = aArg;
    
    return self;
}

- (void)run
{
    [m_Object performSelector:m_Selector withObject:m_Arg];
}

- (void)dealloc
{
    [m_Object release];
    if (m_Arg != nil)
        [m_Arg release];
}
@end

@implementation VBoxTaskComposite
- (id)init
{
    int rc = RTCritSectInit(&m_Lock);
    if (!RT_SUCCESS(rc))
    {
        DEBUG_WARN(("RTCritSectInit failed %d\n", rc));
        return nil;
    }

    m_CurIndex = 0;
    
    m_pArray = [[NSMutableArray alloc] init];
    return self;
}

- (void)add:(VBoxTask*)pTask
{
    [pTask retain];
    int rc = RTCritSectEnter(&m_Lock);
    if (RT_SUCCESS(rc))
    {
        [m_pArray addObject:pTask];
        RTCritSectLeave(&m_Lock);
    }
    else
    {
        DEBUG_WARN(("RTCritSectEnter failed %d\n", rc));
        [pTask release];
    }
}

- (void)run
{
    for(;;)
    {
        int rc = RTCritSectEnter(&m_Lock);
        if (RT_FAILURE(rc))
        {
            DEBUG_WARN(("RTCritSectEnter failed %d\n", rc));
            break;
        }
        
        NSUInteger count = [m_pArray count];
        Assert(m_CurIndex <= count);
        if (m_CurIndex == count)
        {
            [m_pArray removeAllObjects];
            m_CurIndex = 0;
            RTCritSectLeave(&m_Lock);
            break;
        }

        VBoxTask* pTask = (VBoxTask*)[m_pArray objectAtIndex:m_CurIndex];
        Assert(pTask != nil);
        
        ++m_CurIndex;
        
        if (m_CurIndex > 1024)
        {
            NSRange range;
            range.location = 0;
            range.length = m_CurIndex;
            [m_pArray removeObjectsInRange:range];
            m_CurIndex = 0;
        }
        RTCritSectLeave(&m_Lock);
        
        [pTask run];
        [pTask release];
    }
}

- (void)dealloc
{
    NSUInteger count = [m_pArray count];
    for(;m_CurIndex < count; ++m_CurIndex)
    {
        VBoxTask* pTask = (VBoxTask*)[m_pArray objectAtIndex:m_CurIndex];
        DEBUG_WARN(("dealloc with non-empty tasks! %p\n", pTask));
        [pTask release];
    }
    
    [m_pArray release];
    RTCritSectDelete(&m_Lock);
}
@end

@interface VBoxMainThreadTaskRunner : NSObject
{
@private
    VBoxTaskComposite *m_pTasks;
}
- (id)init;
- (void)add:(VBoxTask*)pTask;
- (void)addObj:(id)aObject selector:(SEL)aSelector arg:(id)aArg;
- (void)runTasks;
- (bool)runTasksSyncIfPossible;
- (void)dealloc;
+ (VBoxMainThreadTaskRunner*) globalInstance;
@end

@implementation VBoxMainThreadTaskRunner
- (id)init
{
    self = [super init];
    if (self)
    {
        m_pTasks = [[VBoxTaskComposite alloc] init];
    }
    
    return self;
}

+ (VBoxMainThreadTaskRunner*) globalInstance
{
    static dispatch_once_t dispatchOnce;
    static VBoxMainThreadTaskRunner *pRunner = nil;
    dispatch_once(&dispatchOnce, ^{
        pRunner = [[VBoxMainThreadTaskRunner alloc] init];
    });
    return pRunner;
}

typedef struct CR_RCD_RUN
{
    VBoxMainThreadTaskRunner *pRunner;
} CR_RCD_RUN;

static DECLCALLBACK(void) vboxRcdRun(void *pvCb)
{
    DEBUG_FUNC_ENTER();
    CR_RCD_RUN * pRun = (CR_RCD_RUN*)pvCb;
    [pRun->pRunner runTasks];
    DEBUG_FUNC_LEAVE();
}

- (void)add:(VBoxTask*)pTask
{
    DEBUG_FUNC_ENTER();
    [m_pTasks add:pTask];
    [self retain];

    if (![self runTasksSyncIfPossible])
    {
        DEBUG_MSG(("task will be processed async\n"));
        [self performSelectorOnMainThread:@selector(runTasks) withObject:nil waitUntilDone:NO];
    }
    
    DEBUG_FUNC_LEAVE();
}

- (void)addObj:(id)aObject selector:(SEL)aSelector arg:(id)aArg
{
    VBoxTaskPerformSelector *pSelTask = [[VBoxTaskPerformSelector alloc] initWithObject:aObject selector:aSelector arg:aArg];
    [self add:pSelTask];
    [pSelTask release];
}

- (void)runTasks
{
    BOOL fIsMain = [NSThread isMainThread];
    Assert(fIsMain);
    if (fIsMain)
    {
        [m_pTasks run];
        [self release];
    }
    else
    {
        DEBUG_WARN(("run tasks called not on main thread!\n"));
        [self performSelectorOnMainThread:@selector(runTasks) withObject:nil waitUntilDone:YES];
    }
}

- (bool)runTasksSyncIfPossible
{
    if (renderspuCalloutAvailable())
    {
        CR_RCD_RUN Run;
        Run.pRunner = self;
        Assert(![NSThread isMainThread]);
        renderspuCalloutClient(vboxRcdRun, &Run);
        return true;
    }
    
    if ([NSThread isMainThread])
    {
        [self runTasks];
        return true;
    }
    
    return false;
}

- (void)dealloc
{
    [m_pTasks release];
}

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
    NSRect           m_RootRect;
    float            m_yInvRootOffset;
    
    CR_BLITTER *m_pBlitter;
    WindowInfo *m_pWinInfo;
    bool m_fNeedViewportUpdate;
    bool m_fNeedCtxUpdate;
    bool m_fDataVisible;
    bool m_fCleanupNeeded;
    bool m_fEverSized;
}
- (id)initWithFrame:(NSRect)frame thread:(RTTHREAD)aThread parentView:(NSView*)pParentView winInfo:(WindowInfo*)pWinInfo;
- (void)setGLCtx:(NSOpenGLContext*)pCtx;
- (NSOpenGLContext*)glCtx;

- (void)setParentView: (NSView*)view;
- (NSView*)parentView;
- (void)setOverlayWin: (NSWindow*)win;
- (NSWindow*)overlayWin;

- (void)vboxSetPos:(NSPoint)pos;
- (void)vboxSetPosUI:(NSPoint)pos;
- (void)vboxSetPosUIObj:(NSValue*)pPos;
- (NSPoint)pos;
- (bool)isEverSized;
- (void)vboxDestroy;
- (void)vboxSetSizeUI:(NSSize)size;
- (void)vboxSetSizeUIObj:(NSValue*)pSize;
- (void)vboxSetSize:(NSSize)size;
- (NSSize)size;
- (void)updateViewportCS;
- (void)vboxReshapePerform;
- (void)vboxReshapeOnResizePerform;
- (void)vboxReshapeOnReparentPerform;

- (void)createDockTile;
- (void)deleteDockTile;

- (void)makeCurrentFBO;
- (void)swapFBO;
- (void)vboxSetVisible:(GLboolean)fVisible;
- (void)vboxSetVisibleUIObj:(NSNumber*)pVisible;
- (void)vboxSetVisibleUI:(GLboolean)fVisible;
- (void)vboxTryDraw;
- (void)vboxTryDrawUI;
- (void)vboxReparent:(NSView*)pParentView;
- (void)vboxReparentUI:(NSView*)pParentView;
- (void)vboxPresent:(const VBOXVR_SCR_COMPOSITOR*)pCompositor;
- (void)vboxPresentCS:(const VBOXVR_SCR_COMPOSITOR*)pCompositor;
- (void)vboxPresentToDockTileCS:(const VBOXVR_SCR_COMPOSITOR*)pCompositor;
- (void)vboxPresentToViewCS:(const VBOXVR_SCR_COMPOSITOR*)pCompositor;
- (void)presentComposition:(const VBOXVR_SCR_COMPOSITOR_ENTRY*)pChangedEntry;
- (void)vboxBlitterSyncWindow;

- (void)clearVisibleRegions;
- (void)setVisibleRegions:(GLint)cRects paRects:(const GLint*)paRects;
- (GLboolean)vboxNeedsEmptyPresent;

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
    DEBUG_FUNC_ENTER();
    self = [super init];

    if (self)
    {
        /* We need a lock cause the thumb image could be accessed from the main
         * thread when someone is calling display on the dock tile & from the
         * OpenGL thread when the thumbnail is updated. */
        m_Lock = [[NSLock alloc] init];
    }

    DEBUG_FUNC_LEAVE();
    
    return self;
}

- (void)dealloc
{
    DEBUG_FUNC_ENTER();
    
    [self cleanup];
    [m_Lock release];

    [super dealloc];
    
    DEBUG_FUNC_LEAVE();
}

- (void)cleanup
{
    DEBUG_FUNC_ENTER();
    
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
    
    DEBUG_FUNC_LEAVE();
}

- (void)lock
{
    DEBUG_FUNC_ENTER();
    [m_Lock lock];
    DEBUG_FUNC_LEAVE();
}

- (void)unlock
{
    DEBUG_FUNC_ENTER();
    [m_Lock unlock];
    DEBUG_FUNC_LEAVE();
}

- (void)setFrame:(NSRect)frame
{
    DEBUG_FUNC_ENTER();
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
    DEBUG_FUNC_LEAVE();
}

- (BOOL)isFlipped
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_LEAVE();
    return YES;
}

- (void)drawRect:(NSRect)aRect
{
    DEBUG_FUNC_ENTER();
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
    DEBUG_FUNC_LEAVE();
}

- (NSBitmapImageRep*)thumbBitmap
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_LEAVE();
    return m_ThumbBitmap;
}

- (NSImage*)thumbImage
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_LEAVE();

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
    DEBUG_FUNC_ENTER();

    m_pPixelFormat = NULL;
    m_pView = NULL;

    self = [super initWithFormat:format shareContext:share];
    if (self)
        m_pPixelFormat = format;

    DEBUG_MSG(("OCTX(%p): init OverlayOpenGLContext\n", (void*)self));

    DEBUG_FUNC_LEAVE();

    return self;
}

- (void)dealloc
{
    DEBUG_FUNC_ENTER();

    DEBUG_MSG(("OCTX(%p): dealloc OverlayOpenGLContext\n", (void*)self));

    [m_pPixelFormat release];

    [super dealloc];
    
    DEBUG_FUNC_LEAVE();
}

-(bool)isDoubleBuffer
{
    DEBUG_FUNC_ENTER();

    GLint val;
    [m_pPixelFormat getValues:&val forAttribute:NSOpenGLPFADoubleBuffer forVirtualScreen:0];
    
    DEBUG_FUNC_LEAVE();
    
    return val == GL_TRUE ? YES : NO;
}

-(void)setView:(NSView*)view
{
    DEBUG_FUNC_ENTER();

    DEBUG_MSG(("OCTX(%p): setView: new view: %p\n", (void*)self, (void*)view));

#if 1 /* def FBO */
    m_pView = view;;
#else
    [super setView: view];
#endif

    DEBUG_FUNC_LEAVE();
}

-(NSView*)view
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_LEAVE();

#if 1 /* def FBO */
    return m_pView;
#else
    return [super view];
#endif
}

-(void)clearDrawable
{
    DEBUG_FUNC_ENTER();

    DEBUG_MSG(("OCTX(%p): clearDrawable\n", (void*)self));

    m_pView = NULL;;
    [super clearDrawable];
    
    DEBUG_FUNC_LEAVE();
}

-(NSOpenGLPixelFormat*)openGLPixelFormat
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_LEAVE();

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
    DEBUG_FUNC_ENTER();

    self = [super initWithFrame:NSZeroRect];

    m_pOverlayWindow = pOverlayWindow;

    DEBUG_MSG(("OHVW(%p): init OverlayHelperView\n", (void*)self));

    DEBUG_FUNC_LEAVE();

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
* OverlayWindow class implementation
*
********************************************************************************/
@implementation OverlayWindow

- (id)initWithParentView:(NSView*)pParentView overlayView:(OverlayView*)pOverlayView
{
    DEBUG_FUNC_ENTER();

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

    DEBUG_FUNC_LEAVE();

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

    /* Reposition this window with the help of the OverlayView. Perform the
     * call in the OpenGL thread. */
    /*
    [m_pOverlayView performSelector:@selector(vboxReshapePerform) onThread:m_Thread withObject:nil waitUntilDone:YES];
    */

    if ([m_pOverlayView isEverSized])
    {    
        if([NSThread isMainThread])
            [m_pOverlayView vboxReshapePerform];
        else
            [self performSelectorOnMainThread:@selector(vboxReshapePerform) withObject:nil waitUntilDone:NO];
    }
    
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
        /* Reshape the overlay view after a short waiting time to let the main
         * window resize itself properly. */
        /*
        [m_pOverlayView performSelector:@selector(vboxReshapePerform) withObject:nil afterDelay:0.2];
        [NSTimer scheduledTimerWithTimeInterval:0.2 target:m_pOverlayView selector:@selector(vboxReshapePerform) userInfo:nil repeats:NO];
        */

        if ([m_pOverlayView isEverSized])
        {    
            if([NSThread isMainThread])
                [m_pOverlayView vboxReshapePerform];
            else
                [self performSelectorOnMainThread:@selector(vboxReshapePerform) withObject:nil waitUntilDone:NO];
        }        
    }
    
    DEBUG_FUNC_LEAVE();
}

@end

/********************************************************************************
*
* OverlayView class implementation
*
********************************************************************************/
@implementation OverlayView

- (id)initWithFrame:(NSRect)frame thread:(RTTHREAD)aThread parentView:(NSView*)pParentView winInfo:(WindowInfo*)pWinInfo
{
    DEBUG_FUNC_ENTER();

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
    m_RootRect                = NSMakeRect(0, 0, m_Size.width, m_Size.height);
    m_yInvRootOffset          = 0;
    m_pBlitter                = nil;
    m_pWinInfo             	  = pWinInfo;
    m_fNeedViewportUpdate     = true;        
    m_fNeedCtxUpdate          = true;
    m_fDataVisible            = false;
    m_fCleanupNeeded          = false;
    m_fEverSized              = false;
    
    self = [super initWithFrame:frame];

    DEBUG_MSG(("OVIW(%p): init OverlayView\n", (void*)self));

    DEBUG_FUNC_LEAVE();

    return self;
}

- (void)cleanupData
{
    DEBUG_FUNC_ENTER();

    [self deleteDockTile];
    
    [self setGLCtx:nil];
    
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

    [self clearVisibleRegions];
    
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

- (void)drawRect:(NSRect)aRect
{
    [self vboxTryDrawUI];
}

- (void)setGLCtx:(NSOpenGLContext*)pCtx
{
    DEBUG_FUNC_ENTER();

    DEBUG_MSG(("OVIW(%p): setGLCtx: new ctx: %p\n", (void*)self, (void*)pCtx));
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
    DEBUG_FUNC_LEAVE();

    return m_pGLCtx;
}

- (NSView*)parentView
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_LEAVE();

    return m_pParentView;
}

- (void)setParentView:(NSView*)pView
{
    DEBUG_FUNC_ENTER();

    DEBUG_MSG(("OVIW(%p): setParentView: new view: %p\n", (void*)self, (void*)pView));

    m_pParentView = pView;
    
    DEBUG_FUNC_LEAVE();
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
    DEBUG_FUNC_LEAVE();

    return m_pOverlayWin;
}

- (void)vboxSetPosUI:(NSPoint)pos
{
    DEBUG_FUNC_ENTER();

    m_Pos = pos;

    if (m_fEverSized)
        [self vboxReshapePerform];
        
    DEBUG_FUNC_LEAVE();
}

- (void)vboxSetPosUIObj:(NSValue*)pPos
{
    DEBUG_FUNC_ENTER();

    NSPoint pos = [pPos pointValue];
    [self vboxSetPosUI:pos];

    DEBUG_FUNC_LEAVE();
}

typedef struct CR_RCD_SETPOS
{
    OverlayView *pView;
    NSPoint pos;
} CR_RCD_SETPOS;

static DECLCALLBACK(void) vboxRcdSetPos(void *pvCb)
{
    DEBUG_FUNC_ENTER();

    CR_RCD_SETPOS * pReparent = (CR_RCD_SETPOS*)pvCb;
    [pReparent->pView vboxSetPosUI:pReparent->pos];

    DEBUG_FUNC_LEAVE();
}

- (void)vboxSetPos:(NSPoint)pos
{
    DEBUG_FUNC_ENTER();

    DEBUG_MSG(("OVIW(%p): vboxSetPos: new pos: %d, %d\n", (void*)self, (int)pos.x, (int)pos.y));
    VBoxMainThreadTaskRunner *pRunner = [VBoxMainThreadTaskRunner globalInstance];
    NSValue *pPos =  [NSValue valueWithPoint:pos];
    [pRunner addObj:self selector:@selector(vboxSetPosUIObj:) arg:pPos];

    DEBUG_FUNC_LEAVE();
}

- (NSPoint)pos
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_LEAVE();
    return m_Pos;
}

- (bool)isEverSized
{
    DEBUG_FUNC_ENTER();
    DEBUG_FUNC_LEAVE();
    return m_fEverSized;
}

- (void)vboxDestroy
{
    DEBUG_FUNC_ENTER();
    BOOL fIsMain = [NSThread isMainThread];
    NSWindow *pWin = nil;
    
    Assert(fIsMain);

    /* Hide the view early */
    [self setHidden: YES];

    pWin = [self window];
    [[NSNotificationCenter defaultCenter] removeObserver:pWin];
    [pWin setContentView: nil];
    [[pWin parentWindow] removeChildWindow: pWin];
    
    if (fIsMain)
        [pWin release];
    else
    {
        /* We can NOT run synchronously with the main thread since this may lead to a deadlock,
           caused by main thread waiting xpcom thread, xpcom thread waiting to main hgcm thread,
           and main hgcm thread waiting for us, this is why use waitUntilDone:NO, 
           which should cause no harm */ 
        [pWin performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    }

    [self cleanupData];

    if (fIsMain)
        [self release];
    else
    {
        /* We can NOT run synchronously with the main thread since this may lead to a deadlock,
           caused by main thread waiting xpcom thread, xpcom thread waiting to main hgcm thread,
           and main hgcm thread waiting for us, this is why use waitUntilDone:NO. 
           We need to avoid concurrency though, so we cleanup some data right away via a cleanupData call */
        [self performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    }
    
    renderspuWinRelease(m_pWinInfo);
    
    DEBUG_FUNC_LEAVE();
}

- (void)vboxSetSizeUIObj:(NSValue*)pSize
{
    DEBUG_FUNC_ENTER();
    NSSize size = [pSize sizeValue];
    [self vboxSetSizeUI:size];
    DEBUG_FUNC_LEAVE();
}

- (void)vboxSetSizeUI:(NSSize)size
{
    DEBUG_FUNC_ENTER();
    m_Size = size;
    
    m_fEverSized = true;

    DEBUG_MSG(("OVIW(%p): vboxSetSize: new size: %dx%d\n", (void*)self, (int)m_Size.width, (int)m_Size.height));
    [self vboxReshapeOnResizePerform];

    /* ensure window contents is updated after that */
    [self vboxTryDrawUI];
    DEBUG_FUNC_LEAVE();
}

typedef struct CR_RCD_SETSIZE
{
    OverlayView *pView;
    NSSize size;
} CR_RCD_SETSIZE;

static DECLCALLBACK(void) vboxRcdSetSize(void *pvCb)
{
    DEBUG_FUNC_ENTER();
    CR_RCD_SETSIZE * pSetSize = (CR_RCD_SETSIZE*)pvCb;
    [pSetSize->pView vboxSetSizeUI:pSetSize->size];
    DEBUG_FUNC_LEAVE();
}

- (void)vboxSetSize:(NSSize)size
{
    DEBUG_FUNC_ENTER();
    
    VBoxMainThreadTaskRunner *pRunner = [VBoxMainThreadTaskRunner globalInstance];
    NSValue *pSize = [NSValue valueWithSize:size];
    [pRunner addObj:self selector:@selector(vboxSetSizeUIObj:) arg:pSize];

    DEBUG_FUNC_LEAVE();
}

- (NSSize)size
{
    DEBUG_FUNC_ENTER();
    return m_Size;
    DEBUG_FUNC_LEAVE();
}

- (void)updateViewportCS
{
    DEBUG_FUNC_ENTER();
    DEBUG_MSG(("OVIW(%p): updateViewport\n", (void*)self));

    /* Update the viewport for our OpenGL view */
    [m_pSharedGLCtx update];

    [self vboxBlitterSyncWindow];
        
    /* Clear background to transparent */
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    DEBUG_FUNC_LEAVE();
}

- (void)vboxReshapeOnResizePerform
{
    DEBUG_FUNC_ENTER();
    [self vboxReshapePerform];
    
    [self createDockTile];
    /* have to rebind GL_TEXTURE_RECTANGLE_ARB as m_FBOTexId could be changed in updateFBO call */
    m_fNeedViewportUpdate = true;
#if 0
    pCurCtx = [NSOpenGLContext currentContext];
    if (pCurCtx && pCurCtx == m_pGLCtx && (pCurView = [pCurCtx view]) == self)
    {
        [m_pGLCtx update];
        m_fNeedCtxUpdate = false;
    }
    else
    {
        /* do it in a lazy way */
        m_fNeedCtxUpdate = true;
    }
#endif
    DEBUG_FUNC_LEAVE();
}

- (void)vboxReshapeOnReparentPerform
{
    DEBUG_FUNC_ENTER();
    [self createDockTile];
    DEBUG_FUNC_LEAVE();
}

- (void)vboxReshapePerform
{
    DEBUG_FUNC_ENTER();
    NSRect parentFrame = NSZeroRect;
    NSPoint parentPos  = NSZeroPoint;
    NSPoint childPos   = NSZeroPoint;
    NSRect childFrame  = NSZeroRect;
    NSRect newFrame    = NSZeroRect;

    DEBUG_MSG(("OVIW(%p): vboxReshapePerform\n", (void*)self));
    
    parentFrame = [m_pParentView frame];
    DEBUG_MSG(("FIXED parentFrame [%f:%f], [%f:%f]\n", parentFrame.origin.x, parentFrame.origin.y, parentFrame.size.width, parentFrame.size.height));    
    parentPos = parentFrame.origin;
    parentPos.y += parentFrame.size.height;
    DEBUG_MSG(("FIXED(view) parentPos [%f:%f]\n", parentPos.x, parentPos.y));
    parentPos = [m_pParentView convertPoint:parentPos toView:nil];
    DEBUG_MSG(("FIXED parentPos(win) [%f:%f]\n", parentPos.x, parentPos.y));
    parentPos = [[m_pParentView window] convertBaseToScreen:parentPos];
    DEBUG_MSG(("FIXED parentPos(screen) [%f:%f]\n", parentPos.x, parentPos.y));
    parentFrame.origin = parentPos;
    
    childPos = NSMakePoint(m_Pos.x, m_Pos.y + m_Size.height);
    DEBUG_MSG(("FIXED(view) childPos [%f:%f]\n", childPos.x, childPos.y));
    childPos = [m_pParentView convertPoint:childPos toView:nil];
    DEBUG_MSG(("FIXED(win) childPos [%f:%f]\n", childPos.x, childPos.y));
    childPos = [[m_pParentView window] convertBaseToScreen:childPos];
    DEBUG_MSG(("FIXED childPos(screen) [%f:%f]\n", childPos.x, childPos.y));
    childFrame = NSMakeRect(childPos.x, childPos.y, m_Size.width, m_Size.height);
    DEBUG_MSG(("FIXED childFrame [%f:%f], [%f:%f]\n", childFrame.origin.x, childFrame.origin.y, childFrame.size.width, childFrame.size.height));        

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
        VBOX_CR_RENDER_CTX_INFO CtxInfo; 
        vboxCtxEnter(m_pSharedGLCtx, &CtxInfo);
    
        [self updateViewportCS];
    
        vboxCtxLeave(&CtxInfo);
    }
    DEBUG_FUNC_LEAVE();
}

- (void)createDockTile
{
    DEBUG_FUNC_ENTER();
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
    DEBUG_FUNC_LEAVE();
}

- (void)deleteDockTile
{
    DEBUG_FUNC_ENTER();
	if (m_DockTileView != nil)
    {
        [m_DockTileView removeFromSuperview];
        [m_DockTileView release];
        m_DockTileView = nil;
    }
    DEBUG_FUNC_LEAVE();
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
            [m_pGLCtx makeCurrentContext];
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

- (bool)vboxSharedCtxCreate
{
    DEBUG_FUNC_ENTER();
    if (m_pSharedGLCtx)
    {
        DEBUG_FUNC_LEAVE();
        return true;
    }
        
    Assert(!m_pBlitter);
    m_pBlitter = RTMemAlloc(sizeof (*m_pBlitter));
    if (!m_pBlitter)
    {
        DEBUG_WARN(("m_pBlitter allocation failed"));
        DEBUG_FUNC_LEAVE();
        return false;
    }
        
    int rc = CrBltInit(m_pBlitter, NULL, false, false, &render_spu.GlobalShaders, &render_spu.blitterDispatch);
    if (RT_SUCCESS(rc))
    {
        DEBUG_MSG(("blitter created successfully for view 0x%p\n", (void*)self));
    }
    else
    {
        DEBUG_WARN(("CrBltInit failed, rc %d", rc));
        RTMemFree(m_pBlitter);
        m_pBlitter = NULL;
        DEBUG_FUNC_LEAVE();
        return false;
    }        
    
    GLint opaque       = 0;
    /* Create a shared context out of the main context. Use the same pixel format. */
    NSOpenGLContext *pSharedGLCtx = [[NSOpenGLContext alloc] initWithFormat:[(OverlayOpenGLContext*)m_pGLCtx openGLPixelFormat] shareContext:m_pGLCtx];
        
    /* Set the new context as non opaque */
    [pSharedGLCtx setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];
    /* Set this view as the drawable for the new context */
    [pSharedGLCtx setView: self];
    m_fNeedViewportUpdate = true;
    
    m_pSharedGLCtx = pSharedGLCtx;
    
    DEBUG_FUNC_LEAVE();
    return true;
}

- (void)vboxTryDraw
{
    glFlush();
                  
    DEBUG_MSG(("My[%p]: Draw\n", self));      
    /* issue to the gui thread */
    [self performSelectorOnMainThread:@selector(vboxTryDrawUI) withObject:nil waitUntilDone:NO];
}

typedef struct CR_RCD_SETVISIBLE
{
    OverlayView *pView;
    BOOL fVisible;
} CR_RCD_SETVISIBLE;

static DECLCALLBACK(void) vboxRcdSetVisible(void *pvCb)
{
    DEBUG_FUNC_ENTER();
    CR_RCD_SETVISIBLE * pVisible = (CR_RCD_SETVISIBLE*)pvCb;
    
    [pVisible->pView vboxSetVisibleUI:pVisible->fVisible];
    DEBUG_FUNC_LEAVE();
}

- (void)vboxSetVisible:(GLboolean)fVisible
{
    DEBUG_FUNC_ENTER();
    
    VBoxMainThreadTaskRunner *pRunner = [VBoxMainThreadTaskRunner globalInstance];
    NSNumber* pVisObj = [NSNumber numberWithBool:fVisible];
    [pRunner addObj:self selector:@selector(vboxSetVisibleUIObj:) arg:pVisObj];

    DEBUG_FUNC_LEAVE();
}

- (void)vboxSetVisibleUI:(GLboolean)fVisible
{
    DEBUG_FUNC_ENTER();
    [self setHidden: !fVisible];
    DEBUG_FUNC_LEAVE();
}

- (void)vboxSetVisibleUIObj:(NSNumber*)pVisible
{
    DEBUG_FUNC_ENTER();
    BOOL fVisible = [pVisible boolValue];
    [self vboxSetVisibleUI:fVisible];
    DEBUG_FUNC_LEAVE();
}

typedef struct CR_RCD_REPARENT
{
    OverlayView *pView;
    NSView *pParent;
} CR_RCD_REPARENT;

static DECLCALLBACK(void) vboxRcdReparent(void *pvCb)
{
    DEBUG_FUNC_ENTER();
    CR_RCD_REPARENT * pReparent = (CR_RCD_REPARENT*)pvCb;
    [pReparent->pView vboxReparentUI:pReparent->pParent];
    DEBUG_FUNC_LEAVE();
}

- (void)vboxReparent:(NSView*)pParentView
{
    DEBUG_FUNC_ENTER();
    
    VBoxMainThreadTaskRunner *pRunner = [VBoxMainThreadTaskRunner globalInstance];
    [pRunner addObj:self selector:@selector(vboxReparentUI:) arg:pParentView];

    DEBUG_FUNC_LEAVE();
}

- (void)vboxReparentUI:(NSView*)pParentView
{
    DEBUG_FUNC_ENTER();
    /* Make sure the window is removed from any previous parent window. */
    if ([[self overlayWin] parentWindow] != nil)
    {
        [[[self overlayWin] parentWindow] removeChildWindow:[self overlayWin]];
    }

    /* Set the new parent view */
    [self setParentView: pParentView];

    /* Add the overlay window as a child to the new parent window */
    if (pParentView != nil)
    {
        [[pParentView window] addChildWindow:[self overlayWin] ordered:NSWindowAbove];
        if ([self isEverSized])
            [self vboxReshapeOnReparentPerform];
    }
    
    DEBUG_FUNC_LEAVE();
}

- (void)vboxTryDrawUI
{
    DEBUG_MSG(("My[%p]: DrawUI\n", self));
    const VBOXVR_SCR_COMPOSITOR *pCompositor;
    
    if ([self isHidden])
    {
        DEBUG_INFO(("request to draw on a hidden view"));
        return;
    }

    if ([[self overlayWin] parentWindow] == nil)
    {
        DEBUG_INFO(("request to draw a view w/o a parent"));
        return;
    }
    
    int rc = renderspuVBoxCompositorLock(m_pWinInfo, &pCompositor);
    if (RT_FAILURE(rc))
    {
        DEBUG_WARN(("renderspuVBoxCompositorLock failed\n"));
        return;
    }

    if (!pCompositor && !m_fCleanupNeeded)
    {
        DEBUG_MSG(("My[%p]: noCompositorUI\n", self));
        renderspuVBoxCompositorUnlock(m_pWinInfo);
        return;
    }

    VBOXVR_SCR_COMPOSITOR TmpCompositor;
    
    if (pCompositor)
    {
        if (!m_pSharedGLCtx)
        {
            Assert(!m_fDataVisible);
            Assert(!m_fCleanupNeeded);
            renderspuVBoxCompositorRelease(m_pWinInfo);
            if (![self vboxSharedCtxCreate])
            {
                DEBUG_WARN(("vboxSharedCtxCreate failed\n"));
                return;
            }
            
            Assert(m_pSharedGLCtx);
            
            pCompositor = renderspuVBoxCompositorAcquire(m_pWinInfo);
            Assert(!m_fDataVisible);
            Assert(!m_fCleanupNeeded);
            if (!pCompositor)
                return;
        }
    }
    else
    {
        DEBUG_MSG(("My[%p]: NeedCleanup\n", self));
        Assert(m_fCleanupNeeded);
        CrVrScrCompositorInit(&TmpCompositor, NULL);
        pCompositor = &TmpCompositor;
    }
    
    if ([self lockFocusIfCanDraw])
    {
        [self vboxPresent:pCompositor];            
        [self unlockFocus];
    }
    else if (!m_pWinInfo->visible)
    {
        DEBUG_MSG(("My[%p]: NotVisible\n", self));
        m_fCleanupNeeded = false;
    }
    else
    {
        DEBUG_MSG(("My[%p]: Reschedule\n", self));
        [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(vboxTryDrawUI) userInfo:nil repeats:NO];
    }
    
    renderspuVBoxCompositorUnlock(m_pWinInfo);
}

- (void)swapFBO
{
    DEBUG_FUNC_ENTER();
    [m_pGLCtx flushBuffer];
    DEBUG_FUNC_LEAVE();
}

- (void)vboxPresent:(const VBOXVR_SCR_COMPOSITOR*)pCompositor
{
    VBOX_CR_RENDER_CTX_INFO CtxInfo;    
    
    DEBUG_MSG(("OVIW(%p): renderFBOToView\n", (void*)self));    
    
    Assert(pCompositor);

    vboxCtxEnter(m_pSharedGLCtx, &CtxInfo);
    
    [self vboxPresentCS:pCompositor];
    
    vboxCtxLeave(&CtxInfo);
}

- (void)vboxPresentCS:(const VBOXVR_SCR_COMPOSITOR*)pCompositor
{
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
            
            m_fCleanupNeeded = false;
            
            /* Render FBO content to the dock tile when necessary. */
            [self vboxPresentToDockTileCS:pCompositor];
            /* change to #if 0 to see thumbnail image */            
#if 1
            [self vboxPresentToViewCS:pCompositor];
#else
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            [m_pSharedGLCtx flushBuffer];
#endif
           
        }
}

DECLINLINE(void) vboxNSRectToRect(const NSRect *pR, RTRECT *pRect)
{
    pRect->xLeft = (int)pR->origin.x;
    pRect->yTop = (int)pR->origin.y;
    pRect->xRight = (int)(pR->origin.x + pR->size.width);
    pRect->yBottom = (int)(pR->origin.y + pR->size.height);
}

DECLINLINE(void) vboxNSRectToRectUnstretched(const NSRect *pR, RTRECT *pRect, float xStretch, float yStretch)
{
    pRect->xLeft = (int)(pR->origin.x / xStretch);
    pRect->yTop = (int)(pR->origin.y / yStretch);
    pRect->xRight = (int)((pR->origin.x + pR->size.width) / xStretch);
    pRect->yBottom = (int)((pR->origin.y + pR->size.height) / yStretch);
}

DECLINLINE(void) vboxNSRectToRectStretched(const NSRect *pR, RTRECT *pRect, float xStretch, float yStretch)
{
    pRect->xLeft = (int)(pR->origin.x * xStretch);
    pRect->yTop = (int)(pR->origin.y * yStretch);
    pRect->xRight = (int)((pR->origin.x + pR->size.width) * xStretch);
    pRect->yBottom = (int)((pR->origin.y + pR->size.height) * yStretch);
}

- (void)vboxPresentToViewCS:(const VBOXVR_SCR_COMPOSITOR*)pCompositor
{
    NSRect r = [self frame];
    float xStretch, yStretch;
    DEBUG_MSG(("OVIW(%p): rF2V frame: [%i, %i, %i, %i]\n", (void*)self, (int)r.origin.x, (int)r.origin.y, (int)r.size.width, (int)r.size.height));

#if 1 /* Set to 0 to see the docktile instead of the real output */
    VBOXVR_SCR_COMPOSITOR_CONST_ITERATOR CIter;
    const VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry;
        
    CrVrScrCompositorConstIterInit(pCompositor, &CIter);
        
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
    glDrawBuffer(GL_BACK);

    /* Clear background to transparent */
    glClear(GL_COLOR_BUFFER_BIT);
    
    m_fDataVisible = false;
    
    CrVrScrCompositorGetStretching(pCompositor, &xStretch, &yStretch);
        
    while ((pEntry = CrVrScrCompositorConstIterNext(&CIter)) != NULL)
    {
        uint32_t cRegions;
        const RTRECT *paSrcRegions, *paDstRegions;
        int rc = CrVrScrCompositorEntryRegionsGet(pCompositor, pEntry, &cRegions, &paSrcRegions, &paDstRegions, NULL);
        uint32_t fFlags = CrVrScrCompositorEntryFlagsCombinedGet(pCompositor, pEntry);
        if (RT_SUCCESS(rc))
        {
            uint32_t i;
            int rc = CrBltEnter(m_pBlitter);
            if (RT_SUCCESS(rc))
            {                   
                for (i = 0; i < cRegions; ++i)
                {
                    const RTRECT * pSrcRect = &paSrcRegions[i];
                    const RTRECT * pDstRect = &paDstRegions[i];
                    RTRECT DstRect, RestrictDstRect;
                    RTRECT SrcRect, RestrictSrcRect;

                    vboxNSRectToRect(&m_RootRect, &RestrictDstRect);
                    VBoxRectIntersected(&RestrictDstRect, pDstRect, &DstRect);
                    
                    if (VBoxRectIsZero(&DstRect))
                        continue;

                    VBoxRectTranslate(&DstRect, -RestrictDstRect.xLeft, -RestrictDstRect.yTop);

                    vboxNSRectToRectUnstretched(&m_RootRect, &RestrictSrcRect, xStretch, yStretch);
                    VBoxRectTranslate(&RestrictSrcRect, -CrVrScrCompositorEntryRectGet(pEntry)->xLeft, -CrVrScrCompositorEntryRectGet(pEntry)->yTop);
                    VBoxRectIntersected(&RestrictSrcRect, pSrcRect, &SrcRect);
                    
                    if (VBoxRectIsZero(&SrcRect))
                        continue;

                    pSrcRect = &SrcRect;
                    pDstRect = &DstRect;
                    
                    const CR_TEXDATA *pTexData = CrVrScrCompositorEntryTexGet(pEntry);
                    
                    CrBltBlitTexMural(m_pBlitter, true, CrTdTexGet(pTexData), pSrcRect, pDstRect, 1, fFlags | CRBLT_F_NOALPHA);
                    
                    m_fDataVisible = true;
                }
                CrBltLeave(m_pBlitter);
            }
            else
            {
                DEBUG_WARN(("CrBltEnter failed rc %d", rc));
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

- (void)presentComposition:(const VBOXVR_SCR_COMPOSITOR_ENTRY*)pChangedEntry
{
    [self vboxTryDraw];
}

- (void)vboxBlitterSyncWindow
{
    CR_BLITTER_WINDOW WinInfo;
    NSRect r;
    
    if (!m_pBlitter)
        return;
        
    memset(&WinInfo, 0, sizeof (WinInfo));
    
    r = [self frame];
    WinInfo.width = r.size.width;
    WinInfo.height = r.size.height;
    
    Assert(WinInfo.width == m_RootRect.size.width);
    Assert(WinInfo.height == m_RootRect.size.height);

    /*CrBltMuralSetCurrentInfo(m_pBlitter, NULL);*/
    
    CrBltMuralSetCurrentInfo(m_pBlitter, &WinInfo);
    CrBltCheckUpdateViewport(m_pBlitter);
}

#ifdef VBOX_WITH_CRDUMPER_THUMBNAIL
static int g_cVBoxTgaCtr = 0;
#endif
- (void)vboxPresentToDockTileCS:(const VBOXVR_SCR_COMPOSITOR*)pCompositor
{
    NSRect r        = [self frame];
    NSRect rr       = NSZeroRect;
    GLint i         = 0;
    NSDockTile *pDT = nil;
    float xStretch, yStretch;

    if ([m_DockTileView thumbBitmap] != nil)
    {
        /* Only update after at least 200 ms, cause glReadPixels is
         * heavy performance wise. */
        uint64_t uiNewTime = RTTimeMilliTS();
        VBOXVR_SCR_COMPOSITOR_CONST_ITERATOR CIter;
        const VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry;
        
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
            
            CrVrScrCompositorGetStretching(pCompositor, &xStretch, &yStretch);
            
            CrVrScrCompositorConstIterInit(pCompositor, &CIter);
            while ((pEntry = CrVrScrCompositorConstIterNext(&CIter)) != NULL)
            {
                uint32_t cRegions;
                const RTRECT *paSrcRegions, *paDstRegions;
                int rc = CrVrScrCompositorEntryRegionsGet(pCompositor, pEntry, &cRegions, &paSrcRegions, &paDstRegions, NULL);
                uint32_t fFlags = CrVrScrCompositorEntryFlagsCombinedGet(pCompositor, pEntry);
                if (RT_SUCCESS(rc))
                {
                    uint32_t i;
                    int rc = CrBltEnter(m_pBlitter);
                    if (RT_SUCCESS(rc))
                    {                   
                        for (i = 0; i < cRegions; ++i)
                        {
                            const RTRECT * pSrcRect = &paSrcRegions[i];
                            const RTRECT * pDstRect = &paDstRegions[i];
                            RTRECT SrcRect, DstRect, RestrictSrcRect, RestrictDstRect;
                    
                            vboxNSRectToRect(&m_RootRect, &RestrictDstRect);
                            VBoxRectIntersected(&RestrictDstRect, pDstRect, &DstRect);
                            
                            VBoxRectTranslate(&DstRect, -RestrictDstRect.xLeft, -RestrictDstRect.yTop);
                            
                            VBoxRectScale(&DstRect, m_FBOThumbScaleX, m_FBOThumbScaleY);
                    
                            if (VBoxRectIsZero(&DstRect))
                                continue;
                        
                            vboxNSRectToRectUnstretched(&m_RootRect, &RestrictSrcRect, xStretch, yStretch);
                            VBoxRectTranslate(&RestrictSrcRect, -CrVrScrCompositorEntryRectGet(pEntry)->xLeft, -CrVrScrCompositorEntryRectGet(pEntry)->yTop);
                            VBoxRectIntersected(&RestrictSrcRect, pSrcRect, &SrcRect);
                    
                            if (VBoxRectIsZero(&SrcRect))
                                continue;

                            pSrcRect = &SrcRect;
                            pDstRect = &DstRect;
                            
                            const CR_TEXDATA *pTexData = CrVrScrCompositorEntryTexGet(pEntry);
                            
                            CrBltBlitTexMural(m_pBlitter, true, CrTdTexGet(pTexData), pSrcRect, pDstRect, 1, fFlags);
                        }
                        CrBltLeave(m_pBlitter);
                    }
                    else
                    {
                        DEBUG_WARN(("CrBltEnter failed rc %d", rc));
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
            glReadPixels(0, m_RootRect.size.height - rr.size.height, rr.size.width, rr.size.height,
                         GL_BGRA,
                         GL_UNSIGNED_INT_8_8_8_8,
                         [[m_DockTileView thumbBitmap] bitmapData]);
            [m_DockTileView unlock];
            
#ifdef VBOX_WITH_CRDUMPER_THUMBNAIL
            ++g_cVBoxTgaCtr;
            crDumpNamedTGAF((GLint)rr.size.width, (GLint)rr.size.height, 
                [[m_DockTileView thumbBitmap] bitmapData], "/Users/leo/vboxdumps/dump%d.tga", g_cVBoxTgaCtr);
#endif                

            pDT = [[NSApplication sharedApplication] dockTile];

            /* Send a display message to the dock tile in the main thread */
            [[[NSApplication sharedApplication] dockTile] performSelectorOnMainThread:@selector(display) withObject:nil waitUntilDone:NO];
        }
    }
}

- (void)clearVisibleRegions
{
    DEBUG_FUNC_ENTER();
    if(m_paClipRects)
    {
        RTMemFree(m_paClipRects);
        m_paClipRects = NULL;
    }
    m_cClipRects = 0;
    DEBUG_FUNC_LEAVE();
}

- (GLboolean)vboxNeedsEmptyPresent
{
    if (m_fDataVisible)
    {
        m_fCleanupNeeded = true;
        return GL_TRUE;
    }
    
    return GL_FALSE;
}

- (void)setVisibleRegions:(GLint)cRects paRects:(const GLint*)paRects
{
    DEBUG_FUNC_ENTER();
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
    
    DEBUG_FUNC_LEAVE();
}

- (NSView*)dockTileScreen
{
    DEBUG_FUNC_ENTER();
    NSView *contentView = [[[NSApplication sharedApplication] dockTile] contentView];
    NSView *screenContent = nil;
    /* First try the new variant which checks if this window is within the
       screen which is previewed in the dock. */
    if ([contentView respondsToSelector:@selector(screenContentWithParentView:)])
         screenContent = [contentView performSelector:@selector(screenContentWithParentView:) withObject:(id)m_pParentView];
    /* If it fails, fall back to the old variant (VBox...) */
    else if ([contentView respondsToSelector:@selector(screenContent)])
         screenContent = [contentView performSelector:@selector(screenContent)];
    
    DEBUG_FUNC_LEAVE();
    return screenContent;
}

- (void)reshapeDockTile
{
    DEBUG_FUNC_ENTER();
    NSRect newFrame = NSZeroRect;

    NSView *pView = [self dockTileScreen];
    if (pView != nil)
    {
        NSRect dockFrame = [pView frame];
        /* todo: this is not correct, we should use framebuffer size here, while parent view frame size may differ in case of scrolling */
        NSRect parentFrame = [m_pParentView frame];

        m_FBOThumbScaleX = (float)dockFrame.size.width / parentFrame.size.width;
        m_FBOThumbScaleY = (float)dockFrame.size.height / parentFrame.size.height;
        newFrame = NSMakeRect((int)(m_Pos.x * m_FBOThumbScaleX), (int)(dockFrame.size.height - (m_Pos.y + m_Size.height - m_yInvRootOffset) * m_FBOThumbScaleY), (int)(m_Size.width * m_FBOThumbScaleX), (int)(m_Size.height * m_FBOThumbScaleY));
        /*
        NSRect newFrame = NSMakeRect ((int)roundf(m_Pos.x * m_FBOThumbScaleX), (int)roundf(dockFrame.size.height - (m_Pos.y + m_Size.height) * m_FBOThumbScaleY), (int)roundf(m_Size.width * m_FBOThumbScaleX), (int)roundf(m_Size.height * m_FBOThumbScaleY));
        NSRect newFrame = NSMakeRect ((m_Pos.x * m_FBOThumbScaleX), (dockFrame.size.height - (m_Pos.y + m_Size.height) * m_FBOThumbScaleY), (m_Size.width * m_FBOThumbScaleX), (m_Size.height * m_FBOThumbScaleY));
        printf ("%f %f %f %f - %f %f\n", newFrame.origin.x, newFrame.origin.y, newFrame.size.width, newFrame.size.height, m_Size.height, m_FBOThumbScaleY);
        */
        [m_DockTileView setFrame: newFrame];
    }
    DEBUG_FUNC_LEAVE();
}

@end

/********************************************************************************
*
* OpenGL context management
*
********************************************************************************/
void cocoaGLCtxCreate(NativeNSOpenGLContextRef *ppCtx, GLbitfield fVisParams, NativeNSOpenGLContextRef pSharedCtx)
{
    DEBUG_FUNC_ENTER();
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
    
    DEBUG_FUNC_LEAVE();
}

void cocoaGLCtxDestroy(NativeNSOpenGLContextRef pCtx)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [pCtx release];
    /*[pCtx performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];*/

    [pPool release];
    DEBUG_FUNC_LEAVE();
}

/********************************************************************************
*
* View management
*
********************************************************************************/
typedef struct CR_RCD_CREATEVIEW
{
    WindowInfo *pWinInfo;
    NSView *pParentView;
    GLbitfield fVisParams;
    /* out */
    OverlayView *pView;
} CR_RCD_CREATEVIEW;

static OverlayView * vboxViewCreate(WindowInfo *pWinInfo, NativeNSViewRef pParentView)
{
    DEBUG_FUNC_ENTER();
    /* Create our worker view */
    OverlayView* pView = [[OverlayView alloc] initWithFrame:NSZeroRect thread:RTThreadSelf() parentView:pParentView winInfo:pWinInfo];

    if (pView)
    {
        /* We need a real window as container for the view */
        [[OverlayWindow alloc] initWithParentView:pParentView overlayView:pView];
        /* Return the freshly created overlay view */
        DEBUG_FUNC_LEAVE();
        return pView;
    }
    
    DEBUG_FUNC_LEAVE();
    return NULL;
}

static DECLCALLBACK(void) vboxRcdCreateView(void *pvCb)
{
    DEBUG_FUNC_ENTER();
    CR_RCD_CREATEVIEW * pCreateView = (CR_RCD_CREATEVIEW*)pvCb;
    pCreateView->pView = vboxViewCreate(pCreateView->pWinInfo, pCreateView->pParentView);
    DEBUG_FUNC_LEAVE();
}

void cocoaViewCreate(NativeNSViewRef *ppView, WindowInfo *pWinInfo, NativeNSViewRef pParentView, GLbitfield fVisParams)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    VBoxMainThreadTaskRunner *pRunner = [VBoxMainThreadTaskRunner globalInstance];
    /* make sure all tasks are run, to preserve the order */
    [pRunner runTasksSyncIfPossible];
    
    renderspuWinRetain(pWinInfo);

    if (renderspuCalloutAvailable())
    {
        CR_RCD_CREATEVIEW CreateView;
        CreateView.pWinInfo = pWinInfo;
        CreateView.pParentView = pParentView;
        CreateView.fVisParams = fVisParams;
        CreateView.pView = NULL;
        renderspuCalloutClient(vboxRcdCreateView, &CreateView);
        *ppView = CreateView.pView;
    }
    else
    {
        DEBUG_MSG(("no callout available on createWindow\n"));
#if 0
        dispatch_sync(dispatch_get_main_queue(), ^{
#endif
            *ppView = vboxViewCreate(pWinInfo, pParentView);
#if 0
        });
#endif
    }
    
    if (!*ppView)
        renderspuWinRelease(pWinInfo);
    
    [pPool release];
    
    DEBUG_FUNC_LEAVE();
}

void cocoaViewReparent(NativeNSViewRef pView, NativeNSViewRef pParentView)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    OverlayView* pOView = (OverlayView*)pView;

    if (pOView)
    {
        [pOView vboxReparent:pParentView];
    }

    [pPool release];
    
    DEBUG_FUNC_LEAVE();
}

typedef struct CR_RCD_DESTROYVIEW
{
    OverlayView *pView;
} CR_RCD_DESTROYVIEW;

static DECLCALLBACK(void) vboxRcdDestroyView(void *pvCb)
{
    DEBUG_FUNC_ENTER();
    CR_RCD_DESTROYVIEW * pDestroyView = (CR_RCD_DESTROYVIEW*)pvCb;
    [pDestroyView->pView vboxDestroy];
    DEBUG_FUNC_LEAVE();
}

void cocoaViewDestroy(NativeNSViewRef pView)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    VBoxMainThreadTaskRunner *pRunner = [VBoxMainThreadTaskRunner globalInstance];
    [pRunner addObj:pView selector:@selector(vboxDestroy) arg:nil];

    [pPool release];

    DEBUG_FUNC_LEAVE();
}

void cocoaViewShow(NativeNSViewRef pView, GLboolean fShowIt)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView vboxSetVisible:fShowIt];

    [pPool release];
    DEBUG_FUNC_LEAVE();
}

void cocoaViewDisplay(NativeNSViewRef pView)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    DEBUG_WARN(("cocoaViewDisplay should never happen!\n"));
    DEBUG_MSG_1(("cocoaViewDisplay %p\n", (void*)pView));
    [(OverlayView*)pView swapFBO];

    [pPool release];
    
    DEBUG_FUNC_LEAVE();
}

void cocoaViewSetPosition(NativeNSViewRef pView, NativeNSViewRef pParentView, int x, int y)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView vboxSetPos:NSMakePoint(x, y)];

    [pPool release];
    
    DEBUG_FUNC_LEAVE();
}

void cocoaViewSetSize(NativeNSViewRef pView, int w, int h)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView vboxSetSize:NSMakeSize(w, h)];

    [pPool release];
    
    DEBUG_FUNC_LEAVE();
}

typedef struct CR_RCD_GETGEOMETRY
{
    OverlayView *pView;
    NSRect rect;
} CR_RCD_GETGEOMETRY;

static DECLCALLBACK(void) vboxRcdGetGeomerty(void *pvCb)
{
    DEBUG_FUNC_ENTER();
    CR_RCD_GETGEOMETRY * pGetGeometry = (CR_RCD_GETGEOMETRY*)pvCb;
    pGetGeometry->rect = [[pGetGeometry->pView window] frame];
    DEBUG_FUNC_LEAVE();
}

void cocoaViewGetGeometry(NativeNSViewRef pView, int *pX, int *pY, int *pW, int *pH)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    NSRect frame;
    VBoxMainThreadTaskRunner *pRunner = [VBoxMainThreadTaskRunner globalInstance];
    /* make sure all tasks are run, to preserve the order */
    [pRunner runTasksSyncIfPossible];
    
    
    if (renderspuCalloutAvailable())
    {
        CR_RCD_GETGEOMETRY GetGeometry;
        GetGeometry.pView = (OverlayView*)pView;
        renderspuCalloutClient(vboxRcdGetGeomerty, &GetGeometry);
        frame = GetGeometry.rect;
    }
    else
    {
        DEBUG_MSG(("no callout available on getGeometry\n"));
        frame = [[pView window] frame];
    }
    
    *pX = frame.origin.x;
    *pY = frame.origin.y;
    *pW = frame.size.width;
    *pH = frame.size.height;

    [pPool release];
    
    DEBUG_FUNC_LEAVE();
}

void cocoaViewPresentComposition(NativeNSViewRef pView, const struct VBOXVR_SCR_COMPOSITOR_ENTRY *pChangedEntry)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
    NSOpenGLContext *pCtx;
    
    /* view should not necesserily have a context set */
    pCtx = [(OverlayView*)pView glCtx];
    if (!pCtx)
    {
        ContextInfo * pCtxInfo = renderspuDefaultSharedContextAcquire();
        if (!pCtxInfo)
        {
            DEBUG_WARN(("renderspuDefaultSharedContextAcquire returned NULL"));
            
            [pPool release];
            DEBUG_FUNC_LEAVE();
            return;
        }
        
        pCtx = pCtxInfo->context;
        
        [(OverlayView*)pView setGLCtx:pCtx];
    }
    
    [(OverlayView*)pView presentComposition:pChangedEntry];

    [pPool release];
    
    DEBUG_FUNC_LEAVE();
}

void cocoaViewMakeCurrentContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    DEBUG_MSG(("cocoaViewMakeCurrentContext(%p, %p)\n", (void*)pView, (void*)pCtx));

    if (pView)
    {
        [(OverlayView*)pView setGLCtx:pCtx];
        [(OverlayView*)pView makeCurrentFBO];
    }
    else
    {
    	[NSOpenGLContext clearCurrentContext];
    }

    [pPool release];
    
    DEBUG_FUNC_LEAVE();
}

GLboolean cocoaViewNeedsEmptyPresent(NativeNSViewRef pView)
{
    DEBUG_FUNC_ENTER();
    
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    GLboolean fNeedsPresent = [(OverlayView*)pView vboxNeedsEmptyPresent];

    [pPool release];
    
    DEBUG_FUNC_LEAVE();
    
    return fNeedsPresent;
}

void cocoaViewSetVisibleRegion(NativeNSViewRef pView, GLint cRects, const GLint* paRects)
{
    DEBUG_FUNC_ENTER();
    NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

    [(OverlayView*)pView setVisibleRegions:cRects paRects:paRects];

    [pPool release];
    
    DEBUG_FUNC_LEAVE();
}
