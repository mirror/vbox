/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGMachinePreview class implementation
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QTimer>

/* GUI includes: */
#include "UIGMachinePreview.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIImageTools.h"
#include "VBoxGlobal.h"

/* COM includes: */
#include "CConsole.h"
#include "CDisplay.h"

UpdateIntervalMap CreateUpdateIntervalMap()
{
    UpdateIntervalMap map;
    map[UpdateInterval_Disabled] = "disabled";
    map[UpdateInterval_500ms]    = "500";
    map[UpdateInterval_1000ms]   = "1000";
    map[UpdateInterval_2000ms]   = "2000";
    map[UpdateInterval_5000ms]   = "5000";
    map[UpdateInterval_10000ms]  = "10000";
    return map;
}
UpdateIntervalMap UIGMachinePreview::m_intervals = CreateUpdateIntervalMap();

UIGMachinePreview::UIGMachinePreview(QIGraphicsWidget *pParent)
    : QIWithRetranslateUI4<QIGraphicsWidget>(pParent)
    , m_machineState(KMachineState_Null)
    , m_pUpdateTimer(new QTimer(this))
    , m_pUpdateTimerMenu(0)
    , m_vMargin(10)
    , m_pbgImage(0)
    , m_pPreviewImg(0)
    , m_pGlossyImg(0)
{
    /* Setup contents: */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    /* Create session instance: */
    m_session.createInstance(CLSID_Session);

    /* Create the context menu: */
    m_pUpdateTimerMenu = new QMenu;
    QActionGroup *pUpdateTimeG = new QActionGroup(this);
    pUpdateTimeG->setExclusive(true);
    for(int i = 0; i < UpdateInterval_Max; ++i)
    {
        QAction *pUpdateTime = new QAction(pUpdateTimeG);
        pUpdateTime->setData(i);
        pUpdateTime->setCheckable(true);
        pUpdateTimeG->addAction(pUpdateTime);
        m_pUpdateTimerMenu->addAction(pUpdateTime);
        m_actions[static_cast<UpdateInterval>(i)] = pUpdateTime;
    }
    m_pUpdateTimerMenu->insertSeparator(m_actions[static_cast<UpdateInterval>(UpdateInterval_500ms)]);

    /* Load preview update interval: */
    QString strInterval = vboxGlobal().virtualBox().GetExtraData(GUI_PreviewUpdate);
    /* Parse loaded value: */
    UpdateInterval interval = m_intervals.key(strInterval, UpdateInterval_1000ms);
    /* Initialize with the new update interval: */
    setUpdateInterval(interval, false);

    /* Setup connections: */
    connect(m_pUpdateTimer, SIGNAL(timeout()), this, SLOT(sltRecreatePreview()));
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)),
            this, SLOT(sltMachineStateChange(QString, KMachineState)));

    /* Retranslate the UI */
    retranslateUi();
}

UIGMachinePreview::~UIGMachinePreview()
{
    /* Close any open session: */
    if (m_session.GetState() == KSessionState_Locked)
        m_session.UnlockMachine();
    if (m_pbgImage)
        delete m_pbgImage;
    if (m_pGlossyImg)
        delete m_pGlossyImg;
    if (m_pPreviewImg)
        delete m_pPreviewImg;
    if (m_pUpdateTimerMenu)
        delete m_pUpdateTimerMenu;
}

void UIGMachinePreview::setMachine(const CMachine& machine)
{
    stop();
    m_machine = machine;
    restart();
}

CMachine UIGMachinePreview::machine() const
{
    return m_machine;
}

void UIGMachinePreview::sltMachineStateChange(QString strId, KMachineState state)
{
    if (!m_machine.isNull() && m_machine.GetId() == strId)
    {
        /* Cache the machine state: */
        m_machineState = state;
        restart();
    }
}

void UIGMachinePreview::sltRecreatePreview()
{
    /* Only do this if we are visible: */
    if (!isVisible())
        return;

    /* Remove preview if any: */
    if (m_pPreviewImg)
    {
        delete m_pPreviewImg;
        m_pPreviewImg = 0;
    }

    /* We are not creating preview for inaccessible VMs: */
    if (m_machineState == KMachineState_Null)
        return;

    if (!m_machine.isNull() && m_vRect.width() > 0 && m_vRect.height() > 0)
    {
        QImage image(size().toSize(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        bool fDone = false;

        /* Preview enabled? */
        if (m_pUpdateTimer->interval() > 0)
        {
            /* Use the image which may be included in the save state. */
            if (m_machineState == KMachineState_Saved || m_machineState == KMachineState_Restoring)
            {
                ULONG width = 0, height = 0;
                QVector<BYTE> screenData = m_machine.ReadSavedScreenshotPNGToArray(0, width, height);
                if (screenData.size() != 0)
                {
                    QImage shot = QImage::fromData(screenData.data(), screenData.size(), "PNG")
                            .scaled(m_vRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    dimImage(shot);
                    painter.drawImage(m_vRect.x(), m_vRect.y(), shot);
                    fDone = true;
                }
            }
            /* Use the current VM output. */
            else if (m_machineState == KMachineState_Running || m_machineState == KMachineState_Paused)
            {
                if (m_session.GetState() == KSessionState_Locked)
                {
                    CVirtualBox vbox = vboxGlobal().virtualBox();
                    if (vbox.isOk())
                    {
                        const CConsole& console = m_session.GetConsole();
                        if (!console.isNull())
                        {
                            CDisplay display = console.GetDisplay();
                            /* Todo: correct aspect radio */
//                            ULONG w, h, bpp;
//                            display.GetScreenResolution(0, w, h, bpp);
//                            QImage shot = QImage(w, h, QImage::Format_RGB32);
//                            shot.fill(Qt::black);
//                            display.TakeScreenShot(0, shot.bits(), shot.width(), shot.height());
                            QVector<BYTE> screenData = display.TakeScreenShotToArray(0, m_vRect.width(), m_vRect.height());
                            if (display.isOk() && screenData.size() != 0)
                            {
                                /* Unfortunately we have to reorder the pixel
                                 * data, cause the VBox API returns RGBA data,
                                 * which is not a format QImage understand.
                                 * Todo: check for 32bit alignment, for both
                                 * the data and the scanlines. Maybe we need to
                                 * copy the data in any case. */
                                uint32_t *d = (uint32_t*)screenData.data();
                                for (int i = 0; i < screenData.size() / 4; ++i)
                                {
                                    uint32_t e = d[i];
                                    d[i] = RT_MAKE_U32_FROM_U8(RT_BYTE3(e), RT_BYTE2(e), RT_BYTE1(e), RT_BYTE4(e));
                                }

                                QImage shot = QImage((uchar*)d, m_vRect.width(), m_vRect.height(), QImage::Format_RGB32);

                                if (m_machineState == KMachineState_Paused)
                                    dimImage(shot);
                                painter.drawImage(m_vRect.x(), m_vRect.y(), shot);
                                fDone = true;
                            }
                        }
                    }
                }
            }
        }
        if (fDone)
            m_pPreviewImg = new QImage(image);
    }
    update();
}

void UIGMachinePreview::resizeEvent(QGraphicsSceneResizeEvent *pEvent)
{
    repaintBGImages();
    sltRecreatePreview();
    QIGraphicsWidget::resizeEvent(pEvent);
}

void UIGMachinePreview::showEvent(QShowEvent *pEvent)
{
    restart();
    QIGraphicsWidget::showEvent(pEvent);
}

void UIGMachinePreview::hideEvent(QHideEvent *pEvent)
{
    stop();
    QIGraphicsWidget::hideEvent(pEvent);
}

void UIGMachinePreview::contextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent)
{
    QAction *pReturn = m_pUpdateTimerMenu->exec(pEvent->screenPos(), 0);
    if (pReturn)
    {
        UpdateInterval interval = static_cast<UpdateInterval>(pReturn->data().toInt());
        setUpdateInterval(interval, true);
        restart();
    }
}

void UIGMachinePreview::retranslateUi()
{
    m_actions.value(UpdateInterval_Disabled)->setText(tr("Update disabled"));
    m_actions.value(UpdateInterval_500ms)->setText(tr("Every 0.5 s"));
    m_actions.value(UpdateInterval_1000ms)->setText(tr("Every 1 s"));
    m_actions.value(UpdateInterval_2000ms)->setText(tr("Every 2 s"));
    m_actions.value(UpdateInterval_5000ms)->setText(tr("Every 5 s"));
    m_actions.value(UpdateInterval_10000ms)->setText(tr("Every 10 s"));
}

QSizeF UIGMachinePreview::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    if (which == Qt::MinimumSize)
        return QSize(220, (int)(220 * 3.0/4.0));
    return QIGraphicsWidget::sizeHint(which, constraint);
}

void UIGMachinePreview::paint(QPainter *pPainter, const QStyleOptionGraphicsItem*, QWidget*)
{
    /* Where should the content go: */
    QRect cr = contentsRect().toRect();
    if (!cr.isValid())
        return;
    /* Draw the background with the monitor and the shadow: */
    if (m_pbgImage)
        pPainter->drawImage(cr.x(), cr.y(), *m_pbgImage);

    /* If there is a preview image available: */
    if (m_pPreviewImg)
    {
        /* Draw that image: */
        pPainter->drawImage(0, 0, *m_pPreviewImg);
    }
    else
    {
        /* Fill rectangle with black color: */
        pPainter->fillRect(m_vRect, Qt::black);

        /* Compose name: */
        QString strName = tr("No preview");
        if (!m_machine.isNull())
            strName = m_machine.GetAccessible() ? m_machine.GetName() :
                      QApplication::translate("UIVMListView", "Inaccessible");
        /* Paint that name: */
        QFont font = pPainter->font();
        font.setBold(true);
        int fFlags = Qt::AlignCenter | Qt::TextWordWrap;
        float h = m_vRect.size().height() * .2;
        QRect r;
        /* Make a little magic to find out if the given text fits into our rectangle.
         * Decrease the font pixel size as long as it doesn't fit. */
        int cMax = 30;
        do
        {
            h = h * .8;
            font.setPixelSize((int)h);
            pPainter->setFont(font);
            r = pPainter->boundingRect(m_vRect, fFlags, strName);
        }
        while ((r.height() > m_vRect.height() || r.width() > m_vRect.width()) && cMax-- != 0);
        pPainter->setPen(Qt::white);
        pPainter->drawText(m_vRect, fFlags, strName);
    }

    /* Draw the glossy overlay last: */
    if (m_pGlossyImg)
        pPainter->drawImage(m_vRect.x(), m_vRect.y(), *m_pGlossyImg);
}

void UIGMachinePreview::setUpdateInterval(UpdateInterval interval, bool fSave)
{
    switch (interval)
    {
        case UpdateInterval_Disabled:
        {
            m_pUpdateTimer->setInterval(0);
            m_pUpdateTimer->stop();
            m_actions[interval]->setChecked(true);
            break;
        }
        case UpdateInterval_500ms:
        {
            m_pUpdateTimer->setInterval(500);
            m_actions[interval]->setChecked(true);
            break;
        }
        case UpdateInterval_1000ms:
        {
            m_pUpdateTimer->setInterval(1000);
            m_actions[interval]->setChecked(true);
            break;
        }
        case UpdateInterval_2000ms:
        {
            m_pUpdateTimer->setInterval(2000);
            m_actions[interval]->setChecked(true);
            break;
        }
        case UpdateInterval_5000ms:
        {
            m_pUpdateTimer->setInterval(5000);
            m_actions[interval]->setChecked(true);
            break;
        }
        case UpdateInterval_10000ms:
        {
            m_pUpdateTimer->setInterval(10000);
            m_actions[interval]->setChecked(true);
            break;
        }
        case UpdateInterval_Max: break;
    }
    if (fSave && m_intervals.contains(interval))
        vboxGlobal().virtualBox().SetExtraData(GUI_PreviewUpdate, m_intervals[interval]);
}

void UIGMachinePreview::repaintBGImages()
{
    /* Delete the old images: */
    if (m_pbgImage)
    {
        delete m_pbgImage;
        m_pbgImage = 0;
    }
    if (m_pGlossyImg)
    {
        delete m_pGlossyImg;
        m_pGlossyImg = 0;
    }

    /* Check that there is enough room for our fancy stuff.
     * If not we just draw nothing (the border and the blur radius). */
    QRect cr = contentsRect().toRect();
    if (cr.width()  < 41 || cr.height() < 41)
        return;

    QPalette pal = palette();
    m_wRect = cr.adjusted(10, 10, -10, -10);
    m_vRect = m_wRect.adjusted(m_vMargin, m_vMargin, -m_vMargin, -m_vMargin).adjusted(-3, -3, 3, 3);

    /* First draw the shadow. Its a rounded rectangle which get blurred: */
    QImage imageW(cr.size(), QImage::Format_ARGB32);
    QColor bg = pal.color(QPalette::Base);
    bg.setAlpha(0); /* We want blur to transparent _and_ whatever the base color is. */
    imageW.fill(bg.rgba());
    QPainter pW(&imageW);
    pW.setBrush(QColor(30, 30, 30)); /* Dark gray */
    pW.setPen(Qt::NoPen);
    pW.drawRoundedRect(QRect(QPoint(0, 0), cr.size()).adjusted(10, 10, -10, -10), m_vMargin, m_vMargin);
    pW.end();
    /* Blur the rectangle */
    QImage imageO(cr.size(), QImage::Format_ARGB32);
    blurImage(imageW, imageO, 10);
    QPainter pO(&imageO);

    /* Now paint the border with a gradient to get a look of a monitor: */
    QRect rr = QRect(QPoint(0, 0), cr.size()).adjusted(10, 10, -10, -10);
    QLinearGradient lg(0, rr.y(), 0, rr.height());
    QColor base(200, 200, 200); /* light variant */
    // QColor base(80, 80, 80); /* Dark variant */
    lg.setColorAt(0, base);
    lg.setColorAt(0.4, base.darker(300));
    lg.setColorAt(0.5, base.darker(400));
    lg.setColorAt(0.7, base.darker(300));
    lg.setColorAt(1, base);
    pO.setBrush(lg);
    pO.setPen(QPen(base.darker(150), 1));
    pO.drawRoundedRect(rr, m_vMargin, m_vMargin);
    pO.end();

    /* Make a copy of the new bg image: */
    m_pbgImage = new QImage(imageO);

    /* Now the glossy overlay has to be created.
     * Start with defining a nice looking painter path. */
    QRect gRect = QRect(QPoint(0, 0), m_vRect.size());
    QPainterPath glossyPath(QPointF(gRect.x(), gRect.y()));
    glossyPath.lineTo(gRect.x() + gRect.width(), gRect.y());
    glossyPath.lineTo(gRect.x() + gRect.width(), gRect.y() + gRect.height() * 1.0/3.0);
    glossyPath.cubicTo(gRect.x() + gRect.width() / 2.0, gRect.y() + gRect.height() * 1.0/3.0,
                       gRect.x() + gRect.width() / 2.0, gRect.y() + gRect.height() * 2.0/3.0,
                       gRect.x(), gRect.y() + gRect.height() * 2.0/3.0);
    glossyPath.closeSubpath();

    /* Paint the glossy path on a QImage: */
    QImage image(m_vRect.size(), QImage::Format_ARGB32);
    QColor bg1(Qt::white); /* We want blur to transparent _and_ white. */
    bg1.setAlpha(0);
    image.fill(bg1.rgba());
    QPainter painter(&image);
    painter.fillPath(glossyPath, QColor(255, 255, 255, 80));
    painter.end();
    /* Blur the image to get a much more smooth feeling */
    QImage image1(m_vRect.size(), QImage::Format_ARGB32);
    blurImage(image, image1, 7);
    m_pGlossyImg = new QImage(image1);

    /* Repaint: */
    update();
}

void UIGMachinePreview::restart()
{
    /* Reopen session if necessary: */
    if (m_session.GetState() == KSessionState_Locked)
        m_session.UnlockMachine();
    if (!m_machine.isNull())
    {
        /* Fetch the latest machine state: */
        m_machineState = m_machine.GetState();
        /* Lock the session for the current machine: */
        if (m_machineState == KMachineState_Running || m_machineState == KMachineState_Paused)
            m_machine.LockMachine(m_session, KLockType_Shared);
    }

    /* Recreate the preview image: */
    sltRecreatePreview();

    /* Start the timer if necessary: */
    if (!m_machine.isNull())
    {
        if (m_pUpdateTimer->interval() > 0 && m_machineState == KMachineState_Running)
            m_pUpdateTimer->start();
    }
}

void UIGMachinePreview::stop()
{
    /* Stop the timer: */
    m_pUpdateTimer->stop();
}

