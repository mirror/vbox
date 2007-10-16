/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxRegistrationDlg UI include (Qt Designer)
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

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

    bool isValid() const                { return mIsValid; }
    bool isRegistered() const           { return mIsRegistered; }

    const QString &data() const         { return mData; }
    const QString &name() const         { return mName; }
    const QString &email() const        { return mEmail; }
    const QString &isPrivate() const    { return mPrivate; }

    uint triesLeft() const              { return mTriesLeft; }

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
        for (ulong i = 0; i < crcData.length(); i += 2)
        {
            crcNeed <<= 8;
            uchar curByte = (uchar) crcData.mid (i, 2).toUShort (0, 16);
            crcNeed += curByte;
        }
        data.truncate (data.length() - 2 * sizeof (ulong));

        /* Decoding data */
        QString result;
        for (ulong i = 0; i < data.length(); i += 2)
            result += QChar (data.mid (i, 2).toUShort (0, 16));
        ulong crcNow = crc32 ((uchar*)result.ascii(), result.length());

        /* Check the CRC32 */
        if (crcNeed != crcNow)
            return;

        /* Initialize values */
        QStringList dataList = QStringList::split ("|", result);
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
        for (ulong i = 0; i < data.length(); ++ i)
        {
            QString curPair = QString::number (data.at (i).unicode(), 16);
            if (curPair.length() == 1)
                curPair.prepend ("0");
            mData += curPair;
        }

        /* Enconding CRC32 */
        ulong crcNow = crc32 ((uchar*)data.ascii(), data.length());
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

/* static */
bool VBoxRegistrationDlg::hasToBeShown()
{
    VBoxRegistrationData regData (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_RegistrationData));

    return !regData.isValid() ||
        (!regData.isRegistered() && regData.triesLeft() > 0);
}

/* Default constructor initialization. */
void VBoxRegistrationDlg::init()
{
    /* Hide unnecessary buttons */
    helpButton()->setShown (false);
    cancelButton()->setShown (false);
    backButton()->setShown (false);

    /* Confirm button initially disabled */
    finishButton()->setEnabled (false);
    finishButton()->setAutoDefault (true);
    finishButton()->setDefault (true);

    /* Setup the label colors for nice scaling */
    VBoxGlobal::adoptLabelPixmap (pictureLabel);

    /* Adjust text label size */
    mTextLabel->setMinimumWidth (widthSpacer->minimumSize().width());

    /* Setup validations and maximum text-edit text length */
    QRegExp nameExp ("[a-zA-Z0-9\\(\\)_\\-\\.\\s]+");
    QRegExp emailExp ("[a-zA-Z][a-zA-Z0-9_\\-\\.!#$%\\*/?|^{}`~&'\\+=]*[a-zA-Z0-9]@[a-zA-Z][a-zA-Z0-9_\\-\\.]*[a-zA-Z]");
    mNameEdit->setValidator (new QRegExpValidator (nameExp, this));
    mEmailEdit->setValidator (new QRegExpValidator (emailExp, this));
    mNameEdit->setMaxLength (50);
    mEmailEdit->setMaxLength (50);

    /* Create connection-timeout timer */
    mTimeout = new QTimer (this);

    /* Load language constraints */
    languageChangeImp();

    /* Set required boolean flags */
    mSuicide = false;
    mHandshake = true;

    /* Network framework */
    mNetfw = 0;

    /* Setup connections */
    disconnect (finishButton(), SIGNAL (clicked()), 0, 0);
    connect (finishButton(), SIGNAL (clicked()), SLOT (registration()));
    connect (mTimeout, SIGNAL (timeout()), this, SLOT (processTimeout()));
    connect (mNameEdit, SIGNAL (textChanged (const QString&)), this, SLOT (validate()));
    connect (mEmailEdit, SIGNAL (textChanged (const QString&)), this, SLOT (validate()));
    connect (mTextLabel, SIGNAL (clickedOnLink (const QString &)),
             &vboxGlobal(), SLOT (openURL (const QString &)));

    /* Resize the dialog initially to minimum size */
    resize (minimumSize());
}

/* Default destructor cleanup. */
void VBoxRegistrationDlg::destroy()
{
    delete mNetfw;
    *mSelf = 0;
}

/* Setup necessary dialog parameters. */
void VBoxRegistrationDlg::setup (VBoxRegistrationDlg **aSelf)
{
    mSelf = aSelf;
    *mSelf = this;
    mUrl = "http://www.innotek.de/register762.php";

    VBoxRegistrationData regData (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_RegistrationData));

    mNameEdit->setText (regData.name());
    mEmailEdit->setText (regData.email());
    mUseCheckBox->setChecked (regData.isPrivate() == "yes");
}

/* String constants initializer. */
void VBoxRegistrationDlg::languageChangeImp()
{
    finishButton()->setText (tr ("&Confirm"));
}

void VBoxRegistrationDlg::postRequest (const QString &aHost,
                                       const QString &aUrl)
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

    mNetfw->postRequest (aHost, aUrl);
}


/* Post the handshake request into the innotek register site. */
void VBoxRegistrationDlg::registration()
{
    /* Disable control elements */
    mNameEdit->setEnabled (false);
    mEmailEdit->setEnabled (false);
    mUseCheckBox->setEnabled (false);
    finishButton()->setEnabled (false);

    /* Handshake arguments initializing */
    QString version = vboxGlobal().virtualBox().GetVersion();
    QUrl::encode (version);

    /* Handshake */
    QString argument = QString ("?version=%1").arg (version);
    mTimeout->start (20000, true);
    postRequest (mUrl.host(), mUrl.path() + argument);
}

/* This slot is used to control the connection timeout. */
void VBoxRegistrationDlg::processTimeout()
{
    abortRegisterRequest (tr ("Connection timed out."));
}

/* Handles the network request begining. */
void VBoxRegistrationDlg::onNetBegin (int aStatus)
{
    if (aStatus == 404)
        abortRegisterRequest (tr ("Could not locate the registration form on "
            "the server (response: %1).").arg (aStatus));
    else
        mTimeout->start (20000, true);
}

/* Handles the network request data incoming. */
void VBoxRegistrationDlg::onNetData (const QByteArray&)
{
    if (!mSuicide)
        mTimeout->start (20000, true);
}

/* Handles the network request end. */
void VBoxRegistrationDlg::onNetEnd (const QByteArray &aTotalData)
{
    if (mSuicide)
        return;

    mTimeout->stop();
    if (mHandshake)
    {
        /* Registration arguments initializing */
        QString version = vboxGlobal().virtualBox().GetVersion();
        QString key (aTotalData);
        QString platform = getPlatform();
        QString name = mNameEdit->text();
        QString email = mEmailEdit->text();
        QString prvt = mUseCheckBox->isChecked() ? "1" : "0";
        QUrl::encode (version);
        QUrl::encode (platform);
        QUrl::encode (name);
        QUrl::encode (email);

        /* Registration */
        QString argument;
        argument += QString ("?version=%1").arg (version);
        argument += QString ("&key=%1").arg (key);
        argument += QString ("&platform=%1").arg (platform);
        argument += QString ("&name=%1").arg (name);
        argument += QString ("&email=%1").arg (email);
        argument += QString ("&private=%1").arg (prvt);

        mHandshake = false;
        mTimeout->start (20000, true);
        postRequest (mUrl.host(), mUrl.path() + argument);
    }
    else
    {
        /* Show registration result */
        QString result (aTotalData);
        vboxProblem().showRegisterResult (this, result);

        /* Close the dialog */
        result == "OK" ? accept() : reject();
    }
}

void VBoxRegistrationDlg::onNetError (const QString &aError)
{
    abortRegisterRequest (aError);
}

/* SLOT: QDialog accept slot reimplementation. */
void VBoxRegistrationDlg::accept()
{
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_RegistrationDlgWinID,
                                            QString::null);

    VBoxRegistrationData regData (mNameEdit->text(), mEmailEdit->text(),
        mUseCheckBox->isChecked() ? "yes" : "no");
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_RegistrationData,
                                            regData.data());

    QDialog::accept();
}

/* SLOT: QDialog reject slot reimplementation. */
void VBoxRegistrationDlg::reject()
{
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_RegistrationDlgWinID,
                                            QString::null);

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

    QDialog::reject();
}

void VBoxRegistrationDlg::validate()
{
    int pos = 0;
    QString name = mNameEdit->text();
    QString email = mEmailEdit->text();
    finishButton()->setEnabled (
        mNameEdit->validator()->validate (name, pos) == QValidator::Acceptable &&
        mEmailEdit->validator()->validate (email, pos) == QValidator::Acceptable);
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

    return platform;
}

/* This wrapper displays an error message box (unless aReason is
 * QString::null) with the cause of the request-send procedure
 * termination. After the message box is dismissed, the downloader signals
 * to close itself on the next event loop iteration. */
void VBoxRegistrationDlg::abortRegisterRequest (const QString &aReason)
{
    /* Protect against double kill request. */
    if (mSuicide)
        return;
    mSuicide = true;

    if (!aReason.isNull())
        vboxProblem().cannotConnectRegister (this, mUrl.toString(), aReason);

    /* Allows all the queued signals to be processed before quit. */
    QTimer::singleShot (0, this, SLOT (reject()));
}

void VBoxRegistrationDlg::showEvent (QShowEvent *aEvent)
{
    validate();
    QWizard::showEvent (aEvent);
}
