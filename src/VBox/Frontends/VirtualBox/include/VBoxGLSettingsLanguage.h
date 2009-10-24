/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsLanguage class declaration
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

#ifndef __VBoxGLSettingsLanguage_h__
#define __VBoxGLSettingsLanguage_h__

#include "VBoxSettingsPage.h"
#include "VBoxGLSettingsLanguage.gen.h"

class VBoxGLSettingsLanguage : public VBoxSettingsPage,
                               public Ui::VBoxGLSettingsLanguage
{
    Q_OBJECT;

public:

    VBoxGLSettingsLanguage();

protected:

    void getFrom (const CSystemProperties &aProps,
                  const VBoxGlobalSettings &aGs);
    void putBackTo (CSystemProperties &aProps,
                    VBoxGlobalSettings &aGs);

    void setOrderAfter (QWidget *aWidget);

    void reload (const QString &aLangId);

    void retranslateUi();

private slots:

    void mTwItemPainted (QTreeWidgetItem *aItem, QPainter *aPainter);
    void mTwLanguageChanged (QTreeWidgetItem *aItem);

private:

    bool mLanguageChanged;
};

#endif // __VBoxGLSettingsLanguage_h__

