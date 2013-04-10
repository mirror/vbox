/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupCenter class implementation
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
#include <QVBoxLayout>
#include <QPushButton>
#include <QEvent>

/* GUI includes: */
#include "UIPopupCenter.h"
#include "UIModalWindowManager.h"
#include "QIRichTextLabel.h"
#include "QIDialogButtonBox.h"

/* Other VBox includes: */
#include <VBox/sup.h>

/* static */
UIPopupCenter* UIPopupCenter::m_spInstance = 0;
UIPopupCenter* UIPopupCenter::instance() { return m_spInstance; }

/* static */
void UIPopupCenter::create()
{
    /* Make sure instance is NOT created yet: */
    if (m_spInstance)
        return;

    /* Create instance: */
    new UIPopupCenter;
}

/* static */
void UIPopupCenter::destroy()
{
    /* Make sure instance is NOT destroyed yet: */
    if (!m_spInstance)
        return;

    /* Destroy instance: */
    delete m_spInstance;
}

UIPopupCenter::UIPopupCenter()
{
    /* Assign instance: */
    m_spInstance = this;
}

UIPopupCenter::~UIPopupCenter()
{
    /* Unassign instance: */
    m_spInstance = 0;
}

void UIPopupCenter::message(QWidget *pParent,
                            const QString &strMessage, const QString &strDetails,
                            const char *pcszAutoConfirmId /*= 0*/,
                            int iButton1 /*= 0*/, int iButton2 /*= 0*/, int iButton3 /*= 0*/,
                            const QString &strButtonText1 /* = QString() */,
                            const QString &strButtonText2 /* = QString() */,
                            const QString &strButtonText3 /* = QString() */) const
{
    showPopupBox(pParent,
                 strMessage, strDetails,
                 iButton1, iButton2, iButton3,
                 strButtonText1, strButtonText2, strButtonText3,
                 QString(pcszAutoConfirmId));
}

void UIPopupCenter::error(QWidget *pParent,
                          const QString &strMessage, const QString &strDetails,
                          const char *pcszAutoConfirmId /*= 0*/) const
{
    message(pParent,
            strMessage, strDetails,
            pcszAutoConfirmId,
            AlertButton_Ok | AlertButtonOption_Default);
}

void UIPopupCenter::alert(QWidget *pParent,
                          const QString &strMessage,
                          const char *pcszAutoConfirmId /*= 0*/) const
{
    error(pParent,
          strMessage, QString(),
          pcszAutoConfirmId);
}

void UIPopupCenter::question(QWidget *pParent,
                             const QString &strMessage,
                             const char *pcszAutoConfirmId /*= 0*/,
                             int iButton1 /*= 0*/, int iButton2 /*= 0*/, int iButton3 /*= 0*/,
                             const QString &strButtonText1 /*= QString()*/,
                             const QString &strButtonText2 /*= QString()*/,
                             const QString &strButtonText3 /*= QString()*/) const
{
    message(pParent,
            strMessage, QString(),
            pcszAutoConfirmId,
            iButton1, iButton2, iButton3,
            strButtonText1, strButtonText2, strButtonText3);
}

void UIPopupCenter::questionBinary(QWidget *pParent,
                                   const QString &strMessage,
                                   const char *pcszAutoConfirmId /*= 0*/,
                                   const QString &strOkButtonText /*= QString()*/,
                                   const QString &strCancelButtonText /*= QString()*/) const
{
    question(pParent,
             strMessage,
             pcszAutoConfirmId,
             AlertButton_Ok | AlertButtonOption_Default,
             AlertButton_Cancel | AlertButtonOption_Escape,
             0 /* third button */,
             strOkButtonText,
             strCancelButtonText,
             QString() /* third button */);
}

void UIPopupCenter::questionTrinary(QWidget *pParent,
                                    const QString &strMessage,
                                    const char *pcszAutoConfirmId /*= 0*/,
                                    const QString &strChoice1ButtonText /*= QString()*/,
                                    const QString &strChoice2ButtonText /*= QString()*/,
                                    const QString &strCancelButtonText /*= QString()*/) const
{
    question(pParent,
             strMessage,
             pcszAutoConfirmId,
             AlertButton_Choice1,
             AlertButton_Choice2 | AlertButtonOption_Default,
             AlertButton_Cancel | AlertButtonOption_Escape,
             strChoice1ButtonText,
             strChoice2ButtonText,
             strCancelButtonText);
}

void UIPopupCenter::showPopupBox(QWidget *pParent,
                                 const QString &strMessage, const QString &strDetails,
                                 int iButton1, int iButton2, int iButton3,
                                 const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3,
                                 const QString &strAutoConfirmId) const
{
    /* Choose at least one button by 'default': */
    if (iButton1 == 0 && iButton2 == 0 && iButton3 == 0)
        iButton1 = AlertButton_Ok | AlertButtonOption_Default;

    /* Create popup-box window: */
    QWidget *pPopupBoxParent = windowManager().realParentWindow(pParent ? pParent : windowManager().mainWindowShown());
    UIPopupPane *pPopupBox = new UIPopupPane(pPopupBoxParent, strAutoConfirmId,
                                             strMessage, strDetails,
                                             iButton1, iButton2, iButton3,
                                             strButtonText1, strButtonText2, strButtonText3);
    m_popups.insert(strAutoConfirmId, pPopupBox);
    connect(pPopupBox, SIGNAL(sigDone(int)), this, SLOT(sltPopupDone(int)));
    pPopupBox->show();
}

void UIPopupCenter::sltPopupDone(int iButtonCode) const
{
    /* Make sure the sender is popup: */
    UIPopupPane *pPopup = qobject_cast<UIPopupPane*>(sender());
    if (!pPopup)
        return;

    /* Get the popup ID: */
    const QString strPopupID(pPopup->id());

    /* Make sure the popup still here: */
    const bool fIsPopupStillHere = m_popups.contains(strPopupID);
    AssertMsg(fIsPopupStillHere, ("Popup already destroyed!"));
    if (!fIsPopupStillHere)
        return;

    /* Notify listeners: */
    emit sigPopupDone(strPopupID, iButtonCode);

    /* Cleanup the popup: */
    m_popups.remove(strPopupID);
    delete pPopup;
}

UIPopupPane::UIPopupPane(QWidget *pParent, const QString &strId,
                         const QString &strMessage, const QString &strDetails,
                         int iButton1, int iButton2, int iButton3,
                         const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3)
    : QWidget(pParent, Qt::Window | Qt::FramelessWindowHint)
    , m_fPolished(false)
    , m_strId(strId)
    , m_strMessage(strMessage)
    , m_strDetails(strDetails)
    , m_iButton1(iButton1)
    , m_iButton2(iButton2)
    , m_iButton3(iButton3)
    , m_strButtonText1(strButtonText1)
    , m_strButtonText2(strButtonText2)
    , m_strButtonText3(strButtonText3)
{
    /* Make sure popup will NOT be deleted on close: */
    setAttribute(Qt::WA_DeleteOnClose, false);
    /* Install event-filter: */
    pParent->installEventFilter(this);
    /* Configure window: */
    setWindowOpacity(0.7);

    /* Prepare content: */
    prepareContent();
}

UIPopupPane::~UIPopupPane()
{
}

bool UIPopupPane::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Make sure its parent event came: */
    if (pWatched != parent())
        return false;

    /* Make sure its move|rezize event came: */
    if (   pEvent->type() != QEvent::Move
        && pEvent->type() != QEvent::Resize)
        return false;

    /* Process event: */
    switch (pEvent->type())
    {
        /* Move event: */
        case QEvent::Move:
        {
            moveAccordingParent();
            break;
        }
        /* Resize event: */
        case QEvent::Resize:
        {
            resizeAccordingParent();
            break;
        }
        /* Default case: */
        default: break;
    }

    /* Do not filter by default: */
    return false;
}

void UIPopupPane::showEvent(QShowEvent *pEvent)
{
    /* Make sure we should polish dialog: */
    if (m_fPolished)
        return;

    /* Call to polish-event: */
    polishEvent(pEvent);

    /* Mark dialog as polished: */
    m_fPolished = true;
}

void UIPopupPane::polishEvent(QShowEvent*)
{
    /* Tune geometry: */
    moveAccordingParent();
    resizeAccordingParent();
}

void UIPopupPane::moveAccordingParent()
{
    /* Move according parent: */
    QRect geo = parentWidget()->geometry();
    move(geo.x(), geo.y());
}

void UIPopupPane::resizeAccordingParent()
{
    /* Resize according parent: */
    int iWidth = parentWidget()->width();
    resize(iWidth, minimumSizeHint().height());

    /* Resize text-pane according new size: */
    int iMinimumTextWidth = iWidth;
    int iLeft, iTop, iRight, iBottom;
    m_pMainLayout->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
    iMinimumTextWidth -= iLeft;
    iMinimumTextWidth -= iRight;
    m_pTextPane->setMinimumTextWidth(iMinimumTextWidth);
}

void UIPopupPane::prepareContent()
{
    /* Crete main-layout: */
    m_pMainLayout = new QVBoxLayout(this);
    {
        /* Configure layout: */
#ifdef Q_WS_MAC
        m_pMainLayout->setContentsMargins(20, 5, 20, 5);
        m_pMainLayout->setSpacing(0);
#else /* Q_WS_MAC */
        m_pMainLayout->setContentsMargins(5, 5, 5, 5);
        m_pMainLayout->setSpacing(5);
#endif /* !Q_WS_MAC */
        /* Create message-label: */
        m_pTextPane = new QIRichTextLabel;
        {
            /* Add into layout: */
            m_pMainLayout->addWidget(m_pTextPane);
            /* Configure label: */
            m_pTextPane->setText(m_strMessage);
            m_pTextPane->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        }
        /* Create button-box: */
        m_pButtonBox = new QIDialogButtonBox;
        {
            /* Add into layout: */
            m_pMainLayout->addWidget(m_pButtonBox);
            /* Configure button-box: */
            m_pButton1 = createButton(m_iButton1);
            if (m_pButton1)
            {
                if (!m_strButtonText1.isEmpty())
                    m_pButton1->setText(m_strButtonText1);
                connect(m_pButton1, SIGNAL(clicked()), SLOT(done1()));
            }
            m_pButton2 = createButton(m_iButton2);
            if (m_pButton2)
            {
                if (!m_strButtonText2.isEmpty())
                    m_pButton1->setText(m_strButtonText2);
                connect(m_pButton2, SIGNAL(clicked()), SLOT(done2()));
            }
            m_pButton3 = createButton(m_iButton3);
            if (m_pButton3)
            {
                if (!m_strButtonText3.isEmpty())
                    m_pButton1->setText(m_strButtonText3);
                connect(m_pButton3, SIGNAL(clicked()), SLOT(done3()));
            }
        }
    }
}

QPushButton* UIPopupPane::createButton(int iButton)
{
    /* Not for AlertButton_NoButton: */
    if (iButton == 0)
        return 0;

    /* Prepare button text & role: */
    QString strText;
    QDialogButtonBox::ButtonRole role;
    switch (iButton & AlertButtonMask)
    {
        case AlertButton_Ok:      strText = tr("OK");     role = QDialogButtonBox::AcceptRole; break;
        case AlertButton_Cancel:  strText = tr("Cancel"); role = QDialogButtonBox::RejectRole; break;
        case AlertButton_Choice1: strText = tr("Yes");    role = QDialogButtonBox::YesRole; break;
        case AlertButton_Choice2: strText = tr("No");     role = QDialogButtonBox::NoRole; break;
        case AlertButton_Copy:    strText = tr("Copy");   role = QDialogButtonBox::ActionRole; break;
        default: return 0;
    }

    /* Create push-button: */
    QPushButton *pButton = m_pButtonBox->addButton(strText, role);

    /* Configure <default> button: */
    if (iButton & AlertButtonOption_Default)
    {
        pButton->setDefault(true);
        pButton->setFocus();
    }

    /* Return button: */
    return pButton;
}

void UIPopupPane::done(int iButtonCode)
{
    /* Close the window: */
    close();

    /* Notify listeners: */
    emit sigDone(iButtonCode);
}

