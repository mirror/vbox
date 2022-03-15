/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalDisplayFeaturesEditor class implementation.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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
#include <QLabel>

/* GUI includes: */
#include "UIGlobalDisplayFeaturesEditor.h"
#ifdef VBOX_WS_X11
# include "VBoxUtils-x11.h"
#endif


UIGlobalDisplayFeaturesEditor::UIGlobalDisplayFeaturesEditor(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fActivateOnMouseHover(false)
    , m_fDisableHostScreenSaver(false)
    , m_pLabel(0)
    , m_pCheckBoxActivateOnMouseHover(0)
    , m_pCheckBoxDisableHostScreenSaver(0)
{
    prepare();
}

void UIGlobalDisplayFeaturesEditor::setActivateOnMouseHover(bool fOn)
{
    if (m_pCheckBoxActivateOnMouseHover)
    {
        /* Update cached value and
         * check-box if value has changed: */
        if (m_fActivateOnMouseHover != fOn)
        {
            m_fActivateOnMouseHover = fOn;
            m_pCheckBoxActivateOnMouseHover->setCheckState(fOn ? Qt::Checked : Qt::Unchecked);
        }
    }
}

bool UIGlobalDisplayFeaturesEditor::activateOnMouseHover() const
{
    return   m_pCheckBoxActivateOnMouseHover
           ? m_pCheckBoxActivateOnMouseHover->checkState() == Qt::Checked
           : m_fActivateOnMouseHover;
}

void UIGlobalDisplayFeaturesEditor::setDisableHostScreenSaver(bool fOn)
{
    if (m_pCheckBoxDisableHostScreenSaver)
    {
        /* Update cached value and
         * check-box if value has changed: */
        if (m_fDisableHostScreenSaver != fOn)
        {
            m_fDisableHostScreenSaver = fOn;
            m_pCheckBoxDisableHostScreenSaver->setCheckState(fOn ? Qt::Checked : Qt::Unchecked);
        }
    }
}

bool UIGlobalDisplayFeaturesEditor::disableHostScreenSaver() const
{
    return   m_pCheckBoxDisableHostScreenSaver
           ? m_pCheckBoxDisableHostScreenSaver->checkState() == Qt::Checked
           : m_fDisableHostScreenSaver;
}

int UIGlobalDisplayFeaturesEditor::minimumLabelHorizontalHint() const
{
    return m_pLabel->minimumSizeHint().width();
}

void UIGlobalDisplayFeaturesEditor::setMinimumLayoutIndent(int iIndent)
{
    if (m_pLayout)
        m_pLayout->setColumnMinimumWidth(0, iIndent);
}

void UIGlobalDisplayFeaturesEditor::retranslateUi()
{
    if (m_pLabel)
        m_pLabel->setText(tr("Extended Features:"));
    if (m_pCheckBoxActivateOnMouseHover)
    {
        m_pCheckBoxActivateOnMouseHover->setToolTip(tr("When checked, machine windows will be raised "
                                                       "when the mouse pointer moves over them."));
        m_pCheckBoxActivateOnMouseHover->setText(tr("&Raise Window Under Mouse Pointer"));
    }
    if (m_pCheckBoxDisableHostScreenSaver)
    {
        m_pCheckBoxDisableHostScreenSaver->setToolTip(tr("When checked, screen saver of "
                                                         "the host OS is disabled."));
        m_pCheckBoxDisableHostScreenSaver->setText(tr("&Disable Host Screen Saver"));
    }
}

void UIGlobalDisplayFeaturesEditor::prepare()
{
    /* Prepare main layout: */
    m_pLayout = new QGridLayout(this);
    if (m_pLayout)
    {
        m_pLayout->setContentsMargins(0, 0, 0, 0);
        m_pLayout->setColumnStretch(1, 1);

        /* Prepare label: */
        m_pLabel = new QLabel(this);
        if (m_pLabel)
        {
            m_pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_pLayout->addWidget(m_pLabel, 0, 0);
        }
        /* Prepare 'activate on mouse hover' check-box: */
        m_pCheckBoxActivateOnMouseHover = new QCheckBox(this);
        if (m_pCheckBoxActivateOnMouseHover)
            m_pLayout->addWidget(m_pCheckBoxActivateOnMouseHover, 0, 1);
        /* Prepare 'disable host screen saver' check-box: */
#if defined(VBOX_WS_WIN)
        m_pCheckBoxDisableHostScreenSaver = new QCheckBox(this);
#elif defined(VBOX_WS_X11)
        if (NativeWindowSubsystem::X11CheckDBusScreenSaverServices())
            m_pCheckBoxDisableHostScreenSaver = new QCheckBox(this);
#endif /* VBOX_WS_X11 */
        if (m_pCheckBoxDisableHostScreenSaver)
            m_pLayout->addWidget(m_pCheckBoxDisableHostScreenSaver, 1, 1);
    }

    /* Apply language settings: */
    retranslateUi();
}
