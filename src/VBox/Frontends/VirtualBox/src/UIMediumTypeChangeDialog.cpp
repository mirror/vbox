/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMediumTypeChangeDialog class implementation
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include <QVBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QPushButton>

/* GUI includes: */
#include "UIMediumTypeChangeDialog.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "QILabel.h"
#include "QIDialogButtonBox.h"
#include "COMEnumsWrapper.h"

/* Constructor: */
UIMediumTypeChangeDialog::UIMediumTypeChangeDialog(QWidget *pParent, const QString &strMediumId)
    : QIWithRetranslateUI<QIDialog>(pParent)
{
#ifdef Q_WS_MAC
    setWindowFlags(Qt::Sheet);
#else /* Q_WS_MAC */
    /* Enable size-grip: */
    setSizeGripEnabled(true);
#endif /* Q_WS_MAC */

    /* Search for corresponding medium: */
    m_medium = vboxGlobal().findMedium(strMediumId).medium();
    m_oldMediumType = m_medium.GetType();
    m_newMediumType = m_oldMediumType;

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    /* Create description label: */
    m_pLabel = new QILabel(this);
    m_pLabel->setWordWrap(true);
    m_pLabel->useSizeHintForWidth(450);
    m_pLabel->updateGeometry();
    pMainLayout->addWidget(m_pLabel);

    /* Create group-box: */
    m_pGroupBox = new QGroupBox(this);
    pMainLayout->addWidget(m_pGroupBox);

    /* Create button-box: */
    m_pButtonBox = new QIDialogButtonBox(this);
    m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_pButtonBox->button(QDialogButtonBox::Ok)->setDefault(true);
    connect(m_pButtonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), this, SLOT(sltAccept()));
    connect(m_pButtonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(sltReject()));
    pMainLayout->addWidget(m_pButtonBox);

    /* Finally, add a stretch: */
    pMainLayout->addStretch();

    /* Create radio-buttons: */
    createMediumTypeButtons();

    /* Retranslate: */
    retranslateUi();

    /* Resize: */
    resize(minimumSizeHint());
}

/* Accept finisher: */
void UIMediumTypeChangeDialog::sltAccept()
{
    /* Try to assign new medium type: */
    m_medium.SetType(m_newMediumType);
    /* Check for result: */
    if (!m_medium.isOk())
    {
        /* Show error message: */
        msgCenter().cannotChangeMediumType(this, m_medium, m_oldMediumType, m_newMediumType);
        return;
    }
    /* Accept dialog with parent class method: */
    QIWithRetranslateUI<QIDialog>::accept();
}

/* Reject finisher: */
void UIMediumTypeChangeDialog::sltReject()
{
    /* Reject dialog with parent class method: */
    QIWithRetranslateUI<QIDialog>::reject();
}

/* Translation stuff: */
void UIMediumTypeChangeDialog::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(tr("Modify medium attributes"));

    /* Translate description: */
    m_pLabel->setText(tr("<p>You are about to change the attributes of the virtual disk located in <b>%1</b>.</p>"
                         "<p>Please choose one of the following medium types and press <b>%2</b> "
                         "to proceed or <b>%3</b> otherwise.</p>")
                      .arg(m_medium.GetLocation())
                      .arg(VBoxGlobal::removeAccelMark(m_pButtonBox->button(QDialogButtonBox::Ok)->text()))
                      .arg(VBoxGlobal::removeAccelMark(m_pButtonBox->button(QDialogButtonBox::Cancel)->text())));

    /* Translate group-box: */
    m_pGroupBox->setTitle(tr("Choose medium type:"));

    /* Translate radio-buttons: */
    QList<QRadioButton*> buttons = findChildren<QRadioButton*>();
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setText(gCOMenum->toString(buttons[i]->property("mediumType").value<KMediumType>()));
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
    /* Determine chosen type: */
    m_newMediumType = pCheckedButton->property("mediumType").value<KMediumType>();
    /* Enable/disable OK button depending on chosen type,
     * for now only the previous type is restricted, others are free to choose: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_oldMediumType != m_newMediumType);
}

/* Create medium-type radio-buttons: */
void UIMediumTypeChangeDialog::createMediumTypeButtons()
{
    /* Register required meta-type: */
    qRegisterMetaType<KMediumType>();
    /* Create group-box layout: */
    m_pGroupBoxLayout = new QVBoxLayout(m_pGroupBox);
    /* Populate radio-buttons: */
    createMediumTypeButton(KMediumType_Normal);
    createMediumTypeButton(KMediumType_Immutable);
    createMediumTypeButton(KMediumType_Writethrough);
    createMediumTypeButton(KMediumType_Shareable);
    createMediumTypeButton(KMediumType_MultiAttach);
    /* Make sure button reflecting previoius type is checked: */
    QList<QRadioButton*> buttons = findChildren<QRadioButton*>();
    for (int i = 0; i < buttons.size(); ++i)
    {
        if (buttons[i]->property("mediumType").value<KMediumType>() == m_oldMediumType)
        {
            buttons[i]->setChecked(true);
            buttons[i]->setFocus();
            break;
        }
    }
    /* Revalidate finally: */
    sltValidate();
}

/* Create radio-button for the passed medium-type: */
void UIMediumTypeChangeDialog::createMediumTypeButton(KMediumType mediumType)
{
    /* Create corresponding radio-button: */
    QRadioButton *pRadioButton = new QRadioButton(m_pGroupBox);
    connect(pRadioButton, SIGNAL(clicked()), this, SLOT(sltValidate()));
    pRadioButton->setProperty("mediumType", QVariant::fromValue(mediumType));
    m_pGroupBoxLayout->addWidget(pRadioButton);
}

