/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
# include <QComboBox>
# include <QHBoxLayout>
# include <QFontDatabase>
# include <QFontDialog>
# include <QCheckBox>
# include <QLabel>
# include <QSpinBox>

/* GUI includes: */
# include "QIToolButton.h"
# include "UIIconPool.h"
# include "UIVMLogViewerSettingsPanel.h"
# include "UIVMLogViewerWidget.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIVMLogViewerSettingsPanel::UIVMLogViewerSettingsPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
    : UIVMLogViewerPanel(pParent, pViewer)
    , m_pLineNumberCheckBox(0)
    , m_pWrapLinesCheckBox(0)
    , m_pFontSizeSpinBox(0)
    , m_pFontSizeLabel(0)
    , m_pOpenFontDialogButton(0)
    , m_pResetToDefaultsButton(0)
    , m_iDefaultFontSize(9)
{
    prepare();
}

void UIVMLogViewerSettingsPanel::setShowLineNumbers(bool bShowLineNumbers)
{
    if (!m_pLineNumberCheckBox)
        return;
    if (m_pLineNumberCheckBox->isChecked() == bShowLineNumbers)
        return;
    m_pLineNumberCheckBox->setChecked(bShowLineNumbers);
}

void UIVMLogViewerSettingsPanel::setWrapLines(bool bWrapLines)
{
    if (!m_pWrapLinesCheckBox)
        return;
    if (m_pWrapLinesCheckBox->isChecked() == bWrapLines)
        return;
    m_pWrapLinesCheckBox->setChecked(bWrapLines);
}

void UIVMLogViewerSettingsPanel::setFontSizeInPoints(int fontSizeInPoints)
{
    if (!m_pFontSizeSpinBox)
        return;
    if (m_pFontSizeSpinBox->value() == fontSizeInPoints)
        return;
    m_pFontSizeSpinBox->setValue(fontSizeInPoints);
}

void UIVMLogViewerSettingsPanel::prepareWidgets()
{
    if (!mainLayout())
        return;

    m_pLineNumberCheckBox = new QCheckBox;
    if (m_pLineNumberCheckBox)
    {
        m_pLineNumberCheckBox->setChecked(true);
        mainLayout()->addWidget(m_pLineNumberCheckBox, 0, Qt::AlignLeft);
    }

    m_pWrapLinesCheckBox = new QCheckBox;
    if (m_pWrapLinesCheckBox)
    {
        m_pWrapLinesCheckBox->setChecked(false);
        mainLayout()->addWidget(m_pWrapLinesCheckBox, 0, Qt::AlignLeft);
    }

    m_pFontSizeSpinBox = new QSpinBox;
    if (m_pFontSizeSpinBox)
    {
        mainLayout()->addWidget(m_pFontSizeSpinBox, 0, Qt::AlignLeft);
        m_pFontSizeSpinBox->setValue(m_iDefaultFontSize);
        m_pFontSizeSpinBox->setMaximum(44);
        m_pFontSizeSpinBox->setMinimum(6);
    }

    m_pFontSizeLabel = new QLabel;
    if (m_pFontSizeLabel)
    {
        mainLayout()->addWidget(m_pFontSizeLabel, 0, Qt::AlignLeft);
    }

    m_pOpenFontDialogButton = new QIToolButton;
    if (m_pOpenFontDialogButton)
    {
        mainLayout()->addWidget(m_pOpenFontDialogButton, 0);
        m_pOpenFontDialogButton->setIcon(UIIconPool::iconSet(":/log_viewer_goto_selected_bookmark_16px.png"));
    }

    m_pResetToDefaultsButton = new QIToolButton;
    if (m_pResetToDefaultsButton)
    {
        mainLayout()->addWidget(m_pResetToDefaultsButton, 0);
        m_pResetToDefaultsButton->setIcon(UIIconPool::iconSet(":/log_viewer_goto_selected_bookmark_16px.png"));
    }
    mainLayout()->addStretch(2);
}

void UIVMLogViewerSettingsPanel::prepareConnections()
{
    if (m_pLineNumberCheckBox)
        connect(m_pLineNumberCheckBox, &QCheckBox::toggled, this, &UIVMLogViewerSettingsPanel::sigShowLineNumbers);
    if (m_pWrapLinesCheckBox)
        connect(m_pWrapLinesCheckBox, &QCheckBox::toggled, this, &UIVMLogViewerSettingsPanel::sigWrapLines);
    if (m_pFontSizeSpinBox)
        connect(m_pFontSizeSpinBox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this, &UIVMLogViewerSettingsPanel::sigChangeFontSizeInPoints);
    if (m_pOpenFontDialogButton)
        connect(m_pOpenFontDialogButton, &QIToolButton::clicked, this, &UIVMLogViewerSettingsPanel::sltOpenFontDialog);
    if (m_pResetToDefaultsButton)
        connect(m_pResetToDefaultsButton, &QIToolButton::clicked, this, &UIVMLogViewerSettingsPanel::sigResetToDefaults);
}

void UIVMLogViewerSettingsPanel::retranslateUi()
{
    UIVMLogViewerPanel::retranslateUi();
    if (m_pLineNumberCheckBox)
    {
        m_pLineNumberCheckBox->setText(UIVMLogViewerWidget::tr("Show Line Numbers"));
        m_pLineNumberCheckBox->setToolTip(UIVMLogViewerWidget::tr("Show Line Numbers"));
    }

    if (m_pWrapLinesCheckBox)
    {
        m_pWrapLinesCheckBox->setText(UIVMLogViewerWidget::tr("Wrap Lines"));
        m_pWrapLinesCheckBox->setToolTip(UIVMLogViewerWidget::tr("Wrap Lines"));
    }

    if (m_pFontSizeLabel)
    {
        m_pFontSizeLabel->setText(UIVMLogViewerWidget::tr("Font Size"));
        m_pFontSizeSpinBox->setToolTip(UIVMLogViewerWidget::tr("Log Viewer Font Size"));
    }

    if (m_pOpenFontDialogButton)
        m_pOpenFontDialogButton->setToolTip(UIVMLogViewerWidget::tr("Open a font dialog to select font face for the logviewer"));

    if (m_pResetToDefaultsButton)
        m_pResetToDefaultsButton->setToolTip(UIVMLogViewerWidget::tr("Reset settings to application defaults"));
}

void UIVMLogViewerSettingsPanel::sltOpenFontDialog()
{
    QFont currentFont;
    UIVMLogViewerWidget* parentWidget = qobject_cast<UIVMLogViewerWidget*>(parent());
    if (!parentWidget)
        return;

    currentFont = parentWidget->currentFont();
    bool ok;
    QFont font =
        QFontDialog::getFont(&ok, currentFont, this, "Logviewer font");

    if (ok)
        emit sigChangeFont(font);
}
