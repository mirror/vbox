/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumTypeChangeDialog class implementation.
 */

/*
 * Copyright (C) 2011-2016 Oracle Corporation
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
# include <QGroupBox>
# include <QPushButton>
# include <QRadioButton>
# include <QVBoxLayout>

/* GUI includes: */
# include "UIMediumTypeChangeDialog.h"
# include "UIMessageCenter.h"
# include "UIConverter.h"
# include "UIMedium.h"
# include "VBoxGlobal.h"
# include "QIDialogButtonBox.h"
# include "QILabel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMediumTypeChangeDialog::UIMediumTypeChangeDialog(QWidget *pParent, const QString &strMediumID)
    : QIWithRetranslateUI<QIDialog>(pParent)
    , m_strMediumID(strMediumID)
{
    /* Prepare: */
    prepare();
}

void UIMediumTypeChangeDialog::sltAccept()
{
    /* Try to assign new medium type: */
    m_medium.SetType(m_enmMediumTypeNew);
    /* Check for result: */
    if (!m_medium.isOk())
    {
        /* Show error message: */
        msgCenter().cannotChangeMediumType(m_medium, m_enmMediumTypeOld, m_enmMediumTypeNew, this);
        return;
    }

    /* Call to base-class: */
    QIWithRetranslateUI<QIDialog>::accept();
}

void UIMediumTypeChangeDialog::sltReject()
{
    /* Call to base-class: */
    QIWithRetranslateUI<QIDialog>::reject();
}

void UIMediumTypeChangeDialog::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(tr("Modify medium attributes"));

    /* Translate label: */
    m_pLabel->setText(tr("<p>You are about to change the settings of the disk image file <b>%1</b>.</p>"
                         "<p>Please choose one of the following modes and press <b>%2</b> "
                         "to proceed or <b>%3</b> otherwise.</p>")
                      .arg(m_medium.GetLocation())
                      .arg(VBoxGlobal::removeAccelMark(m_pButtonBox->button(QDialogButtonBox::Ok)->text()))
                      .arg(VBoxGlobal::removeAccelMark(m_pButtonBox->button(QDialogButtonBox::Cancel)->text())));

    /* Translate group-box name: */
    m_pGroupBox->setTitle(tr("Choose mode:"));

    /* Translate radio-buttons: */
    QList<QRadioButton*> buttons = findChildren<QRadioButton*>();
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setText(gpConverter->toString(buttons[i]->property("mediumType").value<KMediumType>()));
}

void UIMediumTypeChangeDialog::sltValidate()
{
    /* Search for the checked button: */
    QRadioButton *pCheckedButton = 0;
    QList<QRadioButton*> buttons = findChildren<QRadioButton*>();
    for (int i = 0; i < buttons.size(); ++i)
    {
        if (buttons[i]->isChecked())
        {
            pCheckedButton = buttons[i];
            break;
        }
    }
    AssertPtrReturnVoid(pCheckedButton);

    /* Determine chosen type: */
    m_enmMediumTypeNew = pCheckedButton->property("mediumType").value<KMediumType>();

    /* Enable/disable OK button depending on chosen type,
     * for now only the previous type is restricted, others are free to choose: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_enmMediumTypeOld != m_enmMediumTypeNew);
}

void UIMediumTypeChangeDialog::prepare()
{
#ifdef VBOX_WS_MAC
    // TODO: Is that necessary?
    setWindowFlags(Qt::Sheet);
#else /* !VBOX_WS_MAC */
    /* Enable size-grip: */
    setSizeGripEnabled(true);
#endif /* !VBOX_WS_MAC */

    /* Search for corresponding medium: */
    m_medium = vboxGlobal().medium(m_strMediumID).medium();
    m_enmMediumTypeOld = m_medium.GetType();
    m_enmMediumTypeNew = m_enmMediumTypeOld;

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    AssertPtrReturnVoid(pMainLayout);
    {
        /* Create label: */
        m_pLabel = new QILabel;
        AssertPtrReturnVoid(m_pLabel);
        {
            /* Configure label: */
            m_pLabel->setWordWrap(true);
            m_pLabel->useSizeHintForWidth(450);
            m_pLabel->updateGeometry();
        }
        /* Add label into main layout: */
        pMainLayout->addWidget(m_pLabel);

        /* Create group-box: */
        m_pGroupBox = new QGroupBox;
        AssertPtrReturnVoid(m_pGroupBox);
        {
            /* Configure group-box: */
            m_pGroupBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

            /* Prepares radio-buttons: */
            prepareMediumTypeButtons();
        }
        /* Add group-box into main layout: */
        pMainLayout->addWidget(m_pGroupBox);

        /* Add a stretch to main layout: */
        pMainLayout->addStretch();

        /* Create button-box: */
        m_pButtonBox = new QIDialogButtonBox;
        AssertPtrReturnVoid(m_pButtonBox);
        {
            /* Configure button-box: */
            m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            m_pButtonBox->button(QDialogButtonBox::Ok)->setDefault(true);
            connect(m_pButtonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), this, SLOT(sltAccept()));
            connect(m_pButtonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(sltReject()));
        }
        /* Add button-box into main layout: */
        pMainLayout->addWidget(m_pButtonBox);
    }

    /* Retranslate: */
    retranslateUi();

    /* Revalidate: */
    sltValidate();

    /* Resize to minimum size: */
    resize(minimumSizeHint());
}

void UIMediumTypeChangeDialog::prepareMediumTypeButtons()
{
    /* Register required meta-type: */
    qRegisterMetaType<KMediumType>();

    /* Create group-box layout: */
    m_pGroupBoxLayout = new QVBoxLayout(m_pGroupBox);
    AssertPtrReturnVoid(m_pGroupBoxLayout);
    {
        /* Populate radio-buttons: */
        prepareMediumTypeButton(KMediumType_Normal);
        prepareMediumTypeButton(KMediumType_Immutable);
        prepareMediumTypeButton(KMediumType_Writethrough);
        prepareMediumTypeButton(KMediumType_Shareable);
        prepareMediumTypeButton(KMediumType_MultiAttach);
        /* Make sure button reflecting current type is checked: */
        QList<QRadioButton*> buttons = m_pGroupBox->findChildren<QRadioButton*>();
        for (int i = 0; i < buttons.size(); ++i)
        {
            if (buttons[i]->property("mediumType").value<KMediumType>() == m_enmMediumTypeOld)
            {
                buttons[i]->setChecked(true);
                buttons[i]->setFocus();
                break;
            }
        }
    }
}

void UIMediumTypeChangeDialog::prepareMediumTypeButton(KMediumType mediumType)
{
    /* Create radio-button: */
    QRadioButton *pRadioButton = new QRadioButton(m_pGroupBox);
    AssertPtrReturnVoid(pRadioButton);
    {
        /* Configure radio-button: */
        connect(pRadioButton, SIGNAL(clicked()), this, SLOT(sltValidate()));
        pRadioButton->setProperty("mediumType", QVariant::fromValue(mediumType));
    }
    /* Add radio-button into layout: */
    m_pGroupBoxLayout->addWidget(pRadioButton);
}

