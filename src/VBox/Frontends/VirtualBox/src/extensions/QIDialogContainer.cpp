/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIDialogContainer class implementation.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
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
#include <QGridLayout>
#include <QProgressBar>
#include <QPushButton>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QIDialogContainer.h"

/* Other VBox includes: */
#include "iprt/assert.h"


QIDialogContainer::QIDialogContainer(QWidget *pParent /* = 0 */, Qt::WindowFlags enmFlags /* = Qt::WindowFlags() */)
    : QDialog(pParent, enmFlags)
    , m_pLayout(0)
    , m_pWidget(0)
    , m_pProgressBar(0)
    , m_pButtonBox(0)
{
    prepare();
}

void QIDialogContainer::setWidget(QWidget *pWidget)
{
    delete m_pWidget;
    m_pWidget = pWidget;
    if (m_pWidget)
        m_pLayout->addWidget(m_pWidget, 0, 0);
}

void QIDialogContainer::setProgressBarHidden(bool fHidden)
{
    AssertPtrReturnVoid(m_pProgressBar);
    m_pProgressBar->setHidden(fHidden);
}

void QIDialogContainer::setOkButtonEnabled(bool fEnabled)
{
    AssertPtrReturnVoid(m_pButtonBox);
    AssertPtrReturnVoid(m_pButtonBox->button(QDialogButtonBox::Ok));
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(fEnabled);
}

void QIDialogContainer::prepare()
{
    /* Prepare layout: */
    m_pLayout = new QGridLayout(this);
    if (m_pLayout)
    {
        /* Prepare horizontal layout: */
        QHBoxLayout *pHLayout = new QHBoxLayout;
        if (pHLayout)
        {
            /* Prepare progress-bar: */
            m_pProgressBar = new QProgressBar(this);
            if (m_pProgressBar)
            {
                m_pProgressBar->setHidden(true);
                m_pProgressBar->setMinimum(0);
                m_pProgressBar->setMaximum(0);

                /* Add into layout: */
                pHLayout->addWidget(m_pProgressBar);
            }

            /* Prepare dialog button-box: */
            m_pButtonBox = new QIDialogButtonBox(this);
            if (m_pButtonBox)
            {
                m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok);
                connect(m_pButtonBox, &QIDialogButtonBox::accepted,
                        this, &QDialog::accept);
                connect(m_pButtonBox, &QIDialogButtonBox::rejected,
                        this, &QDialog::reject);

                /* Add into layout: */
                pHLayout->addWidget(m_pButtonBox);
            }

            /* Add into layout: */
            m_pLayout->addLayout(pHLayout, 1, 0);
        }
    }
}
