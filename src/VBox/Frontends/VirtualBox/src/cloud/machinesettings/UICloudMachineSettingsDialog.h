/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudMachineSettingsDialog class declaration.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_cloud_machinesettings_UICloudMachineSettingsDialog_h
#define FEQT_INCLUDED_SRC_cloud_machinesettings_UICloudMachineSettingsDialog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QDialog>
#include <QPointer>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UICloudMachineSettingsDialogPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudMachine.h"
#include "CForm.h"

/* Forward declarations: */
class QIDialogButtonBox;

/** Cloud machine settings dialog. */
class UICloudMachineSettingsDialog : public QIWithRetranslateUI<QDialog>
{
    Q_OBJECT;

public:

    /** Constructs @a comCloudMachine settings dialog passing @a pParent to the base-class. */
    UICloudMachineSettingsDialog(QWidget *pParent, const CCloudMachine &comCloudMachine);

public slots:

    /** Shows the dialog as a modal dialog, blocking until the user closes it. */
    virtual int exec() /* override */;

    /** Hides the modal dialog and sets the result code to Accepted. */
    virtual void accept() /* override */;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Sets Ok button to be @a fEnabled. */
    void setOkButtonEnabled(bool fEnabled);

    /** Performs dialog refresh. */
    void sltRefresh();

private:

    /** Prepares all. */
    void prepare();

    /** Holds the cloud machine object reference. */
    CCloudMachine  m_comCloudMachine;
    /** Holds the cloud machine settings form object reference. */
    CForm          m_comForm;
    /** Holds the cloud machine name. */
    QString        m_strName;

    /** Holds the cloud machine settings dialog page instance. */
    UISafePointerCloudMachineSettingsDialogPage  m_pPage;
    /** Holds the dialog button-box instance. */
    QIDialogButtonBox                           *m_pButtonBox;
};

/** Safe pointer to cloud machine settings dialog. */
typedef QPointer<UICloudMachineSettingsDialog> UISafePointerCloudMachineSettingsDialog;

#endif /* !FEQT_INCLUDED_SRC_cloud_machinesettings_UICloudMachineSettingsDialog_h */
