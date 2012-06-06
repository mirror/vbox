/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMInformationDlg class declaration
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxVMInformationDlg_h__
#define __VBoxVMInformationDlg_h__

/* Local includes: */
#include "VBoxVMInformationDlg.gen.h"
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"
#include "CSession.h"

class UIMachineWindow;
class QTimer;

class VBoxVMInformationDlg : public QIWithRetranslateUI2 <QIMainDialog>, public Ui::VBoxVMInformationDlg
{
    Q_OBJECT;

public:

    typedef QMap <QString, QString> DataMapType;
    typedef QMap <QString, QStringList> LinksMapType;
    struct CounterElementType { QString type; DataMapType list; };
    typedef QMap <QString, VBoxVMInformationDlg*> InfoDlgMap;

    static void createInformationDlg(UIMachineWindow *pMachineWindow);

protected:

    VBoxVMInformationDlg (UIMachineWindow *pMachineWindow, Qt::WindowFlags aFlags);
   ~VBoxVMInformationDlg();

    void retranslateUi();

    virtual bool event (QEvent *aEvent);
    virtual void resizeEvent (QResizeEvent *aEvent);
    virtual void showEvent (QShowEvent *aEvent);

private slots:

    void updateDetails();
    void processStatistics();
    void onPageChanged (int aIndex);

private:

    QString parseStatistics (const QString &aText);
    void refreshStatistics();

    QString formatValue (const QString &aValueName, const QString &aValue, int aMaxSize);
    QString formatMedium (const QString &aCtrName, LONG aPort, LONG aDevice, const QString &aBelongsTo);
    QString formatAdapter (ULONG aSlot, const QString &aBelongsTo);

    QString composeArticle (const QString &aBelongsTo, int aSpacesCount = 0);

    static InfoDlgMap  mSelfArray;

    CSession           mSession;
    bool               mIsPolished;
    QTimer            *mStatTimer;

    int                mWidth;
    int                mHeight;
    bool               mMax;

    DataMapType        mNamesMap;
    DataMapType        mValuesMap;
    DataMapType        mUnitsMap;
    LinksMapType       mLinksMap;
};

#endif // __VBoxVMInformationDlg_h__

