/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationConfiguration class declaration.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_runtime_information_UIInformationConfiguration_h
#define FEQT_INCLUDED_SRC_runtime_information_UIInformationConfiguration_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CGuest.h"
#include "CMachine.h"
#include "CConsole.h"

/* GUI includes: */
#include "UITextTable.h"
#include "UIInformationWidget.h"

/* Forward declarations: */
class QTableWidgetItem;
class QTextDocument;


/** QWidget extension
  * providing GUI with configuration-information tab in session-information window. */
class UIInformationConfiguration : public UIInformationWidget
{
    Q_OBJECT;

public:

    /** Constructs information-tab passing @a pParent to the QWidget base-class constructor.
      * @param machine is machine reference.
      * @param console is machine console reference. */
    UIInformationConfiguration(QWidget *pParent, const CMachine &machine, const CConsole &console);

protected:

    void retranslateUi() /* override */;

private slots:

    void sltMachineDataChanged();

private:

    void createTableItems();

   /** @name Cached translated string.
      * @{ */
        QString m_strGeneralTitle;
        QString m_strSystemTitle;
        QString m_strDisplayTitle;
        QString m_strStorageTitle;
        QString m_strAudioTitle;
        QString m_strNetworkTitle;
        QString m_strSerialPortsTitle;
        QString m_strUSBTitle;
        QString m_strSharedFoldersTitle;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_runtime_information_UIInformationConfiguration_h */
