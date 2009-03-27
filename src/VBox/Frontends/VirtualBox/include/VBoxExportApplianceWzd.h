/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxExportApplianceWzd class declaration
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

#ifndef __VBoxExportApplianceWzd_h__
#define __VBoxExportApplianceWzd_h__

#include "VBoxExportApplianceWzd.gen.h"
#include "QIAbstractWizard.h"
#include "QIWithRetranslateUI.h"

class QIWidgetValidator;
class CAppliance;

class VBoxExportApplianceWzd : public QIWithRetranslateUI<QIAbstractWizard>,
                               public Ui::VBoxExportApplianceWzd
{
    Q_OBJECT;

public:
    VBoxExportApplianceWzd (QWidget* aParent = NULL, const QString& aSelectName = QString::null);

protected:
    void retranslateUi();

private slots:
    void revalidate (QIWidgetValidator *aWval);
    void enableNext (const QIWidgetValidator *aWval);

    void accept();

    void showNextPage();
    void onPageShow();

private:
    void addListViewVMItems (const QString& aSelectName);
    bool prepareSettingsWidget();
    bool exportVMs (CAppliance &aAppliance);

    /* Private member vars */
    QString mDefaultApplianceName;

    QIWidgetValidator *mWValVMSelector;
    QIWidgetValidator *mWValFileSelector;
};

#endif /* __VBoxExportApplianceWzd_h__ */

