/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachine class implementation.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#ifdef VBOX_WS_WIN
# include <QBitmap>
#endif

/* GUI includes: */
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIMachine.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CMachine.h"
#include "CSession.h"
#include "CConsole.h"
#include "CSnapshot.h"
#include "CProgress.h"


/* static */
UIMachine *UIMachine::m_spInstance = 0;

/* static */
bool UIMachine::startMachine(const QUuid &uID)
{
    /* Make sure machine is not created: */
    AssertReturn(!m_spInstance, false);

    /* Restore current snapshot if requested: */
    if (uiCommon().shouldRestoreCurrentSnapshot())
    {
        /* Create temporary session: */
        CSession session = uiCommon().openSession(uID, KLockType_VM);
        if (session.isNull())
            return false;

        /* Which VM we operate on? */
        CMachine machine = session.GetMachine();
        /* Which snapshot we are restoring? */
        CSnapshot snapshot = machine.GetCurrentSnapshot();

        /* Prepare restore-snapshot progress: */
        CProgress progress = machine.RestoreSnapshot(snapshot);
        if (!machine.isOk())
            return msgCenter().cannotRestoreSnapshot(machine, snapshot.GetName(), machine.GetName());

        /* Show the snapshot-discarding progress: */
        msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_snapshot_discard_90px.png");
        if (progress.GetResultCode() != 0)
            return msgCenter().cannotRestoreSnapshot(progress, snapshot.GetName(), machine.GetName());

        /* Unlock session finally: */
        session.UnlockMachine();

        /* Clear snapshot-restoring request: */
        uiCommon().setShouldRestoreCurrentSnapshot(false);
    }

    /* For separate process we should launch VM before UI: */
    if (uiCommon().isSeparateProcess())
    {
        /* Get corresponding machine: */
        CMachine machine = uiCommon().virtualBox().FindMachine(uiCommon().managedVMUuid().toString());
        AssertMsgReturn(!machine.isNull(), ("UICommon::managedVMUuid() should have filter that case before!\n"), false);

        /* Try to launch corresponding machine: */
        if (!UICommon::launchMachine(machine, UILaunchMode_Separate))
            return false;
    }

    /* Try to create machine UI: */
    return create();
}

/* static */
bool UIMachine::create()
{
    /* Make sure machine is not created: */
    AssertReturn(!m_spInstance, false);

    /* Create machine UI: */
    new UIMachine;
    /* Make sure it's prepared: */
    if (!m_spInstance->prepare())
    {
        /* Destroy machine UI otherwise: */
        destroy();
        /* False in that case: */
        return false;
    }
    /* True by default: */
    return true;
}

/* static */
void UIMachine::destroy()
{
    /* Make sure machine is created: */
    if (!m_spInstance)
        return;

    /* Protect versus recursive call: */
    UIMachine *pInstance = m_spInstance;
    m_spInstance = 0;
    /* Cleanup machine UI: */
    pInstance->cleanup();
    /* Destroy machine UI: */
    delete pInstance;
}

QWidget* UIMachine::activeWindow() const
{
    return   machineLogic() && machineLogic()->activeMachineWindow()
           ? machineLogic()->activeMachineWindow()
           : 0;
}

void UIMachine::asyncChangeVisualState(UIVisualStateType visualState)
{
    emit sigRequestAsyncVisualStateChange(visualState);
}

void UIMachine::setRequestedVisualState(UIVisualStateType visualStateType)
{
    /* Remember requested visual state: */
    m_enmRequestedVisualState = visualStateType;

    /* Save only if it's different from Invalid and from current one: */
    if (   m_enmRequestedVisualState != UIVisualStateType_Invalid
        && gEDataManager->requestedVisualState(uiCommon().managedVMUuid()) != m_enmRequestedVisualState)
        gEDataManager->setRequestedVisualState(m_enmRequestedVisualState, uiCommon().managedVMUuid());
}

UIVisualStateType UIMachine::requestedVisualState() const
{
    return m_enmRequestedVisualState;
}

void UIMachine::closeRuntimeUI()
{
    /* Quit application: */
    QApplication::quit();
}

void UIMachine::sltChangeVisualState(UIVisualStateType visualState)
{
    /* Create new machine-logic: */
    UIMachineLogic *pMachineLogic = UIMachineLogic::create(this, m_pSession, visualState);

    /* First we have to check if the selected machine-logic is available at all.
     * Only then we delete the old machine-logic and switch to the new one. */
    if (pMachineLogic->checkAvailability())
    {
        /* Delete previous machine-logic if exists: */
        if (m_pMachineLogic)
        {
            m_pMachineLogic->cleanup();
            UIMachineLogic::destroy(m_pMachineLogic);
            m_pMachineLogic = 0;
        }

        /* Set the new machine-logic as current one: */
        m_pMachineLogic = pMachineLogic;
        m_pMachineLogic->prepare();

        /* Remember new visual state: */
        m_visualState = visualState;

        /* Save requested visual state: */
        gEDataManager->setRequestedVisualState(m_visualState, uiCommon().managedVMUuid());
    }
    else
    {
        /* Delete temporary created machine-logic: */
        pMachineLogic->cleanup();
        UIMachineLogic::destroy(pMachineLogic);
    }

    /* Make sure machine-logic exists: */
    if (!m_pMachineLogic)
    {
        /* Reset initial visual state  to normal: */
        m_initialVisualState = UIVisualStateType_Normal;
        /* Enter initial visual state again: */
        enterInitialVisualState();
    }
}

void UIMachine::sltHandleKeyboardLedsChange(bool fNumLock, bool fCapsLock, bool fScrollLock)
{
    /* Check if something had changed: */
    if (   m_fNumLock != fNumLock
        || m_fCapsLock != fCapsLock
        || m_fScrollLock != fScrollLock)
    {
        /* Store new num lock data: */
        if (m_fNumLock != fNumLock)
        {
            m_fNumLock = fNumLock;
            m_uNumLockAdaptionCnt = 2;
        }

        /* Store new caps lock data: */
        if (m_fCapsLock != fCapsLock)
        {
            m_fCapsLock = fCapsLock;
            m_uCapsLockAdaptionCnt = 2;
        }

        /* Store new scroll lock data: */
        if (m_fScrollLock != fScrollLock)
        {
            m_fScrollLock = fScrollLock;
        }

        /* Notify listeners: */
        emit sigKeyboardLedsChange();
    }
}

void UIMachine::sltMousePointerShapeChange(const UIMousePointerShapeData &shapeData)
{
    LogRelFlow(("GUI: UIMachine::sltMousePointerShapeChange: "
                "Is visible: %s, Has alpha: %s, "
                "Hot spot: %dx%d, Shape size: %dx%d, "
                "Shape data: %s\n",
                shapeData.isVisible() ? "TRUE" : "FALSE",
                shapeData.hasAlpha() ? "TRUE" : "FALSE",
                shapeData.hotSpot().x(), shapeData.hotSpot().y(),
                shapeData.shapeSize().width(), shapeData.shapeSize().height(),
                shapeData.shape().isEmpty() ? "EMPTY" : "PRESENT"));

    /* In case if shape itself is present: */
    if (shapeData.shape().size() > 0)
    {
        /* We are ignoring visibility flag: */
        m_fIsHidingHostPointer = false;

        /* And updating current shape data: */
        m_shapeData = shapeData;
        updateMousePointerShape();
    }
    /* In case if shape itself is NOT present: */
    else
    {
        /* Remember if we should hide the cursor: */
        m_fIsHidingHostPointer = !shapeData.isVisible();
    }

    /* Notify listeners: */
    emit sigMousePointerShapeChange();
}

void UIMachine::sltMouseCapabilityChange(bool fSupportsAbsolute, bool fSupportsRelative,
                                         bool fSupportsTouchScreen, bool fSupportsTouchPad,
                                         bool fNeedsHostCursor)
{
    LogRelFlow(("GUI: UIMachine::sltMouseCapabilityChange: "
                "Supports absolute: %s, Supports relative: %s, "
                "Supports touchscreen: %s, Supports touchpad: %s, "
                "Needs host cursor: %s\n",
                fSupportsAbsolute ? "TRUE" : "FALSE", fSupportsRelative ? "TRUE" : "FALSE",
                fSupportsTouchScreen ? "TRUE" : "FALSE", fSupportsTouchPad ? "TRUE" : "FALSE",
                fNeedsHostCursor ? "TRUE" : "FALSE"));

    /* Check if something had changed: */
    if (   m_fIsMouseSupportsAbsolute != fSupportsAbsolute
        || m_fIsMouseSupportsRelative != fSupportsRelative
        || m_fIsMouseSupportsTouchScreen != fSupportsTouchScreen
        || m_fIsMouseSupportsTouchPad != fSupportsTouchPad
        || m_fIsMouseHostCursorNeeded != fNeedsHostCursor)
    {
        /* Store new data: */
        m_fIsMouseSupportsAbsolute = fSupportsAbsolute;
        m_fIsMouseSupportsRelative = fSupportsRelative;
        m_fIsMouseSupportsTouchScreen = fSupportsTouchScreen;
        m_fIsMouseSupportsTouchPad = fSupportsTouchPad;
        m_fIsMouseHostCursorNeeded = fNeedsHostCursor;

        /* Notify listeners: */
        emit sigMouseCapabilityChange();
    }
}

void UIMachine::sltCursorPositionChange(bool fContainsData, unsigned long uX, unsigned long uY)
{
    LogRelFlow(("GUI: UIMachine::sltCursorPositionChange: "
                "Cursor position valid: %d, Cursor position: %dx%d\n",
                fContainsData ? "TRUE" : "FALSE", uX, uY));

    /* Check if something had changed: */
    if (   m_fIsValidCursorPositionPresent != fContainsData
        || m_cursorPosition.x() != (int)uX
        || m_cursorPosition.y() != (int)uY)
    {
        /* Store new data: */
        m_fIsValidCursorPositionPresent = fContainsData;
        m_cursorPosition = QPoint(uX, uY);

        /* Notify listeners: */
        emit sigCursorPositionChange();
    }
}

UIMachine::UIMachine()
    : QObject(0)
    , m_pSession(0)
    , m_allowedVisualStates(UIVisualStateType_Invalid)
    , m_initialVisualState(UIVisualStateType_Normal)
    , m_visualState(UIVisualStateType_Invalid)
    , m_enmRequestedVisualState(UIVisualStateType_Invalid)
    , m_pMachineLogic(0)
    , m_pMachineWindowIcon(0)
    , m_fNumLock(false)
    , m_fCapsLock(false)
    , m_fScrollLock(false)
    , m_uNumLockAdaptionCnt(2)
    , m_uCapsLockAdaptionCnt(2)
    , m_iKeyboardState(0)
    , m_fIsHidingHostPointer(true)
    , m_fIsValidPointerShapePresent(false)
    , m_fIsValidCursorPositionPresent(false)
    , m_fIsMouseSupportsAbsolute(false)
    , m_fIsMouseSupportsRelative(false)
    , m_fIsMouseSupportsTouchScreen(false)
    , m_fIsMouseSupportsTouchPad(false)
    , m_fIsMouseHostCursorNeeded(false)
    , m_fIsMouseCaptured(false)
    , m_fIsMouseIntegrated(true)
    , m_iMouseState(0)
{
    m_spInstance = this;
}

UIMachine::~UIMachine()
{
    m_spInstance = 0;
}

bool UIMachine::prepare()
{
    /* Try to create session UI: */
    if (!UISession::create(m_pSession, this))
        return false;

    /* Make sure session UI created: */
    AssertReturn(m_pSession, false);

    /* Cache media data early if necessary: */
    if (uiCommon().agressiveCaching())
        uiCommon().enumerateMedia(m_pSession->machineMedia());

    /* Prepare stuff: */
    prepareSessionConnections();
    prepareMachineWindowIcon();
    prepareMachineLogic();

    /* Load settings: */
    loadSettings();

    /* Try to initialize session UI: */
    if (!uisession()->initialize())
        return false;

    /* Update stuff: */
    updateMouseState();

    /* True by default: */
    return true;
}

void UIMachine::prepareSessionConnections()
{
    /* Keyboard stuff: */
    connect(uisession(), &UISession::sigKeyboardLedsChange,
            this, &UIMachine::sltHandleKeyboardLedsChange);

    /* Mouse stuff: */
    connect(uisession(), &UISession::sigMousePointerShapeChange,
            this, &UIMachine::sltMousePointerShapeChange);
    connect(uisession(), &UISession::sigMouseCapabilityChange,
            this, &UIMachine::sltMouseCapabilityChange);
    connect(uisession(), &UISession::sigCursorPositionChange,
            this, &UIMachine::sltCursorPositionChange);
}

void UIMachine::prepareMachineWindowIcon()
{
    /* Acquire user machine-window icon: */
    QIcon icon = generalIconPool().userMachineIcon(uisession()->machine());
    /* Use the OS type icon if user one was not set: */
    if (icon.isNull())
        icon = generalIconPool().guestOSTypeIcon(uisession()->machine().GetOSTypeId());
    /* Use the default icon if nothing else works: */
    if (icon.isNull())
        icon = QIcon(":/VirtualBox_48px.png");
    /* Store the icon dynamically: */
    m_pMachineWindowIcon = new QIcon(icon);
}

void UIMachine::prepareMachineLogic()
{
    /* Prepare async visual state type change handler: */
    qRegisterMetaType<UIVisualStateType>();
    connect(this, &UIMachine::sigRequestAsyncVisualStateChange,
            this, &UIMachine::sltChangeVisualState,
            Qt::QueuedConnection);

    /* Load restricted visual states: */
    UIVisualStateType restrictedVisualStates = gEDataManager->restrictedVisualStates(uiCommon().managedVMUuid());
    /* Acquire allowed visual states: */
    m_allowedVisualStates = static_cast<UIVisualStateType>(UIVisualStateType_All ^ restrictedVisualStates);

    /* Load requested visual state, it can override initial one: */
    m_enmRequestedVisualState = gEDataManager->requestedVisualState(uiCommon().managedVMUuid());
    /* Check if requested visual state is allowed: */
    if (isVisualStateAllowed(m_enmRequestedVisualState))
    {
        switch (m_enmRequestedVisualState)
        {
            /* Direct transition allowed to scale/fullscreen modes only: */
            case UIVisualStateType_Scale:      m_initialVisualState = UIVisualStateType_Scale; break;
            case UIVisualStateType_Fullscreen: m_initialVisualState = UIVisualStateType_Fullscreen; break;
            default: break;
        }
    }

    /* Enter initial visual state: */
    enterInitialVisualState();
}

void UIMachine::cleanupMachineLogic()
{
    /* Destroy machine-logic if exists: */
    if (m_pMachineLogic)
    {
        m_pMachineLogic->cleanup();
        UIMachineLogic::destroy(m_pMachineLogic);
        m_pMachineLogic = 0;
    }
}

void UIMachine::loadSettings()
{
    /* Load extra-data settings: */
    {
        /* Get machine ID: */
        const QUuid uMachineID = uiCommon().managedVMUuid();
        Q_UNUSED(uMachineID);

#ifndef VBOX_WS_MAC
        /* Load user's machine-window name postfix: */
        m_strMachineWindowNamePostfix = gEDataManager->machineWindowNamePostfix(uMachineID);
#endif
    }
}

void UIMachine::cleanupMachineWindowIcon()
{
    /* Cleanup machine-window icon: */
    delete m_pMachineWindowIcon;
    m_pMachineWindowIcon = 0;
}

void UIMachine::cleanupSession()
{
    /* Destroy session UI if exists: */
    if (uisession())
        UISession::destroy(m_pSession);
}

void UIMachine::cleanup()
{
    /* Preprocess all the meta-events: */
    QApplication::sendPostedEvents(0, QEvent::MetaCall);

    /* Cleanup stuff: */
    cleanupMachineLogic();
    cleanupMachineWindowIcon();

    /* Cleanup session UI: */
    cleanupSession();
}

void UIMachine::enterInitialVisualState()
{
    sltChangeVisualState(m_initialVisualState);
}

void UIMachine::updateMousePointerShape()
{
    /* Fetch incoming shape data: */
    const bool fHasAlpha = m_shapeData.hasAlpha();
    const uint uWidth = m_shapeData.shapeSize().width();
    const uint uHeight = m_shapeData.shapeSize().height();
    const uchar *pShapeData = m_shapeData.shape().constData();
    AssertMsgReturnVoid(pShapeData, ("Shape data must not be NULL!\n"));

    /* Invalidate mouse pointer shape initially: */
    m_fIsValidPointerShapePresent = false;
    m_cursorShapePixmap = QPixmap();
    m_cursorMaskPixmap = QPixmap();

    /* Parse incoming shape data: */
    const uchar *pSrcAndMaskPtr = pShapeData;
    const uint uAndMaskSize = (uWidth + 7) / 8 * uHeight;
    const uchar *pSrcShapePtr = pShapeData + ((uAndMaskSize + 3) & ~3);

#if defined (VBOX_WS_WIN)

    /* Create an ARGB image out of the shape data: */

    // WORKAROUND:
    // Qt5 QCursor recommends 32 x 32 cursor, therefore the original data is copied to
    // a larger QImage if necessary. Cursors like 10x16 did not work correctly (Solaris 10 guest).
    // Align the cursor dimensions to 32 bit pixels, because for example a 56x56 monochrome cursor
    // did not work correctly on Windows host.
    const uint uCursorWidth = RT_ALIGN_32(uWidth, 32);
    const uint uCursorHeight = RT_ALIGN_32(uHeight, 32);

    if (fHasAlpha)
    {
        QImage image(uCursorWidth, uCursorHeight, QImage::Format_ARGB32);
        memset(image.bits(), 0, image.byteCount());

        const uint32_t *pu32SrcShapeScanline = (uint32_t *)pSrcShapePtr;
        for (uint y = 0; y < uHeight; ++y, pu32SrcShapeScanline += uWidth)
            memcpy(image.scanLine(y), pu32SrcShapeScanline, uWidth * sizeof(uint32_t));

        m_cursorShapePixmap = QPixmap::fromImage(image);
    }
    else
    {
        if (isPointer1bpp(pSrcShapePtr, uWidth, uHeight))
        {
            // Incoming data consist of 32 bit BGR XOR mask and 1 bit AND mask.
            // XOR pixels contain either 0x00000000 or 0x00FFFFFF.
            //
            // Originally intended result (F denotes 0x00FFFFFF):
            // XOR AND
            //   0   0 black
            //   F   0 white
            //   0   1 transparent
            //   F   1 xor'd
            //
            // Actual Qt5 result for color table 0:0xFF000000, 1:0xFFFFFFFF
            // (tested on Windows 7 and 10 64 bit hosts):
            // Bitmap Mask
            //  0   0 black
            //  1   0 white
            //  0   1 xor
            //  1   1 transparent

            QVector<QRgb> colors(2);
            colors[0] = UINT32_C(0xFF000000);
            colors[1] = UINT32_C(0xFFFFFFFF);

            QImage bitmap(uCursorWidth, uCursorHeight, QImage::Format_Mono);
            bitmap.setColorTable(colors);
            memset(bitmap.bits(), 0xFF, bitmap.byteCount());

            QImage mask(uCursorWidth, uCursorHeight, QImage::Format_Mono);
            mask.setColorTable(colors);
            memset(mask.bits(), 0xFF, mask.byteCount());

            const uint8_t *pu8SrcAndScanline = pSrcAndMaskPtr;
            const uint32_t *pu32SrcShapeScanline = (uint32_t *)pSrcShapePtr;
            for (uint y = 0; y < uHeight; ++y)
            {
                for (uint x = 0; x < uWidth; ++x)
                {
                    const uint8_t u8Bit = (uint8_t)(1 << (7 - x % 8));

                    const uint8_t u8SrcMaskByte = pu8SrcAndScanline[x / 8];
                    const uint8_t u8SrcMaskBit = u8SrcMaskByte & u8Bit;
                    const uint32_t u32SrcPixel = pu32SrcShapeScanline[x] & UINT32_C(0xFFFFFF);

                    uint8_t *pu8DstMaskByte = &mask.scanLine(y)[x / 8];
                    uint8_t *pu8DstBitmapByte = &bitmap.scanLine(y)[x / 8];

                    if (u8SrcMaskBit == 0)
                    {
                        if (u32SrcPixel == 0)
                        {
                            /* Black: Qt Bitmap = 0, Mask = 0 */
                            *pu8DstMaskByte &= ~u8Bit;
                            *pu8DstBitmapByte &= ~u8Bit;
                        }
                        else
                        {
                            /* White: Qt Bitmap = 1, Mask = 0 */
                            *pu8DstMaskByte &= ~u8Bit;
                            *pu8DstBitmapByte |= u8Bit;
                        }
                    }
                    else
                    {
                        if (u32SrcPixel == 0)
                        {
                            /* Transparent: Qt Bitmap = 1, Mask = 1 */
                            *pu8DstMaskByte |= u8Bit;
                            *pu8DstBitmapByte |= u8Bit;
                        }
                        else
                        {
                            /* Xor'ed: Qt Bitmap = 0, Mask = 1 */
                            *pu8DstMaskByte |= u8Bit;
                            *pu8DstBitmapByte &= ~u8Bit;
                        }
                    }
                }

                pu8SrcAndScanline += (uWidth + 7) / 8;
                pu32SrcShapeScanline += uWidth;
            }

            m_cursorShapePixmap = QBitmap::fromImage(bitmap);
            m_cursorMaskPixmap = QBitmap::fromImage(mask);
        }
        else
        {
            /* Assign alpha channel values according to the AND mask: 1 -> 0x00, 0 -> 0xFF: */
            QImage image(uCursorWidth, uCursorHeight, QImage::Format_ARGB32);
            memset(image.bits(), 0, image.byteCount());

            const uint8_t *pu8SrcAndScanline = pSrcAndMaskPtr;
            const uint32_t *pu32SrcShapeScanline = (uint32_t *)pSrcShapePtr;

            for (uint y = 0; y < uHeight; ++y)
            {
                uint32_t *pu32DstPixel = (uint32_t *)image.scanLine(y);

                for (uint x = 0; x < uWidth; ++x)
                {
                    const uint8_t u8Bit = (uint8_t)(1 << (7 - x % 8));
                    const uint8_t u8SrcMaskByte = pu8SrcAndScanline[x / 8];

                    if (u8SrcMaskByte & u8Bit)
                        *pu32DstPixel++ = pu32SrcShapeScanline[x] & UINT32_C(0x00FFFFFF);
                    else
                        *pu32DstPixel++ = pu32SrcShapeScanline[x] | UINT32_C(0xFF000000);
                }

                pu32SrcShapeScanline += uWidth;
                pu8SrcAndScanline += (uWidth + 7) / 8;
            }

            m_cursorShapePixmap = QPixmap::fromImage(image);
        }
    }

    /* Mark mouse pointer shape valid: */
    m_fIsValidPointerShapePresent = true;

#elif defined(VBOX_WS_X11) || defined(VBOX_WS_MAC)

    /* Create an ARGB image out of the shape data: */
    QImage image(uWidth, uHeight, QImage::Format_ARGB32);

    if (fHasAlpha)
    {
        memcpy(image.bits(), pSrcShapePtr, uHeight * uWidth * 4);
    }
    else
    {
        renderCursorPixels((uint32_t *)pSrcShapePtr, pSrcAndMaskPtr,
                           uWidth, uHeight,
                           (uint32_t *)image.bits(), uHeight * uWidth * 4);
    }

    /* Create cursor-pixmap from the image: */
    m_cursorShapePixmap = QPixmap::fromImage(image);

    /* Mark mouse pointer shape valid: */
    m_fIsValidPointerShapePresent = true;

#else

# warning "port me"

#endif

    /* Cache cursor pixmap size and hotspot: */
    m_cursorSize = m_cursorShapePixmap.size();
    m_cursorHotspot = m_shapeData.hotSpot();
}

void UIMachine::updateMouseState()
{
    m_fIsMouseSupportsAbsolute = uisession()->mouse().GetAbsoluteSupported();
    m_fIsMouseSupportsRelative = uisession()->mouse().GetRelativeSupported();
    m_fIsMouseSupportsTouchScreen = uisession()->mouse().GetTouchScreenSupported();
    m_fIsMouseSupportsTouchPad = uisession()->mouse().GetTouchPadSupported();
    m_fIsMouseHostCursorNeeded = uisession()->mouse().GetNeedsHostCursor();
}

#if defined(VBOX_WS_X11) || defined(VBOX_WS_MAC)
/* static */
void UIMachine::renderCursorPixels(const uint32_t *pu32XOR, const uint8_t *pu8AND,
                                   uint32_t u32Width, uint32_t u32Height,
                                   uint32_t *pu32Pixels, uint32_t cbPixels)
{
    /* Output pixels set to 0 which allow to not write transparent pixels anymore. */
    memset(pu32Pixels, 0, cbPixels);

    const uint32_t *pu32XORSrc = pu32XOR;  /* Iterator for source XOR pixels. */
    const uint8_t *pu8ANDSrcLine = pu8AND; /* The current AND mask scanline. */
    uint32_t *pu32Dst = pu32Pixels;        /* Iterator for all destination BGRA pixels. */

    /* Some useful constants. */
    const int cbANDLine = ((int)u32Width + 7) / 8;

    int y;
    for (y = 0; y < (int)u32Height; ++y)
    {
        int x;
        for (x = 0; x < (int)u32Width; ++x)
        {
            const uint32_t u32Pixel = *pu32XORSrc; /* Current pixel at (x,y) */
            const uint8_t *pu8ANDSrc = pu8ANDSrcLine + x / 8; /* Byte which containt current AND bit. */

            if ((*pu8ANDSrc << (x % 8)) & 0x80)
            {
                if (u32Pixel)
                {
                    const uint32_t u32PixelInverted = ~u32Pixel;

                    /* Scan neighbor pixels and assign them if they are transparent. */
                    int dy;
                    for (dy = -1; dy <= 1; ++dy)
                    {
                        const int yn = y + dy;
                        if (yn < 0 || yn >= (int)u32Height)
                            continue; /* Do not cross the bounds. */

                        int dx;
                        for (dx = -1; dx <= 1; ++dx)
                        {
                            const int xn = x + dx;
                            if (xn < 0 || xn >= (int)u32Width)
                                continue;  /* Do not cross the bounds. */

                            if (dx != 0 || dy != 0)
                            {
                                /* Check if the neighbor pixel is transparent. */
                                const uint32_t *pu32XORNeighborSrc = &pu32XORSrc[dy * (int)u32Width + dx];
                                const uint8_t *pu8ANDNeighborSrc = pu8ANDSrcLine + dy * cbANDLine + xn / 8;
                                if (   *pu32XORNeighborSrc == 0
                                    && ((*pu8ANDNeighborSrc << (xn % 8)) & 0x80) != 0)
                                {
                                    /* Transparent neighbor pixels are replaced with the source pixel value. */
                                    uint32_t *pu32PixelNeighborDst = &pu32Dst[dy * (int)u32Width + dx];
                                    *pu32PixelNeighborDst = u32Pixel | 0xFF000000;
                                }
                            }
                            else
                            {
                                /* The pixel itself is replaced with inverted value. */
                                *pu32Dst = u32PixelInverted | 0xFF000000;
                            }
                        }
                    }
                }
                else
                {
                    /* The pixel does not affect the screen.
                     * Do nothing. Do not touch destination which can already contain generated pixels.
                     */
                }
            }
            else
            {
                /* AND bit is 0, the pixel will be just drawn. */
                *pu32Dst = u32Pixel | 0xFF000000;
            }

            ++pu32XORSrc; /* Next source pixel. */
            ++pu32Dst;    /* Next destination pixel. */
        }

        /* Next AND scanline. */
        pu8ANDSrcLine += cbANDLine;
    }
}
#endif /* VBOX_WS_X11 || VBOX_WS_MAC */

#ifdef VBOX_WS_WIN
/* static */
bool UIMachine::isPointer1bpp(const uint8_t *pu8XorMask,
                              uint uWidth,
                              uint uHeight)
{
    /* Check if the pointer has only 0 and 0xFFFFFF pixels, ignoring the alpha channel. */
    const uint32_t *pu32Src = (uint32_t *)pu8XorMask;

    uint y;
    for (y = 0; y < uHeight ; ++y)
    {
        uint x;
        for (x = 0; x < uWidth; ++x)
        {
            const uint32_t u32Pixel = pu32Src[x] & UINT32_C(0xFFFFFF);
            if (u32Pixel != 0 && u32Pixel != UINT32_C(0xFFFFFF))
                return false;
        }

        pu32Src += uWidth;
    }

    return true;
}
#endif /* VBOX_WS_WIN */
