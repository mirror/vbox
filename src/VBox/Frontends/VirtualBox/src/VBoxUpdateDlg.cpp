/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxUpdateDlg class implementation
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
#include "precomp.h"
#else /* !VBOX_WITH_PRECOMPILED_HEADERS */
/* Global includes */
#include <QNetworkAccessManager>
#include <QNetworkReply>
/* Local includes */
#include "VBoxUpdateDlg.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "UIIconPool.h"
#include "VBoxUtils.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/**
 *  This class is used to store VBox version data.
 */
class VBoxVersion
{
public:

    VBoxVersion(const QString &strVersion)
        : x(0), y(0), z(0)
    {
        QStringList versionStack = strVersion.split('.');
        if (versionStack.size() > 0)
            x = versionStack[0].toInt();
        if (versionStack.size() > 1)
            y = versionStack[1].toInt();
        if (versionStack.size() > 2)
            z = versionStack[2].toInt();
    }

    bool operator<(const VBoxVersion &other) const
    {
        return (x <  other.x) ||
               (x == other.x && y <  other.y) ||
               (x == other.x && y == other.y && z <  other.z);
    }

    QString toString() const
    {
        return QString("%1.%2.%3").arg(x).arg(y).arg(z);
    }

private:

    int x;
    int y;
    int z;
};

/* VBoxUpdateData stuff: */
QList<UpdateDay> VBoxUpdateData::m_dayList = QList<UpdateDay>();

void VBoxUpdateData::populate()
{
    m_dayList.clear();

    /* To avoid re-translation complexity all
     * have to be retranslated separately: */

    /* Separately retranslate each day: */
    m_dayList << UpdateDay(VBoxUpdateDlg::tr("1 day"),  "1 d");
    m_dayList << UpdateDay(VBoxUpdateDlg::tr("2 days"), "2 d");
    m_dayList << UpdateDay(VBoxUpdateDlg::tr("3 days"), "3 d");
    m_dayList << UpdateDay(VBoxUpdateDlg::tr("4 days"), "4 d");
    m_dayList << UpdateDay(VBoxUpdateDlg::tr("5 days"), "5 d");
    m_dayList << UpdateDay(VBoxUpdateDlg::tr("6 days"), "6 d");

    /* Separately retranslate each week */
    m_dayList << UpdateDay(VBoxUpdateDlg::tr("1 week"),  "1 w");
    m_dayList << UpdateDay(VBoxUpdateDlg::tr("2 weeks"), "2 w");
    m_dayList << UpdateDay(VBoxUpdateDlg::tr("3 weeks"), "3 w");

    /* Separately retranslate each month */
    m_dayList << UpdateDay(VBoxUpdateDlg::tr("1 month"), "1 m");
}

QStringList VBoxUpdateData::list()
{
    QStringList result;
    for (int i = 0; i < m_dayList.size(); ++i)
        result << m_dayList[i].val;
    return result;
}

VBoxUpdateData::VBoxUpdateData(const QString &strData)
    : m_strData(strData)
    , m_periodIndex(Period1Day)
    , m_branchIndex(BranchStable)
{
    decode();
}

VBoxUpdateData::VBoxUpdateData(PeriodType periodIndex, BranchType branchIndex)
    : m_strData(QString())
    , m_periodIndex(periodIndex)
    , m_branchIndex(branchIndex)
{
    encode();
}

bool VBoxUpdateData::isNecessary()
{
    return m_periodIndex != PeriodNever && QDate::currentDate() >= m_date;
}

bool VBoxUpdateData::isNoNeedToCheck()
{
    return m_periodIndex == PeriodNever;
}

QString VBoxUpdateData::data() const
{
    return m_strData;
}

VBoxUpdateData::PeriodType VBoxUpdateData::periodIndex() const
{
    return m_periodIndex;
}

QString VBoxUpdateData::date() const
{
    return m_periodIndex == PeriodNever ? VBoxUpdateDlg::tr("Never")
                                        : m_date.toString(Qt::LocaleDate);
}

VBoxUpdateData::BranchType VBoxUpdateData::branchIndex() const
{
    return m_branchIndex;
}

QString VBoxUpdateData::branchName() const
{
    switch (m_branchIndex)
    {
        case BranchStable:
            return "stable";
        case BranchAllRelease:
            return "allrelease";
        case BranchWithBetas:
            return "withbetas";
    }
    return QString();
}

void VBoxUpdateData::decode()
{
    /* Parse standard values: */
    if (m_strData == "never")
        m_periodIndex = PeriodNever;
    /* Parse other values: */
    else
    {
        QStringList parser(m_strData.split(", ", QString::SkipEmptyParts));

        /* Parse 'period' value: */
        if (parser.size() > 0)
        {
            if (m_dayList.isEmpty())
                populate();
            PeriodType index = (PeriodType)m_dayList.indexOf(UpdateDay(QString(), parser[0]));
            m_periodIndex = index == PeriodUndefined ? Period1Day : index;
        }

        /* Parse 'date' value: */
        if (parser.size() > 1)
        {
            QDate date = QDate::fromString(parser[1], Qt::ISODate);
            m_date = date.isValid() ? date : QDate::currentDate();
        }

        /* Parse 'branch' value: */
        if (parser.size() > 2)
        {
            QString branch(parser[2]);
            m_branchIndex = branch == "withbetas" ? BranchWithBetas :
                            branch == "allrelease" ? BranchAllRelease : BranchStable;
        }
    }
}

void VBoxUpdateData::encode()
{
    /* Encode standard values: */
    if (m_periodIndex == PeriodNever)
        m_strData = "never";
    /* Encode other values: */
    else
    {
        /* Encode 'period' value: */
        if (m_dayList.isEmpty())
            populate();
        QString remindPeriod = m_dayList[m_periodIndex].key;

        /* Encode 'date' value: */
        m_date = QDate::currentDate();
        QStringList parser(remindPeriod.split(' '));
        if (parser[1] == "d")
            m_date = m_date.addDays(parser[0].toInt());
        else if (parser[1] == "w")
            m_date = m_date.addDays(parser[0].toInt() * 7);
        else if (parser[1] == "m")
            m_date = m_date.addMonths(parser[0].toInt());
        QString remindDate = m_date.toString(Qt::ISODate);

        /* Encode 'branch' value: */
        QString branchValue = m_branchIndex == BranchWithBetas ? "withbetas" :
                              m_branchIndex == BranchAllRelease ? "allrelease" : "stable";

        /* Composite m_strData: */
        m_strData = QString("%1, %2, %3").arg(remindPeriod, remindDate, branchValue);
    }
}

/* VBoxUpdateDlg stuff: */
bool VBoxUpdateDlg::isNecessary()
{
    VBoxUpdateData data(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_UpdateDate));
    return data.isNecessary();
}

VBoxUpdateDlg::VBoxUpdateDlg(VBoxUpdateDlg **ppSelf, bool fForceRun, QWidget *pParent)
    : QIWithRetranslateUI<QDialog>(pParent)
    , m_ppSelf(ppSelf)
    , m_pNetworkManager(new QNetworkAccessManager(this))
    , m_url("http://update.virtualbox.org/query.php")
    , m_fForceRun(fForceRun)
{
    /* Store external pointer to this dialog: */
    *m_ppSelf = this;

    /* Apply UI decorations: */
    Ui::VBoxUpdateDlg::setupUi(this);

    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(QSize(32, 32), QSize(16, 16),
                                          ":/refresh_32px.png", ":/refresh_16px.png"));

    /* Setup other connections: */
    connect(mBtnCheck, SIGNAL(clicked()), this, SLOT(search()));
    connect(mBtnFinish, SIGNAL(clicked()), this, SLOT(accept()));
    connect(this, SIGNAL(sigDelayedAcception()), this, SLOT(accept()), Qt::QueuedConnection);

    /* Setup initial condition: */
    mPbCheck->setMinimumWidth(mLogoUpdate->width() + mLogoUpdate->frameWidth() * 2);
    mPbCheck->hide();
    mTextSuccessInfo->hide();
    mTextFailureInfo->hide();
    mTextNotFoundInfo->hide();

    /* Retranslate string constants: */
    retranslateUi();
}

VBoxUpdateDlg::~VBoxUpdateDlg()
{
    /* Erase dialog handle in config file: */
    vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_UpdateDlgWinID, QString());

    /* Erase external pointer to this dialog: */
    *m_ppSelf = 0;
}

void VBoxUpdateDlg::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::VBoxUpdateDlg::retranslateUi(this);

    /* For wizard update: */
    if (!isHidden())
    {
        setWindowTitle(tr("VirtualBox Update Wizard"));

        mPageUpdateHdr->setText(tr("Check for Updates"));
        mBtnCheck->setText(tr("Chec&k"));
        mBtnCancel->setText(tr("Cancel"));

        mPageFinishHdr->setText(tr("Summary"));
        mBtnFinish->setText(tr("&Close"));

        mTextUpdateInfo->setText(tr("<p>This wizard will connect to the VirtualBox "
                                    "web-site and check if a newer version of "
                                    "VirtualBox is available.</p><p>Use the "
                                    "<b>Check</b> button to check for a new version "
                                    "now or the <b>Cancel</b> button if you do not "
                                    "want to perform this check.</p><p>You can run "
                                    "this wizard at any time by choosing <b>Check "
                                    "for Updates...</b> from the <b>Help</b> menu.</p>"));

        mTextSuccessInfo->setText(tr("<p>A new version of VirtualBox has been released! "
                                     "Version <b>%1</b> is available at "
                                     "<a href=\"http://www.virtualbox.org/\">virtualbox.org</a>.</p>"
                                     "<p>You can download this version using the link:</p>"
                                     "<p><a href=%2>%3</a></p>"));

        mTextFailureInfo->setText(tr("<p>Unable to obtain the new version information "
                                     "due to the following network error:</p><p><b>%1</b></p>"));

        mTextNotFoundInfo->setText(tr("You are already running the most recent version of VirtualBox."));
    }
}

void VBoxUpdateDlg::accept()
{
    /* Recalculate new update data: */
    VBoxUpdateData oldData(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_UpdateDate));
    VBoxUpdateData newData(oldData.periodIndex(), oldData.branchIndex());
    vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_UpdateDate, newData.data());
    /* Call to base-class: */
    QDialog::accept();
}

void VBoxUpdateDlg::search()
{
    /* Calculate the count of checks left: */
    int cCount = 1;
    QString strCount = vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_UpdateCheckCount);
    if (!strCount.isEmpty())
    {
        bool ok = false;
        int c = strCount.toLongLong(&ok);
        if (ok) cCount = c;
    }

    /* Compose query: */
    QUrl url(m_url);
    url.addQueryItem("platform", vboxGlobal().virtualBox().GetPackageType());
    /* Branding: Check whether we have a local branding file which tells us our version suffix "FOO"
                 (e.g. 3.06.54321_FOO) to identify this installation */
    if (vboxGlobal().brandingIsActive())
    {
        url.addQueryItem("version", QString("%1_%2_%3").arg(vboxGlobal().virtualBox().GetVersion())
                                                       .arg(vboxGlobal().virtualBox().GetRevision())
                                                       .arg(vboxGlobal().brandingGetKey("VerSuffix")));
    }
    else
    {
        /* Use hard coded version set by VBOX_VERSION_STRING: */
        url.addQueryItem("version", QString("%1_%2").arg(vboxGlobal().virtualBox().GetVersion())
                                                    .arg(vboxGlobal().virtualBox().GetRevision()));
    }
    url.addQueryItem("count", QString::number(cCount));
    url.addQueryItem("branch", VBoxUpdateData(vboxGlobal().virtualBox().
                                              GetExtraData(VBoxDefs::GUI_UpdateDate)).branchName());
    QString strUserAgent(QString("VirtualBox %1 <%2>")
                            .arg(vboxGlobal().virtualBox().GetVersion())
                            .arg(vboxGlobal().platformInfo()));

    /* Show progress bar: */
    mPbCheck->show();

    /* Setup GET request: */
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", strUserAgent.toAscii());
    QNetworkReply *pReply = m_pNetworkManager->get(request);
    connect(pReply, SIGNAL(finished()), this, SLOT(sltHandleReply()));
    connect(pReply, SIGNAL(sslErrors(QList<QSslError>)), pReply, SLOT(ignoreSslErrors()));
}

void VBoxUpdateDlg::sltHandleReply()
{
    /* Get corresponding network reply object: */
    QNetworkReply *pReply = qobject_cast<QNetworkReply*>(sender());
    /* And ask it for suicide: */
    pReply->deleteLater();

    /* Hide progress bar: */
    mPbCheck->hide();

    /* Handle normal result: */
    if (pReply->error() == QNetworkReply::NoError)
    {
        /* Deserialize incoming data: */
        QString strResponseData(pReply->readAll());

        /* Newer version of necessary package found: */
        if (strResponseData.indexOf(QRegExp("^\\d+\\.\\d+\\.\\d+ \\S+$")) == 0)
        {
            QStringList response = strResponseData.split(" ", QString::SkipEmptyParts);

            /* For background update: */
            if (isHidden())
            {
                vboxProblem().showUpdateSuccess(vboxGlobal().mainWindow(), response[0], response[1]);
                acceptLater();
            }
            /* For wizard update: */
            else
            {
                mTextSuccessInfo->setText(mTextSuccessInfo->text().arg(response[0], response[1], response[1]));
                mTextSuccessInfo->show();
                mPageStack->setCurrentIndex(1);
            }
        }
        /* No newer version of necessary package found: */
        else
        {
            /* For background update: */
            if (isHidden())
            {
                if (m_fForceRun)
                    vboxProblem().showUpdateNotFound(vboxGlobal().mainWindow());
                acceptLater();
            }
            /* For wizard update: */
            else
            {
                mTextNotFoundInfo->show();
                mPageStack->setCurrentIndex(1);
            }
        }

        /* Save left count of checks: */
        int cCount = 1;
        QString strCount = vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_UpdateCheckCount);
        if (!strCount.isEmpty())
        {
            bool ok = false;
            int c = strCount.toLongLong(&ok);
            if (ok) cCount = c;
        }
        vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_UpdateCheckCount, QString("%1").arg((qulonglong)cCount + 1));
    }
    /* Handle errors: */
    else
    {
        /* For background update: */
        if (isHidden())
        {
            if (m_fForceRun)
                vboxProblem().showUpdateFailure(vboxGlobal().mainWindow(), pReply->errorString());
            acceptLater();
        }
        /* For wizard update: */
        else
        {
            mTextFailureInfo->setText(mTextFailureInfo->text().arg(pReply->errorString()));
            mTextFailureInfo->show();
            mPageStack->setCurrentIndex(1);
        }
    }
}

