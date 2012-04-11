/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxOSTypeSelectorWidget class declaration
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxOSTypeSelectorWidget_h__
#define __VBoxOSTypeSelectorWidget_h__

#include "QIWithRetranslateUI.h"
#include "VBoxGlobal.h"

#include <QWidget>

class QComboBox;
class QLabel;

class VBoxOSTypeSelectorWidget : public QIWithRetranslateUI <QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(CGuestOSType type READ type WRITE setType);

public:

    VBoxOSTypeSelectorWidget (QWidget *aParent);

    void setType (const CGuestOSType &aType);
    CGuestOSType type() const;

    void setLayoutPosition (int aPos);
    void activateLayout();

signals:

    void osTypeChanged();

protected:

    void retranslateUi();
    void showEvent (QShowEvent *aEvent);

private slots:

    void onFamilyChanged (int aIndex);
    void onTypeChanged (int aIndex);

private:

    QLabel *mTxFamilyName;
    QLabel *mTxTypeName;
    QLabel *mPxTypeIcon;
    QComboBox *mCbFamily;
    QComboBox *mCbType;

    CGuestOSType mType;
    QMap <QString, QString> mCurrentIds;

    int mLayoutPosition;
    bool mLayoutActivated;

    bool m_fSupportsHWVirtEx;
    bool m_fSupportsLongMode;
};

#endif /* __VBoxOSTypeSelectorWidget_h__ */

