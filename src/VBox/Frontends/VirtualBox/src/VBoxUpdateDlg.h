/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxUpdateDlg class declaration
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxUpdateDlg_h__
#define __VBoxUpdateDlg_h__

/* Global includes: */
#include <QDate>
#include <QUrl>

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "VBoxUpdateDlg.gen.h"

/* Forward declarations: */
class QNetworkAccessManager;

/**
 *  This structure is used to store retranslated reminder values.
 */
struct UpdateDay
{
    UpdateDay(const QString &strVal, const QString &strKey)
        : val(strVal), key(strKey) {}

    bool operator==(const UpdateDay &other)
    {
        return val == other.val || key == other.key;
    }

    QString val;
    QString key;
};

/**
 *  This class is used to encode/decode the registration data.
 */
class VBoxUpdateData
{
public:

    enum PeriodType
    {
        PeriodNever     = -2,
        PeriodUndefined = -1,
        Period1Day      =  0,
        Period2Days     =  1,
        Period3Days     =  2,
        Period4Days     =  3,
        Period5Days     =  4,
        Period6Days     =  5,
        Period1Week     =  6,
        Period2Weeks    =  7,
        Period3Weeks    =  8,
        Period1Month    =  9
    };

    enum BranchType
    {
        BranchStable     = 0,
        BranchAllRelease = 1,
        BranchWithBetas  = 2
    };

    static void populate();
    static QStringList list();

    VBoxUpdateData(const QString &strData);
    VBoxUpdateData(PeriodType periodIndex, BranchType branchIndex);

    bool isNecessary();
    bool isNoNeedToCheck();

    QString data() const;
    PeriodType periodIndex() const;
    QString date() const;
    BranchType branchIndex() const;
    QString branchName() const;

private:

    /* Private functions */
    void decode();
    void encode();

    /* Private variables */
    static QList <UpdateDay> m_dayList;

    QString m_strData;
    PeriodType m_periodIndex;
    QDate m_date;
    BranchType m_branchIndex;
};

class VBoxUpdateDlg : public QIWithRetranslateUI<QDialog>, public Ui::VBoxUpdateDlg
{
    Q_OBJECT;

public:

    static bool isNecessary();

    VBoxUpdateDlg(VBoxUpdateDlg **ppSelf, bool fForceRun, QWidget *pParent = 0);
    ~VBoxUpdateDlg();

signals:

    void sigDelayedAcception();

public slots:

    void search();

protected:

    void retranslateUi();
    void acceptLater() { emit sigDelayedAcception(); }

private slots:

    void accept();
    void sltHandleReply();

private:

    VBoxUpdateDlg         **m_ppSelf;
    QNetworkAccessManager  *m_pNetworkManager;
    QUrl                    m_url;
    bool                    m_fForceRun;
};

#endif // __VBoxUpdateDlg_h__

