/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxUpdateDlg class declaration
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

#ifndef __VBoxUpdateDlg_h__
#define __VBoxUpdateDlg_h__

/* Common includes */
#include "QIAbstractWizard.h"
#include "VBoxUpdateDlg.gen.h"
#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QUrl>
#include <QDate>

class VBoxNetworkFramework;

/**
 *  This structure is used to store retranslated reminder values.
 */
struct UpdateDay
{
    UpdateDay (const QString &aVal, const QString &aKey)
        : val (aVal), key (aKey) {}

    QString val;
    QString key;

    bool operator== (const UpdateDay &aOther)
    {
        return val == aOther.val || key == aOther.key;
    }
};

/**
 *  This class is used to encode/decode the registration data.
 */
class VBoxUpdateData
{
public:

    enum
    {
        NeverCheck = -2,
        AutoCheck  = -3
    };

    static void populate();
    static QStringList list();

    VBoxUpdateData (const QString &aData);
    VBoxUpdateData (int aIndex);

    bool isNecessary();
    bool isAutomatic();

    QString data() const;
    int index() const;
    QString date() const;

private:

    /* Private functions */
    void decode (const QString &aData);
    void encode (int aIndex);

    /* Private variables */
    static QList<UpdateDay> mDayList;

    QString mData;
    int mIndex;
    QDate mDate;
};

class VBoxUpdateDlg : public QIWithRetranslateUI2<QIAbstractWizard>,
                      public Ui::VBoxUpdateDlg
{
    Q_OBJECT;

public:

    static bool isNecessary();
    static bool isAutomatic();

    VBoxUpdateDlg (VBoxUpdateDlg **aSelf, bool aForceRun,
                   QWidget *aParent = 0, Qt::WindowFlags aFlags = 0);
   ~VBoxUpdateDlg();

public slots:

    void accept();
    void search();

protected:

    void retranslateUi();

private slots:

    void processTimeout();

    void onNetBegin (int aStatus);
    void onNetData (const QByteArray &aData);
    void onNetEnd (const QByteArray &aData);
    void onNetError (const QString &aError);

    void onPageShow();

private:

    /* Private functions */
    void searchAbort (const QString &aReason);
    void searchComplete (const QString &aFullList);

    /* Private variables */
    VBoxUpdateDlg **mSelf;
    VBoxNetworkFramework *mNetfw;
    QTimer *mTimeout;
    QUrl mUrl;
    bool mForceRun;
    bool mSuicide;
};

#endif // __VBoxUpdateDlg_h__

