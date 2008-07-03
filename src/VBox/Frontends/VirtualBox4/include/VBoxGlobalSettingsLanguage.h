/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGlobalSettingsLanguage class declaration
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

#ifndef __VBoxGlobalSettingsLanguage_h__
#define __VBoxGlobalSettingsLanguage_h__

#include "VBoxGlobalSettingsLanguage.gen.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

class VBoxGlobalSettingsDlg;
class VBoxGlobalSettings;

class VBoxGlobalSettingsLanguage : public QIWithRetranslateUI<QWidget>,
                                   public Ui::VBoxGlobalSettingsLanguage
{
    Q_OBJECT;

public:

    static void getFrom (const CSystemProperties &aProps,
                         const VBoxGlobalSettings &aGs,
                         QWidget *aPage,
                         VBoxGlobalSettingsDlg *aDlg);
    static void putBackTo (CSystemProperties &aProps,
                           VBoxGlobalSettings &aGs);

protected:

    VBoxGlobalSettingsLanguage (QWidget *aParent);

    void load (const CSystemProperties &aProps,
               const VBoxGlobalSettings &aGs);
    void save (CSystemProperties &aProps,
               VBoxGlobalSettings &aGs);

    void reload (const QString &aLangId);

    void retranslateUi();

private slots:

    void mTwLanguageChanged (QTreeWidgetItem *aCurr, QTreeWidgetItem *aPrev);

private:

    static VBoxGlobalSettingsLanguage *mSettings;

    bool mLanguageChanged;
};

#endif // __VBoxGlobalSettingsLanguage_h__

