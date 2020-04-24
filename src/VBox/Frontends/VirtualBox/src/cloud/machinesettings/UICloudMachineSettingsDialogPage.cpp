/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudMachineSettingsDialogPage class implementation.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#include <QHeaderView>
#include <QVBoxLayout>

/* GUI includes: */
#include "UICloudMachineSettingsDialogPage.h"


UICloudMachineSettingsDialogPage::UICloudMachineSettingsDialogPage(QWidget *pParent)
    : QWidget(pParent)
{
    prepare();
}

void UICloudMachineSettingsDialogPage::setForm(const CForm &comForm)
{
    if (m_pFormEditor)
        m_pFormEditor->setForm(comForm);
}

void UICloudMachineSettingsDialogPage::makeSureDataCommitted()
{
    if (m_pFormEditor)
        m_pFormEditor->makeSureEditorDataCommitted();
}

void UICloudMachineSettingsDialogPage::prepare()
{
    /* Prepare layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Prepare form editor widget: */
        m_pFormEditor = new UIFormEditorWidget(this);
        if (m_pFormEditor)
        {
            /* Make form-editor fit 12 sections in height by default: */
            const int iDefaultSectionHeight = m_pFormEditor->verticalHeader()
                                            ? m_pFormEditor->verticalHeader()->defaultSectionSize()
                                            : 0;
            if (iDefaultSectionHeight > 0)
            {
                const int iProposedHeight = iDefaultSectionHeight * 12;
                const int iProposedWidth = iProposedHeight * 1.66;
                m_pFormEditor->setMinimumSize(iProposedWidth, iProposedHeight);
            }

            /* Add into layout: */
            pLayout->addWidget(m_pFormEditor);
        }
    }
}
