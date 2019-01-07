/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoCreator class implementation.
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
#include <QVBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIIconPool.h"
#include "UIToolBar.h"
#include "UIVisoHostBrowser.h"
#include "UIVisoCreator.h"
#include "UIVisoCreatorOptionsDialog.h"
#include "UIVisoContentBrowser.h"


UIVisoCreator::UIVisoCreator(QWidget *pParent /* =0 */)
    : QIWithRetranslateUI<QIDialog>(pParent)
    , m_pMainLayout(0)
    , m_pVerticalSplitter(0)
    , m_pHostBrowser(0)
    , m_pVisoBrowser(0)
    , m_pButtonBox(0)
    , m_pToolBar(0)
    , m_pActionOptions(0)
{

    prepareActions();
    prepareObjects();
    prepareConnections();
}

UIVisoCreator::~UIVisoCreator()
{
}

QStringList UIVisoCreator::pathList()
{
    if (!m_pVisoBrowser)
        return QStringList();
    return m_pVisoBrowser->pathList();
}

void UIVisoCreator::retranslateUi()
{
    if (m_pActionOptions)
    {
        m_pActionOptions->setText(tr("&Options..."));
        m_pActionOptions->setToolTip(tr("VISO Options"));
        m_pActionOptions->setStatusTip(tr("Manage VISO Options"));
    }
}

void UIVisoCreator::sltHandleAddObjectsToViso(QStringList pathList)
{
    if (m_pVisoBrowser)
        m_pVisoBrowser->addObjectsToViso(pathList);
}

void UIVisoCreator::sltHandleOptionsAction()
{
    UIVisoCreatorOptionsDialog *pDialog = new UIVisoCreatorOptionsDialog(m_visoOptions, m_browserOptions, this);

    if(!pDialog)
        return;
    if (pDialog->execute(true, false))
    {
        /** Check if any of the options has been modified: */
        checkBrowserOptions(pDialog->browserOptions());
        checkVisoOptions(pDialog->visoOptions());
    }
    delete pDialog;
}


void UIVisoCreator::prepareObjects()
{
    m_pMainLayout = new QVBoxLayout;
    if (!m_pMainLayout)
        return;

    m_pToolBar = new UIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        /* Add toolbar actions: */
        if (m_pActionOptions)
            m_pToolBar->addAction(m_pActionOptions);
        m_pMainLayout->addWidget(m_pToolBar);
    }

    m_pVerticalSplitter = new QSplitter;
    if (!m_pVerticalSplitter)
        return;

    m_pMainLayout->addWidget(m_pVerticalSplitter);
    m_pVerticalSplitter->setOrientation(Qt::Vertical);
    m_pVerticalSplitter->setHandleWidth(1);

    m_pHostBrowser = new UIVisoHostBrowser;
    if (m_pHostBrowser)
    {
        m_pVerticalSplitter->addWidget(m_pHostBrowser);
        connect(m_pHostBrowser, &UIVisoHostBrowser::sigAddObjectsToViso,
                this, &UIVisoCreator::sltHandleAddObjectsToViso);
    }
    m_pVisoBrowser = new UIVisoContentBrowser;
    if (m_pVisoBrowser)
    {
        m_pVerticalSplitter->addWidget(m_pVisoBrowser);
        m_pVisoBrowser->setVisoName(m_visoOptions.m_strVisoName);
    }
    m_pButtonBox = new QIDialogButtonBox;
    if (m_pButtonBox)
    {
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        m_pMainLayout->addWidget(m_pButtonBox);
    }
    retranslateUi();
    setLayout(m_pMainLayout);
}

void UIVisoCreator::prepareConnections()
{
    if (m_pButtonBox)
    {
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIVisoCreator::close);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIVisoCreator::accept);
    }
    if (m_pActionOptions)
    {
        connect(m_pActionOptions, &QAction::triggered, this, &UIVisoCreator::sltHandleOptionsAction);

    }
}

void UIVisoCreator::prepareActions()
{
    m_pActionOptions = new QAction(this);
    if (m_pActionOptions)
    {
        /* Configure add-action: */
        m_pActionOptions->setShortcut(QKeySequence("Ctrl+A"));

        m_pActionOptions->setIcon(UIIconPool::iconSetFull(":/file_manager_options_32px.png",
                                                          ":/%file_manager_options_16px.png",
                                                          ":/file_manager_options_disabled_32px.png",
                                                          ":/file_manager_options_disabled_16px.png"));
    }
}

void UIVisoCreator::checkBrowserOptions(const BrowserOptions &browserOptions)
{
    if (browserOptions == m_browserOptions)
        return;
    if (browserOptions.m_bShowHiddenObjects != m_browserOptions.m_bShowHiddenObjects)
    {
        if (m_pHostBrowser)
            m_pHostBrowser->showHideHiddenObjects(browserOptions.m_bShowHiddenObjects);
        if(m_pVisoBrowser)
            m_pVisoBrowser->showHideHiddenObjects(browserOptions.m_bShowHiddenObjects);
    }
    m_browserOptions = browserOptions;
}

void UIVisoCreator::checkVisoOptions(const VisoOptions &visoOptions)
{
    if (visoOptions == m_visoOptions)
        return;
    if (visoOptions.m_strVisoName != m_visoOptions.m_strVisoName)
    {
        if(m_pVisoBrowser)
            m_pVisoBrowser->setVisoName(visoOptions.m_strVisoName);
    }
    m_visoOptions = visoOptions;

}
