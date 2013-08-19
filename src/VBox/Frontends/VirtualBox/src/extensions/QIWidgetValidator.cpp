/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIWidgetValidator class implementation
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "QIWidgetValidator.h"

/* GUI includes: */
#include "UISettingsPage.h"


UIPageValidator::UIPageValidator(QObject *pParent, UISettingsPage *pPage)
    : QObject(pParent)
    , m_pPage(pPage)
    , m_fIsValid(true)
{
}

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


QValidator::State QIULongValidator::validate (QString &aInput, int &aPos) const
{
    Q_UNUSED (aPos);

    QString stripped = aInput.trimmed();

    if (stripped.isEmpty() ||
        stripped.toUpper() == QString ("0x").toUpper())
        return Intermediate;

    bool ok;
    ulong entered = aInput.toULong (&ok, 0);

    if (!ok)
        return Invalid;

    if (entered >= mBottom && entered <= mTop)
        return Acceptable;

    return (entered > mTop ) ? Invalid : Intermediate;
}

