/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QILineEdit class implementation.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
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
#include "QILineEdit.h"

/* Qt includes: */
#include <QPalette>
#include <QStyleOptionFrame>

QILineEdit::QILineEdit(QWidget *pParent /* = 0 */)
    : QLineEdit(pParent)
{
    m_originalBaseColor = palette().color(QPalette::Base);
}

void QILineEdit::setMinimumWidthByText(const QString &strText)
{
    setMinimumWidth(featTextWidth(strText).width());
}

void QILineEdit::setFixedWidthByText(const QString &strText)
{
    setFixedWidth(featTextWidth(strText).width());
}

QSize QILineEdit::featTextWidth(const QString &strText) const
{
    QStyleOptionFrame sof;
    sof.initFrom(this);
    sof.rect = contentsRect();
    sof.lineWidth = hasFrame() ? style()->pixelMetric(QStyle::PM_DefaultFrameWidth) : 0;
    sof.midLineWidth = 0;
    sof.state |= QStyle::State_Sunken;

    /** @todo make it wise.. */
    // WORKAROUND:
    // The margins are based on qlineedit.cpp of Qt.
    // Maybe they where changed at some time in the future.
    QSize sc(fontMetrics().width(strText) + 2 * 2,
             fontMetrics().xHeight()     + 2 * 1);
    const QSize sa = style()->sizeFromContents(QStyle::CT_LineEdit, &sof, sc, this);

    return sa;
}

void QILineEdit::mark(bool fError)
{
    QPalette newPalette = palette();
    if (fError)
        newPalette.setColor(QPalette::Base, QColor(255, 180, 180));
    else
        newPalette.setColor(QPalette::Base, m_originalBaseColor);
    setPalette(newPalette);
}
