/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxFrameBuffer class and subclasses declarations
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef ___VBoxFrameBuffer_h___
#define ___VBoxFrameBuffer_h___
//#define VBOXQGL_PROF_BASE 1
//#define VBOXQGL_DBG_SURF 1
#include "COMDefs.h"
#include <iprt/critsect.h>

/* Qt includes */
#include <QImage>
#include <QPixmap>
#include <QMutex>
#include <QPaintEvent>
#include <QMoveEvent>
#if defined (VBOX_GUI_USE_QGL)
#include <QGLWidget>
#endif

#if defined (VBOX_GUI_USE_SDL)
#include <SDL.h>
#include <signal.h>
#endif

#if defined (Q_WS_WIN) && defined (VBOX_GUI_USE_DDRAW)
// VBox/cdefs.h defines these:
#undef LOWORD
#undef HIWORD
#undef LOBYTE
#undef HIBYTE
#include <ddraw.h>
#endif

class VBoxConsoleView;

/////////////////////////////////////////////////////////////////////////////

/**
 *  Frame buffer resize event.
 */
class VBoxResizeEvent : public QEvent
{
public:

    VBoxResizeEvent (ulong aPixelFormat, uchar *aVRAM,
                     ulong aBitsPerPixel, ulong aBytesPerLine,
                     ulong aWidth, ulong aHeight) :
        QEvent ((QEvent::Type) VBoxDefs::ResizeEventType),
        mPixelFormat (aPixelFormat), mVRAM (aVRAM), mBitsPerPixel (aBitsPerPixel),
        mBytesPerLine (aBytesPerLine), mWidth (aWidth), mHeight (aHeight) {}
    ulong pixelFormat() { return mPixelFormat; }
    uchar *VRAM() { return mVRAM; }
    ulong bitsPerPixel() { return mBitsPerPixel; }
    ulong bytesPerLine() { return mBytesPerLine; }
    ulong width() { return mWidth; }
    ulong height() { return mHeight; }

private:

    ulong mPixelFormat;
    uchar *mVRAM;
    ulong mBitsPerPixel;
    ulong mBytesPerLine;
    ulong mWidth;
    ulong mHeight;
};

/**
 *  Frame buffer repaint event.
 */
class VBoxRepaintEvent : public QEvent
{
public:
    VBoxRepaintEvent (int x, int y, int w, int h) :
        QEvent ((QEvent::Type) VBoxDefs::RepaintEventType),
        ex (x), ey (y), ew (w), eh (h)
    {}
    int x() { return ex; }
    int y() { return ey; }
    int width() { return ew; }
    int height() { return eh; }
private:
    int ex, ey, ew, eh;
};

/**
 *  Frame buffer set region event.
 */
class VBoxSetRegionEvent : public QEvent
{
public:
    VBoxSetRegionEvent (const QRegion &aReg)
        : QEvent ((QEvent::Type) VBoxDefs::SetRegionEventType)
        , mReg (aReg) {}
    QRegion region() { return mReg; }
private:
    QRegion mReg;
};

#ifdef VBOX_GUI_USE_QGL
typedef enum
{
    VBOXVHWA_PIPECMD_PAINT = 1,
    VBOXVHWA_PIPECMD_VHWA,
    VBOXVHWA_PIPECMD_OP,
}VBOXVHWA_PIPECMD_TYPE;

typedef DECLCALLBACK(void) FNVBOXVHWACALLBACK(void * pContext);
typedef FNVBOXVHWACALLBACK *PFNVBOXVHWACALLBACK;

typedef struct VBOXVHWACALLBACKINFO
{
    PFNVBOXVHWACALLBACK pfnCallback;
    void * pContext;
}VBOXVHWACALLBACKINFO;
class VBoxVHWACommandElement
{
public:
    void setVHWACmd(struct _VBOXVHWACMD * pCmd)
    {
        mType = VBOXVHWA_PIPECMD_VHWA;
        u.mpCmd = pCmd;
    }

    void setPaintCmd(const QRect & aRect)
    {
        mType = VBOXVHWA_PIPECMD_PAINT;
        mRect = aRect;
    }

    void setOp(const VBOXVHWACALLBACKINFO & aOp)
    {
        mType = VBOXVHWA_PIPECMD_OP;
        u.mCallback = aOp;
    }

    void setData(VBOXVHWA_PIPECMD_TYPE aType, void * pvData)
    {
        switch(aType)
        {
        case VBOXVHWA_PIPECMD_PAINT:
            setPaintCmd(*((QRect*)pvData));
            break;
        case VBOXVHWA_PIPECMD_VHWA:
            setVHWACmd((struct _VBOXVHWACMD *)pvData);
            break;
        case VBOXVHWA_PIPECMD_OP:
            setOp(*((VBOXVHWACALLBACKINFO *)pvData));
            break;
        default:
            Assert(0);
            break;
        }
    }

    VBOXVHWA_PIPECMD_TYPE type() const {return mType;}
    const QRect & rect() const {return mRect;}
    struct _VBOXVHWACMD * vhwaCmd() const {return u.mpCmd;}

private:
    VBoxVHWACommandElement * mpNext;
    VBOXVHWA_PIPECMD_TYPE mType;
    union
    {
        struct _VBOXVHWACMD * mpCmd;
        VBOXVHWACALLBACKINFO mCallback;
    }u;
    QRect                 mRect;

    friend class VBoxVHWACommandElementPipe;
    friend class VBoxVHWACommandElementStack;
    friend class VBoxGLWidget;
};

class VBoxVHWACommandElementPipe
{
public:
    VBoxVHWACommandElementPipe() :
        mpFirst(NULL),
        mpLast(NULL)
    {}

    void put(VBoxVHWACommandElement *pCmd)
    {
        if(mpLast)
        {
            Assert(mpFirst);
            mpLast->mpNext = pCmd;
            mpLast = pCmd;
        }
        else
        {
            Assert(!mpFirst);
            mpFirst = pCmd;
            mpLast = pCmd;
        }
        pCmd->mpNext= NULL;

    }

    VBoxVHWACommandElement * detachList()
    {
        if(mpLast)
        {
            VBoxVHWACommandElement * pHead = mpFirst;
            mpFirst = NULL;
            mpLast = NULL;
            return pHead;
        }
        return NULL;
    }
private:
    VBoxVHWACommandElement *mpFirst;
    VBoxVHWACommandElement *mpLast;
};

class VBoxVHWACommandElementStack
{
public:
    VBoxVHWACommandElementStack() :
        mpFirst(NULL) {}

    void push(VBoxVHWACommandElement *pCmd)
    {
        pCmd->mpNext = mpFirst;
        mpFirst = pCmd;
    }

    void pusha(VBoxVHWACommandElement *pFirst, VBoxVHWACommandElement *pLast)
    {
        pLast->mpNext = mpFirst;
        mpFirst = pFirst;
    }

    VBoxVHWACommandElement * pop()
    {
        if(mpFirst)
        {
            VBoxVHWACommandElement * ret = mpFirst;
            mpFirst = ret->mpNext;
            return ret;
        }
        return NULL;
    }
private:
    VBoxVHWACommandElement *mpFirst;
};

class VBoxVHWACommandProcessEvent : public QEvent
{
public:
    VBoxVHWACommandProcessEvent (VBoxVHWACommandElement *pEl)
        : QEvent ((QEvent::Type) VBoxDefs::VHWACommandProcessType)
    {
        mCmdPipe.put(pEl);
    }
    VBoxVHWACommandElementPipe & pipe() { return mCmdPipe; }
private:
    VBoxVHWACommandElementPipe mCmdPipe;
    VBoxVHWACommandProcessEvent *mpNext;

    friend class VBoxGLWidget;
};

#endif

/////////////////////////////////////////////////////////////////////////////

/**
 *  Common IFramebuffer implementation for all methods used by GUI to maintain
 *  the VM display video memory.
 *
 *  Note that although this class can be called from multiple threads
 *  (in particular, the GUI thread and EMT) it doesn't protect access to every
 *  data field using its mutex lock. This is because all synchronization between
 *  the GUI and the EMT thread is supposed to be done using the
 *  IFramebuffer::NotifyUpdate() and IFramebuffer::RequestResize() methods
 *  (in particular, the \a aFinished parameter of these methods is responsible
 *  for the synchronization). These methods are always called on EMT and
 *  therefore always follow one another but never in parallel.
 *
 *  Using this object's mutex lock (exposed also in IFramebuffer::Lock() and
 *  IFramebuffer::Unlock() implementations) usually makes sense only if some
 *  third-party thread (i.e. other than GUI or EMT) needs to make sure that
 *  *no* VM display update or resize event can occur while it is accessing
 *  IFramebuffer properties or the underlying display memory storage area.
 *
 *  See IFramebuffer documentation for more info.
 */

class VBoxFrameBuffer : VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
public:

    VBoxFrameBuffer (VBoxConsoleView *aView);
    virtual ~VBoxFrameBuffer();

    NS_DECL_ISUPPORTS

#if defined (Q_OS_WIN32)

    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement (&refcnt);
    }

    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement (&refcnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
#endif
    VBOX_SCRIPTABLE_DISPATCH_IMPL(IFramebuffer)

    // IFramebuffer COM methods
    STDMETHOD(COMGETTER(Address)) (BYTE **aAddress);
    STDMETHOD(COMGETTER(Width)) (ULONG *aWidth);
    STDMETHOD(COMGETTER(Height)) (ULONG *aHeight);
    STDMETHOD(COMGETTER(BitsPerPixel)) (ULONG *aBitsPerPixel);
    STDMETHOD(COMGETTER(BytesPerLine)) (ULONG *aBytesPerLine);
    STDMETHOD(COMGETTER(PixelFormat)) (ULONG *aPixelFormat);
    STDMETHOD(COMGETTER(UsesGuestVRAM)) (BOOL *aUsesGuestVRAM);
    STDMETHOD(COMGETTER(HeightReduction)) (ULONG *aHeightReduction);
    STDMETHOD(COMGETTER(Overlay)) (IFramebufferOverlay **aOverlay);
    STDMETHOD(COMGETTER(WinId)) (ULONG64 *winId);

    STDMETHOD(Lock)();
    STDMETHOD(Unlock)();

    STDMETHOD(RequestResize) (ULONG aScreenId, ULONG aPixelFormat,
                              BYTE *aVRAM, ULONG aBitsPerPixel, ULONG aBytesPerLine,
                              ULONG aWidth, ULONG aHeight,
                              BOOL *aFinished);

    STDMETHOD(VideoModeSupported) (ULONG aWidth, ULONG aHeight, ULONG aBPP,
                                   BOOL *aSupported);

    STDMETHOD(GetVisibleRegion)(BYTE *aRectangles, ULONG aCount, ULONG *aCountCopied);
    STDMETHOD(SetVisibleRegion)(BYTE *aRectangles, ULONG aCount);

    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand);

    ulong width() { return mWdt; }
    ulong height() { return mHgt; }

    virtual ulong pixelFormat()
    {
        return FramebufferPixelFormat_FOURCC_RGB;
    }

    virtual bool usesGuestVRAM()
    {
        return false;
    }

    void lock() { RTCritSectEnter(&mCritSect); }
    void unlock() { RTCritSectLeave(&mCritSect); }

    virtual uchar *address() = 0;
    virtual ulong bitsPerPixel() = 0;
    virtual ulong bytesPerLine() = 0;

    /**
     *  Called on the GUI thread (from VBoxConsoleView) when some part of the
     *  VM display viewport needs to be repainted on the host screen.
     */
    virtual void paintEvent (QPaintEvent *pe) = 0;

    /**
     *  Called on the GUI thread (from VBoxConsoleView) after it gets a
     *  VBoxResizeEvent posted from the RequestResize() method implementation.
     */
    virtual void resizeEvent (VBoxResizeEvent *re)
    {
        mWdt = re->width();
        mHgt = re->height();
    }

    /**
     *  Called on the GUI thread (from VBoxConsoleView) when the VM console
     *  window is moved.
     */
    virtual void moveEvent (QMoveEvent * /*me*/ ) {}

#ifdef VBOX_GUI_USE_QGL
    /* this method is called from the GUI thread
     * to perform the actual Video HW Acceleration command processing */
    virtual void doProcessVHWACommand(VBoxVHWACommandProcessEvent * pEvent);
#endif

protected:

    VBoxConsoleView *mView;
    RTCRITSECT mCritSect;
    int mWdt;
    int mHgt;
    uint64_t mWinId;

#if defined (Q_OS_WIN32)
private:
    long refcnt;
#endif
};

/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_QIMAGE)

class VBoxQImageFrameBuffer : public VBoxFrameBuffer
{
public:

    VBoxQImageFrameBuffer (VBoxConsoleView *aView);

    STDMETHOD(NotifyUpdate) (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH);

    ulong pixelFormat() { return mPixelFormat; }
    bool usesGuestVRAM() { return mUsesGuestVRAM; }

    uchar *address() { return mImg.bits(); }
    ulong bitsPerPixel() { return mImg.depth(); }
    ulong bytesPerLine() { return mImg.bytesPerLine(); }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);

private:

    QPixmap mPM;
    QImage mImg;
    ulong mPixelFormat;
    bool mUsesGuestVRAM;
};

#endif

/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_QGL)

#ifdef DEBUG
#include "iprt/stream.h"
#define VBOXQGLLOG(_m) RTPrintf _m
#else
#define VBOXQGLLOG(_m)
#endif
#define VBOXQGLLOG_ENTER(_m)
//do{VBOXQGLLOG(("==>[%s]:", __FUNCTION__)); VBOXQGLLOG(_m);}while(0)
#define VBOXQGLLOG_EXIT(_m)
//do{VBOXQGLLOG(("<==[%s]:", __FUNCTION__)); VBOXQGLLOG(_m);}while(0)
#ifdef DEBUG
#define VBOXQGL_ASSERTNOERR() \
    do { GLenum err = glGetError(); \
        if(err != GL_NO_ERROR) VBOXQGLLOG(("gl error ocured (0x%x)\n", err)); \
        Assert(err == GL_NO_ERROR); \
    }while(0)

#define VBOXQGL_CHECKERR(_op) \
    do { \
        glGetError(); \
        _op \
        VBOXQGL_ASSERTNOERR(); \
    }while(0)
#else
#define VBOXQGL_ASSERTNOERR() \
    do {}while(0)

#define VBOXQGL_CHECKERR(_op) \
    do { \
        _op \
    }while(0)
#endif

#ifdef DEBUG
#include <iprt/time.h>

#define VBOXGETTIME() RTTimeNanoTS()

#define VBOXPRINTDIF(_nano, _m) do{\
        uint64_t cur = VBOXGETTIME(); \
        VBOXQGLLOG(_m); \
        VBOXQGLLOG(("(%Lu)\n", cur - (_nano))); \
    }while(0)

class VBoxVHWADbgTimeCounter
{
public:
    VBoxVHWADbgTimeCounter(const char* msg) {mTime = VBOXGETTIME(); mMsg=msg;}
    ~VBoxVHWADbgTimeCounter() {VBOXPRINTDIF(mTime, (mMsg));}
private:
    uint64_t mTime;
    const char* mMsg;
};

#define VBOXQGLLOG_METHODTIME(_m) VBoxVHWADbgTimeCounter _dbgTimeCounter(_m)
#else
#define VBOXQGLLOG_METHODTIME(_m)
#endif

#define VBOXQGLLOG_QRECT(_p, _pr, _s) do{\
    VBOXQGLLOG((_p " x(%d), y(%d), w(%d), h(%d)" _s, (_pr)->x(), (_pr)->y(), (_pr)->width(), (_pr)->height()));\
    }while(0)

#define VBOXQGLLOG_CKEY(_p, _pck, _s) do{\
    VBOXQGLLOG((_p " l(0x%x), u(0x%x)" _s, (_pck)->lower(), (_pck)->upper()));\
    }while(0)

class VBoxVHWADirtyRect
{
public:
    VBoxVHWADirtyRect() :
        mIsClear(true)
    {}

    VBoxVHWADirtyRect(const QRect & aRect)
    {
        if(aRect.isEmpty())
        {
            mIsClear = false;
            mRect = aRect;
        }
        else
        {
            mIsClear = true;
        }
    }

    bool isClear() const { return mIsClear; }

    void add(const QRect & aRect)
    {
        if(aRect.isEmpty())
            return;

        mRect = mIsClear ? aRect : mRect.united(aRect);
        mIsClear = false;
    }

    void add(const VBoxVHWADirtyRect & aRect)
    {
        if(aRect.isClear())
            return;
        add(aRect.rect());
    }

    void set(const QRect & aRect)
    {
        if(aRect.isEmpty())
        {
            mIsClear = true;
        }
        else
        {
            mRect = aRect;
            mIsClear = false;
        }
    }

    void clear() { mIsClear = true; }

    const QRect & rect() const {return mRect;}

    bool intersects(const QRect & aRect) const {return mIsClear ? false : mRect.intersects(aRect);}

    bool intersects(const VBoxVHWADirtyRect & aRect) const {return mIsClear ? false : aRect.intersects(mRect);}

    QRect united(const QRect & aRect) const {return mIsClear ? aRect : aRect.united(mRect);}

    bool contains(const QRect & aRect) const {return mIsClear ? false : aRect.contains(mRect);}

    void subst(const VBoxVHWADirtyRect & aRect) { if(!mIsClear && aRect.contains(mRect)) clear(); }

private:
    QRect mRect;
    bool mIsClear;
};

class VBoxVHWAColorKey
{
public:
    VBoxVHWAColorKey() :
        mUpper(0),
        mLower(0)
    {}

    VBoxVHWAColorKey(uint32_t aUpper, uint32_t aLower) :
        mUpper(aUpper),
        mLower(aLower)
    {}

    uint32_t upper() const {return mUpper; }
    uint32_t lower() const {return mLower; }

    bool operator==(const VBoxVHWAColorKey & other) const { return mUpper == other.mUpper && mLower == other.mLower; }
private:
    uint32_t mUpper;
    uint32_t mLower;
};

class VBoxVHWAColorComponent
{
public:
    VBoxVHWAColorComponent() :
        mMask(0),
        mRange(0),
        mOffset(32),
        mcBits(0)
    {}

    VBoxVHWAColorComponent(uint32_t aMask);

    uint32_t mask() const { return mMask; }
    uint32_t range() const { return mRange; }
    uint32_t offset() const { return mOffset; }
    uint32_t cBits() const { return mcBits; }
    uint32_t colorVal(uint32_t col) const { return (col & mMask) >> mOffset; }
    float colorValNorm(uint32_t col) const { return ((float)colorVal(col))/mRange; }
private:
    uint32_t mMask;
    uint32_t mRange;
    uint32_t mOffset;
    uint32_t mcBits;
};

class VBoxVHWAColorFormat
{
public:

//    VBoxVHWAColorFormat(GLint aInternalFormat, GLenum aFormat, GLenum aType, uint32_t aDataFormat);
    VBoxVHWAColorFormat(uint32_t bitsPerPixel, uint32_t r, uint32_t g, uint32_t b);
    VBoxVHWAColorFormat(uint32_t fourcc);
    VBoxVHWAColorFormat(){}
    GLint internalFormat() const {return mInternalFormat; }
    GLenum format() const {return mFormat; }
    GLenum type() const {return mType; }
    bool isValid() const {return mBitsPerPixel != 0; }
    uint32_t fourcc() const {return mDataFormat;}
    uint32_t bitsPerPixel() const { return mBitsPerPixel; }
    uint32_t bitsPerPixelTex() const { return mBitsPerPixelTex; }
//    uint32_t bitsPerPixelDd() const { return mBitsPerPixelDd; }
    void pixel2Normalized(uint32_t pix, float *r, float *g, float *b) const;
    uint32_t widthCompression() const {return mWidthCompression;}
    uint32_t heightCompression() const {return mHeightCompression;}
    const VBoxVHWAColorComponent& r() const {return mR;}
    const VBoxVHWAColorComponent& g() const {return mG;}
    const VBoxVHWAColorComponent& b() const {return mB;}
    const VBoxVHWAColorComponent& a() const {return mA;}

private:
    void VBoxVHWAColorFormat::init(uint32_t bitsPerPixel, uint32_t r, uint32_t g, uint32_t b);
    void VBoxVHWAColorFormat::init(uint32_t fourcc);

    GLint mInternalFormat;
    GLenum mFormat;
    GLenum mType;
    uint32_t mDataFormat;

    uint32_t mBitsPerPixel;
    uint32_t mBitsPerPixelTex;
//    uint32_t mBitsPerPixelDd;
    uint32_t mWidthCompression;
    uint32_t mHeightCompression;
    VBoxVHWAColorComponent mR;
    VBoxVHWAColorComponent mG;
    VBoxVHWAColorComponent mB;
    VBoxVHWAColorComponent mA;
};

class VBoxVHWATexture
{
public:
    VBoxVHWATexture() {}
    VBoxVHWATexture(const QRect * pRect, const VBoxVHWAColorFormat *pFormat);
    virtual ~VBoxVHWATexture();
    virtual void init(uchar *pvMem);
    void setAddress(uchar *pvMem) {mAddress = pvMem;}
    void update(const QRect * pRect) { doUpdate(mAddress, pRect);}
    void bind() {glBindTexture(texTarget(), mTexture);}

    virtual void texCoord(int x, int y);
    virtual void multiTexCoord(GLenum texUnit, int x, int y);

//    GLuint texture() {return mTexture;}
    const QRect & texRect() {return mTexRect;}
    const QRect & rect() {return mRect;}
    uchar * address(){ return mAddress; }
    uint32_t rectSizeTex(const QRect * pRect) {return pRect->width() * pRect->height() * mBytesPerPixelTex;}
    uchar * pointAddress(int x, int y)
    {
        x = toXTex(x);
        y = toYTex(y);
        return pointAddressTex(x, y);
    }
    uint32_t pointOffsetTex(int x, int y) { return y*mBytesPerLine + x*mBytesPerPixelTex; }
    uchar * pointAddressTex(int x, int y) { return mAddress + pointOffsetTex(x, y); }
    int toXTex(int x) {return x/mColorFormat.widthCompression();}
    int toYTex(int y) {return y/mColorFormat.heightCompression();}
    ulong memSize(){ return mBytesPerLine * mRect.height(); }
    uint32_t bytesPerLine() {return mBytesPerLine; }

protected:
    virtual void doUpdate(uchar * pAddress, const QRect * pRect);
    virtual void initParams();
    virtual void load();
    virtual GLenum texTarget() {return GL_TEXTURE_2D; }


    QRect mTexRect; /* texture size */
    QRect mRect; /* img size */
    uchar * mAddress;
    GLuint mTexture;
    uint32_t mBytesPerPixel;
    uint32_t mBytesPerPixelTex;
    uint32_t mBytesPerLine;
    VBoxVHWAColorFormat mColorFormat;
private:
    void uninit();
};

class VBoxVHWATextureNP2 : public VBoxVHWATexture
{
public:
    VBoxVHWATextureNP2() : VBoxVHWATexture() {}
    VBoxVHWATextureNP2(const QRect * pRect, const VBoxVHWAColorFormat *pFormat) :
        VBoxVHWATexture(pRect, pFormat){
        mTexRect = QRect(0, 0, pRect->width()/pFormat->widthCompression(), pRect->height()/pFormat->heightCompression());
    }
};

class VBoxVHWATextureNP2Rect : public VBoxVHWATextureNP2
{
public:
    VBoxVHWATextureNP2Rect() : VBoxVHWATextureNP2() {}
    VBoxVHWATextureNP2Rect(const QRect * pRect, const VBoxVHWAColorFormat *pFormat) :
        VBoxVHWATextureNP2(pRect, pFormat){}

    virtual void texCoord(int x, int y);
    virtual void multiTexCoord(GLenum texUnit, int x, int y);
protected:
    virtual GLenum texTarget();
};

class VBoxVHWATextureNP2RectPBO : public VBoxVHWATextureNP2Rect
{
public:
	VBoxVHWATextureNP2RectPBO() : VBoxVHWATextureNP2Rect() {}
	VBoxVHWATextureNP2RectPBO(const QRect * pRect, const VBoxVHWAColorFormat *pFormat) :
		VBoxVHWATextureNP2Rect(pRect, pFormat){}
    virtual ~VBoxVHWATextureNP2RectPBO();

    virtual void init(uchar *pvMem);
protected:
    virtual void load();
    virtual void doUpdate(uchar * pAddress, const QRect * pRect);
private:
    void updateBuffer(uchar * pBuf, const QRect * pRect);
    GLuint mPBO;
};

class VBoxVHWAHandleTable
{
public:
    VBoxVHWAHandleTable(uint32_t initialSize);
    ~VBoxVHWAHandleTable();
    uint32_t put(void * data);
    bool mapPut(uint32_t h, void * data);
    void* get(uint32_t h);
    void* remove(uint32_t h);
private:
    void doPut(uint32_t h, void * data);
    void doRemove(uint32_t h);
    void** mTable;
    uint32_t mcSize;
    uint32_t mcUsage;
    uint32_t mCursor;
};

/* data flow:
 * I. NON-Yinverted surface:
 * 1.direct memory update (paint, lock/unlock):
 *  mem->tex->fb
 * 2.blt
 *  srcTex->invFB->tex->fb
 *              |->mem
 *
 * II. Yinverted surface:
 * 1.direct memory update (paint, lock/unlock):
 *  mem->tex->fb
 * 2.blt
 *  srcTex->fb->tex
 *           |->mem
 *
 * III. flip support:
 * 1. Yinverted<->NON-YInverted conversion :
 *  mem->tex-(rotate model view, force LAZY complete fb update)->invFB->tex
 *  fb-->|                                                           |->mem
 * */
class VBoxVHWASurfaceBase
{
public:
    VBoxVHWASurfaceBase(
            class VBoxGLWidget *mWidget,
#if 0
            class VBoxVHWAGlContextState *aState,
            bool aIsYInverted,
#endif
            const QSize * aSize, const QSize * aTargetSize,
            VBoxVHWAColorFormat & aColorFormat,
            VBoxVHWAColorKey * pSrcBltCKey, VBoxVHWAColorKey * pDstBltCKey,
            VBoxVHWAColorKey * pSrcOverlayCKey, VBoxVHWAColorKey * pDstOverlayCKey,
            bool bVGA);

    virtual ~VBoxVHWASurfaceBase();

    void init(VBoxVHWASurfaceBase * pPrimary, uchar *pvMem);
    void setupMatricies(VBoxVHWASurfaceBase *pPrimary);

    void uninit();

    static void globalInit();

//    int blt(const QRect * aDstRect, VBoxVHWASurfaceBase * aSrtSurface, const QRect * aSrcRect, const VBoxVHWAColorKey * pDstCKeyOverride, const VBoxVHWAColorKey * pSrcCKeyOverride);
//    int overlay(VBoxVHWASurfaceBase * aOverlaySurface);

    int lock(const QRect * pRect, uint32_t flags);

    int unlock();

    void updatedMem(const QRect * aRect);

    void performDisplay(VBoxVHWASurfaceBase *pPrimary);

    void setRects(VBoxVHWASurfaceBase *pPrimary, const QRect * aTargRect, const QRect * aSrcRect);
    void setTargetRectPosition(VBoxVHWASurfaceBase *pPrimary, const QPoint * aPoint);

    static ulong calcBytesPerPixel(GLenum format, GLenum type);

    static GLsizei makePowerOf2(GLsizei val);

    bool    addressAlocated() const { return mFreeAddress; }
    uchar * address(){ return mAddress; }

    ulong   memSize();

    ulong width()  { return mRect.width();  }
    ulong height() { return mRect.height(); }

    GLenum format() {return mColorFormat.format(); }
    GLint  internalFormat() { return mColorFormat.internalFormat(); }
    GLenum type() { return mColorFormat.type(); }
    uint32_t fourcc() {return mColorFormat.fourcc(); }

//    ulong  bytesPerPixel() { return mpTex[0]->bytesPerPixel(); }
    ulong  bitsPerPixel() { return mColorFormat.bitsPerPixel(); }
//    ulong  bitsPerPixelDd() { return mColorFormat.bitsPerPixelDd(); }
    ulong  bytesPerLine() { return mpTex[0]->bytesPerLine(); }

    const VBoxVHWAColorKey * dstBltCKey() const { return mpDstBltCKey; }
    const VBoxVHWAColorKey * srcBltCKey() const { return mpSrcBltCKey; }
    const VBoxVHWAColorKey * dstOverlayCKey() const { return mpDstOverlayCKey; }
    const VBoxVHWAColorKey * defaultSrcOverlayCKey() const { return mpDefaultSrcOverlayCKey; }
    const VBoxVHWAColorKey * defaultDstOverlayCKey() const { return mpDefaultDstOverlayCKey; }
    const VBoxVHWAColorKey * srcOverlayCKey() const { return mpSrcOverlayCKey; }
    void resetDefaultSrcOverlayCKey() { mpSrcOverlayCKey = mpDefaultSrcOverlayCKey; }
    void resetDefaultDstOverlayCKey() { mpDstOverlayCKey = mpDefaultDstOverlayCKey; }

    void setDstBltCKey(const VBoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mDstBltCKey = *ckey;
            mpDstBltCKey = &mDstBltCKey;
        }
        else
        {
            mpDstBltCKey = NULL;
        }
    }

    void setSrcBltCKey(const VBoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mSrcBltCKey = *ckey;
            mpSrcBltCKey = &mSrcBltCKey;
        }
        else
        {
            mpSrcBltCKey = NULL;
        }
    }

    void setDefaultDstOverlayCKey(const VBoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mDefaultDstOverlayCKey = *ckey;
            mpDefaultDstOverlayCKey = &mDefaultDstOverlayCKey;
        }
        else
        {
            mpDefaultDstOverlayCKey = NULL;
        }
    }

    void setDefaultSrcOverlayCKey(const VBoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mDefaultSrcOverlayCKey = *ckey;
            mpDefaultSrcOverlayCKey = &mDefaultSrcOverlayCKey;
        }
        else
        {
            mpDefaultSrcOverlayCKey = NULL;
        }
    }

    void setOverriddenDstOverlayCKey(const VBoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mOverriddenDstOverlayCKey = *ckey;
            mpDstOverlayCKey = &mOverriddenDstOverlayCKey;
        }
        else
        {
            mpDstOverlayCKey = NULL;
        }
    }

    void setOverriddenSrcOverlayCKey(const VBoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mOverriddenSrcOverlayCKey = *ckey;
            mpSrcOverlayCKey = &mOverriddenSrcOverlayCKey;
        }
        else
        {
            mpSrcOverlayCKey = NULL;
        }
    }

    const VBoxVHWAColorKey * getActiveSrcOverlayCKey()
    {
        return mpSrcOverlayCKey;
    }

    const VBoxVHWAColorKey * getActiveDstOverlayCKey(VBoxVHWASurfaceBase * pPrimary)
    {
        return mpDstOverlayCKey ? mpDefaultDstOverlayCKey : pPrimary->mpDstOverlayCKey;
    }

    const VBoxVHWAColorFormat & colorFormat() {return mColorFormat; }

    void setAddress(uchar * addr);

    const QRect& rect() const {return mRect;}
    const QRect& srcRect() const {return mSrcRect; }
    const QRect& targRect() const {return mTargRect; }
    class VBoxVHWASurfList * getComplexList() {return mComplexList; }

    class VBoxVHWAGlProgramMngr * getGlProgramMngr();
    static int setCKey(class VBoxVHWAGlProgramVHWA * pProgram, const VBoxVHWAColorFormat * pFormat, const VBoxVHWAColorKey * pCKey, bool bDst);

    uint64_t handle() {return mHGHandle;}
    void setHandle(uint64_t h) {mHGHandle = h;}
private:
    void setComplexList(VBoxVHWASurfList *aComplexList) { mComplexList = aComplexList; }
    void initDisplay(VBoxVHWASurfaceBase *pPrimary);
    void deleteDisplay();

    GLuint createDisplay(VBoxVHWASurfaceBase *pPrimary);
    void doDisplay(VBoxVHWASurfaceBase *pPrimary, VBoxVHWAGlProgramVHWA * pProgram, bool bBindDst);
    void synchTexMem(const QRect * aRect);

    int performBlt(const QRect * pDstRect, VBoxVHWASurfaceBase * pSrcSurface, const QRect * pSrcRect, const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool blt);

    void doTex2FB(const QRect * pDstRect, const QRect * pSrcRect);
    void doMultiTex2FB(const QRect * pDstRect, VBoxVHWATexture * pDstTex, const QRect * pSrcRect, int cSrcTex);
    void doMultiTex2FB(const QRect * pDstRect, const QRect * pSrcRect, int cSrcTex);

    void doSetupMatrix(const QSize * pSize , bool bInverted);

    QRect mRect; /* == Inv FB size */

    QRect mSrcRect;
    QRect mTargRect; /* == Vis FB size */
    QRect mTargSize;

    GLuint mVisibleDisplay;

    bool mVisibleDisplayInitialized;

    uchar * mAddress;
    VBoxVHWATexture *mpTex[3];

    VBoxVHWAColorFormat mColorFormat;

    VBoxVHWAColorKey *mpSrcBltCKey;
    VBoxVHWAColorKey *mpDstBltCKey;
    VBoxVHWAColorKey *mpSrcOverlayCKey;
    VBoxVHWAColorKey *mpDstOverlayCKey;

    VBoxVHWAColorKey *mpDefaultDstOverlayCKey;
    VBoxVHWAColorKey *mpDefaultSrcOverlayCKey;

    VBoxVHWAColorKey mSrcBltCKey;
    VBoxVHWAColorKey mDstBltCKey;
    VBoxVHWAColorKey mOverriddenSrcOverlayCKey;
    VBoxVHWAColorKey mOverriddenDstOverlayCKey;
    VBoxVHWAColorKey mDefaultDstOverlayCKey;
    VBoxVHWAColorKey mDefaultSrcOverlayCKey;

    int mLockCount;
    /* memory buffer not reflected in fm and texture, e.g if memory buffer is replaced or in case of lock/unlock  */
    VBoxVHWADirtyRect mUpdateMem2TexRect;

    bool mFreeAddress;

    class VBoxVHWASurfList *mComplexList;

    class VBoxGLWidget *mWidget;

    uint64_t mHGHandle;
protected:

    friend class VBoxVHWASurfList;
#ifdef DEBUG
public:
    uint64_t cFlipsCurr;
    uint64_t cFlipsTarg;
#endif
};

typedef std::list <VBoxVHWASurfaceBase*> SurfList;
typedef std::list <VBoxVHWASurfList*> OverlayList;

class VBoxVHWASurfList
{
public:

    VBoxVHWASurfList() : mCurrent(NULL) {}
    void add(VBoxVHWASurfaceBase *pSurf)
    {
        VBoxVHWASurfList * pOld = pSurf->getComplexList();
        if(pOld)
        {
            pOld->remove(pSurf);
        }
        mSurfaces.push_back(pSurf);
        pSurf->setComplexList(this);
    }

    void clear()
    {
        for (SurfList::iterator it = mSurfaces.begin();
             it != mSurfaces.end(); ++ it)
        {
            (*it)->setComplexList(NULL);
        }
        mSurfaces.clear();
        mCurrent = NULL;
    }

    void remove(VBoxVHWASurfaceBase *pSurf)
    {
        mSurfaces.remove(pSurf);
        pSurf->setComplexList(NULL);
        if(mCurrent == pSurf)
            mCurrent = NULL;
    }

    bool empty() { return mSurfaces.empty(); }

    void setCurrentVisible(VBoxVHWASurfaceBase *pSurf)
    {
        mCurrent = pSurf;
    }

    VBoxVHWASurfaceBase * current() { return mCurrent; }
    const SurfList & surfaces() const {return mSurfaces;}

private:

    SurfList mSurfaces;
    VBoxVHWASurfaceBase* mCurrent;
};

class VBoxVHWADisplay
{
public:
    VBoxVHWADisplay() :
        mSurfVGA(NULL)
//        ,
//        mSurfPrimary(NULL)
    {}

    VBoxVHWASurfaceBase * setVGA(VBoxVHWASurfaceBase * pVga)
    {
        VBoxVHWASurfaceBase * old = mSurfVGA;
        mSurfVGA = pVga;
        mPrimary.clear();
        if(pVga)
        {
            mPrimary.add(pVga);
            mPrimary.setCurrentVisible(pVga);
        }
//        mSurfPrimary = pVga;
        mOverlays.clear();
        return old;
    }

    VBoxVHWASurfaceBase * getVGA()
    {
        return mSurfVGA;
    }

    VBoxVHWASurfaceBase * getPrimary()
    {
        return mPrimary.current();
    }
//
//    void setPrimary(VBoxVHWASurfList * pSurf)
//    {
//        mSurfPrimary = pSurf;
//    }

    void addOverlay(VBoxVHWASurfList * pSurf)
    {
        mOverlays.push_back(pSurf);
    }

    void checkAddOverlay(VBoxVHWASurfList * pSurf)
    {
        if(!hasOverlay(pSurf))
            addOverlay(pSurf);
    }

    bool hasOverlay(VBoxVHWASurfList * pSurf)
    {
        for (OverlayList::iterator it = mOverlays.begin();
             it != mOverlays.end(); ++ it)
        {
            if((*it) == pSurf)
            {
                return true;
            }
        }
        return false;
    }

    void removeOverlay(VBoxVHWASurfList * pSurf)
    {
        mOverlays.remove(pSurf);
    }

    void performDisplay()
    {
        VBoxVHWASurfaceBase * pPrimary = mPrimary.current();
        pPrimary->performDisplay(NULL);

        for (OverlayList::const_iterator it = mOverlays.begin();
             it != mOverlays.end(); ++ it)
        {
            VBoxVHWASurfaceBase * pOverlay = (*it)->current();
            if(pOverlay)
            {
                pOverlay->performDisplay(pPrimary);
            }
        }
    }

    const OverlayList & overlays() const {return mOverlays;}

private:
    VBoxVHWASurfaceBase *mSurfVGA;
    VBoxVHWASurfList mPrimary;

    OverlayList mOverlays;
};

typedef void (VBoxGLWidget::*PFNVBOXQGLOP)(void* );

class VBoxGLWidget : public QGLWidget
{
public:
    VBoxGLWidget (VBoxConsoleView *aView, QWidget *aParent);
    ~VBoxGLWidget();

    ulong vboxPixelFormat() { return mPixelFormat; }
    bool vboxUsesGuestVRAM() { return mUsesGuestVRAM; }

    uchar *vboxAddress() { return mDisplay.getVGA() ? mDisplay.getVGA()->address() : NULL; }

#ifdef VBOX_WITH_VIDEOHWACCEL
    uchar *vboxVRAMAddressFromOffset(uint64_t offset);
    uint64_t vboxVRAMOffsetFromAddress(uchar* addr);
    uint64_t vboxVRAMOffset(VBoxVHWASurfaceBase * pSurf);

    void vhwaSaveExec(struct SSMHANDLE * pSSM);
    int vhwaLoadExec(struct SSMHANDLE * pSSM, uint32_t u32Version);
#endif

    ulong vboxBitsPerPixel() { return mDisplay.getVGA()->bitsPerPixel(); }
    ulong vboxBytesPerLine() { return mDisplay.getVGA() ? mDisplay.getVGA()->bytesPerLine() : NULL; }

    void vboxPaintEvent (QPaintEvent *pe) {vboxPerformGLOp(&VBoxGLWidget::vboxDoPaint, pe);}
    void vboxResizeEvent (VBoxResizeEvent *re) {vboxPerformGLOp(&VBoxGLWidget::vboxDoResize, re);}

    void vboxProcessVHWACommands(VBoxVHWACommandProcessEvent * pEvent) {vboxPerformGLOp(&VBoxGLWidget::vboxDoProcessVHWACommands, pEvent);}
#ifdef VBOX_WITH_VIDEOHWACCEL
    void vboxVHWACmd (struct _VBOXVHWACMD * pCmd) {vboxPerformGLOp(&VBoxGLWidget::vboxDoVHWACmd, pCmd);}
#endif
    class VBoxVHWAGlProgramMngr * vboxVHWAGetGlProgramMngr() { return mpMngr; }

    VBoxVHWASurfaceBase * vboxGetVGASurface() { return mDisplay.getVGA(); }

    void postCmd(VBOXVHWA_PIPECMD_TYPE aType, void * pvData);
protected:

    void paintGL()
    {
        if(mpfnOp)
        {
            (this->*mpfnOp)(mOpContext);
            mpfnOp = NULL;
        }
        else
        {
            mDisplay.performDisplay();
        }
    }

    void initializeGL();
private:
    void vboxDoResize(void *re);
    void vboxDoPaint(void *rec);

    void vboxDoUpdateRect(const QRect * pRect);
#ifdef VBOXQGL_DBG_SURF
    void vboxDoTestSurfaces(void *context);
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
    void vboxDoVHWACmdExec(void *cmd);
    void vboxDoVHWACmd(void *cmd);

    void vboxCheckUpdateAddress (VBoxVHWASurfaceBase * pSurface, uint64_t offset)
    {
#ifndef VBOXQGL_DBG_SURF
    	if(offset == 0xffffffffffffffffL)
    	{
    		return;
    	}
#endif
        if (pSurface->addressAlocated())
        {
            uchar * addr = vboxVRAMAddressFromOffset(offset);
            if(addr)
            {
                pSurface->setAddress(addr);
            }
        }
    }
    int vhwaSurfaceCanCreate(struct _VBOXVHWACMD_SURF_CANCREATE *pCmd);
    int vhwaSurfaceCreate(struct _VBOXVHWACMD_SURF_CREATE *pCmd);
    int vhwaSurfaceDestroy(struct _VBOXVHWACMD_SURF_DESTROY *pCmd);
    int vhwaSurfaceLock(struct _VBOXVHWACMD_SURF_LOCK *pCmd);
    int vhwaSurfaceUnlock(struct _VBOXVHWACMD_SURF_UNLOCK *pCmd);
    int vhwaSurfaceBlt(struct _VBOXVHWACMD_SURF_BLT *pCmd);
    int vhwaSurfaceFlip(struct _VBOXVHWACMD_SURF_FLIP *pCmd);
    int vhwaSurfaceOverlayUpdate(struct _VBOXVHWACMD_SURF_OVERLAY_UPDATE *pCmf);
    int vhwaSurfaceOverlaySetPosition(struct _VBOXVHWACMD_SURF_OVERLAY_SETPOSITION *pCmd);
    int vhwaSurfaceColorkeySet(struct _VBOXVHWACMD_SURF_COLORKEY_SET *pCmd);
    int vhwaQueryInfo1(struct _VBOXVHWACMD_QUERYINFO1 *pCmd);
    int vhwaQueryInfo2(struct _VBOXVHWACMD_QUERYINFO2 *pCmd);
    int vhwaConstruct(struct _VBOXVHWACMD_HH_CONSTRUCT *pCmd);

    int vhwaSaveSurface(struct SSMHANDLE * pSSM, VBoxVHWASurfaceBase *pSurf, uint32_t surfCaps);
    int vhwaLoadSurface(struct SSMHANDLE * pSSM, uint32_t u32Version);
    int vhwaSaveOverlayData(struct SSMHANDLE * pSSM, VBoxVHWASurfaceBase *pSurf, bool bVisible);
    int vhwaLoadOverlayData(struct SSMHANDLE * pSSM, uint32_t u32Version);

    void vhwaDoSurfaceOverlayUpdate(VBoxVHWASurfaceBase *pDstSurf, VBoxVHWASurfaceBase *pSrcSurf, struct _VBOXVHWACMD_SURF_OVERLAY_UPDATE *pCmd);
#endif
    static const QGLFormat & vboxGLFormat();

    VBoxVHWADisplay mDisplay;


    /* we do all opengl stuff in the paintGL context,
     * submit the operation to be performed
     * @todo: could be moved outside the updateGL */
    void vboxPerformGLOp(PFNVBOXQGLOP pfn, void* pContext) {mpfnOp = pfn; mOpContext = pContext; updateGL();}

    /* posts op to UI thread */
    int vboxExecOpSynch(PFNVBOXQGLOP pfn, void* pContext);

    void cmdPipeInit();
    void cmdPipeDelete();
    void vboxDoProcessVHWACommands(void *pContext);

    VBoxVHWACommandElement * detachCmdList(VBoxVHWACommandElement * pFirst2Free, VBoxVHWACommandElement * pLast2Free);
    VBoxVHWACommandElement * processCmdList(VBoxVHWACommandElement * pCmd);

    VBoxVHWASurfaceBase* handle2Surface(uint32_t h) { return (VBoxVHWASurfaceBase*)mSurfHandleTable.get(h); }

    VBoxVHWAHandleTable mSurfHandleTable;

    PFNVBOXQGLOP mpfnOp;
    void *mOpContext;

    ulong  mPixelFormat;
    bool   mUsesGuestVRAM;
    bool   mVGASurfCreated;

    RTCRITSECT mCritSect;
    VBoxVHWACommandProcessEvent *mpFirstEvent;
    VBoxVHWACommandProcessEvent *mpLastEvent;
    bool mbNewEvent;
    VBoxVHWACommandElementStack mFreeElements;
    VBoxVHWACommandElement mElementsBuffer[2048];

    VBoxConsoleView *mView;

    VBoxVHWASurfList *mConstructingList;
    int32_t mcRemaining2Contruct;

    class VBoxVHWAGlProgramMngr *mpMngr;
};


class VBoxQGLFrameBuffer : public VBoxFrameBuffer
{
public:

    VBoxQGLFrameBuffer (VBoxConsoleView *aView);

    STDMETHOD(NotifyUpdate) (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH);
#ifdef VBOXQGL_PROF_BASE
    STDMETHOD(RequestResize) (ULONG aScreenId, ULONG aPixelFormat,
                              BYTE *aVRAM, ULONG aBitsPerPixel, ULONG aBytesPerLine,
                              ULONG aWidth, ULONG aHeight,
                              BOOL *aFinished);
#endif

#ifdef VBOX_WITH_VIDEOHWACCEL
    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand);


    static bool isAcceleration2DVideoAvailable();
#endif

    ulong pixelFormat() { return vboxWidget()->vboxPixelFormat(); }
    bool usesGuestVRAM() { return vboxWidget()->vboxUsesGuestVRAM(); }

    uchar *address() { return vboxWidget()->vboxAddress(); }
    ulong bitsPerPixel() { return vboxWidget()->vboxBitsPerPixel(); }
    ulong bytesPerLine() { return vboxWidget()->vboxBytesPerLine(); }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);
    void doProcessVHWACommand(VBoxVHWACommandProcessEvent * pEvent);

private:
//    void vboxMakeCurrent();
    VBoxGLWidget * vboxWidget();
};


#endif

/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_SDL)

class VBoxSDLFrameBuffer : public VBoxFrameBuffer
{
public:

    VBoxSDLFrameBuffer (VBoxConsoleView *aView);
    virtual ~VBoxSDLFrameBuffer();

    STDMETHOD(NotifyUpdate) (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH);

    uchar *address()
    {
        SDL_Surface *surf = mSurfVRAM ? mSurfVRAM : mScreen;
        return surf ? (uchar *) (uintptr_t) surf->pixels : 0;
    }

    ulong bitsPerPixel()
    {
        SDL_Surface *surf = mSurfVRAM ? mSurfVRAM : mScreen;
        return surf ? surf->format->BitsPerPixel : 0;
    }

    ulong bytesPerLine()
    {
        SDL_Surface *surf = mSurfVRAM ? mSurfVRAM : mScreen;
        return surf ? surf->pitch : 0;
    }

    ulong pixelFormat()
    {
        return mPixelFormat;
    }

    bool usesGuestVRAM()
    {
        return mSurfVRAM != NULL;
    }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);

private:

    SDL_Surface *mScreen;
    SDL_Surface *mSurfVRAM;

    ulong mPixelFormat;
};

#endif

/////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_GUI_USE_DDRAW)

class VBoxDDRAWFrameBuffer : public VBoxFrameBuffer
{
public:

    VBoxDDRAWFrameBuffer (VBoxConsoleView *aView);
    virtual ~VBoxDDRAWFrameBuffer();

    STDMETHOD(NotifyUpdate) (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH);

    uchar *address() { return (uchar *) mSurfaceDesc.lpSurface; }
    ulong bitsPerPixel() { return mSurfaceDesc.ddpfPixelFormat.dwRGBBitCount; }
    ulong bytesPerLine() { return (ulong) mSurfaceDesc.lPitch; }

    ulong pixelFormat() { return mPixelFormat; };

    bool usesGuestVRAM() { return mUsesGuestVRAM; }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);
    void moveEvent (QMoveEvent *me);

private:

    void releaseObjects();

    bool createSurface (ULONG aPixelFormat, uchar *pvVRAM,
                        ULONG aBitsPerPixel, ULONG aBytesPerLine,
                        ULONG aWidth, ULONG aHeight);
    void deleteSurface();
    void drawRect (ULONG x, ULONG y, ULONG w, ULONG h);
    void getWindowPosition (void);

    LPDIRECTDRAW7        mDDRAW;
    LPDIRECTDRAWCLIPPER  mClipper;
    LPDIRECTDRAWSURFACE7 mSurface;
    DDSURFACEDESC2       mSurfaceDesc;
    LPDIRECTDRAWSURFACE7 mPrimarySurface;

    ulong mPixelFormat;

    bool mUsesGuestVRAM;

    int mWndX;
    int mWndY;

    bool mSynchronousUpdates;
};

#endif

/////////////////////////////////////////////////////////////////////////////

#if defined (Q_WS_MAC) && defined (VBOX_GUI_USE_QUARTZ2D)

#include <Carbon/Carbon.h>

class VBoxQuartz2DFrameBuffer : public VBoxFrameBuffer
{
public:

    VBoxQuartz2DFrameBuffer (VBoxConsoleView *aView);
    virtual ~VBoxQuartz2DFrameBuffer ();

    STDMETHOD (NotifyUpdate) (ULONG aX, ULONG aY,
                              ULONG aW, ULONG aH);
    STDMETHOD (SetVisibleRegion) (BYTE *aRectangles, ULONG aCount);

    uchar *address() { return mDataAddress; }
    ulong bitsPerPixel() { return CGImageGetBitsPerPixel (mImage); }
    ulong bytesPerLine() { return CGImageGetBytesPerRow (mImage); }
    ulong pixelFormat() { return mPixelFormat; };
    bool usesGuestVRAM() { return mBitmapData == NULL; }

    const CGImageRef imageRef() const { return mImage; }

    void paintEvent (QPaintEvent *pe);
    void resizeEvent (VBoxResizeEvent *re);

private:

    void clean();

    uchar *mDataAddress;
    void *mBitmapData;
    ulong mPixelFormat;
    CGImageRef mImage;
    typedef struct
    {
        /** The size of this structure expressed in rcts entries. */
        ULONG allocated;
        /** The number of entries in the rcts array. */
        ULONG used;
        /** Variable sized array of the rectangle that makes up the region. */
        CGRect rcts[1];
    } RegionRects;
    /** The current valid region, all access is by atomic cmpxchg or atomic xchg.
     *
     * The protocol for updating and using this has to take into account that
     * the producer (SetVisibleRegion) and consumer (paintEvent) are running
     * on different threads. Therefore the producer will create a new RegionRects
     * structure before atomically replace the existing one. While the consumer
     * will read the value by atomically replace it by NULL, and then when its
     * done try restore it by cmpxchg. If the producer has already put a new
     * region there, it will be discarded (see mRegionUnused).
     */
    RegionRects volatile *mRegion;
    /** For keeping the unused region and thus avoid some RTMemAlloc/RTMemFree calls.
     * This is operated with atomic cmpxchg and atomic xchg. */
    RegionRects volatile *mRegionUnused;
};

#endif /* Q_WS_MAC && VBOX_GUI_USE_QUARTZ2D */

#endif // !___VBoxFrameBuffer_h___
