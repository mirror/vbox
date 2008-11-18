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

#include "QIAbstractWizard.h"
#include "QIWithRetranslateUI.h"
#include "VBoxUpdateDlg.gen.h"

/* Qt includes */
#include <QUrl>
#include <QDate>

class QIHttp;

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
        NeverCheck = -2
    };

    static void populate();
    static QStringList list();

    VBoxUpdateData (const QString &aData);
    VBoxUpdateData (int aIndex);

    bool isNecessary();
    bool isNeverCheck();

    QString data() const;
    int index() const;
    QString date() const;

private:

    /* Private functions */
    void decode (const QString &aData);
    void encode (int aIndex);

    /* Private variables */
    static QList <UpdateDay> mDayList;

    QString mData;
    int mIndex;
    QDate mDate;
};

class VBoxUpdateDlg : public QIWithRetranslateUI <QIAbstractWizard>,
                      public Ui::VBoxUpdateDlg
{
    Q_OBJECT;

public:

    static bool isNecessary();

    VBoxUpdateDlg (VBoxUpdateDlg **aSelf, bool aForceRun, QWidget *aParent = 0);
   ~VBoxUpdateDlg();

public slots:

    void search();

protected:

    void retranslateUi();

private slots:

    void accept();
    void searchResponse (bool aError);
    void onPageShow();

private:

    void abortRequest (const QString &aReason);

    /* Private variables */
    VBoxUpdateDlg **mSelf;
    QUrl            mUrl;
    QIHttp         *mHttp;
    bool            mForceRun;
};

#endif // __VBoxUpdateDlg_h__

