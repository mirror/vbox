/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIRichTextLabel class implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QVBoxLayout>

/* Local includes: */
#include "QIRichTextLabel.h"

/* Constructor: */
QIRichTextLabel::QIRichTextLabel(QWidget *pParent)
    : QWidget(pParent)
    , m_pTextEdit(new QTextEdit(this))
{
    /* Setup text-edit: */
    m_pTextEdit->setReadOnly(true);
    m_pTextEdit->setFrameShape(QFrame::NoFrame);
    m_pTextEdit->viewport()->setAutoFillBackground(false);
    m_pTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTextEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    /* Add into parent: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setMargin(0);
    pMainLayout->addWidget(m_pTextEdit);
}

/* Minimum text-width setter: */
void QIRichTextLabel::setMinimumTextWidth(int iMinimumTextWidth)
{
    /* Get corresponding QTextDocument: */
    QTextDocument *pTextDocument = m_pTextEdit->document();
    /* Bug in QTextDocument (?) : setTextWidth doesn't work from the first time. */
    for (int iTry = 0; pTextDocument->textWidth() != iMinimumTextWidth && iTry < 3; ++iTry)
        pTextDocument->setTextWidth(iMinimumTextWidth);
}

/* Text setter: */
void QIRichTextLabel::setText(const QString &strText)
{
    /* Set text: */
    m_pTextEdit->setText(strText);
    /* Get corresponding QTextDocument: */
    QTextDocument *pTextDocument = m_pTextEdit->document();
    /* Get corresponding QTextDocument size: */
    QSize size = pTextDocument->size().toSize();
    /* Check if current size is valid, otherwise adjust it: */
    if (!size.isValid())
    {
        pTextDocument->adjustSize();
        size = pTextDocument->size().toSize();
    }
    /* Resize to content size: */
    m_pTextEdit->setMinimumSize(size);
}

