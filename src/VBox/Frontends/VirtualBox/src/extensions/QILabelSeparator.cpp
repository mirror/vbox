/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QILabelSeparator class implementation.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
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
# include <QHBoxLayout>
# include <QLabel>

/* GUI includes: */
# include "QILabelSeparator.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


QILabelSeparator::QILabelSeparator(QWidget *pParent /* = 0 */, Qt::WindowFlags fFlags /* = 0 */)
    : QWidget(pParent, fFlags)
    , m_pLabel(0)
{
    prepare();
}

QILabelSeparator::QILabelSeparator(const QString &strText, QWidget *pParent /* = 0 */, Qt::WindowFlags fFlags /* = 0 */)
    : QWidget(pParent, fFlags)
    , m_pLabel(0)
{
    prepare();
    setText(strText);
}

QString QILabelSeparator::text() const
{
    return m_pLabel->text();
}

void QILabelSeparator::setBuddy(QWidget *pBuddy)
{
    m_pLabel->setBuddy(pBuddy);
}

void QILabelSeparator::clear()
{
    m_pLabel->clear();
}

void QILabelSeparator::setText(const QString &strText)
{
    m_pLabel->setText(strText);
}

void QILabelSeparator::prepare()
{
    /* Create layout: */
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    if (pLayout)
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Create label: */
        m_pLabel = new QLabel;
        if (m_pLabel)
        {
            /* Add into layout: */
            pLayout->addWidget(m_pLabel);
        }

        /* Create separator: */
        QFrame *pSeparator = new QFrame;
        {
            /* Configure separator: */
            pSeparator->setFrameShape(QFrame::HLine);
            pSeparator->setFrameShadow(QFrame::Sunken);
            pSeparator->setEnabled(false);
            pSeparator->setContentsMargins(0, 0, 0, 0);
            // pSeparator->setStyleSheet("QFrame {border: 1px outset black; }");
            pSeparator->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

            /* Add into layout: */
            pLayout->addWidget(pSeparator, Qt::AlignBottom);
        }
    }
}
