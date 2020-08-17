/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsUSBFilterDetails class declaration.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsUSBFilterDetails_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsUSBFilterDetails_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "QIWithRetranslateUI.h"
#include "QIDialog.h"
#include "UIMachineSettingsUSB.h"

/* Forward declarations: */
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;

class SHARED_LIBRARY_STUFF UIMachineSettingsUSBFilterDetails : public QIWithRetranslateUI2<QIDialog>
{
    Q_OBJECT;

public:

    UIMachineSettingsUSBFilterDetails(QWidget *pParent = 0);

private:

    void prepareWidgets();
    void retranslateUi();

    /** @name Widgets
     * @{ */
       QComboBox *m_pComboBoxRemote;
       QLineEdit *m_pLineEditName;
       QLineEdit *m_pLineEditVendorID;
       QLineEdit *m_pLineEditProductID;
       QLineEdit *m_pLineEditRevision;
       QLineEdit *m_pLineEditPort;
       QLineEdit *m_pLineEditManufacturer;
       QLineEdit *m_pLineEditProduct;
       QLineEdit *m_pLineEditSerialNo;
       QLabel *m_pLabelPort;
       QLabel *m_pLabelRemote;
       QLabel *m_pLabelProductID;
       QLabel *m_pLabelName;
       QLabel *m_pLabelVendorID;
       QLabel *m_pLabelRevision;
       QLabel *m_pLabelManufacturer;
       QLabel *m_pLabelProduct;
       QLabel *m_pLabelSerialNo;
       QGridLayout *gridLayout;
    /** @} */

    friend class UIMachineSettingsUSB;
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsUSBFilterDetails_h */
