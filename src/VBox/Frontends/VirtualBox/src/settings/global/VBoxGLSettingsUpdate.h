/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsUpdate class declaration
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

#ifndef __VBoxGLSettingsUpdate_h__
#define __VBoxGLSettingsUpdate_h__

#include "VBoxSettingsPage.h"
#include "VBoxGLSettingsUpdate.gen.h"
#include "VBoxUpdateDlg.h"

class VBoxGLSettingsUpdate : public VBoxSettingsPage,
                             public Ui::VBoxGLSettingsUpdate
{
    Q_OBJECT;

public:

    VBoxGLSettingsUpdate();

protected:

    void getFrom (const CSystemProperties &aProps,
                  const VBoxGlobalSettings &aGs);
    void putBackTo (CSystemProperties &aProps,
                    VBoxGlobalSettings &aGs);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private slots:

    void toggleUpdater (bool aOn);
    void activatedPeriod (int aIndex);
    void toggledBranch();

private:

    void showEvent (QShowEvent *aEvent);

    VBoxUpdateData::PeriodType periodType() const;
    VBoxUpdateData::BranchType branchType() const;

    bool mSettingsChanged;
    QRadioButton *mLastChosen;
};

#endif // __VBoxGLSettingsUpdate_h__

