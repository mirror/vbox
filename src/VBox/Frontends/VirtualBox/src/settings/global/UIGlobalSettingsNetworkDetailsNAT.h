/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsNetworkDetailsNAT class declaration
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsNetworkDetailsNAT_h__
#define __UIGlobalSettingsNetworkDetailsNAT_h__

/* Local includes */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIGlobalSettingsNetworkDetailsNAT.gen.h"

/* Forward decalrations: */
struct UIDataNetworkNAT;

/* Global settings / Network page / Details sub-dialog: */
class UIGlobalSettingsNetworkDetailsNAT : public QIWithRetranslateUI2<QIDialog>, public Ui::UIGlobalSettingsNetworkDetailsNAT
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsNetworkDetailsNAT(QWidget *pParent, UIDataNetworkNAT &data);

protected:

    /* Handler: Translation stuff: */
    void retranslateUi();

    /* Handler: Polish event: */
    void polishEvent(QShowEvent *pEvent);

private slots:

    /* Handler: Port-forwarding stuff: */
    void sltEditPortForwarding();

    /* Handler: Dialog stuff: */
    void accept();

private:

    /* Helpers: Load/Save stuff: */
    void load();
    void save();

    /* Variable: External data reference: */
    UIDataNetworkNAT &m_data;
};

#endif /* __UIGlobalSettingsNetworkDetailsNAT_h__ */
