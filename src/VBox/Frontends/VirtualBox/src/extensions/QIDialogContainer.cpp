/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIDialogContainer class implementation.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QIDialogContainer.h"

QIDialogContainer::QIDialogContainer(QWidget *pParent /* = 0 */, Qt::WindowFlags enmFlags /* = Qt::WindowFlags() */)
    : QDialog(pParent, enmFlags)
    , m_pLayout(0)
    , m_pWidget(0)
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

void QIDialogContainer::prepare()
{
    /* Prepare layout: */
    m_pLayout = new QGridLayout(this);
    if (m_pLayout)
    {
        /* Prepare dialog button-box: */
        QIDialogButtonBox *pButtonBox = new QIDialogButtonBox(this);
        if (pButtonBox)
        {
            pButtonBox->setStandardButtons(QDialogButtonBox::Ok);
            connect(pButtonBox, &QIDialogButtonBox::accepted,
                    this, &QDialog::accept);
            connect(pButtonBox, &QIDialogButtonBox::rejected,
                    this, &QDialog::reject);

            m_pLayout->addWidget(pButtonBox, 1, 0);
        }
    }
}
