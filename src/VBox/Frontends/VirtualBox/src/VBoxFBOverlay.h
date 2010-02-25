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
#if defined (VBOX_GUI_USE_QGL) || defined(VBOX_WITH_VIDEOHWACCEL)

//#define VBOXQGL_PROF_BASE 1
//#define VBOXQGL_DBG_SURF 1

//#define VBOXVHWADBG_RENDERCHECK

#include "COMDefs.h"
#include <QGLWidget>
#include <iprt/assert.h>
#include <iprt/critsect.h>

#include <VBox/VBoxGL2D.h>
#include "VBoxFBOverlayCommon.h"

#define VBOXVHWA_ALLOW_PRIMARY_AND_OVERLAY_ONLY 1

#ifdef DEBUG_misha
# define VBOXVHWA_PROFILE_FPS
#endif

#ifdef DEBUG
class VBoxVHWADbgTimer
{
public:
    VBoxVHWADbgTimer(uint32_t cPeriods);
    ~VBoxVHWADbgTimer();
    void frame();
    uint64_t everagePeriod() {return mPeriodSum / mcPeriods; }
    double fps() {return ((double)1000000000.0) / everagePeriod(); }
    uint64_t frames() {return mcFrames; }
private:
    uint64_t mPeriodSum;
    uint64_t *mpaPeriods;
    uint64_t mPrevTime;
    uint64_t mcFrames;
    uint32_t mcPeriods;
    uint32_t miPeriod;
};

#endif

class VBoxVHWASettings
{
public:
    VBoxVHWASettings (CSession &session);

    int fourccEnabledCount() const { return mFourccEnabledCount; }
    const uint32_t * fourccEnabledList() const { return mFourccEnabledList; }

    bool isStretchLinearEnabled() const { return mStretchLinearEnabled; }

    static int calcIntersection (int c1, const uint32_t *a1, int c2, const uint32_t *a2, int cOut, uint32_t *aOut);

    int getIntersection (const VBoxVHWAInfo &aInfo, int cOut, uint32_t *aOut)
    {
        return calcIntersection (mFourccEnabledCount, mFourccEnabledList, aInfo.getFourccSupportedCount(), aInfo.getFourccSupportedList(), cOut, aOut);
    }

    bool isSupported(const VBoxVHWAInfo &aInfo, uint32_t format)
    {
        return calcIntersection (mFourccEnabledCount, mFourccEnabledList, 1, &format, 0, NULL)
                && calcIntersection (aInfo.getFourccSupportedCount(), aInfo.getFourccSupportedList(), 1, &format, 0, NULL);
    }
private:
    uint32_t mFourccEnabledList[VBOXVHWA_NUMFOURCC];
    int mFourccEnabledCount;
    bool mStretchLinearEnabled;
};

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
    VBoxVHWATexture() :
            mAddress(NULL),
            mTexture(0),
            mBytesPerPixel(0),
            mBytesPerPixelTex(0),
            mBytesPerLine(0),
            mScaleFuncttion(GL_NEAREST)
{}
    VBoxVHWATexture(const QRect & aRect, const VBoxVHWAColorFormat &aFormat, GLint scaleFuncttion);
    virtual ~VBoxVHWATexture();
    virtual void init(uchar *pvMem);
    void setAddress(uchar *pvMem) {mAddress = pvMem;}
    void update(const QRect * pRect) { doUpdate(mAddress, pRect);}
    void bind() {glBindTexture(texTarget(), mTexture);}

    virtual void texCoord(int x, int y);
    virtual void multiTexCoord(GLenum texUnit, int x, int y);

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
    GLuint texture() {return mTexture;}

    QRect mTexRect; /* texture size */
    QRect mRect; /* img size */
    uchar * mAddress;
    GLuint mTexture;
    uint32_t mBytesPerPixel;
    uint32_t mBytesPerPixelTex;
    uint32_t mBytesPerLine;
    VBoxVHWAColorFormat mColorFormat;
    GLint mScaleFuncttion;
private:
    void uninit();

    friend class VBoxVHWAFBO;
};

class VBoxVHWATextureNP2 : public VBoxVHWATexture
{
public:
    VBoxVHWATextureNP2() : VBoxVHWATexture() {}
    VBoxVHWATextureNP2(const QRect & aRect, const VBoxVHWAColorFormat &aFormat, GLint scaleFuncttion) :
        VBoxVHWATexture(aRect, aFormat, scaleFuncttion){
        mTexRect = QRect(0, 0, aRect.width()/aFormat.widthCompression(), aRect.height()/aFormat.heightCompression());
    }
};

class VBoxVHWATextureNP2Rect : public VBoxVHWATextureNP2
{
public:
    VBoxVHWATextureNP2Rect() : VBoxVHWATextureNP2() {}
    VBoxVHWATextureNP2Rect(const QRect & aRect, const VBoxVHWAColorFormat &aFormat, GLint scaleFuncttion) :
        VBoxVHWATextureNP2(aRect, aFormat, scaleFuncttion){}

    virtual void texCoord(int x, int y);
    virtual void multiTexCoord(GLenum texUnit, int x, int y);
protected:
    virtual GLenum texTarget();
};

class VBoxVHWATextureNP2RectPBO : public VBoxVHWATextureNP2Rect
{
public:
    VBoxVHWATextureNP2RectPBO() :
        VBoxVHWATextureNP2Rect(),
        mPBO(0)
    {}
    VBoxVHWATextureNP2RectPBO(const QRect & aRect, const VBoxVHWAColorFormat &aFormat, GLint scaleFuncttion) :
        VBoxVHWATextureNP2Rect(aRect, aFormat, scaleFuncttion),
        mPBO(0)
    {}

    virtual ~VBoxVHWATextureNP2RectPBO();

    virtual void init(uchar *pvMem);
protected:
    virtual void load();
    virtual void doUpdate(uchar * pAddress, const QRect * pRect);
    GLuint mPBO;
};

class VBoxVHWATextureNP2RectPBOMapped : public VBoxVHWATextureNP2RectPBO
{
public:
    VBoxVHWATextureNP2RectPBOMapped() :
        VBoxVHWATextureNP2RectPBO(),
        mpMappedAllignedBuffer(NULL),
        mcbAllignedBufferSize(0),
        mcbOffset(0)
    {}
    VBoxVHWATextureNP2RectPBOMapped(const QRect & aRect, const VBoxVHWAColorFormat &aFormat, GLint scaleFuncttion) :
            VBoxVHWATextureNP2RectPBO(aRect, aFormat, scaleFuncttion),
            mpMappedAllignedBuffer(NULL),
            mcbOffset(0)
    {
        mcbAllignedBufferSize = alignSize((size_t)memSize());
        mcbActualBufferSize = mcbAllignedBufferSize + 0x1fff;
    }

    uchar* mapAlignedBuffer();
    void   unmapBuffer();
    size_t alignedBufferSize() { return mcbAllignedBufferSize; }

    static size_t alignSize(size_t size)
    {
        size_t alSize = size & ~((size_t)0xfff);
        return alSize == size ? alSize : alSize + 0x1000;
    }

    static void* alignBuffer(void* pvMem) { return (void*)(((uintptr_t)pvMem) & ~((uintptr_t)0xfff)); }
    static size_t calcOffset(void* pvBase, void* pvOffset) { return (size_t)(((uintptr_t)pvBase) - ((uintptr_t)pvOffset)); }
protected:
    virtual void load();
    virtual void doUpdate(uchar * pAddress, const QRect * pRect);
private:
    uchar* mpMappedAllignedBuffer;
    size_t mcbAllignedBufferSize;
    size_t mcbOffset;
    size_t mcbActualBufferSize;
};

#define VBOXVHWAIMG_PBO    0x00000001U
#define VBOXVHWAIMG_PBOIMG 0x00000002U
#define VBOXVHWAIMG_FBO    0x00000004U
#define VBOXVHWAIMG_LINEAR 0x00000008U
typedef uint32_t VBOXVHWAIMG_TYPE;

class VBoxVHWATextureImage
{
public:
    VBoxVHWATextureImage(const QRect &size, const VBoxVHWAColorFormat &format, class VBoxVHWAGlProgramMngr * aMgr, VBOXVHWAIMG_TYPE flags);

    virtual ~VBoxVHWATextureImage()
    {
        for(uint i = 0; i < mcTex; i++)
        {
            delete mpTex[i];
        }
    }

    virtual void init(uchar *pvMem)
    {
        for(uint32_t i = 0; i < mcTex; i++)
        {
            mpTex[i]->init(pvMem);
            pvMem += mpTex[i]->memSize();
        }
    }

    virtual void update(const QRect * pRect)
    {
        mpTex[0]->update(pRect);
        if(mColorFormat.fourcc() == FOURCC_YV12)
        {
            if(pRect)
            {
                QRect rect(pRect->x()/2, pRect->y()/2,
                        pRect->width()/2, pRect->height()/2);
                mpTex[1]->update(&rect);
                mpTex[2]->update(&rect);
            }
            else
            {
                mpTex[1]->update(NULL);
                mpTex[2]->update(NULL);
            }
        }
    }

    virtual void display(VBoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool bNotIntersected);


    virtual void display();

    void deleteDisplay();

    int initDisplay(VBoxVHWATextureImage *pDst,
            const QRect * pDstRect, const QRect * pSrcRect,
            const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool bNotIntersected);

    bool displayInitialized() { return !!mVisibleDisplay;}

    virtual void setAddress(uchar *pvMem)
    {
        for(uint32_t i = 0; i < mcTex; i++)
        {
            mpTex[i]->setAddress(pvMem);
            pvMem += mpTex[i]->memSize();
        }
    }

    const QRect &rect()
    {
        return mpTex[0]->rect();
    }

    size_t memSize()
    {
        size_t size = 0;
        for(uint32_t i = 0; i < mcTex; i++)
        {
            size+=mpTex[i]->memSize();
        }
        return size;
    }

    uint32_t bytesPerLine() { return mpTex[0]->bytesPerLine(); }

    const VBoxVHWAColorFormat &colorFormat() { return mColorFormat; }

    uint32_t numComponents() {return mcTex;}

    VBoxVHWATexture* component(uint32_t i) {return mpTex[i]; }

    const VBoxVHWATextureImage *dst() { return mpDst;}
    const QRect& dstRect() { return mDstRect; }
    const QRect& srcRect() { return mSrcRect; }
    const VBoxVHWAColorKey* dstCKey() { return mpDstCKey; }
    const VBoxVHWAColorKey* srcCKey() { return mpSrcCKey; }
    bool notIntersectedMode() { return mbNotIntersected; }
protected:
    static int setCKey(class VBoxVHWAGlProgramVHWA * pProgram, const VBoxVHWAColorFormat * pFormat, const VBoxVHWAColorKey * pCKey, bool bDst);

    static bool matchCKeys(const VBoxVHWAColorKey * pCKey1, const VBoxVHWAColorKey * pCKey2)
    {
        return (pCKey1 == NULL && pCKey2 == NULL)
                || (*pCKey1 == *pCKey2);
    }

    void runDisplay(VBoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect)
    {
        bind(pDst);

        draw(pDst, pDstRect, pSrcRect);
    }

    virtual void draw(VBoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect);

    virtual uint32_t texCoord(GLenum tex, int x, int y)
    {
        uint32_t c = 1;
        mpTex[0]->multiTexCoord(tex, x, y);
        if(mColorFormat.fourcc() == FOURCC_YV12)
        {
            int x2 = x/2;
            int y2 = y/2;
            mpTex[1]->multiTexCoord(tex + 1, x2, y2);
            ++c;
        }
        return c;
    }

    virtual void bind(VBoxVHWATextureImage * pPrimary);

    virtual uint32_t calcProgramType(VBoxVHWATextureImage *pDst, const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool bNotIntersected);

    virtual class VBoxVHWAGlProgramVHWA * calcProgram(VBoxVHWATextureImage *pDst, const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool bNotIntersected);

    virtual int createDisplay(VBoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool bNotIntersected,
            GLuint *pDisplay, class VBoxVHWAGlProgramVHWA ** ppProgram);

    int createSetDisplay(VBoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool bNotIntersected);

    virtual int createDisplayList(VBoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool bNotIntersected,
            GLuint *pDisplay);

    virtual void deleteDisplayList();

    virtual void updateCKeys(VBoxVHWATextureImage * pDst, class VBoxVHWAGlProgramVHWA * pProgram, const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey);
    virtual void updateSetCKeys(const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey);

    void internalSetDstCKey(const VBoxVHWAColorKey * pDstCKey);
    void internalSetSrcCKey(const VBoxVHWAColorKey * pSrcCKey);

    VBoxVHWATexture *mpTex[3];
    uint32_t mcTex;
    GLuint mVisibleDisplay;
    class VBoxVHWAGlProgramVHWA * mpProgram;
    class VBoxVHWAGlProgramMngr * mProgramMngr;
    VBoxVHWAColorFormat mColorFormat;

    /* display info */
    VBoxVHWATextureImage *mpDst;
    QRect mDstRect;
    QRect mSrcRect;
    VBoxVHWAColorKey * mpDstCKey;
    VBoxVHWAColorKey * mpSrcCKey;
    VBoxVHWAColorKey mDstCKey;
    VBoxVHWAColorKey mSrcCKey;
    bool mbNotIntersected;
};

class VBoxVHWATextureImagePBO : public VBoxVHWATextureImage
{
public:
    VBoxVHWATextureImagePBO(const QRect &size, const VBoxVHWAColorFormat &format, class VBoxVHWAGlProgramMngr * aMgr, VBOXVHWAIMG_TYPE flags) :
            VBoxVHWATextureImage(size, format, aMgr, flags & (~VBOXVHWAIMG_PBO)),
            mPBO(0)
    {
    }

    virtual ~VBoxVHWATextureImagePBO()
    {
        if(mPBO)
        {
            VBOXQGL_CHECKERR(
                    vboxglDeleteBuffers(1, &mPBO);
                    );
        }
    }

    virtual void init(uchar *pvMem)
    {
        VBoxVHWATextureImage::init(pvMem);

        VBOXQGL_CHECKERR(
                vboxglGenBuffers(1, &mPBO);
                );
        mAddress = pvMem;

        VBOXQGL_CHECKERR(
                vboxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);
            );

        VBOXQGL_CHECKERR(
                vboxglBufferData(GL_PIXEL_UNPACK_BUFFER, memSize(), NULL, GL_STREAM_DRAW);
            );

        GLvoid *buf = vboxglMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        Assert(buf);
        if(buf)
        {
            memcpy(buf, mAddress, memSize());

            bool unmapped = vboxglUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            Assert(unmapped); NOREF(unmapped);
        }

        vboxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    }

    virtual void update(const QRect * pRect)
    {
        VBOXQGL_CHECKERR(
                vboxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);
        );

        GLvoid *buf;

        VBOXQGL_CHECKERR(
                buf = vboxglMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                );
        Assert(buf);
        if(buf)
        {
#ifdef VBOXVHWADBG_RENDERCHECK
            uint32_t * pBuf32 = (uint32_t*)buf;
            uchar * pBuf8 = (uchar*)buf;
            for(uint32_t i = 0; i < mcTex; i++)
            {
                uint32_t dbgSetVal = 0x40404040 * (i+1);
                for(uint32_t k = 0; k < mpTex[i]->memSize()/sizeof(pBuf32[0]); k++)
                {
                    pBuf32[k] = dbgSetVal;
                }

                pBuf8 += mpTex[i]->memSize();
                pBuf32 = (uint32_t *)pBuf8;
            }
#else
            memcpy(buf, mAddress, memSize());
#endif

            bool unmapped;
            VBOXQGL_CHECKERR(
                    unmapped = vboxglUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                    );

            Assert(unmapped);

            VBoxVHWATextureImage::setAddress(0);

            VBoxVHWATextureImage::update(NULL);

            VBoxVHWATextureImage::setAddress(mAddress);

            vboxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
        else
        {
            VBOXQGLLOGREL(("failed to map PBO, trying fallback to non-PBO approach\n"));

            VBoxVHWATextureImage::setAddress(mAddress);

            VBoxVHWATextureImage::update(pRect);
        }
    }

    virtual void setAddress(uchar *pvMem)
    {
        mAddress = pvMem;
    }
private:
    GLuint mPBO;
    uchar* mAddress;
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
    VBoxVHWASurfaceBase (
            class VBoxGLWidget *aWidget,
            const QSize & aSize,
            const QRect & aTargRect,
            const QRect & aSrcRect,
            const QRect & aVisTargRect,
            VBoxVHWAColorFormat & aColorFormat,
            VBoxVHWAColorKey * pSrcBltCKey, VBoxVHWAColorKey * pDstBltCKey,
            VBoxVHWAColorKey * pSrcOverlayCKey, VBoxVHWAColorKey * pDstOverlayCKey,
            VBOXVHWAIMG_TYPE aImgFlags);

    virtual ~VBoxVHWASurfaceBase();

    void init (VBoxVHWASurfaceBase * pPrimary, uchar *pvMem);

    void uninit();

    static void globalInit();

    int lock (const QRect * pRect, uint32_t flags);

    int unlock();

    void updatedMem (const QRect * aRect);

    bool performDisplay (VBoxVHWASurfaceBase *pPrimary, bool bForce);

    void setRects (const QRect & aTargRect, const QRect & aSrcRect);
    void setTargRectPosition (const QPoint & aPoint);

    void updateVisibility (VBoxVHWASurfaceBase *pPrimary, const QRect & aVisibleTargRect, bool bNotIntersected, bool bForce);

    static ulong calcBytesPerPixel (GLenum format, GLenum type);

    static GLsizei makePowerOf2 (GLsizei val);

    bool    addressAlocated() const { return mFreeAddress; }
    uchar * address() { return mAddress; }

    ulong   memSize();

    ulong width()  { return mRect.width();  }
    ulong height() { return mRect.height(); }
    const QSize size() {return mRect.size();}

    uint32_t fourcc() {return mImage->colorFormat().fourcc(); }

    ulong  bitsPerPixel() { return mImage->colorFormat().bitsPerPixel(); }
    ulong  bytesPerLine() { return mImage->bytesPerLine(); }

    const VBoxVHWAColorKey * dstBltCKey() const { return mpDstBltCKey; }
    const VBoxVHWAColorKey * srcBltCKey() const { return mpSrcBltCKey; }
    const VBoxVHWAColorKey * dstOverlayCKey() const { return mpDstOverlayCKey; }
    const VBoxVHWAColorKey * defaultSrcOverlayCKey() const { return mpDefaultSrcOverlayCKey; }
    const VBoxVHWAColorKey * defaultDstOverlayCKey() const { return mpDefaultDstOverlayCKey; }
    const VBoxVHWAColorKey * srcOverlayCKey() const { return mpSrcOverlayCKey; }
    void resetDefaultSrcOverlayCKey() { mpSrcOverlayCKey = mpDefaultSrcOverlayCKey; }
    void resetDefaultDstOverlayCKey() { mpDstOverlayCKey = mpDefaultDstOverlayCKey; }

    void setDstBltCKey (const VBoxVHWAColorKey * ckey)
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

    void setSrcBltCKey (const VBoxVHWAColorKey * ckey)
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

    void setDefaultDstOverlayCKey (const VBoxVHWAColorKey * ckey)
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

    void setDefaultSrcOverlayCKey (const VBoxVHWAColorKey * ckey)
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

    void setOverriddenDstOverlayCKey (const VBoxVHWAColorKey * ckey)
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

    void setOverriddenSrcOverlayCKey (const VBoxVHWAColorKey * ckey)
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

    const VBoxVHWAColorKey * getActiveDstOverlayCKey (VBoxVHWASurfaceBase * pPrimary)
    {
        return mpDstOverlayCKey ? mpDefaultDstOverlayCKey : (pPrimary ? pPrimary->mpDstOverlayCKey : NULL);
    }

    const VBoxVHWAColorFormat & colorFormat() { return mImage->colorFormat(); }

    void setAddress(uchar * addr);

    const QRect& rect() const {return mRect;}
    const QRect& srcRect() const {return mSrcRect; }
    const QRect& targRect() const {return mTargRect; }
    class VBoxVHWASurfList * getComplexList() {return mComplexList; }

    class VBoxVHWAGlProgramMngr * getGlProgramMngr();

    uint32_t handle() const {return mHGHandle;}
    void setHandle(uint32_t h) {mHGHandle = h;}

    const VBoxVHWADirtyRect & getDirtyRect() { return mUpdateMem2TexRect; }

    VBoxVHWASurfaceBase * primary() { return mpPrimary; }
    void setPrimary(VBoxVHWASurfaceBase *apPrimary) { mpPrimary = apPrimary; }
private:
    void setRectValues (const QRect & aTargRect, const QRect & aSrcRect);
    void setVisibleRectValues (const QRect & aVisTargRect);

    void setComplexList (VBoxVHWASurfList *aComplexList) { mComplexList = aComplexList; }
    void initDisplay();

    bool synchTexMem (const QRect * aRect);

    int performBlt (const QRect * pDstRect, VBoxVHWASurfaceBase * pSrcSurface, const QRect * pSrcRect, const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool blt);

    QRect mRect; /* == Inv FB size */

    QRect mSrcRect;
    QRect mTargRect; /* == Vis FB size */

    QRect mVisibleTargRect;
    QRect mVisibleSrcRect;

    class VBoxVHWATextureImage * mImage;

    uchar * mAddress;

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
    bool mbNotIntersected;

    class VBoxVHWASurfList *mComplexList;

    VBoxVHWASurfaceBase *mpPrimary;

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
        mSurfVGA(NULL),
        mbDisplayPrimary(true)
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

    bool performDisplay(bool bForce)
    {
        VBoxVHWASurfaceBase * pPrimary = mPrimary.current();

        if(mbDisplayPrimary)
        {
#ifdef DEBUG_misha
            /* should only display overlay now */
            AssertBreakpoint();
#endif
            bForce |= pPrimary->performDisplay(NULL, bForce);
        }

        for (OverlayList::const_iterator it = mOverlays.begin();
             it != mOverlays.end(); ++ it)
        {
            VBoxVHWASurfaceBase * pOverlay = (*it)->current();
            if(pOverlay)
            {
                bForce |= pOverlay->performDisplay(pPrimary, bForce);
            }
        }
        return bForce;
    }

    bool isPrimary(VBoxVHWASurfaceBase * pSurf) { return pSurf->getComplexList() == &mPrimary; }

    void setDisplayPrimary(bool bDisplay) { mbDisplayPrimary = bDisplay; }

    const OverlayList & overlays() const {return mOverlays;}
    const VBoxVHWASurfList & primaries() const { return mPrimary; }

private:
    VBoxVHWASurfaceBase *mSurfVGA;
    VBoxVHWASurfList mPrimary;

    OverlayList mOverlays;

    bool mbDisplayPrimary;
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
    VBoxVHWACommandElementProcessor(QObject *pParent);
    ~VBoxVHWACommandElementProcessor();
    void postCmd(VBOXVHWA_PIPECMD_TYPE aType, void * pvData, uint32_t flags);
    void completeCurrentEvent();
    class VBoxVHWACommandElement * detachCmdList(class VBoxVHWACommandElement * pFirst2Free, VBoxVHWACommandElement * pLast2Free);
    void reset(class VBoxVHWACommandElement ** ppHead, class VBoxVHWACommandElement ** ppTail);
private:
    RTCRITSECT mCritSect;
    class VBoxVHWACommandProcessEvent *mpFirstEvent;
    class VBoxVHWACommandProcessEvent *mpLastEvent;
    QObject *m_pParent;
    bool mbNewEvent;
    bool mbProcessingList;
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
    VBoxGLWidget (class VBoxConsoleView *aView, QWidget *aParent, VBoxVHWASettings *aSettings);
    ~VBoxGLWidget();

    ulong vboxPixelFormat() { return mPixelFormat; }
    bool vboxUsesGuestVRAM() { return mUsesGuestVRAM; }

    uchar *vboxAddress() { return mDisplay.getVGA() ? mDisplay.getVGA()->address() : NULL; }

#ifdef VBOX_WITH_VIDEOHWACCEL
    uchar *vboxVRAMAddressFromOffset(uint64_t offset);
    uint64_t vboxVRAMOffsetFromAddress(uchar* addr);
    uint64_t vboxVRAMOffset(VBoxVHWASurfaceBase * pSurf);

    void vhwaSaveExec(struct SSMHANDLE * pSSM);
    static void vhwaSaveExecVoid(struct SSMHANDLE * pSSM);
    static int vhwaLoadExec(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version);

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
    QRect overlaysRectUnion();
    QRect overlaysRectIntersection();
#endif

    int reset(VHWACommandList * pCmdList);

    ulong vboxBitsPerPixel() { return mDisplay.getVGA()->bitsPerPixel(); }
    ulong vboxBytesPerLine() { return mDisplay.getVGA() ? mDisplay.getVGA()->bytesPerLine() : 0; }
    int vboxFbWidth() {return mDisplay.getVGA()->width(); }
    int vboxFbHeight() {return mDisplay.getVGA()->height(); }
    bool vboxIsInitialized() {return mDisplay.getVGA() != NULL; }

    void vboxDoResize(void *re);

    void vboxResizeEvent (class VBoxResizeEvent *re) {vboxPerformGLOp(&VBoxGLWidget::vboxDoResize, re); }

    void vboxProcessVHWACommands(class VBoxVHWACommandElementProcessor * pPipe) {vboxPerformGLOp(&VBoxGLWidget::vboxDoProcessVHWACommands, pPipe);}

    class VBoxVHWAGlProgramMngr * vboxVHWAGetGlProgramMngr() { return mpMngr; }

    VBoxVHWASurfaceBase * vboxGetVGASurface() { return mDisplay.getVGA(); }

#ifdef VBOXVHWA_OLD_COORD
    static void doSetupMatrix(const QSize & aSize, bool bInverted);
#endif

    void vboxDoUpdateViewport(const QRect & aRect);
    void vboxDoUpdateRect(const QRect * pRect);

    const QRect & vboxViewport() const {return mViewport;}

#ifdef VBOXVHWA_PROFILE_FPS
    void reportNewFrame() { mbNewFrame = true; }
#endif

    bool performDisplayAndSwap(bool bForce)
    {
        bForce = mDisplay.performDisplay(bForce | mRepaintNeeded);
        if(bForce)
        {
            swapBuffers();
        }

#ifdef VBOXVHWA_PROFILE_FPS
        if(mbNewFrame)
        {
            mFPSCounter.frame();
            double fps = mFPSCounter.fps();
            if(!(mFPSCounter.frames() % 31))
            {
                printf("fps: %f\n", fps);
            }
            mbNewFrame = false;
        }
#endif
        return bForce;
    }

    VHWACommandList &onResizeCmdList() { return mOnResizeCmdList; }

    static void pushSettingsAndSetupViewport(const QSize &display, const QRect &viewport)
    {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        setupMatricies(display, false);
        adjustViewport(display, viewport);
    }

    static void popSettingsAfterSetupViewport()
    {
        glPopAttrib();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }

protected:

    void paintGL()
    {
        if(mpfnOp)
        {
            (this->*mpfnOp)(mOpContext);
            mpfnOp = NULL;
        }
        VBOXQGLLOG(("paintGL\n"));

        mDisplay.performDisplay(true);
    }

    void initializeGL();

private:
    static void setupMatricies(const QSize &display, bool bInvert);
    static void adjustViewport(const QSize &display, const QRect &viewport);


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
    static int vhwaLoadSurface(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t cBackBuffers, uint32_t u32Version);
    int vhwaSaveOverlayData(struct SSMHANDLE * pSSM, VBoxVHWASurfaceBase *pSurf, bool bVisible);
    static int vhwaLoadOverlayData(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version);
    static int vhwaLoadVHWAEnable(VHWACommandList * pCmdList);

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

    bool mRepaintNeeded;

    QRect mViewport;

    class VBoxConsoleView *mView;

    VBoxVHWASurfList *mConstructingList;
    int32_t mcRemaining2Contruct;

    /* this is used in saved state restore to postpone surface restoration
     * till the framebuffer size is restored */
    VHWACommandList mOnResizeCmdList;

    class VBoxVHWAGlProgramMngr *mpMngr;

    VBoxVHWASettings *mSettings;

#ifdef VBOXVHWA_PROFILE_FPS
    VBoxVHWADbgTimer mFPSCounter;
    bool mbNewFrame;
#endif
};

class VBoxVHWAFBO
{
public:
    VBoxVHWAFBO() :
            mFBO(0)
    {}

    ~VBoxVHWAFBO()
    {
        if(mFBO)
        {
            vboxglDeleteFramebuffers(1, &mFBO);
        }
    }

    void init()
    {
        VBOXQGL_CHECKERR(
                vboxglGenFramebuffers(1, &mFBO);
        );
    }

    void bind()
    {
        VBOXQGL_CHECKERR(
            vboxglBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        );
    }

    void unbind()
    {
        VBOXQGL_CHECKERR(
            vboxglBindFramebuffer(GL_FRAMEBUFFER, 0);
        );
    }

    void attachBound(VBoxVHWATexture *pTex)
    {
        VBOXQGL_CHECKERR(
                vboxglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pTex->texTarget(), pTex->texture(), 0);
        );
    }

private:
    GLuint mFBO;
};

template <class T>
class VBoxVHWATextureImageFBO : public T
{
public:
    VBoxVHWATextureImageFBO(const QRect &size, const VBoxVHWAColorFormat &format, class VBoxVHWAGlProgramMngr * aMgr, VBOXVHWAIMG_TYPE flags) :
            T(size, format, aMgr, flags & (~VBOXVHWAIMG_FBO)),
            mFBOTex(size, VBoxVHWAColorFormat(32, 0xff0000, 0xff00, 0xff), aMgr, (flags & (~VBOXVHWAIMG_FBO)) | VBOXVHWAIMG_LINEAR),
            mpvFBOTexMem(NULL)
    {
    }

    virtual ~VBoxVHWATextureImageFBO()
    {
        if(mpvFBOTexMem)
            free(mpvFBOTexMem);
    }

    virtual void init(uchar *pvMem)
    {
        mFBO.init();
        mpvFBOTexMem = (uchar*)malloc(mFBOTex.memSize());
        mFBOTex.init(mpvFBOTexMem);
        T::init(pvMem);
        mFBO.bind();
        mFBO.attachBound(mFBOTex.component(0));
        mFBO.unbind();
    }

    virtual int createDisplay(VBoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool bNotIntersected,
            GLuint *pDisplay, class VBoxVHWAGlProgramVHWA ** ppProgram)
    {
        T::createDisplay(NULL, &mFBOTex.rect(), &rect(),
                NULL, NULL, false,
                pDisplay, ppProgram);

        return mFBOTex.initDisplay(pDst, pDstRect, pSrcRect,
                pDstCKey, pSrcCKey, bNotIntersected);
    }

    virtual void update(const QRect * pRect)
    {
        T::update(pRect);

        VBoxGLWidget::pushSettingsAndSetupViewport(rect().size(), rect());
        mFBO.bind();
        T::display();
        mFBO.unbind();
        VBoxGLWidget::popSettingsAfterSetupViewport();
    }

    virtual void display(VBoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VBoxVHWAColorKey * pDstCKey, const VBoxVHWAColorKey * pSrcCKey, bool bNotIntersected)
    {
        mFBOTex.display(pDst, pDstRect, pSrcRect, pDstCKey, pSrcCKey, bNotIntersected);
    }

    virtual void display()
    {
        mFBOTex.display();
    }

    const QRect &rect() { return T::rect(); }
private:
    VBoxVHWAFBO mFBO;
    VBoxVHWATextureImage mFBOTex;
    uchar * mpvFBOTexMem;
};

class VBoxQGLOverlay
{
public:
    VBoxQGLOverlay (class VBoxConsoleView *aView, class VBoxFrameBuffer * aContainer, CSession * aSession);

    int onVHWACommand (struct _VBOXVHWACMD * pCommand);

    void onVHWACommandEvent (QEvent * pEvent);

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
    bool onRequestResize (ULONG /*aScreenId*/, ULONG /*aPixelFormat*/,
                          BYTE * /*aVRAM*/, ULONG /*aBitsPerPixel*/, ULONG /*aBytesPerLine*/,
                          ULONG /*aWidth*/, ULONG /*aHeight*/,
                          BOOL * /*aFinished*/)
    {
        mCmdPipe.completeCurrentEvent();
        return false;
    }

    void onResizeEvent (const class VBoxResizeEvent *re);
    void onResizeEventPostprocess (const class VBoxResizeEvent *re);

    void onViewportResized (QResizeEvent * /*re*/)
    {
        vboxDoCheckUpdateViewport();
        mGlCurrent = false;
    }

    void onViewportScrolled (int /*dx*/, int /*dy*/)
    {
        vboxDoCheckUpdateViewport();
        mGlCurrent = false;
    }

    static bool isAcceleration2DVideoAvailable();

    /** additional video memory required for the best 2D support performance
     *  total amount of VRAM required is thus calculated as requiredVideoMemory + required2DOffscreenVideoMemory  */
    static quint64 required2DOffscreenVideoMemory();

    /* not supposed to be called by clients */
    int vhwaLoadExec (struct SSMHANDLE * pSSM, uint32_t u32Version);
    void vhwaSaveExec (struct SSMHANDLE * pSSM);
private:
    int vhwaSurfaceUnlock (struct _VBOXVHWACMD_SURF_UNLOCK *pCmd);

    void repaintMain();
    void repaintOverlay()
    {
        if(mNeedOverlayRepaint)
        {
            mNeedOverlayRepaint = false;
            performDisplayOverlay();
        }
        if(mNeedSetVisible)
        {
            mNeedSetVisible = false;
            mpOverlayWidget->setVisible (true);
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
            makeCurrent();
            mpOverlayWidget->performDisplayAndSwap (false);
        }
    }

    void vboxSetGlOn (bool on);
    bool vboxGetGlOn() { return mGlOn; }
    bool vboxSynchGl();
    void vboxDoVHWACmdExec(void *cmd);
    void vboxShowOverlay (bool show);
    void vboxDoCheckUpdateViewport();
    void vboxDoVHWACmd (void *cmd);
    void addMainDirtyRect (const QRect & aRect);
    void vboxCheckUpdateOverlay (const QRect & rect);
    VBoxVHWACommandElement * processCmdList (VBoxVHWACommandElement * pCmd);

    int vhwaConstruct (struct _VBOXVHWACMD_HH_CONSTRUCT *pCmd);

    int reset();

    int resetGl();

    void initGl();

    VBoxGLWidget *mpOverlayWidget;
    class VBoxConsoleView *mView;
    class VBoxFrameBuffer *mContainer;
    bool mGlOn;
    bool mOverlayWidgetVisible;
    bool mOverlayVisible;
    bool mGlCurrent;
    bool mProcessingCommands;
    bool mNeedOverlayRepaint;
    bool mNeedSetVisible;
    QRect mOverlayViewport;
    VBoxVHWADirtyRect mMainDirtyRect;

    VBoxVHWACommandElementProcessor mCmdPipe;

    /* this is used in saved state restore to postpone surface restoration
     * till the framebuffer size is restored */
    VHWACommandList mOnResizeCmdList;

    VBoxVHWASettings mSettings;
};


template <class T>
class VBoxOverlayFrameBuffer : public T
{
public:
    VBoxOverlayFrameBuffer (class VBoxConsoleView *aView, CSession * aSession)
        : T (aView),
          mOverlay (aView, this, aSession)
    {}


    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand)
    {
        return mOverlay.onVHWACommand ((struct _VBOXVHWACMD*)pCommand);
    }

    void doProcessVHWACommand (QEvent * pEvent)
    {
        mOverlay.onVHWACommandEvent (pEvent);
    }

    STDMETHOD(RequestResize) (ULONG aScreenId, ULONG aPixelFormat,
                              BYTE *aVRAM, ULONG aBitsPerPixel, ULONG aBytesPerLine,
                              ULONG aWidth, ULONG aHeight,
                              BOOL *aFinished)
   {
        if (mOverlay.onRequestResize (aScreenId, aPixelFormat,
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
        if (mOverlay.onNotifyUpdate (aX, aY, aW, aH))
            return S_OK;
        return T::NotifyUpdate (aX, aY, aW, aH);
    }

    void resizeEvent (VBoxResizeEvent *re)
    {
        mOverlay.onResizeEvent (re);
        T::resizeEvent (re);
        mOverlay.onResizeEventPostprocess (re);
    }

    void viewportResized (QResizeEvent * re)
    {
        mOverlay.onViewportResized (re);
        T::viewportResized (re);
    }

    void viewportScrolled (int dx, int dy)
    {
        mOverlay.onViewportScrolled (dx, dy);
        T::viewportScrolled (dx, dy);
    }
private:
    VBoxQGLOverlay mOverlay;
};

#endif

#endif /* #ifndef __VBoxFBOverlay_h__ */

