/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxImportApplianceWgt class declaration
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

#ifndef __VBoxImportApplianceWgt_h__
#define __VBoxImportApplianceWgt_h__

#include "VBoxImportApplianceWgt.gen.h"
#include "QIWithRetranslateUI.h"

class CAppliance;
class VirtualSystemModel;

class VBoxImportApplianceWgt : public QIWithRetranslateUI<QWidget>,
                               public Ui::VBoxImportApplianceWgt
{
    Q_OBJECT;

public:
    VBoxImportApplianceWgt (QWidget *aParent);

    bool setFile (const QString& aFile);
    void prepareImport();
    bool import();

    bool isValid() const { return mAppliance != NULL; }
    QList < QPair <QString, QString> > licenseAgreements() const;

    static int minGuestRAM() { return mMinGuestRAM; }
    static int maxGuestRAM() { return mMaxGuestRAM; }
    static int minGuestCPUCount() { return mMinGuestCPUCount; }
    static int maxGuestCPUCount() { return mMaxGuestCPUCount; }

public slots:
    void restoreDefaults();

protected:
    void retranslateUi();

private:
    static void initSystemSettings();

    /* Private member vars */
    CAppliance *mAppliance;
    VirtualSystemModel *mModel;

    static int mMinGuestRAM;
    static int mMaxGuestRAM;
    static int mMinGuestCPUCount;
    static int mMaxGuestCPUCount;
};

#endif /* __VBoxImportApplianceWgt_h__ */

