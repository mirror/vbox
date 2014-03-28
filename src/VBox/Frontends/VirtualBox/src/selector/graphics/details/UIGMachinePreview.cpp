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
    , m_pUpdateTimer(new QTimer(this))
    , m_pUpdateTimerMenu(0)
    , m_iMargin(0)
    , m_pbgEmptyImage(0)
    , m_pbgFullImage(0)
    , m_pPreviewImg(0)
{
    /* Setup contents: */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    /* Create session instance: */
    m_session.createInstance(CLSID_Session);

    /* Create bg images: */
    m_pbgEmptyImage = new QPixmap(":/preview_empty_228x168px.png");
    m_pbgFullImage = new QPixmap(":/preview_full_228x168px.png");

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
            this, SLOT(sltMachineStateChange(QString)));

    /* Retranslate the UI */
    retranslateUi();
}

UIGMachinePreview::~UIGMachinePreview()
{
    /* Close any open session: */
    if (m_session.GetState() == KSessionState_Locked)
        m_session.UnlockMachine();
    delete m_pbgEmptyImage;
    delete m_pbgFullImage;
    if (m_pPreviewImg)
        delete m_pPreviewImg;
    if (m_pUpdateTimerMenu)
        delete m_pUpdateTimerMenu;
}

void UIGMachinePreview::setMachine(const CMachine& machine)
{
    /* Pause: */
    stop();

    /* Assign new machine: */
    m_machine = machine;

    /* Fetch machine data: */
    m_strPreviewName = tr("No preview");
    if (!m_machine.isNull())
        m_strPreviewName = m_machine.GetAccessible() ? m_machine.GetName() :
                           QApplication::translate("UIVMListView", "Inaccessible");

    /* Resume: */
    restart();
}

CMachine UIGMachinePreview::machine() const
{
    return m_machine;
}

void UIGMachinePreview::sltMachineStateChange(QString strId)
{
    /* Make sure its the event for our machine: */
    if (m_machine.isNull() || m_machine.GetId() != strId)
        return;

    /* Restart the preview: */
    restart();
}

void UIGMachinePreview::sltRecreatePreview()
{
    /* Only do this if we are visible: */
    if (!isVisible())
        return;

    /* Cleanup preview first: */
    if (m_pPreviewImg)
    {
        delete m_pPreviewImg;
        m_pPreviewImg = 0;
    }

    /* Fetch the latest machine-state: */
    KMachineState machineState = m_machine.isNull() ? KMachineState_Null : m_machine.GetState();

    /* We are creating preview only for assigned and accessible VMs: */
    if (!m_machine.isNull() && machineState != KMachineState_Null &&
        m_vRect.width() > 0 && m_vRect.height() > 0)
    {
        QImage image(size().toSize(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        bool fDone = false;

        /* Preview enabled? */
        if (m_pUpdateTimer->interval() > 0)
        {
            /* Use the image which may be included in the save state. */
            if (machineState == KMachineState_Saved || machineState == KMachineState_Restoring)
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
            else if (machineState == KMachineState_Running || machineState == KMachineState_Paused)
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
//                            LONG xOrigin, yOrigin;
//                            display.GetScreenResolution(0, w, h, bpp, xOrigin, yOrigin);
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

                                if (machineState == KMachineState_Paused)
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

    /* Redraw preview in any case! */
    update();
}

void UIGMachinePreview::resizeEvent(QGraphicsSceneResizeEvent *pEvent)
{
    recalculatePreviewRectangle();
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
        return QSize(228 /* pixmap width */ + 2 * m_iMargin,
                     168 /* pixmap height */ + 2 * m_iMargin);
    return QIGraphicsWidget::sizeHint(which, constraint);
}

void UIGMachinePreview::paint(QPainter *pPainter, const QStyleOptionGraphicsItem*, QWidget*)
{
    /* Where should the content go: */
    QRect cr = contentsRect().toRect();
    if (!cr.isValid())
        return;

    /* If there is a preview image available: */
    if (m_pPreviewImg)
    {
        /* Draw empty background: */
        pPainter->drawPixmap(cr.x() + m_iMargin, cr.y() + m_iMargin, *m_pbgEmptyImage);

        /* Draw that image: */
        pPainter->drawImage(0, 0, *m_pPreviewImg);
    }
    else
    {
        /* Draw full background: */
        pPainter->drawPixmap(cr.x() + m_iMargin, cr.y() + m_iMargin, *m_pbgFullImage);

        /* Paint preview name: */
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
            r = pPainter->boundingRect(m_vRect, fFlags, m_strPreviewName);
        }
        while ((r.height() > m_vRect.height() || r.width() > m_vRect.width()) && cMax-- != 0);
        pPainter->setPen(Qt::white);
        pPainter->drawText(m_vRect, fFlags, m_strPreviewName);
    }
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

void UIGMachinePreview::recalculatePreviewRectangle()
{
    /* Contents rectangle: */
    QRect cr = contentsRect().toRect();
    m_vRect = cr.adjusted(21 + m_iMargin, 17 + m_iMargin, -21 - m_iMargin, -20 - m_iMargin);
}

void UIGMachinePreview::restart()
{
    /* Fetch the latest machine-state: */
    KMachineState machineState = m_machine.isNull() ? KMachineState_Null : m_machine.GetState();

    /* Reopen session if necessary: */
    if (m_session.GetState() == KSessionState_Locked)
        m_session.UnlockMachine();
    if (!m_machine.isNull())
    {
        /* Lock the session for the current machine: */
        if (machineState == KMachineState_Running || machineState == KMachineState_Paused)
            m_machine.LockMachine(m_session, KLockType_Shared);
    }

    /* Recreate the preview image: */
    sltRecreatePreview();

    /* Start the timer if necessary: */
    if (!m_machine.isNull())
    {
        if (m_pUpdateTimer->interval() > 0 && machineState == KMachineState_Running)
            m_pUpdateTimer->start();
    }
}

void UIGMachinePreview::stop()
{
    /* Stop the timer: */
    m_pUpdateTimer->stop();
}

