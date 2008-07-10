/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxUpdateDlg class implementation
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

/* Common includes */
#include "VBoxUpdateDlg.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxNetworkFramework.h"

/* Qt includes */
#include <QTimer>
#include <QDate>

/* Time to auto-disconnect if no network answer received. */
static const int MaxWaitTime = 20000;
/* Inner index number used as 'never' show identifier. */
static const int IndexNever = -1;
/* Inner index number used as 'auto' search identifier. */
static const int IndexAuto = -2;


/* VBoxVersion stuff */
class VBoxVersion
{
public:

    VBoxVersion (const QString &aVersion)
        : x (0), y (0), z (0)
    {
        QStringList versionStack = aVersion.split ('.');
        if (versionStack.size() > 0)
            x = versionStack [0].toInt();
        if (versionStack.size() > 1)
            y = versionStack [1].toInt();
        if (versionStack.size() > 2)
            z = versionStack [2].toInt();
    }

    bool operator< (const VBoxVersion &aOther) const
    {
        return x <  aOther.x ||
               x == aOther.x && y <  aOther.y ||
               x == aOther.x && y == aOther.y && z <  aOther.z;
    }

    QString toString() const
    {
        return QString ("%1.%2.%3").arg (x).arg (y).arg (z);
    }

private:

    int x;
    int y;
    int z;
};


/* VBoxUpdateData stuff */
QList<UpdateDay> VBoxUpdateData::mDayList = QList<UpdateDay>();

void VBoxUpdateData::populate()
{
    mDayList.clear();

    /* To avoid re-translation complexity all
     * have to be retranslated separately. */

    /* Separately retranslate each day */
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("in 1 day"),  "1 d");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("in 2 days"), "2 d");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("in 3 days"), "3 d");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("in 4 days"), "4 d");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("in 5 days"), "5 d");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("in 6 days"), "6 d");

    /* Separately retranslate each week */
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("in 1 week"),  "1 w");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("in 2 weeks"), "2 w");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("in 3 weeks"), "3 w");

    /* Separately retranslate each month */
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("in 1 month"), "1 m");

    /* Retranslate 'never' */
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("never"), "never");
}

QStringList VBoxUpdateData::list()
{
    QStringList result;
    for (int i = 0; i < mDayList.size(); ++ i)
        result << mDayList [i].val;
    return result;
}

VBoxUpdateData::VBoxUpdateData (const QString &aData, bool aEncode)
{
    if (aEncode)
    {
        if (aData.isNull())
            mIndex = IndexAuto;
        else
        {
            mIndex = mDayList.indexOf (UpdateDay (aData, QString::null));
            Assert (mIndex >= 0);
        }

        if (mIndex >= 0 && mDayList [mIndex].key == "never")
            mIndex = IndexNever;

        mData = encode (mIndex);
    }
    else
    {
        mData = aData;
        mIndex = decode (mData);
    }
}

bool VBoxUpdateData::isNecessary()
{
    return mIndex != IndexNever;
}

bool VBoxUpdateData::isAutomatic()
{
    return mIndex == IndexAuto;
}

int VBoxUpdateData::decode (const QString &aData) const
{
    int result = 0;

    if (aData == "auto")
        result = IndexAuto;
    else if (aData == "never")
        result = IndexNever;
    else
    {
        QDate date = QDate::fromString (aData);
        if (date.isValid() && QDate::currentDate() < date)
            result = IndexNever;
    }

    return result;
}

QString VBoxUpdateData::encode (int aIndex) const
{
    QString result;

    if (aIndex == IndexAuto)
        result = "auto";
    else if (aIndex == IndexNever)
        result = "never";
    else
    {
        QDate date = QDate::currentDate();
        QString remindTime = mDayList [aIndex].key;
        QStringList parser = remindTime.split (' ');
        if (parser [1] == "d")
            date = date.addDays (parser [0].toInt());
        else if (parser [1] == "w")
            date = date.addDays (parser [0].toInt() * 7);
        else if (parser [1] == "m")
            date = date.addMonths (parser [0].toInt());
        result = date.toString();
    }

    return result;
}


/* VBoxUpdateDlg stuff */
bool VBoxUpdateDlg::isNecessary()
{
    VBoxUpdateData data (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_UpdateDate), false);

    return data.isNecessary();
}

bool VBoxUpdateDlg::isAutomatic()
{
    VBoxUpdateData data (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_UpdateDate), false);

    return data.isAutomatic();
}

VBoxUpdateDlg::VBoxUpdateDlg (VBoxUpdateDlg **aSelf,
                              QWidget *aParent,
                              Qt::WindowFlags aFlags)
    : QIWithRetranslateUI2<QIAbstractWizard> (aParent, aFlags)
    , mSelf (aSelf)
    , mNetfw (0)
    , mTimeout (new QTimer (this))
    , mUrl ("http://www.virtualbox.org/download/latest_version")
    , mSuicide (false)
{
    /* Store external pointer to this dialog. */
    *mSelf = this;

    /* Apply UI decorations */
    Ui::VBoxUpdateDlg::setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Network outage - single shot timer */
    mTimeout->setSingleShot (true);

    /* Setup other connections */
    connect (mTimeout, SIGNAL (timeout()), this, SLOT (processTimeout()));
    connect (mRbCheck, SIGNAL (toggled (bool)), this, SLOT (onToggleFirstPage()));
    connect (mRbRemind, SIGNAL (toggled (bool)), this, SLOT (onToggleFirstPage()));
    connect (mRbAutoCheck, SIGNAL (toggled (bool)), this, SLOT (onToggleSecondPage()));
    connect (mRbRecheck, SIGNAL (toggled (bool)), this, SLOT (onToggleSecondPage()));

    /* Initialize wizard hdr */
    initializeWizardFtr();

    /* Setup initial condition */
    onToggleFirstPage();
    onToggleSecondPage();

    mPbCheck->setMinimumWidth (mLogoUpdate->width() +
                               mLogoUpdate->frameWidth() * 2);
    mPbCheck->hide();

    mTextSuccessInfo->hide();
    mTextFailureInfo->hide();
    mTextNotFoundInfo->hide();

    /* Retranslate string constants */
    retranslateUi();
}

VBoxUpdateDlg::~VBoxUpdateDlg()
{
    /* Delete network framework. */
    delete mNetfw;

    /* Erase dialog handle in config file. */
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateDlgWinID,
                                            QString::null);

    /* Erase external pointer to this dialog. */
    *mSelf = 0;
}

void VBoxUpdateDlg::accept()
{
    if (mPageStack->currentWidget() == mPageUpdate)
    {
        vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateDate,
            VBoxUpdateData (mCbRemindTime->currentText(), true).data());
    } else
    if (mPageStack->currentWidget() == mPageFinish)
    {
        if (mRbAutoCheck->isChecked())
            vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateDate,
                VBoxUpdateData (QString::null, true).data());
        else
            vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateDate,
                VBoxUpdateData (mCbRecheckTime->currentText(), true).data());
    } else
    Assert (0);

    QIAbstractWizard::accept();
}

void VBoxUpdateDlg::reject()
{
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateDate,
        VBoxUpdateData (VBoxUpdateData::list() [2], true).data());

    QIAbstractWizard::reject();
}

void VBoxUpdateDlg::search()
{
    /* Show progress bar */
    mPbCheck->show();

    /* Start downloading latest releases file */
    mNetfw = new VBoxNetworkFramework();
    connect (mNetfw, SIGNAL (netBegin (int)),
             SLOT (onNetBegin (int)));
    connect (mNetfw, SIGNAL (netData (const QByteArray&)),
             SLOT (onNetData (const QByteArray&)));
    connect (mNetfw, SIGNAL (netEnd (const QByteArray&)),
             SLOT (onNetEnd (const QByteArray&)));
    connect (mNetfw, SIGNAL (netError (const QString&)),
             SLOT (onNetError (const QString&)));
    mTimeout->start (MaxWaitTime);
    mNetfw->postRequest (mUrl.host(), mUrl.path());
}

void VBoxUpdateDlg::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxUpdateDlg::retranslateUi (this);

    int ci1 = mCbRemindTime->currentIndex();
    int ci2 = mCbRecheckTime->currentIndex();

    mCbRemindTime->clear();
    mCbRecheckTime->clear();

    VBoxUpdateData::populate();
    mCbRemindTime->insertItems (0, VBoxUpdateData::list());
    mCbRecheckTime->insertItems (0, VBoxUpdateData::list());

    mCbRemindTime->setCurrentIndex (ci1 == -1 ? 0 : ci1);
    mCbRecheckTime->setCurrentIndex (ci2 == -1 ? 0 : ci2);
}

void VBoxUpdateDlg::processTimeout()
{
    searchAbort (tr ("Connection timed out."));
}

void VBoxUpdateDlg::onToggleFirstPage()
{
    disconnect (mBtnConfirm, SIGNAL (clicked()), 0, 0);
    if (mRbCheck->isChecked())
        connect (mBtnConfirm, SIGNAL (clicked()), this, SLOT (search()));
    else
        connect (mBtnConfirm, SIGNAL (clicked()), this, SLOT (accept()));

    mCbRemindTime->setEnabled (mRbRemind->isChecked());
}

void VBoxUpdateDlg::onToggleSecondPage()
{
    mCbRecheckTime->setEnabled (mRbRecheck->isChecked());
}

/* Handles the network request begining. */
void VBoxUpdateDlg::onNetBegin (int aStatus)
{
    if (mSuicide) return;

    if (aStatus == 404)
        searchAbort (tr ("Could not locate the latest version "
            "list on the server (response: %1).").arg (aStatus));
    else
        mTimeout->start (MaxWaitTime);
}

/* Handles the network request data incoming. */
void VBoxUpdateDlg::onNetData (const QByteArray&)
{
    if (mSuicide) return;

    mTimeout->start (MaxWaitTime);
}

/* Handles the network request end. */
void VBoxUpdateDlg::onNetEnd (const QByteArray &aTotalData)
{
    if (mSuicide) return;

    mTimeout->stop();
    searchComplete (aTotalData);
}

/* Handles the network error. */
void VBoxUpdateDlg::onNetError (const QString &aError)
{
    searchAbort (aError);
}

void VBoxUpdateDlg::onPageShow()
{
    if (mPageStack->currentWidget() == mPageUpdate)
        mRbCheck->isChecked() ? mRbCheck->setFocus() :
                                mRbRemind->setFocus();
    else
        mRbAutoCheck->isChecked() ? mRbAutoCheck->setFocus() :
                                    mRbRecheck->setFocus();
}

void VBoxUpdateDlg::searchAbort (const QString &aReason)
{
    /* Protect against double kill request. */
    if (mSuicide) return;
    mSuicide = true;

    /* Hide progress bar */
    mPbCheck->hide();

    if (isHidden())
    {
        /* For background update */
        mPageStack->setCurrentIndex (1);
        QTimer::singleShot (0, this, SLOT (accept()));
    }
    else
    {
        /* For wizard update */
        mTextFailureInfo->setText (mTextFailureInfo->text().arg (aReason));
        mTextFailureInfo->show();
        mPageStack->setCurrentIndex (1);
    }
}

void VBoxUpdateDlg::searchComplete (const QString &aFullList)
{
    /* Hide progress bar */
    mPbCheck->hide();

    QStringList list = aFullList.split ("\n", QString::SkipEmptyParts);

//  gcc-3.2 cannot grok this expression for some reason
//  QString latestVersion (QString (list [0]).remove (QRegExp ("Version: ")));
    QString latestVersion = ((QString)(list [0])).remove (QRegExp ("Version: "));
    QString currentVersion (vboxGlobal().virtualBox().GetVersion());
    VBoxVersion cv (currentVersion);
    VBoxVersion lv (latestVersion);

    if (cv < lv)
    {
        /* Search for the current platform link */
        QString packageType = vboxGlobal().virtualBox().GetPackageType();
        for (int i = 1; i < list.size(); ++ i)
        {
            QStringList platformInfo = list [i].split (": ");
            if (packageType == platformInfo [0])
            {
                /* If newer version of necessary package found */
                if (isHidden())
                {
                    /* For background update */
                    vboxProblem().showUpdateResult (this,
                        lv.toString(), platformInfo [1]);
                    mPageStack->setCurrentIndex (1);
                    QTimer::singleShot (0, this, SLOT (accept()));
                }
                else
                {
                    /* For wizard update */
                    mTextSuccessInfo->setText (mTextSuccessInfo->text()
                        .arg (lv.toString(), platformInfo [1], platformInfo [1]));
                    mTextSuccessInfo->show();
                    mPageStack->setCurrentIndex (1);
                }
                return;
            }
        }
    }

    /* If no newer version of necessary package found */
    if (isHidden())
    {
        /* For background update */
        mPageStack->setCurrentIndex (1);
        QTimer::singleShot (0, this, SLOT (accept()));
    }
    else
    {
        /* For wizard update */
        mTextNotFoundInfo->show();
        mPageStack->setCurrentIndex (1);
    }
}

