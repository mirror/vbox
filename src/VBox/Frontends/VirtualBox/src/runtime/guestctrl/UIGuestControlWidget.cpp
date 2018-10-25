/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlWidget class implementation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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
# include <QVBoxLayout>

/* GUI includes: */
# include "QITabWidget.h"
# include "UIActionPool.h"
# include "UIGuestControlFileManager.h"
# include "UIGuestControlWidget.h"
# include "UIGuestProcessControlWidget.h"
# include "VBoxGlobal.h"


/* COM includes: */
# include "CGuest.h"
# include "CEventSource.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIGuestControlWidget::UIGuestControlWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                           const CGuest &comGuest, bool fShowToolbar /* = true */, QWidget *pParent /*= 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_comGuest(comGuest)
    , m_pTabWidget(0)
    , m_pProcessControlWidget(0)
    , m_pFileManager(0)
    , m_fShowToolbar(fShowToolbar)
{
    prepare();
}

void UIGuestControlWidget::retranslateUi()
{
    if (m_pTabWidget)
    {
        m_pTabWidget->setTabText(TabIndex_FileManager, tr("File Manager"));
        m_pTabWidget->setTabText(TabIndex_ProcessControl, tr("Process Control"));
    }
}

void UIGuestControlWidget::prepare()
{
    QVBoxLayout* pMainLayout = new QVBoxLayout;
    if (!pMainLayout)
        return;
    setLayout(pMainLayout);
    m_pTabWidget = new QITabWidget;
    if (!m_pTabWidget)
        return;

    m_pFileManager = new UIGuestControlFileManager(this, m_comGuest);
    if (m_pFileManager)
        m_pTabWidget->insertTab(TabIndex_FileManager, m_pFileManager, "File Manager");

    m_pProcessControlWidget = new UIGuestProcessControlWidget(this, m_comGuest);
    if (m_pProcessControlWidget)
        m_pTabWidget->insertTab(TabIndex_ProcessControl, m_pProcessControlWidget, "Process Control");

    pMainLayout->addWidget(m_pTabWidget);
}
