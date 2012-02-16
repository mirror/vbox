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
#include <QUrl>

/* Local includes: */
#include "QIRichTextLabel.h"

/* Constructor: */
QIRichTextLabel::QIRichTextLabel(QWidget *pParent)
    : QWidget(pParent)
    , m_pTextEdit(new QTextEdit(this))
    , m_iMinimumTextWidth(0)
{
    /* Setup self: */
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    /* Setup text-edit: */
    m_pTextEdit->setReadOnly(true);
    m_pTextEdit->setFocusPolicy(Qt::NoFocus);
    m_pTextEdit->setFrameShape(QFrame::NoFrame);
    m_pTextEdit->viewport()->setAutoFillBackground(false);
    m_pTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTextEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    /* Add into parent: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setMargin(0);
    pMainLayout->addWidget(m_pTextEdit);
}

/* Text getter: */
QString QIRichTextLabel::text() const
{
    return m_pTextEdit->toHtml();
}

/* Register image: */
void QIRichTextLabel::registerImage(const QImage &image, const QString &strName)
{
    /* Register passed image in internal text-document: */
    m_pTextEdit->document()->addResource(QTextDocument::ImageResource, QUrl(strName), QVariant(image));
}

/* Minimum text-width setter: */
void QIRichTextLabel::setMinimumTextWidth(int iMinimumTextWidth)
{
    /* Remember minimum text width: */
    m_iMinimumTextWidth = iMinimumTextWidth;

    /* Get corresponding QTextDocument: */
    QTextDocument *pTextDocument = m_pTextEdit->document();
    /* Bug in QTextDocument (?) : setTextWidth doesn't work from the first time. */
    for (int iTry = 0; pTextDocument->textWidth() != m_iMinimumTextWidth && iTry < 3; ++iTry)
        pTextDocument->setTextWidth(m_iMinimumTextWidth);
    /* Get corresponding QTextDocument size: */
    QSize size = pTextDocument->size().toSize();

    /* Resize to content size: */
    m_pTextEdit->setMinimumSize(size);
    layout()->activate();
}

/* Text setter: */
void QIRichTextLabel::setText(const QString &strText)
{
    /* Set text: */
    m_pTextEdit->setHtml(strText);

    /* Get corresponding QTextDocument: */
    QTextDocument *pTextDocument = m_pTextEdit->document();
    /* Adjust text-edit size: */
    pTextDocument->adjustSize();
    /* Get corresponding QTextDocument size: */
    QSize size = pTextDocument->size().toSize();

    /* Set minimum text width to corresponding value: */
    setMinimumTextWidth(m_iMinimumTextWidth == 0 ? size.width() : m_iMinimumTextWidth);
}

