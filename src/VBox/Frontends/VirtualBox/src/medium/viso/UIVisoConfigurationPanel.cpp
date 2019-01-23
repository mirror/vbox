/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoConfigurationPanel class implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <QCheckBox>
#include <QGridLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>
#include <QTextBlock>

/* GUI includes: */
#include <QComboBox>
#include "QIDialogButtonBox.h"
#include "UIIconPool.h"
#include "QILabel.h"
#include "QILineEdit.h"
#include "QIToolButton.h"
#include "UIVisoConfigurationPanel.h"

UIVisoConfigurationPanel::UIVisoConfigurationPanel(QWidget *pParent /* =0 */)
    : UIVisoCreatorPanel(pParent)
    , m_pVisoNameLabel(0)
    , m_pCustomOptionsLabel(0)
    , m_pVisoNameLineEdit(0)
    , m_pCustomOptionsComboBox(0)
    , m_pDeleteButton(0)
{
    prepareObjects();
    prepareConnections();
}

UIVisoConfigurationPanel::~UIVisoConfigurationPanel()
{
}

QString UIVisoConfigurationPanel::panelName() const
{
    return "ConfigurationPanel";
}

void UIVisoConfigurationPanel::setVisoName(const QString& strVisoName)
{
    if (m_pVisoNameLineEdit)
        m_pVisoNameLineEdit->setText(strVisoName);
}

void UIVisoConfigurationPanel::setVisoCustomOptions(const QStringList& visoCustomOptions)
{
    if (!m_pCustomOptionsComboBox)
        return;
    m_pCustomOptionsComboBox->clear();
    foreach (const QString &strOption, visoCustomOptions)
    {
        m_pCustomOptionsComboBox->addItem(strOption);
    }
}

void UIVisoConfigurationPanel::prepareObjects()
{
    if (!mainLayout())
        return;

    /* Name edit and and label: */
    m_pVisoNameLabel = new QILabel(QApplication::translate("UIVisoCreator", "VISO Name:"));
    m_pVisoNameLineEdit = new QILineEdit;
    if (m_pVisoNameLabel && m_pVisoNameLineEdit)
    {
        m_pVisoNameLabel->setBuddy(m_pVisoNameLineEdit);
        mainLayout()->addWidget(m_pVisoNameLabel, 0, Qt::AlignLeft);
        mainLayout()->addWidget(m_pVisoNameLineEdit, 0, Qt::AlignLeft);
    }

    addVerticalSeparator();


    /* Cutom Viso options stuff: */
    m_pCustomOptionsLabel = new QILabel(QApplication::translate("UIVisoCreator", "Custom VISO options:"));
    m_pCustomOptionsComboBox = new QComboBox;
    m_pDeleteButton = new QIToolButton;

    if (m_pCustomOptionsLabel && m_pCustomOptionsComboBox && m_pDeleteButton)
    {
        m_pDeleteButton->setIcon(UIIconPool::iconSet(":/log_viewer_delete_current_bookmark_16px.png"));

        m_pCustomOptionsComboBox->setEditable(true);
        m_pCustomOptionsLabel->setBuddy(m_pCustomOptionsComboBox);

        mainLayout()->addWidget(m_pCustomOptionsLabel, 0, Qt::AlignLeft);
        mainLayout()->addWidget(m_pCustomOptionsComboBox, Qt::AlignLeft);
        mainLayout()->addWidget(m_pDeleteButton, 0, Qt::AlignLeft);
    }
    retranslateUi();
}

void UIVisoConfigurationPanel::prepareConnections()
{
    if (m_pVisoNameLineEdit)
        connect(m_pVisoNameLineEdit, &QILineEdit::editingFinished, this, &UIVisoConfigurationPanel::sltHandleVisoNameChanged);
    if (m_pDeleteButton)
        connect(m_pDeleteButton, &QIToolButton::clicked, this, &UIVisoConfigurationPanel::sltHandleDeleteCurrentCustomOption);
}

bool UIVisoConfigurationPanel::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        /* Process key press only: */
    case QEvent::KeyPress:
        {
            /* Cast to corresponding key press event: */
            QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

             if (pKeyEvent->key() == Qt::Key_Return && m_pCustomOptionsComboBox && m_pCustomOptionsComboBox->hasFocus())
                 addCustomVisoOption();
            return true;
            break;
        }
    default:
        break;
    }
    /* Call to base-class: */
    return UIVisoCreatorPanel::eventFilter(pObject, pEvent);
}

void UIVisoConfigurationPanel::retranslateUi()
{
    if (m_pVisoNameLabel)
        m_pVisoNameLabel->setText(QApplication::translate("UIVisoCreator", "VISO Name:"));
    if (m_pCustomOptionsLabel)
        m_pCustomOptionsLabel->setText(QApplication::translate("UIVisoCreator", "Custom VISO options:"));
    if (m_pDeleteButton)
        m_pDeleteButton->setToolTip(QApplication::translate("UIVisoCreator", "Remove current option."));
}

void UIVisoConfigurationPanel::addCustomVisoOption()
{
    if (!m_pCustomOptionsComboBox)
        return;
    if (m_pCustomOptionsComboBox->currentText().isEmpty())
        return;

    emitCustomVisoOptions();
    m_pCustomOptionsComboBox->clearEditText();
}

void UIVisoConfigurationPanel::emitCustomVisoOptions()
{
    if (!m_pCustomOptionsComboBox)
        return;
    QStringList customVisoOptions;
    for (int i = 0; i < m_pCustomOptionsComboBox->count(); ++i)
        customVisoOptions << m_pCustomOptionsComboBox->itemText(i);

    if (!customVisoOptions.isEmpty())
        emit sigCustomVisoOptionsChanged(customVisoOptions);
}

void UIVisoConfigurationPanel::sltHandleVisoNameChanged()
{
    if (m_pVisoNameLineEdit)
        emit sigVisoNameChanged(m_pVisoNameLineEdit->text());
}

void UIVisoConfigurationPanel::sltHandleDeleteCurrentCustomOption()
{
    if (!m_pCustomOptionsComboBox)
        return;
    if (m_pCustomOptionsComboBox->currentText().isEmpty())
        return;
    m_pCustomOptionsComboBox->removeItem(m_pCustomOptionsComboBox->currentIndex());
    emitCustomVisoOptions();
}
