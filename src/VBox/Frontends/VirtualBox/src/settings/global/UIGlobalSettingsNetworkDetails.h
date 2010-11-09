/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsNetworkDetails class declaration
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

#ifndef __UIGlobalSettingsNetworkDetails_h__
#define __UIGlobalSettingsNetworkDetails_h__

/* VBox includes */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIGlobalSettingsNetworkDetails.gen.h"

/* VBox forwards */
class NetworkItem;

class UIGlobalSettingsNetworkDetails : public QIWithRetranslateUI2 <QIDialog>,
                                     public Ui::UIGlobalSettingsNetworkDetails
{
    Q_OBJECT;

public:

    UIGlobalSettingsNetworkDetails (QWidget *aParent);

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

#endif // __UIGlobalSettingsNetworkDetails_h__

