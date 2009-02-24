/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxImportApplianceWzd class declaration
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

#ifndef __VBoxImportApplianceWzd_h__
#define __VBoxImportApplianceWzd_h__

#include "VBoxImportApplianceWzd.gen.h"
#include "QIAbstractWizard.h"
#include "QIWithRetranslateUI.h"

class QIWidgetValidator;

class VBoxImportAppliance
{
public:
    static void import (QWidget *aParent = NULL);

    static int minGuestRAM() { return mMinGuestRAM; }
    static int maxGuestRAM() { return mMaxGuestRAM; }
    static int minGuestCPUCount() { return mMinGuestCPUCount; }
    static int maxGuestCPUCount() { return mMaxGuestCPUCount; }

private:
    /* Private member vars */
    static void initSystemSettings();

    static int mMinGuestRAM;
    static int mMaxGuestRAM;
    static int mMinGuestCPUCount;
    static int mMaxGuestCPUCount;
};

class VBoxImportApplianceWzd : public QIWithRetranslateUI<QIAbstractWizard>,
                               public Ui::VBoxImportApplianceWzd
{
    Q_OBJECT;

public:
    VBoxImportApplianceWzd (QWidget* aParent = NULL);

protected:
    void retranslateUi();

private slots:
    void revalidate (QIWidgetValidator *aWval);
    void enableNext (const QIWidgetValidator *aWval);

    void accept();

    void showNextPage();
    void onPageShow();

private:
    QIWidgetValidator *mWValFileSelector;
};

#endif /* __VBoxImportApplianceWzd_h__ */

