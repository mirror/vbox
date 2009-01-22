/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsSFDetails class declaration
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef __VBoxVMSettingsSFDetails_h__
#define __VBoxVMSettingsSFDetails_h__

#include "VBoxVMSettingsSFDetails.gen.h"
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "VBoxVMSettingsSF.h"

class VBoxVMSettingsSFDetails : public QIWithRetranslateUI2<QIDialog>,
                                public Ui::VBoxVMSettingsSFDetails
{
    Q_OBJECT;

public:

    enum DialogType
    {
        AddType,
        EditType
    };

    VBoxVMSettingsSFDetails (DialogType aType,
                             bool aEnableSelector, /* for "permanent" checkbox */
                             const SFoldersNameList &aUsedNames,
                             QWidget *aParent = NULL);

    void setPath (const QString &aPath);
    QString path() const;

    void setName (const QString &aName);
    QString name() const;

    void setWriteable (bool aWriteable);
    bool isWriteable() const;

    void setPermanent (bool aPermanent);
    bool isPermanent() const;

protected:

    void retranslateUi();

private slots:

    void onSelectPath();
    void validate();

private:

    /* Private member vars */
    DialogType       mType;
    bool             mUsePermanent;
    SFoldersNameList mUsedNames;
};

#endif // __VBoxVMSettingsSFDetails_h__

