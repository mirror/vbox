/* $Id$ */
/** @file
 * VBox Qt GUI - UIFDCreationDialog class implementation.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
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

/* Qt includes */
# include<QVBoxLayout>

/* Other includes */
# include "UIFDCreationDialog.h"
# include "UIFilePathSelector.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIFDCreationDialog::UIFDCreationDialog(QWidget *pParent /* = 0 */,
                                       const QString &strMachineName /* = QString() */,
                                       const QString &strMachineSettingsFilePath /* = QString() */)
   : QIWithRetranslateUI<QDialog>(pParent)
    , m_pFilePathselector(0)
    , m_strMachineName(strMachineName)
    , m_strMachineSettingsFilePath(strMachineSettingsFilePath)
{

    prepare();
    /* Adjust dialog size: */
    adjustSize();

#ifdef VBOX_WS_MAC
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(minimumSize());
#endif /* VBOX_WS_MAC */
}

void UIFDCreationDialog::retranslateUi()
{

}

void UIFDCreationDialog::prepare()
{
    setWindowModality(Qt::WindowModal);

    QVBoxLayout *pMainLayout = new QVBoxLayout;
    if (!pMainLayout)
        return;
    setLayout(pMainLayout);

    m_pFilePathselector = new UIFilePathSelector;
    if (m_pFilePathselector)
    {
        pMainLayout->addWidget(m_pFilePathselector);
        m_pFilePathselector->setMode(UIFilePathSelector::Mode_File_Save);
    }
}
