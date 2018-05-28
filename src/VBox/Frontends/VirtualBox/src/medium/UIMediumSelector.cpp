/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumSelector class implementation.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
# include <QFrame>
# include <QHBoxLayout>
# include <QHeaderView>
# include <QLabel>
# include <QMenuBar>
# include <QProgressBar>
# include <QPushButton>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "QIFileDialog.h"
# include "QILabel.h"
# include "QIMessageBox.h"
# include "QITabWidget.h"
# include "QITreeWidget.h"
# include "VBoxGlobal.h"
# include "UIExtraDataManager.h"
# include "UIMediumManager.h"
# include "UIMediumSelector.h"
# include "UIWizardCloneVD.h"
# include "UIMessageCenter.h"
# include "UIToolBar.h"
# include "UIIconPool.h"
# include "UIMedium.h"

/* COM includes: */
# include "COMEnums.h"
# include "CMachine.h"
# include "CMediumAttachment.h"
# include "CMediumFormat.h"
# include "CStorageController.h"
# include "CSystemProperties.h"

# ifdef VBOX_WS_MAC
#  include "UIWindowMenuManager.h"
# endif /* VBOX_WS_MAC */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMediumSelector::UIMediumSelector(KDeviceType deviceType, QWidget *pParent /* = 0 */, Qt::WindowFlags enmFlags /* = 0 */)
    :QIWithRetranslateUI<QIDialog>(pParent)
    , m_pMainLayout(0)
    , m_pTreeWidget(0)
{
    configure();
    finalize();
}

void UIMediumSelector::retranslateUi()
{
}

void UIMediumSelector::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/diskimage_32px.png", ":/diskimage_16px.png"));
    prepareWidgets();
}

void UIMediumSelector::prepareWidgets()
{
    m_pMainLayout = new QVBoxLayout;
    if (!m_pMainLayout)
        return;
    setLayout(m_pMainLayout);

    m_pTreeWidget = new QITreeWidget;
    if (m_pTreeWidget)
    {
        m_pMainLayout->addWidget(m_pTreeWidget);
    }
    //m_pMainLayout->addWidget(
}

void UIMediumSelector::finalize()
{
    /* Apply language settings: */
    retranslateUi();
}
