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

/* Time to auto-disconnect if no network answer received. */
static const int MaxWaitTime = 20000;


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
               (x == aOther.x && y <  aOther.y) ||
               (x == aOther.x && y == aOther.y && z <  aOther.z);
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
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("1 day"),  "1 d");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("2 days"), "2 d");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("3 days"), "3 d");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("4 days"), "4 d");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("5 days"), "5 d");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("6 days"), "6 d");

    /* Separately retranslate each week */
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("1 week"),  "1 w");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("2 weeks"), "2 w");
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("3 weeks"), "3 w");

    /* Separately retranslate each month */
    mDayList << UpdateDay (VBoxUpdateDlg::tr ("1 month"), "1 m");
}

QStringList VBoxUpdateData::list()
{
    QStringList result;
    for (int i = 0; i < mDayList.size(); ++ i)
        result << mDayList [i].val;
    return result;
}

VBoxUpdateData::VBoxUpdateData (const QString &aData)
    : mData (aData)
    , mIndex (-1)
{
    decode (mData);
}

VBoxUpdateData::VBoxUpdateData (int aIndex)
    : mIndex (aIndex)
{
    encode (mIndex);
}

bool VBoxUpdateData::isNecessary()
{
    return mIndex != NeverCheck &&
           QDate::currentDate() >= mDate;
}

bool VBoxUpdateData::isNeverCheck()
{
    return mIndex == NeverCheck;
}

QString VBoxUpdateData::data() const
{
    return mData;
}

int VBoxUpdateData::index() const
{
    return mIndex;
}

QString VBoxUpdateData::date() const
{
    return mIndex == NeverCheck ? VBoxUpdateDlg::tr ("Never") :
           mDate.toString (Qt::LocaleDate);
}

void VBoxUpdateData::decode (const QString &aData)
{
    /* Parse standard values */
    if (aData == "never")
        mIndex = NeverCheck;
    /* Parse other values */
    else
    {
        QStringList parser (aData.split (", ", QString::SkipEmptyParts));
        if (parser.size() == 2)
        {
            /* Parse 'remind' value */
            if (mDayList.isEmpty())
                populate();
            mIndex = mDayList.indexOf (UpdateDay (QString::null, parser [0]));

            /* Parse 'date' value */
            mDate = QDate::fromString (parser [1], Qt::ISODate);
        }

        /* Incorrect values handles as 'once per day' mode.
         * This is the default value if there is no such extra-data. */
        if (mIndex == -1 || !mDate.isValid())
        {
            mIndex = 0;
            mDate = QDate::currentDate();
        }
    }
}

void VBoxUpdateData::encode (int aIndex)
{
    /* Encode standard values */
    if (aIndex == NeverCheck)
        mData = "never";
    /* Encode other values */
    else
    {
        /* Encode 'remind' value */
        if (mDayList.isEmpty())
            populate();
        QString remindPeriod = mDayList [aIndex].key;

        /* Encode 'date' value */
        mDate = QDate::currentDate();
        QStringList parser (remindPeriod.split (' '));
        if (parser [1] == "d")
            mDate = mDate.addDays (parser [0].toInt());
        else if (parser [1] == "w")
            mDate = mDate.addDays (parser [0].toInt() * 7);
        else if (parser [1] == "m")
            mDate = mDate.addMonths (parser [0].toInt());
        QString remindDate = mDate.toString (Qt::ISODate);

        /* Composite mData */
        mData = QString ("%1, %2").arg (remindPeriod, remindDate);
    }
}


/* VBoxUpdateDlg stuff */
bool VBoxUpdateDlg::isNecessary()
{
    VBoxUpdateData data (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_UpdateDate));

    return data.isNecessary();
}

VBoxUpdateDlg::VBoxUpdateDlg (VBoxUpdateDlg **aSelf, bool aForceRun,
                              QWidget *aParent, Qt::WindowFlags aFlags)
    : QIWithRetranslateUI2<QIAbstractWizard> (aParent, aFlags)
    , mSelf (aSelf)
    , mNetfw (0)
    , mTimeout (new QTimer (this))
    , mUrl ("http://innotek.de/query.php")
    , mForceRun (aForceRun)
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
    connect (mBtnCheck, SIGNAL (clicked()), this, SLOT (search()));

    /* Initialize wizard hdr */
    initializeWizardFtr();

    /* Setup initial condition */
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
    /* Recalculate new update data */
    VBoxUpdateData oldData (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_UpdateDate));
    VBoxUpdateData newData (oldData.index());
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateDate,
                                            newData.data());

    QIAbstractWizard::accept();
}

void VBoxUpdateDlg::search()
{
    /* Create & setup network framework */
    mNetfw = new VBoxNetworkFramework();
    connect (mNetfw, SIGNAL (netBegin (int)),
             SLOT (onNetBegin (int)));
    connect (mNetfw, SIGNAL (netData (const QByteArray&)),
             SLOT (onNetData (const QByteArray&)));
    connect (mNetfw, SIGNAL (netEnd (const QByteArray&)),
             SLOT (onNetEnd (const QByteArray&)));
    connect (mNetfw, SIGNAL (netError (const QString&)),
             SLOT (onNetError (const QString&)));

    QString body = QString ("platform=%1")
                   .arg (vboxGlobal().virtualBox().GetPackageType());
    QStringList header ("User-Agent");
    header << QString ("VirtualBox %1 <%2>")
              .arg (vboxGlobal().virtualBox().GetVersion())
              .arg (vboxGlobal().platformInfo());

    /* Show progress bar & send composed information */
    mPbCheck->show();
    mTimeout->start (MaxWaitTime);
    mNetfw->postRequest (mUrl.host(), mUrl.path(), body, header);
}

void VBoxUpdateDlg::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxUpdateDlg::retranslateUi (this);
}

void VBoxUpdateDlg::processTimeout()
{
    networkAbort (tr ("Connection timed out."));
}

/* Handles the network request begining. */
void VBoxUpdateDlg::onNetBegin (int aStatus)
{
    if (mSuicide) return;

    if (aStatus == 404)
        networkAbort (tr ("Could not locate the latest version "
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
    processResponse (aTotalData);
}

/* Handles the network error. */
void VBoxUpdateDlg::onNetError (const QString &aError)
{
    networkAbort (aError);
}

void VBoxUpdateDlg::onPageShow()
{
    if (mPageStack->currentWidget() == mPageUpdate)
        mBtnCheck->setFocus();
    else
        mBtnFinish->setFocus();
}

void VBoxUpdateDlg::networkAbort (const QString &aReason)
{
    /* Protect against double kill request. */
    if (mSuicide) return;
    mSuicide = true;

    /* Hide progress bar */
    mPbCheck->hide();

    if (isHidden())
    {
        /* For background update */
        if (mForceRun)
            vboxProblem().showUpdateFailure (vboxGlobal().mainWindow(), aReason);
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

void VBoxUpdateDlg::processResponse (const QString &aResponse)
{
    /* Hide progress bar */
    mPbCheck->hide();

    if (aResponse.indexOf (QRegExp ("^\\d+\\.\\d+\\.\\d+ \\S+$")) == 0)
    {
        /* Newer version of necessary package found */
        QStringList response = aResponse.split (" ", QString::SkipEmptyParts);

        if (isHidden())
        {
            /* For background update */
            vboxProblem().showUpdateSuccess (vboxGlobal().mainWindow(),
                response [0], response [1]);
            QTimer::singleShot (0, this, SLOT (accept()));
        }
        else
        {
            /* For wizard update */
            mTextSuccessInfo->setText (mTextSuccessInfo->text()
                .arg (response [0], response [1], response [1]));
            mTextSuccessInfo->show();
            mPageStack->setCurrentIndex (1);
        }
    }
    else /* if (aResponse == "UPTODATE") */
    {
        /* No newer version of necessary package found */
        if (isHidden())
        {
            /* For background update */
            if (mForceRun)
                vboxProblem().showUpdateNotFound (vboxGlobal().mainWindow());
            QTimer::singleShot (0, this, SLOT (accept()));
        }
        else
        {
            /* For wizard update */
            mTextNotFoundInfo->show();
            mPageStack->setCurrentIndex (1);
        }
    }
}

