/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsNetworkDetails class declaration
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxGLSettingsNetworkDetails_h__
#define __VBoxGLSettingsNetworkDetails_h__

/* VBox includes */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "VBoxGLSettingsNetworkDetails.gen.h"

/* VBox forwards */
class NetworkItem;

class VBoxGLSettingsNetworkDetails : public QIWithRetranslateUI2 <QIDialog>,
                                     public Ui::VBoxGLSettingsNetworkDetails
{
    Q_OBJECT;

public:

    VBoxGLSettingsNetworkDetails (QWidget *aParent);

    void getFromItem (NetworkItem *aItem);
    void putBackToItem();

protected:

    void retranslateUi();

private slots:

    void dhcpClientStatusChanged();
    void dhcpServerStatusChanged();

private:

    NetworkItem *mItem;
};

#endif // __VBoxGLSettingsNetworkDetails_h__

