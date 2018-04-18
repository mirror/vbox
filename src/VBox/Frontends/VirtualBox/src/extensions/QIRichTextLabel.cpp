/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIRichTextLabel class implementation.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
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

/* Qt includes: */
# include <QUrl>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIRichTextLabel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


QIRichTextLabel::QIRichTextLabel(QWidget *pParent)
    : QWidget(pParent)
    , m_pTextEdit()
    , m_iMinimumTextWidth(0)
{
    /* Configure self: */
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Configure layout: */
        pMainLayout->setMargin(0);

        /* Create text-edit: */
        m_pTextEdit = new QTextEdit;
        if (m_pTextEdit)
        {
            /* Configure text-edit: */
            m_pTextEdit->setReadOnly(true);
            m_pTextEdit->setFocusPolicy(Qt::NoFocus);
            m_pTextEdit->setFrameShape(QFrame::NoFrame);
            m_pTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_pTextEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

            /* Tune text-edit viewport palette: */
            m_pTextEdit->viewport()->setAutoFillBackground(false);
            QPalette pal = m_pTextEdit->viewport()->palette();
            pal.setColor(QPalette::Active,   QPalette::Text, pal.color(QPalette::Active,   QPalette::WindowText));
            pal.setColor(QPalette::Inactive, QPalette::Text, pal.color(QPalette::Inactive, QPalette::WindowText));
            pal.setColor(QPalette::Disabled, QPalette::Text, pal.color(QPalette::Disabled, QPalette::WindowText));
            m_pTextEdit->viewport()->setPalette(pal);
        }

        /* Add into layout: */
        pMainLayout->addWidget(m_pTextEdit);
    }
}

QString QIRichTextLabel::text() const
{
    return m_pTextEdit->toHtml();
}

void QIRichTextLabel::registerImage(const QImage &image, const QString &strName)
{
    m_pTextEdit->document()->addResource(QTextDocument::ImageResource, QUrl(strName), QVariant(image));
}

QTextOption::WrapMode QIRichTextLabel::wordWrapMode() const
{
    return m_pTextEdit->wordWrapMode();
}

void QIRichTextLabel::setWordWrapMode(QTextOption::WrapMode policy)
{
    m_pTextEdit->setWordWrapMode(policy);
}

void QIRichTextLabel::installEventFilter(QObject *pFilterObj)
{
    QWidget::installEventFilter(pFilterObj);
    m_pTextEdit->installEventFilter(pFilterObj);
}

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
