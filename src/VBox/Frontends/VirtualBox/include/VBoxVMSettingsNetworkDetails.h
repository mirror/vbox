/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsNetworkDetails class declaration
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxVMSettingsNetworkDetails_h__
#define __VBoxVMSettingsNetworkDetails_h__

/* VBox Includes */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "VBoxVMSettingsNetworkDetails.gen.h"
#include "COMDefs.h"

class VBoxVMSettingsNetworkDetails : public QIWithRetranslateUI2 <QIDialog>,
                                     public Ui::VBoxVMSettingsNetworkDetails
{
    Q_OBJECT;

public:

    VBoxVMSettingsNetworkDetails (QWidget *aParent);

    void getFromAdapter (const CNetworkAdapter &mAdapter);
    void putBackToAdapter();

    void loadList (KNetworkAttachmentType aType, const QStringList &aList);

    bool revalidate (KNetworkAttachmentType aType, QString &aWarning);

    QString currentName (KNetworkAttachmentType aType = KNetworkAttachmentType_Null) const;

protected:

    void retranslateUi();
    void showEvent (QShowEvent *aEvent);

private slots:

    void accept();
    void genMACClicked();

private:

    void populateComboboxes();
    void saveAlternative();
    QComboBox* comboBox() const;

    KNetworkAttachmentType mType;
    CNetworkAdapter mAdapter;
};

#endif // __VBoxVMSettingsNetworkDetails_h__

