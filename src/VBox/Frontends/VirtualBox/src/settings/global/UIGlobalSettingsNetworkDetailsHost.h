/* $Id: $ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsNetworkDetailsHost class declaration.
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsNetworkDetailsHost_h__
#define __UIGlobalSettingsNetworkDetailsHost_h__

/* Local includes */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIGlobalSettingsNetworkDetailsHost.gen.h"

/* Forward decalrations: */
struct UIDataNetworkHost;

/* Global settings / Network page / Details sub-dialog: */
class UIGlobalSettingsNetworkDetailsHost : public QIWithRetranslateUI2<QIDialog>, public Ui::UIGlobalSettingsNetworkDetailsHost
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsNetworkDetailsHost(QWidget *pParent, UIDataNetworkHost &data);

protected:

    /* Handler: Translation stuff: */
    void retranslateUi();

private slots:

    /* Handlers: [Re]load stuff: */
    void sltDhcpClientStatusChanged() { loadClientStuff(); }
    void sltDhcpServerStatusChanged() { loadServerStuff(); }

    /* Handler: Dialog stuff: */
    void accept();

private:

    /* Helpers: Load/Save stuff: */
    void load();
    void loadClientStuff();
    void loadServerStuff();
    void save();

    /* Variable: External data reference: */
    UIDataNetworkHost &m_data;
};

#endif /* __UIGlobalSettingsNetworkDetailsHost_h__ */
