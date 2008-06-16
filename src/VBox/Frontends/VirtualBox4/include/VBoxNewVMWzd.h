/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxNewVMWzd class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef __VBoxNewVMWzd_h__
#define __VBoxNewVMWzd_h__

#include "QIAbstractWizard.h"
#include "VBoxNewVMWzd.gen.h"
#include "COMDefs.h"
#include "QIWidgetValidator.h"
#include "QIWithRetranslateUI.h"

class VBoxNewVMWzd : public QIWithRetranslateUI<QIAbstractWizard>,
                     public Ui::VBoxNewVMWzd
{
    Q_OBJECT

public:

    VBoxNewVMWzd (QWidget *aParent = 0);
   ~VBoxNewVMWzd();

    const CMachine& machine() const;

protected:

    void retranslateUi();

private slots:

    void accept();
    void showVDIManager();
    void showNewVDIWizard();
    void slRAMValueChanged (int aValue);
    void leRAMTextChanged (const QString &aTtext);
    void cbOSActivated (int aItem);
    void currentMediaChanged (int aItem);
    void revalidate (QIWidgetValidator *aWval);
    void enableNext (const QIWidgetValidator *aWval);
    void onPageShow();
    void showNextPage();

private:

    bool constructMachine();
    void ensureNewHardDiskDeleted();

    QIWidgetValidator *mWvalNameAndOS;
    QIWidgetValidator *mWvalMemory;
    QIWidgetValidator *mWvalHDD;
    QUuid              uuidHD;
    CHardDisk          chd;
    CMachine           cmachine;
};

#endif // __VBoxNewVMWzd_h__

