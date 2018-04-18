/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIWidgetValidator class implementation.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "QIWidgetValidator.h"
# include "UISettingsPage.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class QObjectValidator implementation.                                                                                       *
*********************************************************************************************************************************/

QObjectValidator::QObjectValidator(QValidator *pValidator, QObject *pParent /* = 0 */)
    : QObject(pParent)
    , m_pValidator(pValidator)
    , m_enmState(QValidator::Invalid)
{
    /* Prepare: */
    prepare();
}

void QObjectValidator::sltValidate(QString strInput /* = QString() */)
{
    /* Make sure validator assigned: */
    AssertPtrReturnVoid(m_pValidator);

    /* Validate: */
    int iPosition = 0;
    const QValidator::State enmState = m_pValidator->validate(strInput, iPosition);

    /* If validity state changed: */
    if (m_enmState != enmState)
    {
        /* Update last validity state: */
        m_enmState = enmState;

        /* Notifies listener(s) about validity change: */
        emit sigValidityChange(m_enmState);
    }
}

void QObjectValidator::prepare()
{
    /* Make sure validator assigned: */
    AssertPtrReturnVoid(m_pValidator);

    /* Register validator as child: */
    m_pValidator->setParent(this);

    /* Validate: */
    sltValidate();
}


/*********************************************************************************************************************************
*   Class QObjectValidatorGroup implementation.                                                                                  *
*********************************************************************************************************************************/

void QObjectValidatorGroup::addObjectValidator(QObjectValidator *pObjectValidator)
{
    /* Make sure object-validator passed: */
    AssertPtrReturnVoid(pObjectValidator);

    /* Register object-validator as child: */
    pObjectValidator->setParent(this);

    /* Insert object-validator to internal map: */
    m_group.insert(pObjectValidator, toResult(pObjectValidator->state()));

    /* Attach object-validator to group: */
    connect(pObjectValidator, &QObjectValidator::sigValidityChange,
            this, &QObjectValidatorGroup::sltValidate);
}

void QObjectValidatorGroup::sltValidate(QValidator::State enmState)
{
    /* Determine sender object-validator: */
    QObjectValidator *pObjectValidatorSender = qobject_cast<QObjectValidator*>(sender());
    /* Make sure that is one of our senders: */
    AssertReturnVoid(pObjectValidatorSender && m_group.contains(pObjectValidatorSender));

    /* Update internal map: */
    m_group[pObjectValidatorSender] = toResult(enmState);

    /* Enumerate all the registered object-validators: */
    bool fResult = true;
    foreach (QObjectValidator *pObjectValidator, m_group.keys())
        if (!toResult(pObjectValidator->state()))
        {
            fResult = false;
            break;
        }

    /* If validity state changed: */
    if (m_fResult != fResult)
    {
        /* Update last validity state: */
        m_fResult = fResult;

        /* Notifies listener(s) about validity change: */
        emit sigValidityChange(m_fResult);
    }
}

/* static */
bool QObjectValidatorGroup::toResult(QValidator::State enmState)
{
    return enmState == QValidator::Acceptable;
}


/*********************************************************************************************************************************
*   Class UIPageValidator implementation.                                                                                        *
*********************************************************************************************************************************/

QPixmap UIPageValidator::warningPixmap() const
{
    return m_pPage->warningPixmap();
}

QString UIPageValidator::internalName() const
{
    return m_pPage->internalName();
}

void UIPageValidator::setLastMessage(const QString &strLastMessage)
{
    /* Remember new message: */
    m_strLastMessage = strLastMessage;

    /* Should we show corresponding warning icon? */
    if (m_strLastMessage.isEmpty())
        emit sigHideWarningIcon();
    else
        emit sigShowWarningIcon();
}

void UIPageValidator::revalidate()
{
    /* Notify listener(s) about validity change: */
    emit sigValidityChanged(this);
}


/*********************************************************************************************************************************
*   Class QIULongValidator implementation.                                                                                       *
*********************************************************************************************************************************/

QValidator::State QIULongValidator::validate(QString &strInput, int &iPosition) const
{
    Q_UNUSED(iPosition);

    /* Get the stripped string: */
    QString strStripped = strInput.trimmed();

    /* 'Intermediate' for empty string or started from '0x': */
    if (strStripped.isEmpty() ||
        strStripped.toUpper() == QString("0x").toUpper())
        return Intermediate;

    /* Convert to ulong: */
    bool fOk;
    ulong uEntered = strInput.toULong(&fOk, 0);

    /* 'Invalid' if failed to convert: */
    if (!fOk)
        return Invalid;

    /* 'Acceptable' if fits the bounds: */
    if (uEntered >= m_uBottom && uEntered <= m_uTop)
        return Acceptable;

    /* 'Invalid' if more than top, 'Intermediate' if less than bottom: */
    return uEntered > m_uTop ? Invalid : Intermediate;
}
