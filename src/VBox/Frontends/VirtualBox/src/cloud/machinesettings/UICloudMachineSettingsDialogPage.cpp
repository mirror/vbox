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

/* Other VBox includes: */
#include "iprt/assert.h"


UICloudMachineSettingsDialogPage::UICloudMachineSettingsDialogPage(QWidget *pParent, bool fFullScale /* = true */)
    : QWidget(pParent)
    , m_fFullScale(fFullScale)
{
    prepare();
}

void UICloudMachineSettingsDialogPage::setForm(const CForm &comForm)
{
    m_comForm = comForm;
    updateEditor();
}

void UICloudMachineSettingsDialogPage::setFilter(const QString &strFilter)
{
    m_strFilter = strFilter;
    updateEditor();
}

void UICloudMachineSettingsDialogPage::makeSureDataCommitted()
{
    AssertPtrReturnVoid(m_pFormEditor.data());
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
                const int iProposedHeight = iDefaultSectionHeight * (m_fFullScale ? 12 : 6);
                const int iProposedWidth = iProposedHeight * 1.66;
                m_pFormEditor->setMinimumSize(iProposedWidth, iProposedHeight);
            }

            /* Add into layout: */
            pLayout->addWidget(m_pFormEditor);
        }
    }
}

void UICloudMachineSettingsDialogPage::updateEditor()
{
    /* Make sure editor present: */
    AssertPtrReturnVoid(m_pFormEditor.data());

    /* Make sure form isn't null: */
    if (m_comForm.isNotNull())
    {
        /* Acquire initial values: */
        const QVector<CFormValue> initialValues = m_comForm.GetValues();

        /* If filter null: */
        if (m_strFilter.isNull())
        {
            /* Push initial values to editor: */
            m_pFormEditor->setValues(initialValues);
        }
        /* If filter present: */
        else
        {
            /* Acquire group fields: */
            const QVector<QString> groupFields = m_comForm.GetFieldGroup(m_strFilter);
            /* Filter out unrelated values: */
            QVector<CFormValue> filteredValues;
            foreach (const CFormValue &comValue, initialValues)
                if (groupFields.contains(comValue.GetLabel()))
                    filteredValues << comValue;
            /* Push filtered values to editor: */
            m_pFormEditor->setValues(filteredValues);
        }
    }

    /* Revalidate: */
    emit sigValidChanged(m_comForm.isNotNull());
}
