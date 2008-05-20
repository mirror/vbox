/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMInformationDlg class declaration
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

#ifndef __VBoxVMInformationDlg_h__
#define __VBoxVMInformationDlg_h__

#include <VBoxVMInformationDlg.gen.h>
#include <QIAbstractDialog.h>
#include <COMDefs.h>

class VBoxConsoleView;
class QTimer;

class VBoxVMInformationDlg : public QIAbstractDialog, public Ui::VBoxVMInformationDlg
{
    Q_OBJECT

public:

    typedef QMap<QString, QString> DataMapType;
    typedef QMap<QString, QStringList> LinksMapType;
    struct CounterElementType {QString type; DataMapType list;};
    typedef QMap <QString, VBoxVMInformationDlg*> InfoDlgMap;

    static void createInformationDlg (const CSession &aSession,
                                      VBoxConsoleView *aConsole);

    VBoxVMInformationDlg (VBoxConsoleView *aConsole, const CSession &aSession,
                          Qt::WindowFlags aFlags);
    ~VBoxVMInformationDlg();

protected:

    virtual void changeEvent (QEvent *aEvent);
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

    QString formatHardDisk (KStorageBus aBus, LONG aChannel, LONG aDevice, const QString &aBelongsTo);
    QString formatAdapter (ULONG aSlot, const QString &aBelongsTo);

    QString composeArticle (const QString &aBelongsTo);

    static InfoDlgMap  mSelfArray;

    bool               mIsPolished;
    VBoxConsoleView   *mConsole;
    CSession           mSession;
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

