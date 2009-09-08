/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxFrameBuffer Overly classes declarations
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
#ifndef __VBoxFBOverlay_h__
#define __VBoxFBOverlay_h__
#if defined (VBOX_GUI_USE_QGL)
#include "COMDefs.h"
#include <QGLWidget>
#include <iprt/assert.h>
#include <iprt/critsect.h>

#ifdef DEBUG
#include "iprt/stream.h"
#define VBOXQGLLOG(_m) RTPrintf _m
#define VBOXQGLLOGREL(_m) do { RTPrintf _m ; LogRel( _m ); } while(0)
#else
#define VBOXQGLLOG(_m)
#define VBOXQGLLOGREL(_m) LogRel( _m )
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

    const QRect & toRect()
    {
        if(isClear())
        {
            mRect.setCoords(0, 0, -1, -1);
        }
        return mRect;
    }

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

    bool equals (const VBoxVHWAColorFormat & other) const;

private:
    void init(uint32_t bitsPerPixel, uint32_t r, uint32_t g, uint32_t b);
    void init(uint32_t fourcc);

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
    VBoxVHWATexture(const QRect & aRect, const VBoxVHWAColorFormat &aFormat);
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
    VBoxVHWATextureNP2(const QRect & aRect, const VBoxVHWAColorFormat &aFormat) :
        VBoxVHWATexture(aRect, aFormat){
        mTexRect = QRect(0, 0, aRect.width()/aFormat.widthCompression(), aRect.height()/aFormat.heightCompression());
    }
};

class VBoxVHWATextureNP2Rect : public VBoxVHWATextureNP2
{
public:
    VBoxVHWATextureNP2Rect() : VBoxVHWATextureNP2() {}
    VBoxVHWATextureNP2Rect(const QRect & aRect, const VBoxVHWAColorFormat &aFormat) :
        VBoxVHWATextureNP2(aRect, aFormat){}

    virtual void texCoord(int x, int y);
    virtual void multiTexCoord(GLenum texUnit, int x, int y);
protected:
    virtual GLenum texTarget();
};

class VBoxVHWATextureNP2RectPBO : public VBoxVHWATextureNP2Rect
{
public:
    VBoxVHWATextureNP2RectPBO() : VBoxVHWATextureNP2Rect() {}
    VBoxVHWATextureNP2RectPBO(const QRect & aRect, const VBoxVHWAColorFormat &aFormat) :
        VBoxVHWATextureNP2Rect(aRect, aFormat){}
    virtual ~VBoxVHWATextureNP2RectPBO();

    virtual void init(uchar *pvMem);
protected:
    virtual void load();
    virtual void doUpdate(uchar * pAddress, const QRect * pRect);
private:
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
            const QSize & aSize,
            const QRect & aTargRect,
            const QRect & aSrcRect,
            const QRect & aVisTargRect,
            VBoxVHWAColorFormat & aColorFormat,
            VBoxVHWAColorKey * pSrcBltCKey, VBoxVHWAColorKey * pDstBltCKey,
            VBoxVHWAColorKey * pSrcOverlayCKey, VBoxVHWAColorKey * pDstOverlayCKey,
            bool bVGA);

    virtual ~VBoxVHWASurfaceBase();

    void init(VBoxVHWASurfaceBase * pPrimary, uchar *pvMem);

    void uninit();

    static void globalInit();

//    int blt(const QRect * aDstRect, VBoxVHWASurfaceBase * aSrtSurface, const QRect * aSrcRect, const VBoxVHWAColorKey * pDstCKeyOverride, const VBoxVHWAColorKey * pSrcCKeyOverride);

    int lock(const QRect * pRect, uint32_t flags);

    int unlock();

    void updatedMem(const QRect * aRect);

    void performDisplay(VBoxVHWASurfaceBase *pPrimary);

    void setRects(VBoxVHWASurfaceBase *pPrimary, const QRect & aTargRect, const QRect & aSrcRect, const QRect & aVisibleTargRect, bool bForceReinit);
    void setTargRectPosition(VBoxVHWASurfaceBase *pPrimary, const QPoint & aPoint, const QRect & aVisibleTargRect);
    void updateVisibleTargRect(VBoxVHWASurfaceBase *pPrimary, const QRect & aVisibleTargRect);

    static ulong calcBytesPerPixel(GLenum format, GLenum type);

    static GLsizei makePowerOf2(GLsizei val);

    bool    addressAlocated() const { return mFreeAddress; }
    uchar * address(){ return mAddress; }

    ulong   memSize();

    ulong width()  { return mRect.width();  }
    ulong height() { return mRect.height(); }
    const QSize size() {return mRect.size();}

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

    uint32_t handle() const {return mHGHandle;}
    void setHandle(uint32_t h) {mHGHandle = h;}

    const VBoxVHWADirtyRect & getDirtyRect() { return mUpdateMem2TexRect; }

private:
    void doSetRectValuesInternal(const QRect & aTargRect, const QRect & aSrcRect, const QRect & aVisTargRect);

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

    QRect mRect; /* == Inv FB size */

    QRect mSrcRect;
    QRect mTargRect; /* == Vis FB size */

    QRect mVisibleTargRect;
    QRect mVisibleSrcRect;

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

    uint32_t mHGHandle;

#ifdef DEBUG
public:
    uint64_t cFlipsCurr;
    uint64_t cFlipsTarg;
#endif
    friend class VBoxVHWASurfList;
};

typedef std::list <VBoxVHWASurfaceBase*> SurfList;
typedef std::list <VBoxVHWASurfList*> OverlayList;
typedef std::list <struct _VBOXVHWACMD *> VHWACommandList;

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

    size_t size() const {return mSurfaces.size(); }

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
            Assert(!pVga->getComplexList());
            mPrimary.add(pVga);
            mPrimary.setCurrentVisible(pVga);
        }
//        mSurfPrimary = pVga;
        mOverlays.clear();
        return old;
    }

    VBoxVHWASurfaceBase * updateVGA(VBoxVHWASurfaceBase * pVga)
    {
        VBoxVHWASurfaceBase * old = mSurfVGA;
        Assert(old);
        mSurfVGA = pVga;
        return old;
    }

    VBoxVHWASurfaceBase * getVGA() const
    {
        return mSurfVGA;
    }

    VBoxVHWASurfaceBase * getPrimary()
    {
        return mPrimary.current();
    }

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
    const VBoxVHWASurfList & primaries() const { return mPrimary; }

private:
    VBoxVHWASurfaceBase *mSurfVGA;
    VBoxVHWASurfList mPrimary;

    OverlayList mOverlays;
};

typedef void (VBoxGLWidget::*PFNVBOXQGLOP)(void* );

typedef void (*PFNVBOXQGLFUNC)(void*, void*);

typedef enum
{
    VBOXVHWA_PIPECMD_PAINT = 1,
    VBOXVHWA_PIPECMD_VHWA,
    VBOXVHWA_PIPECMD_OP,
    VBOXVHWA_PIPECMD_FUNC,
}VBOXVHWA_PIPECMD_TYPE;

typedef struct VBOXVHWACALLBACKINFO
{
    VBoxGLWidget *pThis;
    PFNVBOXQGLOP pfnCallback;
    void * pContext;
}VBOXVHWACALLBACKINFO;

typedef struct VBOXVHWAFUNCCALLBACKINFO
{
    PFNVBOXQGLFUNC pfnCallback;
    void * pContext1;
    void * pContext2;
}VBOXVHWAFUNCCALLBACKINFO;

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

    void setFunc(const VBOXVHWAFUNCCALLBACKINFO & aOp)
    {
        mType = VBOXVHWA_PIPECMD_FUNC;
        u.mFuncCallback = aOp;
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
        case VBOXVHWA_PIPECMD_FUNC:
            setFunc(*((VBOXVHWAFUNCCALLBACKINFO *)pvData));
            break;
        default:
            Assert(0);
            break;
        }
    }

    VBOXVHWA_PIPECMD_TYPE type() const {return mType;}
    const QRect & rect() const {return mRect;}
    struct _VBOXVHWACMD * vhwaCmd() const {return u.mpCmd;}
    const VBOXVHWACALLBACKINFO & op() const {return u.mCallback; }
    const VBOXVHWAFUNCCALLBACKINFO & func() const {return u.mFuncCallback; }

    VBoxVHWACommandElement * mpNext;
private:
    VBOXVHWA_PIPECMD_TYPE mType;
    union
    {
        struct _VBOXVHWACMD * mpCmd;
        VBOXVHWACALLBACKINFO mCallback;
        VBOXVHWAFUNCCALLBACKINFO mFuncCallback;
    }u;
    QRect                 mRect;
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

#define VBOXVHWACMDPIPEC_NEWEVENT      0x00000001
#define VBOXVHWACMDPIPEC_COMPLETEEVENT 0x00000002
class VBoxVHWACommandElementProcessor
{
public:
    VBoxVHWACommandElementProcessor(class VBoxConsoleView *aView);
    ~VBoxVHWACommandElementProcessor();
    void postCmd(VBOXVHWA_PIPECMD_TYPE aType, void * pvData, uint32_t flags);
    void completeCurrentEvent();
    class VBoxVHWACommandElement * detachCmdList(class VBoxVHWACommandElement * pFirst2Free, VBoxVHWACommandElement * pLast2Free);

private:
    RTCRITSECT mCritSect;
    class VBoxVHWACommandProcessEvent *mpFirstEvent;
    class VBoxVHWACommandProcessEvent *mpLastEvent;
    class VBoxConsoleView *mView;
    bool mbNewEvent;
    VBoxVHWACommandElementStack mFreeElements;
    VBoxVHWACommandElement mElementsBuffer[2048];
};

class VBoxVHWACommandsQueue
{
public:
    void enqueue(PFNVBOXQGLFUNC pfn, void* pContext1, void* pContext2);

    VBoxVHWACommandElement * detachList();

    void freeList(VBoxVHWACommandElement * pList);

private:
    VBoxVHWACommandElementPipe mCmds;
};

class VBoxGLWidget : public QGLWidget
{
public:
    VBoxGLWidget (class VBoxConsoleView *aView, QWidget *aParent);
    ~VBoxGLWidget();

    ulong vboxPixelFormat() { return mPixelFormat; }
    bool vboxUsesGuestVRAM() { return mUsesGuestVRAM; }

    uchar *vboxAddress() { return mDisplay.getVGA() ? mDisplay.getVGA()->address() : NULL; }

#ifdef VBOX_WITH_VIDEOHWACCEL
    uchar *vboxVRAMAddressFromOffset(uint64_t offset);
    uint64_t vboxVRAMOffsetFromAddress(uchar* addr);
    uint64_t vboxVRAMOffset(VBoxVHWASurfaceBase * pSurf);

    void vhwaSaveExec(struct SSMHANDLE * pSSM);
    int vhwaLoadExec(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version);

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

    bool hasSurfaces() const;
    bool hasVisibleOverlays();
    const QRect & overlaysRectUnion();
#endif

    ulong vboxBitsPerPixel() { return mDisplay.getVGA()->bitsPerPixel(); }
    ulong vboxBytesPerLine() { return mDisplay.getVGA() ? mDisplay.getVGA()->bytesPerLine() : 0; }
    int vboxFbWidth() {return mDisplay.getVGA()->width(); }
    int vboxFbHeight() {return mDisplay.getVGA()->height(); }
    bool vboxIsInitialized() {return mDisplay.getVGA() != NULL; }

//    void vboxPaintEvent (QPaintEvent *pe) {vboxPerformGLOp(&VBoxGLWidget::vboxDoPaint, pe); }
    void vboxResizeEvent (class VBoxResizeEvent *re) {vboxPerformGLOp(&VBoxGLWidget::vboxDoResize, re); }

    void vboxProcessVHWACommands(class VBoxVHWACommandElementProcessor * pPipe) {vboxPerformGLOp(&VBoxGLWidget::vboxDoProcessVHWACommands, pPipe);}
#ifdef VBOX_WITH_VIDEOHWACCEL
    void vboxVHWACmd (struct _VBOXVHWACMD * pCmd) {vboxPerformGLOp(&VBoxGLWidget::vboxDoVHWACmd, pCmd);}
#endif
    class VBoxVHWAGlProgramMngr * vboxVHWAGetGlProgramMngr() { return mpMngr; }

    VBoxVHWASurfaceBase * vboxGetVGASurface() { return mDisplay.getVGA(); }

    static void doSetupMatrix(const QSize & aSize, bool bInverted);

    void vboxDoUpdateViewport(const QRect & aRect);
    void vboxDoUpdateRect(const QRect * pRect);

    const QRect & vboxViewport() const {return mViewport;}

    void performDisplay() { mDisplay.performDisplay(); }
protected:

    void paintGL()
    {
        if(mpfnOp)
        {
            (this->*mpfnOp)(mOpContext);
            mpfnOp = NULL;
        }
//        else
//        {
            mDisplay.performDisplay();
//        }
    }

    void initializeGL();

private:
    static void setupMatricies(const QSize &display);
    static void adjustViewport(const QSize &display, const QRect &viewport);
    void vboxDoResize(void *re);
//    void vboxDoPaint(void *rec);


#ifdef VBOXQGL_DBG_SURF
    void vboxDoTestSurfaces(void *context);
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
    void vboxDoVHWACmdExec(void *cmd);
    void vboxDoVHWACmdAndFree(void *cmd);
    void vboxDoVHWACmd(void *cmd);

    void vboxCheckUpdateAddress (VBoxVHWASurfaceBase * pSurface, uint64_t offset)
    {
        if (pSurface->addressAlocated())
        {
            uchar * addr = vboxVRAMAddressFromOffset(offset);
            if(addr)
            {
                pSurface->setAddress(addr);
            }
        }
    }

    int vhwaSaveSurface(struct SSMHANDLE * pSSM, VBoxVHWASurfaceBase *pSurf, uint32_t surfCaps);
    int vhwaLoadSurface(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version);
    int vhwaSaveOverlayData(struct SSMHANDLE * pSSM, VBoxVHWASurfaceBase *pSurf, bool bVisible);
    int vhwaLoadOverlayData(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version);
    void vhwaDoSurfaceOverlayUpdate(VBoxVHWASurfaceBase *pDstSurf, VBoxVHWASurfaceBase *pSrcSurf, struct _VBOXVHWACMD_SURF_OVERLAY_UPDATE *pCmd);
#endif
    static const QGLFormat & vboxGLFormat();

    VBoxVHWADisplay mDisplay;


    /* we do all opengl stuff in the paintGL context,
     * submit the operation to be performed
     * @todo: could be moved outside the updateGL */
    void vboxPerformGLOp(PFNVBOXQGLOP pfn, void* pContext)
    {
        mpfnOp = pfn;
        mOpContext = pContext;
        updateGL();
    }

//    /* posts op to UI thread */
//    int vboxExecOpSynch(PFNVBOXQGLOP pfn, void* pContext);
//    void vboxExecOnResize(PFNVBOXQGLOP pfn, void* pContext);

    void vboxDoProcessVHWACommands(void *pContext);

    class VBoxVHWACommandElement * processCmdList(class VBoxVHWACommandElement * pCmd);

    VBoxVHWASurfaceBase* handle2Surface(uint32_t h)
    {
        VBoxVHWASurfaceBase* pSurf = (VBoxVHWASurfaceBase*)mSurfHandleTable.get(h);
        Assert(pSurf);
        return pSurf;
    }

    VBoxVHWAHandleTable mSurfHandleTable;

    PFNVBOXQGLOP mpfnOp;
    void *mOpContext;

    ulong  mPixelFormat;
    bool   mUsesGuestVRAM;
//    bool   mbVGASurfCreated;
    QRect mViewport;

    class VBoxConsoleView *mView;

    VBoxVHWASurfList *mConstructingList;
    int32_t mcRemaining2Contruct;

    /* this is used in saved state restore to postpone surface restoration
     * till the framebuffer size is restored */
    VHWACommandList mOnResizeCmdList;

    class VBoxVHWAGlProgramMngr *mpMngr;
};


typedef enum
{
    VBOXFBOVERLAY_DONE = 1,
    VBOXFBOVERLAY_MODIFIED,
    VBOXFBOVERLAY_UNTOUCHED
} VBOXFBOVERLAY_RESUT;

class VBoxQGLOverlay
{
public:
    VBoxQGLOverlay (class VBoxConsoleView *aView, class VBoxFrameBuffer * aContainer);

    int onVHWACommand(struct _VBOXVHWACMD * pCommand);

    void onVHWACommandEvent(QEvent * pEvent);

    /**
     * to be called on NotifyUpdate framebuffer call
     * @return true if the request was processed & should not be forwarded to the framebuffer
     * false - otherwise */
    bool onNotifyUpdate (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH);

    /**
     * to be called on RequestResize framebuffer call
     * @return true if the request was processed & should not be forwarded to the framebuffer
     * false - otherwise */
    bool onRequestResize (ULONG aScreenId, ULONG aPixelFormat,
                              BYTE *aVRAM, ULONG aBitsPerPixel, ULONG aBytesPerLine,
                              ULONG aWidth, ULONG aHeight,
                              BOOL *aFinished)
    {
        mCmdPipe.completeCurrentEvent();
        return false;
    }

    VBOXFBOVERLAY_RESUT onPaintEvent (const QPaintEvent *pe, QRect *pRect);
    void onResizeEvent (const class VBoxResizeEvent *re);
    void onResizeEventPostprocess (const class VBoxResizeEvent *re);

    void viewportResized(QResizeEvent * re)
    {
        vboxDoCheckUpdateViewport();
        mGlCurrent = false;
    }

    void viewportScrolled(int dx, int dy)
    {
        vboxDoCheckUpdateViewport();
        mGlCurrent = false;
    }

    static bool isAcceleration2DVideoAvailable();

    /* not supposed to be called by clients */
    int vhwaLoadExec(struct SSMHANDLE * pSSM, uint32_t u32Version);
    void vhwaSaveExec(struct SSMHANDLE * pSSM);
private:
    int vhwaSurfaceUnlock(struct _VBOXVHWACMD_SURF_UNLOCK *pCmd);

    void repaintMain();
    void repaintOverlay()
    {
        if(mNeedOverlayRepaint)
        {
            mNeedOverlayRepaint = false;
            performDisplayOverlay();
        }
    }
    void repaint()
    {
        repaintOverlay();
        repaintMain();
    }

    void makeCurrent()
    {
        if(!mGlCurrent)
        {
            mGlCurrent = true;
            mpOverlayWidget->makeCurrent();
        }
    }

    void performDisplayOverlay()
    {
        if(mOverlayVisible)
        {
#if 0
            mpOverlayWidget->updateGL();
#else
            makeCurrent();
            mpOverlayWidget->performDisplay();
            mpOverlayWidget->swapBuffers();
#endif
        }
    }

//    void vboxOpExit()
//    {
//        performDisplayOverlay();
//        mGlCurrent = false;
//    }


    void vboxSetGlOn(bool on);
    bool vboxGetGlOn() { return mGlOn; }
    bool vboxSynchGl();
    void vboxDoVHWACmdExec(void *cmd);
    void vboxShowOverlay(bool show);
    void vboxDoCheckUpdateViewport();
    void vboxDoVHWACmd(void *cmd);
    void addMainDirtyRect(const QRect & aRect);
//    void vboxUpdateOverlayPosition(const QPoint & pos);
    void vboxCheckUpdateOverlay(const QRect & rect);
    VBoxVHWACommandElement * processCmdList(VBoxVHWACommandElement * pCmd);

    int vhwaConstruct(struct _VBOXVHWACMD_HH_CONSTRUCT *pCmd);

    VBoxGLWidget *mpOverlayWidget;
    class VBoxConsoleView *mView;
    class VBoxFrameBuffer *mContainer;
    bool mGlOn;
    bool mOverlayWidgetVisible;
    bool mOverlayVisible;
    bool mGlCurrent;
    bool mProcessingCommands;
    bool mNeedOverlayRepaint;
    QRect mOverlayViewport;
    VBoxVHWADirtyRect mMainDirtyRect;

    VBoxVHWACommandElementProcessor mCmdPipe;

    /* this is used in saved state restore to postpone surface restoration
     * till the framebuffer size is restored */
    VHWACommandList mOnResizeCmdList;
};


template <class T>
class VBoxOverlayFrameBuffer : public T
{
public:
    VBoxOverlayFrameBuffer (class VBoxConsoleView *aView)
        : T(aView),
          mOverlay(aView, this)
    {}


    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand)
    {
        return mOverlay.onVHWACommand((struct _VBOXVHWACMD*)pCommand);
    }

    void doProcessVHWACommand(QEvent * pEvent)
    {
        mOverlay.onVHWACommandEvent(pEvent);
    }

    STDMETHOD(RequestResize) (ULONG aScreenId, ULONG aPixelFormat,
                              BYTE *aVRAM, ULONG aBitsPerPixel, ULONG aBytesPerLine,
                              ULONG aWidth, ULONG aHeight,
                              BOOL *aFinished)
   {
        if(mOverlay.onRequestResize (aScreenId, aPixelFormat,
                aVRAM, aBitsPerPixel, aBytesPerLine,
                aWidth, aHeight,
                aFinished))
        {
            return S_OK;
        }
        return T::RequestResize (aScreenId, aPixelFormat,
                aVRAM, aBitsPerPixel, aBytesPerLine,
                aWidth, aHeight,
                aFinished);
   }

    STDMETHOD(NotifyUpdate) (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH)
    {
        if(mOverlay.onNotifyUpdate(aX, aY, aW, aH))
            return S_OK;
        return T::NotifyUpdate(aX, aY, aW, aH);
    }

    void paintEvent (QPaintEvent *pe)
    {
        QRect rect;
        VBOXFBOVERLAY_RESUT res = mOverlay.onPaintEvent(pe, &rect);
        switch(res)
        {
            case VBOXFBOVERLAY_MODIFIED:
            {
                QPaintEvent modified(rect);
                T::paintEvent(&modified);
            } break;
            case VBOXFBOVERLAY_UNTOUCHED:
                T::paintEvent(pe);
                break;
            default:
                break;
        }
    }

    void resizeEvent (VBoxResizeEvent *re)
    {
        mOverlay.onResizeEvent(re);
        T::resizeEvent(re);
        mOverlay.onResizeEventPostprocess(re);
    }

    void viewportResized(QResizeEvent * re)
    {
        mOverlay.viewportResized(re);
        T::viewportResized(re);
    }

    void viewportScrolled(int dx, int dy)
    {
        mOverlay.viewportScrolled(dx, dy);
        T::viewportScrolled(dx, dy);
    }
private:
    VBoxQGLOverlay mOverlay;
};

#endif

#endif /* #ifndef __VBoxFBOverlay_h__ */
