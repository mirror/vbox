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

/* GUI includes: */
#include "UIPopupCenter.h"
#include "UIModalWindowManager.h"
#include "UIPopupPane.h"

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

void UIPopupCenter::message(QWidget *pParent, const QString &strId,
                            const QString &strMessage, const QString &strDetails,
                            int iButton1 /*= 0*/, int iButton2 /*= 0*/, int iButton3 /*= 0*/,
                            const QString &strButtonText1 /* = QString() */,
                            const QString &strButtonText2 /* = QString() */,
                            const QString &strButtonText3 /* = QString() */) const
{
    showPopupPane(pParent, strId,
                  strMessage, strDetails,
                  iButton1, iButton2, iButton3,
                  strButtonText1, strButtonText2, strButtonText3);
}

void UIPopupCenter::error(QWidget *pParent, const QString &strId,
                          const QString &strMessage, const QString &strDetails) const
{
    message(pParent, strId,
            strMessage, strDetails,
            AlertButton_Ok | AlertButtonOption_Default | AlertButtonOption_Escape);
}

void UIPopupCenter::alert(QWidget *pParent, const QString &strId,
                          const QString &strMessage) const
{
    error(pParent, strId,
          strMessage, QString());
}

void UIPopupCenter::question(QWidget *pParent, const QString &strId,
                             const QString &strMessage,
                             int iButton1 /*= 0*/, int iButton2 /*= 0*/, int iButton3 /*= 0*/,
                             const QString &strButtonText1 /*= QString()*/,
                             const QString &strButtonText2 /*= QString()*/,
                             const QString &strButtonText3 /*= QString()*/) const
{
    message(pParent, strId,
            strMessage, QString(),
            iButton1, iButton2, iButton3,
            strButtonText1, strButtonText2, strButtonText3);
}

void UIPopupCenter::questionBinary(QWidget *pParent, const QString &strId,
                                   const QString &strMessage,
                                   const QString &strOkButtonText /*= QString()*/,
                                   const QString &strCancelButtonText /*= QString()*/) const
{
    question(pParent, strId,
             strMessage,
             AlertButton_Ok | AlertButtonOption_Default,
             AlertButton_Cancel | AlertButtonOption_Escape,
             0 /* third button */,
             strOkButtonText,
             strCancelButtonText,
             QString() /* third button */);
}

void UIPopupCenter::questionTrinary(QWidget *pParent, const QString &strId,
                                    const QString &strMessage,
                                    const QString &strChoice1ButtonText /*= QString()*/,
                                    const QString &strChoice2ButtonText /*= QString()*/,
                                    const QString &strCancelButtonText /*= QString()*/) const
{
    question(pParent, strId,
             strMessage,
             AlertButton_Choice1,
             AlertButton_Choice2 | AlertButtonOption_Default,
             AlertButton_Cancel | AlertButtonOption_Escape,
             strChoice1ButtonText,
             strChoice2ButtonText,
             strCancelButtonText);
}

void UIPopupCenter::showPopupPane(QWidget *pParent, const QString &strId,
                                 const QString &strMessage, const QString &strDetails,
                                 int iButton1, int iButton2, int iButton3,
                                 const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3) const
{
    /* Choose at least one button by 'default': */
    if (iButton1 == 0 && iButton2 == 0 && iButton3 == 0)
        iButton1 = AlertButton_Ok | AlertButtonOption_Default | AlertButtonOption_Escape;

    /* Create popup-box window: */
    QWidget *pPopupBoxParent = windowManager().realParentWindow(pParent ? pParent : windowManager().mainWindowShown());
    UIPopupPane *pPopupBox = new UIPopupPane(pPopupBoxParent, strId,
                                             strMessage, strDetails,
                                             iButton1, iButton2, iButton3,
                                             strButtonText1, strButtonText2, strButtonText3);
    m_popups.insert(strId, pPopupBox);
    connect(pPopupBox, SIGNAL(sigDone(int)), this, SLOT(sltPopupDone(int)));
    pPopupBox->show();
}

void UIPopupCenter::sltPopupDone(int iButtonCode) const
{
    /* Make sure the sender is popup: */
    const UIPopupPane *pPopup = qobject_cast<UIPopupPane*>(sender());
    if (!pPopup)
        return;

    /* Make sure the popup is still exists: */
    const QString strPopupID(pPopup->id());
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

void UIPopupCenter::remindAboutMouseIntegration(bool fSupportsAbsolute) const
{
    if (fSupportsAbsolute)
    {
        alert(0, QString("remindAboutMouseIntegrationOn"),
              tr("<p>The Virtual Machine reports that the guest OS supports <b>mouse pointer integration</b>. "
                 "This means that you do not need to <i>capture</i> the mouse pointer to be able to use it in your guest OS -- "
                 "all mouse actions you perform when the mouse pointer is over the Virtual Machine's display "
                 "are directly sent to the guest OS. If the mouse is currently captured, it will be automatically uncaptured.</p>"
                 "<p>The mouse icon on the status bar will look like&nbsp;<img src=:/mouse_seamless_16px.png/>&nbsp;to inform you "
                 "that mouse pointer integration is supported by the guest OS and is currently turned on.</p>"
                 "<p><b>Note</b>: Some applications may behave incorrectly in mouse pointer integration mode. "
                 "You can always disable it for the current session (and enable it again) "
                 "by selecting the corresponding action from the menu bar.</p>"));
    }
    else
    {
        alert(0, QString("remindAboutMouseIntegrationOff"),
              tr("<p>The Virtual Machine reports that the guest OS does not support <b>mouse pointer integration</b> "
                 "in the current video mode. You need to capture the mouse (by clicking over the VM display "
                 "or pressing the host key) in order to use the mouse inside the guest OS.</p>"));
    }
}

