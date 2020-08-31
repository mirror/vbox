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
class QIDialogButtonBox;

class SHARED_LIBRARY_STUFF UIMachineSettingsUSBFilterDetails : public QIWithRetranslateUI2<QIDialog>
{
    Q_OBJECT;

public:

    UIMachineSettingsUSBFilterDetails(QWidget *pParent = 0);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** @name Widgets
     * @{ */
        /** Holds the main layout instance. */
        QGridLayout       *m_pLayoutMain;
        /** Holds the name label instance. */
        QLabel            *m_pLabelName;
        /** Holds the name editor instance. */
        QLineEdit         *m_pEditorName;
        /** Holds the vendor ID label instance. */
        QLabel            *m_pLabelVendorID;
        /** Holds the vendor ID editor instance. */
        QLineEdit         *m_pEditorVendorID;
        /** Holds the product ID label instance. */
        QLabel            *m_pLabelProductID;
        /** Holds the product ID editor instance. */
        QLineEdit         *m_pEditorProductID;
        /** Holds the revision label instance. */
        QLabel            *m_pLabelRevision;
        /** Holds the revision editor instance. */
        QLineEdit         *m_pEditorRevision;
        /** Holds the manufacturer label instance. */
        QLabel            *m_pLabelManufacturer;
        /** Holds the manufacturer editor instance. */
        QLineEdit         *m_pEditorManufacturer;
        /** Holds the product label instance. */
        QLabel            *m_pLabelProduct;
        /** Holds the product editor instance. */
        QLineEdit         *m_pEditorProduct;
        /** Holds the serial NO label instance. */
        QLabel            *m_pLabelSerialNo;
        /** Holds the serial NO editor instance. */
        QLineEdit         *m_pEditorSerialNo;
        /** Holds the port label instance. */
        QLabel            *m_pLabelPort;
        /** Holds the port editor instance. */
        QLineEdit         *m_pEditorPort;
        /** Holds the remote label instance. */
        QLabel            *m_pLabelRemote;
        /** Holds the remote combo instance. */
        QComboBox         *m_pComboRemote;
        /** Holds the button-box instance. */
        QIDialogButtonBox *m_pButtonBox;
    /** @} */

    friend class UIMachineSettingsUSB;
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsUSBFilterDetails_h */
