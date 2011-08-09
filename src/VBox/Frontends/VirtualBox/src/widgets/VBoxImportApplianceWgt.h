/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxImportApplianceWgt class declaration
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxImportApplianceWgt_h__
#define __VBoxImportApplianceWgt_h__

/* VBox includes */
#include "UIApplianceEditorWidget.h"

class VBoxImportApplianceWgt : public UIApplianceEditorWidget
{
    Q_OBJECT;

public:
    VBoxImportApplianceWgt (QWidget *aParent);

    bool setFile (const QString& aFile);
    void prepareImport();
    bool import();

    QList < QPair <QString, QString> > licenseAgreements() const;
};

#endif /* __VBoxImportApplianceWgt_h__ */

