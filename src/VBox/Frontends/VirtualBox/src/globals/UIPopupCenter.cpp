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
#include <QLabel>

/* GUI includes: */
#include "UIPopupCenter.h"
#include "QIMessageBox.h"
#include "UIModalWindowManager.h"

/* static */
UIPopupCenter* UIPopupCenter::m_spInstance = 0;
UIPopupCenter* UIPopupCenter::instance() { return m_spInstance; }

/* static */
void UIPopupCenter::prepare()
{
    /* Make sure instance is not created yet: */
    if (m_spInstance)
        return;

    /* Create instance: */
    new UIPopupCenter;
}

/* static */
void UIPopupCenter::cleanup()
{
    /* Make sure instance is still created: */
    if (!m_spInstance)
        return;

    /* Create instance: */
    delete m_spInstance;
}

UIPopupCenter::UIPopupCenter()
{
    /* Prepare instance: */
    if (!m_spInstance)
        m_spInstance = this;
}

UIPopupCenter::~UIPopupCenter()
{
    /* Cleanup instance: */
    if (m_spInstance)
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

void UIPopupCenter::showPopupBox(QWidget *,
                                 const QString &, const QString &,
                                 int , int , int ,
                                 const QString &, const QString &, const QString &,
                                 const QString &) const
{
}

