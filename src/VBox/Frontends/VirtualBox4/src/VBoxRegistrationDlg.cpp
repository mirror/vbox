/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxRegistrationDlg class implementation
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

#include "VBoxRegistrationDlg.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxNetworkFramework.h"

/* Qt includes */
#include <QTimer>
#include <QProcess>

#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>

/* Time to auto-disconnect if no network answer received. */
static const int MaxWaitTime = 20000;

/**
 *  This class is used to encode/decode the registration data.
 */
class VBoxRegistrationData
{
public:

    VBoxRegistrationData (const QString &aData)
        : mIsValid (false), mIsRegistered (false)
        , mData (aData)
        , mTriesLeft (3 /* the initial tries value! */)
    {
        decode (aData);
    }

    VBoxRegistrationData (const QString &aName, const QString &aEmail,
                          const QString &aPrivate)
        : mIsValid (true), mIsRegistered (true)
        , mName (aName), mEmail (aEmail), mPrivate (aPrivate)
        , mTriesLeft (0)
    {
        encode (aName, aEmail, aPrivate);
    }

    bool isValid() const              { return mIsValid; }
    bool isRegistered() const         { return mIsRegistered; }

    const QString &data() const       { return mData; }
    const QString &name() const       { return mName; }
    const QString &email() const      { return mEmail; }
    const QString &isPrivate() const  { return mPrivate; }

    uint triesLeft() const            { return mTriesLeft; }

private:

    void decode (const QString &aData)
    {
        mIsValid = mIsRegistered = false;

        if (aData.isEmpty())
            return;

        if (aData.startsWith ("triesLeft="))
        {
            bool ok = false;
            uint triesLeft = aData.section ('=', 1, 1).toUInt (&ok);
            if (!ok)
                return;
            mTriesLeft = triesLeft;
            mIsValid = true;
            return;
        }

        /* Decoding CRC32 */
        QString data = aData;
        QString crcData = data.right (2 * sizeof (ulong));
        ulong crcNeed = 0;
        for (long i = 0; i < crcData.length(); i += 2)
        {
            crcNeed <<= 8;
            uchar curByte = (uchar) crcData.mid (i, 2).toUShort (0, 16);
            crcNeed += curByte;
        }
        data.truncate (data.length() - 2 * sizeof (ulong));

        /* Decoding data */
        QString result;
        for (long i = 0; i < data.length(); i += 4)
            result += QChar (data.mid (i, 4).toUShort (0, 16));
        ulong crcNow = crc32 ((uchar*)result.toAscii().constData(), result.length());

        /* Check the CRC32 */
        if (crcNeed != crcNow)
            return;

        /* Initialize values */
        QStringList dataList = result.split ("|");
        mName = dataList [0];
        mEmail = dataList [1];
        mPrivate = dataList [2];

        mIsValid = true;
        mIsRegistered = true;
    }

    void encode (const QString &aName, const QString &aEmail,
                 const QString &aPrivate)
    {
        /* Encoding data */
        QString data = QString ("%1|%2|%3")
            .arg (aName).arg (aEmail).arg (aPrivate);
        mData = QString::null;
        for (long i = 0; i < data.length(); ++ i)
        {
            QString curPair = QString::number (data.at (i).unicode(), 16);
            while (curPair.length() < 4)
                curPair.prepend ('0');
            mData += curPair;
        }

        /* Enconding CRC32 */
        ulong crcNow = crc32 ((uchar*)data.toAscii().constData(), data.length());
        QString crcData;
        for (ulong i = 0; i < sizeof (ulong); ++ i)
        {
            ushort curByte = crcNow & 0xFF;
            QString curPair = QString::number (curByte, 16);
            if (curPair.length() == 1)
                curPair.prepend ("0");
            crcData = curPair + crcData;
            crcNow >>= 8;
        }

        mData += crcData;
    }

    ulong crc32 (unsigned char *aBufer, int aSize)
    {
        /* Filling CRC32 table */
        ulong crc32;
        ulong crc_table [256];
        ulong temp;
        for (int i = 0; i < 256; ++ i)
        {
            temp = i;
            for (int j = 8; j > 0; -- j)
            {
                if (temp & 1)
                    temp = (temp >> 1) ^ 0xedb88320;
                else
                    temp >>= 1;
            };
            crc_table [i] = temp;
        }

        /* CRC32 calculation */
        crc32 = 0xffffffff;
        for (int i = 0; i < aSize; ++ i)
        {
            crc32 = crc_table [(crc32 ^ (*aBufer ++)) & 0xff] ^ (crc32 >> 8);
        }
        crc32 = crc32 ^ 0xffffffff;
        return crc32;
    };

    bool mIsValid : 1;
    bool mIsRegistered : 1;

    QString mData;
    QString mName;
    QString mEmail;
    QString mPrivate;

    uint mTriesLeft;
};

VBoxRegistrationDlg::VBoxRegistrationDlg (VBoxRegistrationDlg **aSelf,
                                          QWidget *aParent,
                                          Qt::WindowFlags aFlags)
    : QIWithRetranslateUI2<QIAbstractWizard> (aParent, aFlags)
    , mSelf (aSelf)
    , mWvalReg (0)
    , mUrl ("http://www.innotek.de/register762.php")
    , mKey (QString::null)
    , mTimeout (new QTimer (this))
    , mHandshake (true)
    , mSuicide (false)
    , mNetfw (0)
{
    /* Store external pointer to this dialog. */
    *mSelf = this;

    /* Apply UI decorations */
    Ui::VBoxRegistrationDlg::setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Setup validations for line-edit fields */
    QRegExp nameExp ("[\\S\\s]+");
    /* E-mail address is validated according to RFC2821, RFC2822,
     * see http://en.wikipedia.org/wiki/E-mail_address. */
    QRegExp emailExp ("(([a-zA-Z0-9_\\-\\.!#$%\\*/?|^{}`~&'\\+=]*"
                        "[a-zA-Z0-9_\\-!#$%\\*/?|^{}`~&'\\+=])|"
                      "(\"([\\x0001-\\x0008,\\x000B,\\x000C,\\x000E-\\x0019,\\x007F,"
                            "\\x0021,\\x0023-\\x005B,\\x005D-\\x007E,"
                            "\\x0009,\\x0020]|"
                          "(\\\\[\\x0001-\\x0009,\\x000B,\\x000C,"
                                "\\x000E-\\x007F]))*\"))"
                      "@"
                      "[a-zA-Z0-9\\-]+(\\.[a-zA-Z0-9\\-]+)*");
    mLeName->setValidator (new QRegExpValidator (nameExp, this));
    mLeEmail->setValidator (new QRegExpValidator (emailExp, this));
    mLeName->setMaxLength (50);
    mLeEmail->setMaxLength (50);

    /* This is a single shot timer */
    mTimeout->setSingleShot (true);

    /* Setup other connections */
    mWvalReg = new QIWidgetValidator (mPageReg, this);
    connect (mWvalReg, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWvalReg, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (mTimeout, SIGNAL (timeout()), this, SLOT (processTimeout()));
    /* TODO: Implement mouse on link click for QILabel:
     * connect (mTextRegInfo, SIGNAL (clickedOnLink (const QString &)),
     *          &vboxGlobal(), SLOT (openURL (const QString &))); */

    /* Setup initial dialog parameters. */
    VBoxRegistrationData regData (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_RegistrationData));
    mLeName->setText (regData.name());
    mLeEmail->setText (regData.email());
    mCbUse->setChecked (regData.isPrivate() == "yes");

    /* Update the next button state for pages with validation
     * (validityChanged() connected to enableNext() will do the job) */
    mWvalReg->revalidate();

    /* Initialize wizard hdr */
    initializeWizardFtr();

    retranslateUi();
}

VBoxRegistrationDlg::~VBoxRegistrationDlg()
{
    /* Delete network framework. */
    delete mNetfw;

    /* Erase dialog handle in config file. */
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_RegistrationDlgWinID,
                                            QString::null);

    /* Erase external pointer to this dialog. */
    *mSelf = 0;
}

/* Static member to check if registration dialog necessary. */
bool VBoxRegistrationDlg::hasToBeShown()
{
    VBoxRegistrationData regData (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_RegistrationData));

    return !regData.isValid() ||
           (!regData.isRegistered() && regData.triesLeft() > 0);
}

void VBoxRegistrationDlg::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxRegistrationDlg::retranslateUi (this);
}

/* Post the handshake request into the register site. */
void VBoxRegistrationDlg::accept()
{
    /* Disable control elements */
    mLeName->setEnabled (false);
    mLeEmail->setEnabled (false);
    mCbUse->setEnabled (false);
    finishButton()->setEnabled (false);
    cancelButton()->setEnabled (false);

    /* Perform connection handshake */
    QTimer::singleShot (0, this, SLOT (handshake()));
}

void VBoxRegistrationDlg::reject()
{
    /* Decrement the triesLeft. */
    VBoxRegistrationData regData (vboxGlobal().virtualBox().
                                  GetExtraData (VBoxDefs::GUI_RegistrationData));
    if (!regData.isValid() || !regData.isRegistered())
    {
        uint triesLeft = regData.triesLeft();
        if (triesLeft)
        {
            -- triesLeft;
            vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_RegistrationData,
                                                    QString ("triesLeft=%1")
                                                    .arg (triesLeft));
        }
    }

    QIAbstractWizard::reject();
}

void VBoxRegistrationDlg::handshake()
{
    /* Handshake arguments initializing */
    QString version = vboxGlobal().virtualBox().GetVersion();
    version = QUrl::toPercentEncoding (version);

    /* Handshake */
    QString argument = QString ("?version=%1").arg (version);
    postRequest (mUrl.host(), mUrl.path() + argument);
}

void VBoxRegistrationDlg::registration()
{
    /* Registration arguments initializing */
    mHandshake = false;
    QString version = vboxGlobal().virtualBox().GetVersion();
    QString platform = getPlatform();
    QString name = mLeName->text();
    QString email = mLeEmail->text();
    QString prvt = mCbUse->isChecked() ? "1" : "0";
    version = QUrl::toPercentEncoding (version);
    platform = QUrl::toPercentEncoding (platform);
    name = QUrl::toPercentEncoding (name);
    email = QUrl::toPercentEncoding (email);

    /* Registration */
    QString argument;
    argument += QString ("?version=%1").arg (version);
    argument += QString ("&key=%1").arg (mKey);
    argument += QString ("&platform=%1").arg (platform);
    argument += QString ("&name=%1").arg (name);
    argument += QString ("&email=%1").arg (email);
    argument += QString ("&private=%1").arg (prvt);

    postRequest (mUrl.host(), mUrl.path() + argument);
}

void VBoxRegistrationDlg::processTimeout()
{
    abortRegisterRequest (tr ("Connection timed out."));
}

/* Handles the network request begining. */
void VBoxRegistrationDlg::onNetBegin (int aStatus)
{
    if (mSuicide) return;

    if (aStatus == 404)
        abortRegisterRequest (tr ("Could not locate the registration form on "
            "the server (response: %1).").arg (aStatus));
    else
        mTimeout->start (MaxWaitTime);
}

/* Handles the network request data incoming. */
void VBoxRegistrationDlg::onNetData (const QByteArray&)
{
    if (mSuicide) return;

    mTimeout->start (MaxWaitTime);
}

/* Handles the network request end. */
void VBoxRegistrationDlg::onNetEnd (const QByteArray &aTotalData)
{
    if (mSuicide) return;

    mTimeout->stop();
    if (mHandshake)
    {
        /* Verifying key correctness */
        if (QString (aTotalData).indexOf (QRegExp ("^[a-zA-Z0-9]{32}$")))
        {
            abortRegisterRequest (tr ("Could not perform connection handshake."));
            return;
        }
        mKey = aTotalData;

        /* Perform registration */
        QTimer::singleShot (0, this, SLOT (registration()));
    }
    else
    {
        /* Show registration result */
        QString result (aTotalData);
        vboxProblem().showRegisterResult (this, result);

        /* Close the dialog */
        result == "OK" ? finish() : reject();
    }
}

/* Handles the network error. */
void VBoxRegistrationDlg::onNetError (const QString &aError)
{
    abortRegisterRequest (aError);
}

void VBoxRegistrationDlg::revalidate (QIWidgetValidator *aWval)
{
    bool valid = aWval->isOtherValid();
    /* do individual validations for pages here */
    aWval->setOtherValid (valid);
}

void VBoxRegistrationDlg::enableNext (const QIWidgetValidator *aWval)
{
    finishButton()->setEnabled (aWval->isValid());
}

void VBoxRegistrationDlg::onPageShow()
{
    Assert (mPageStack->currentWidget() == mPageReg);
    mLeName->setFocus();
}

void VBoxRegistrationDlg::postRequest (const QString &aHost,
                                       const QString &aPath)
{
    delete mNetfw;
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
    mNetfw->postRequest (aHost, aPath);
}

/* This wrapper displays an error message box (unless aReason is
 * QString::null) with the cause of the request-send procedure
 * termination. After the message box is dismissed, the downloader signals
 * to close itself on the next event loop iteration. */
void VBoxRegistrationDlg::abortRegisterRequest (const QString &aReason)
{
    /* Protect against double kill request. */
    if (mSuicide) return;
    mSuicide = true;

    if (!aReason.isNull())
        vboxProblem().cannotConnectRegister (this, mUrl.toString(), aReason);

    /* Allows all the queued signals to be processed before quit. */
    QTimer::singleShot (0, this, SLOT (reject()));
}

QString VBoxRegistrationDlg::getPlatform()
{
    QString platform;

#if defined (Q_OS_WIN)
    platform = "win";
#elif defined (Q_OS_LINUX)
    platform = "linux";
#elif defined (Q_OS_MACX)
    platform = "macosx";
#elif defined (Q_OS_OS2)
    platform = "os2";
#elif defined (Q_OS_FREEBSD)
    platform = "freebsd";
#elif defined (Q_OS_SOLARIS)
    platform = "solaris";
#else
    platform = "unknown";
#endif

    /* the format is <system>.<bitness> */
    platform += QString (".%1").arg (ARCH_BITS);

    /* add more system information */
#if defined (Q_OS_WIN)
    OSVERSIONINFO versionInfo;
    ZeroMemory (&versionInfo, sizeof (OSVERSIONINFO));
    versionInfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
    GetVersionEx (&versionInfo);
    int major = versionInfo.dwMajorVersion;
    int minor = versionInfo.dwMinorVersion;
    int build = versionInfo.dwBuildNumber;
    QString sp = QString::fromUcs2 ((ushort*)versionInfo.szCSDVersion);

    QString distrib;
    if (major == 6)
        distrib = QString ("Windows Vista %1");
    else if (major == 5)
    {
        if (minor == 2)
            distrib = QString ("Windows Server 2003 %1");
        else if (minor == 1)
            distrib = QString ("Windows XP %1");
        else if (minor == 0)
            distrib = QString ("Windows 2000 %1");
        else
            distrib = QString ("Unknown %1");
    }
    else if (major == 4)
    {
        if (minor == 90)
            distrib = QString ("Windows Me %1");
        else if (minor == 10)
            distrib = QString ("Windows 98 %1");
        else if (minor == 0)
            distrib = QString ("Windows 95 %1");
        else
            distrib = QString ("Unknown %1");
    }
    else
        distrib = QString ("Unknown %1");
    distrib = distrib.arg (sp);
    QString version = QString ("%1.%2").arg (major).arg (minor);
    QString kernel = QString ("%1").arg (build);
    platform += QString (" [Distribution: %1 | Version: %2 | Build: %3]")
        .arg (distrib).arg (version).arg (kernel);
#elif defined (Q_OS_OS2)
    // TODO: add sys info for os2 if any...
#elif defined (Q_OS_LINUX) || defined (Q_OS_MACX) || defined (Q_OS_FREEBSD) || defined (Q_OS_SOLARIS)
    char szAppPrivPath [RTPATH_MAX];
    int rc;

    rc = RTPathAppPrivateNoArch (szAppPrivPath, sizeof (szAppPrivPath));
    Assert (RT_SUCCESS (rc));
    QProcess infoScript (this);
    infoScript.setWorkingDirectory (QString (szAppPrivPath));
    infoScript.start (QString ("VBoxSysInfo.sh"));
    bool result = infoScript.waitForFinished();
    if (result && infoScript.exitStatus() == QProcess::NormalExit)
        platform += QString (" [%1]").arg (QString (infoScript.readAllStandardOutput()));
#endif
    return platform;
}

void VBoxRegistrationDlg::finish()
{
    VBoxRegistrationData regData (mLeName->text(), mLeEmail->text(),
        mCbUse->isChecked() ? "yes" : "no");
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_RegistrationData,
                                            regData.data());

    QIAbstractWizard::accept();
}

