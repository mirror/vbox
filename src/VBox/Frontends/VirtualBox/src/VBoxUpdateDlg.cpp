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
#include "QIHttp.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxUpdateDlg.h"

/* Qt includes */
#include <QTimer>

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
        return (x <  aOther.x) ||
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
QList <UpdateDay> VBoxUpdateData::mDayList = QList <UpdateDay>();

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
    return mIndex != NeverCheck && QDate::currentDate() >= mDate;
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

VBoxUpdateDlg::VBoxUpdateDlg (VBoxUpdateDlg **aSelf, bool aForceRun, QWidget *aParent)
    : QIWithRetranslateUI <QIAbstractWizard> (aParent)
    , mSelf (aSelf)
    , mUrl ("http://update.virtualbox.org/query.php")
    , mHttp (new QIHttp (this, mUrl.host()))
    , mForceRun (aForceRun)
{
    /* Store external pointer to this dialog. */
    *mSelf = this;

    /* Apply UI decorations */
    Ui::VBoxUpdateDlg::setupUi (this);

    /* Apply window icons */
    setWindowIcon (vboxGlobal().iconSetFull (QSize (32, 32), QSize (16, 16),
                                             ":/refresh_32px.png", ":/refresh_16px.png"));

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Setup other connections */
    connect (mBtnCheck, SIGNAL (clicked()), this, SLOT (search()));

    /* Initialize wizard hdr */
    initializeWizardFtr();

    /* Setup initial condition */
    mPbCheck->setMinimumWidth (mLogoUpdate->width() + mLogoUpdate->frameWidth() * 2);
    mPbCheck->hide();
    mTextSuccessInfo->hide();
    mTextFailureInfo->hide();
    mTextNotFoundInfo->hide();

    /* Retranslate string constants */
    retranslateUi();
}

VBoxUpdateDlg::~VBoxUpdateDlg()
{
    /* Erase dialog handle in config file. */
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateDlgWinID, QString::null);

    /* Erase external pointer to this dialog. */
    *mSelf = 0;
}

void VBoxUpdateDlg::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxUpdateDlg::retranslateUi (this);

#if 0 /* All the text constants */
    setWindowTitle (tr ("VirtualBox Update Wizard"));

    mPageUpdateHdr->setText (tr ("Check for Updates"));
    mBtnCheck->setText (tr ("Chec&k"));
    mBtnCancel->setText (tr ("Cancel"));

    mPageFinishHdr->setText (tr ("Summary"));
    mBtnFinish->setText (tr ("&Close"));

    mTextUpdateInfo->setText (tr ("<p>This wizard will connect to the VirtualBox "
                                  "web-site and check if a newer version of "
                                  "VirtualBox is available.</p><p>Use the "
                                  "<b>Check</b> button to check for a new version "
                                  "now or the <b>Cancel</b> button if you do not "
                                  "want to perform this check.</p><p>You can run "
                                  "this wizard at any time by choosing <b>Check "
                                  "for Updates...</b> from the <b>Help</b> menu.</p>"));

    mTextSuccessInfo->setText (tr ("<p>A new version of VirtualBox has been released! "
                                   "Version <b>%1</b> is available at "
                                   "<a href=\"http://www.virtualbox.org/\">virtualbox.org</a>.</p>"
                                   "<p>You can download this version from this direct link:</p>"
                                   "<p><a href=%2>%3</a></p>"));

    mTextFailureInfo->setText (tr ("<p>Unable to obtain the new version information "
                                   "due to the following network error:</p><p><b>%1</b></p>"));

    mTextNotFoundInfo->setText (tr ("You have already installed the latest VirtualBox "
                                    "version. Please repeat the version check later."));
#endif
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
    /* Calculate the count of checks left */
    int count = 1;
    QString sc = vboxGlobal().virtualBox().GetExtraData (VBoxDefs::GUI_UpdateCheckCount);
    if (!sc.isEmpty())
    {
        bool ok = false;
        int c = sc.toLongLong (&ok);
        if (ok) count = c;
    }

    /* Compose query */
    QUrl url (mUrl);
    url.addQueryItem ("platform", vboxGlobal().virtualBox().GetPackageType());
    url.addQueryItem ("version", QString ("%1_%2").arg (vboxGlobal().virtualBox().GetVersion())
                                                  .arg (vboxGlobal().virtualBox().GetRevision()));
    url.addQueryItem ("count", QString::number (count));

    QString userAgent (QString ("VirtualBox %1 <%2>")
                                .arg (vboxGlobal().virtualBox().GetVersion())
                                .arg (vboxGlobal().platformInfo()));

    /* Show progress bar */
    mPbCheck->show();

    /* Send composed information */
    QHttpRequestHeader header ("POST", url.toEncoded());
    header.setValue ("Host", url.host());
    header.setValue ("User-Agent", userAgent);
    mHttp->disconnect (this);
    connect (mHttp, SIGNAL (allIsDone (bool)), this, SLOT (searchResponse (bool)));
    mHttp->request (header);
}

void VBoxUpdateDlg::searchResponse (bool aError)
{
    /* Block all the other incoming signals */
    mHttp->disconnect (this);

    /* Process error if present */
    if (aError)
        return abortRequest (mHttp->errorString());

    /* Hide progress bar */
    mPbCheck->hide();

    /* Parse incoming data */
    QString responseData (mHttp->readAll());

    if (responseData.indexOf (QRegExp ("^\\d+\\.\\d+\\.\\d+ \\S+$")) == 0)
    {
        /* Newer version of necessary package found */
        QStringList response = responseData.split (" ", QString::SkipEmptyParts);

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
    else /* if (responseData == "UPTODATE") */
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

    /* Save left count of checks */
    int count = 1;
    bool ok = false;
    QString sc = vboxGlobal().virtualBox().GetExtraData (VBoxDefs::GUI_UpdateCheckCount);
    if (!sc.isEmpty())
        count = sc.toLongLong (&ok);
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateCheckCount,
                                            QString ("%1").arg ((qulonglong) count + 1));
}

void VBoxUpdateDlg::onPageShow()
{
    if (mPageStack->currentWidget() == mPageUpdate)
        mBtnCheck->setFocus();
    else
        mBtnFinish->setFocus();
}

void VBoxUpdateDlg::abortRequest (const QString &aReason)
{
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

