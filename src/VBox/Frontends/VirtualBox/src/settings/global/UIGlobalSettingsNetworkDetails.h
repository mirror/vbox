/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsNetworkDetails class declaration
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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

/* Local includes */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIGlobalSettingsNetworkDetails.gen.h"

/* Forward decalrations: */
class UIHostInterfaceItem;

/* Global settings / Network page / Details sub-dialog: */
class UIGlobalSettingsNetworkDetails : public QIWithRetranslateUI2<QIDialog>, public Ui::UIGlobalSettingsNetworkDetails
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsNetworkDetails(QWidget *pParent);

    /* Get/return data to/from details sub-dialog: */
    void getFromItem(UIHostInterfaceItem *aItem);
    void putBackToItem();

protected:

    /* Validation stuff: */
    void retranslateUi();

private slots:

    /* Various helper slots: */
    void sltDhcpClientStatusChanged();
    void sltDhcpServerStatusChanged();

private:

    UIHostInterfaceItem *m_pItem;
};

#endif // __UIGlobalSettingsNetworkDetails_h__

