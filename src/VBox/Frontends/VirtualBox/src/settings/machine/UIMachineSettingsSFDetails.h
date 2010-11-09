/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsSFDetails class declaration
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineSettingsSFDetails_h__
#define __UIMachineSettingsSFDetails_h__

#include "UIMachineSettingsSFDetails.gen.h"
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIMachineSettingsSF.h"

class UIMachineSettingsSFDetails : public QIWithRetranslateUI2<QIDialog>,
                                public Ui::UIMachineSettingsSFDetails
{
    Q_OBJECT;

public:

    enum DialogType
    {
        AddType,
        EditType
    };

    UIMachineSettingsSFDetails (DialogType aType,
                             bool aEnableSelector, /* for "permanent" checkbox */
                             const SFoldersNameList &aUsedNames,
                             QWidget *aParent = NULL);

    void setPath (const QString &aPath);
    QString path() const;

    void setName (const QString &aName);
    QString name() const;

    void setWriteable (bool aWriteable);
    bool isWriteable() const;

    void setAutoMount (bool aAutoMount);
    bool isAutoMounted() const;

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

#endif // __UIMachineSettingsSFDetails_h__

