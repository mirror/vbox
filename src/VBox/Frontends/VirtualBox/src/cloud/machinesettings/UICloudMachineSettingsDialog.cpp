/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudMachineSettingsDialog class implementation.
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
#include <QPushButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIMessageCenter.h"
#include "UICloudMachineSettingsDialog.h"
#include "UICloudMachineSettingsDialogPage.h"

/* COM includes: */
#include "CProgress.h"


UICloudMachineSettingsDialog::UICloudMachineSettingsDialog(QWidget *pParent, const CCloudMachine &comCloudMachine)
    : QIWithRetranslateUI<QDialog>(pParent)
    , m_comCloudMachine(comCloudMachine)
    , m_pPage(0)
    , m_pButtonBox(0)
{
    prepare();
}

int UICloudMachineSettingsDialog::exec()
{
    /* Request dialog initialization: */
    QMetaObject::invokeMethod(this, "sltRefresh", Qt::QueuedConnection);

    /* Call to base-class: */
    return QIWithRetranslateUI<QDialog>::exec();
}

void UICloudMachineSettingsDialog::accept()
{
    /** Makes sure page data committed: */
    if (m_pPage)
        m_pPage->makeSureDataCommitted();

    /* Apply form: */
    AssertReturnVoid(m_comForm.isNotNull());
    CProgress comProgress = m_comForm.Apply();
    if (!m_comForm.isOk())
    {
        msgCenter().cannotApplyCloudMachineFormSettings(m_comForm, m_strName, this);
        return;
    }
    msgCenter().showModalProgressDialog(comProgress,
                                        m_strName,
                                        ":/progress_settings_90px.png", this, 0);
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        msgCenter().cannotApplyCloudMachineFormSettings(comProgress, m_strName, this);
        return;
    }

    /* Call to base-class: */
    QIWithRetranslateUI<QDialog>::accept();
}

void UICloudMachineSettingsDialog::retranslateUi()
{
    /* Translate title: */
    const QString strCaption = tr("Settings");
    if (m_strName.isNull())
        setWindowTitle(strCaption);
    else
        setWindowTitle(tr("%1 - %2").arg(m_strName, strCaption));
}

void UICloudMachineSettingsDialog::sltRefresh()
{
    /* Update name: */
    m_strName = m_comCloudMachine.GetName();
    if (!m_comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(m_comCloudMachine, this);
        reject();
    }

    /* Retranslate title: */
    retranslateUi();

    /* Update form: */
    CForm comForm;
    CProgress comProgress = m_comCloudMachine.GetSettingsForm(comForm);
    if (!m_comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(m_comCloudMachine, this);
        reject();
    }
    msgCenter().showModalProgressDialog(comProgress,
                                        m_strName,
                                        ":/progress_settings_90px.png", this, 0);
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        msgCenter().cannotAcquireCloudClientParameter(comProgress, this);
        reject();
    }
    m_comForm = comForm;

    /* Assign page with form: */
    m_pPage->setForm(m_comForm);
}

void UICloudMachineSettingsDialog::prepare()
{
    /* Prepare layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        /* Prepare page: */
        m_pPage = new UICloudMachineSettingsDialogPage(this);
        if (m_pPage)
        {
            /* Add into layout: */
            pLayout->addWidget(m_pPage);
        }

        /* Prepare button-box: */
        m_pButtonBox = new QIDialogButtonBox;
        if (m_pButtonBox)
        {
            m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
            connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UICloudMachineSettingsDialog::accept);
            connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UICloudMachineSettingsDialog::reject);
            /* Add into layout: */
            pLayout->addWidget(m_pButtonBox);
        }
    }

    /* Apply language settings: */
    retranslateUi();
}
