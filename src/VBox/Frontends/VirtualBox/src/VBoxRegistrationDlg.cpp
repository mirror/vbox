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

#include "QIHttp.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxRegistrationDlg.h"

/* Qt includes */
#include <QTimer>

/**
 *  This class is used to encode/decode the registration data.
 */
class VBoxRegistrationData
{
public:

    VBoxRegistrationData (const QString &aData)
        : mIsValid (false), mIsRegistered (false)
        , mData (aData)
        , mTriesLeft (3 /* the initial tries value */)
    {
        decode (aData);
    }

    VBoxRegistrationData (const QString &aName, const QString &aEmail,
                          const QString &aIsPrivate)
        : mIsValid (true), mIsRegistered (true)
        , mName (aName), mEmail (aEmail), mIsPrivate (aIsPrivate)
        , mTriesLeft (0)
    {
        encode (aName, aEmail, aIsPrivate);
    }

    bool isValid() const { return mIsValid; }
    bool isRegistered() const { return mIsRegistered; }

    const QString &data() const { return mData; }
    const QString &name() const { return mName; }
    const QString &email() const { return mEmail; }
    const QString &isPrivate() const { return mIsPrivate; }

    uint triesLeft() const { return mTriesLeft; }

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
        mIsPrivate = dataList [2];

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
    QString mIsPrivate;

    uint mTriesLeft;
};

/* Static member to check if registration dialog necessary. */
bool VBoxRegistrationDlg::hasToBeShown()
{
    VBoxRegistrationData regData (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_RegistrationData));

    return (!regData.isValid()) ||
           (!regData.isRegistered() && regData.triesLeft() > 0);
}

VBoxRegistrationDlg::VBoxRegistrationDlg (VBoxRegistrationDlg **aSelf, QWidget * /* aParent */)
    : QIWithRetranslateUI <QIAbstractWizard> (0)
    , mSelf (aSelf)
    , mWvalReg (0)
    , mUrl ("http://registration.virtualbox.org/register762.php")
    , mHttp (new QIHttp (this, mUrl.host()))
{
    /* Store external pointer to this dialog */
    *mSelf = this;

    /* Apply UI decorations */
    Ui::VBoxRegistrationDlg::setupUi (this);

    /* Apply window icons */
    setWindowIcon (vboxGlobal().iconSetFull (QSize (32, 32), QSize (16, 16),
                                             ":/register_32px.png", ":/register_16px.png"));

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Setup validations for line-edit fields */
    QRegExp nameExp ("[\\S\\s]+");
    /* E-mail address is validated according to RFC2821, RFC2822,
     * see http://en.wikipedia.org/wiki/E-mail_address */
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

    /* Setup other connections */
    mWvalReg = new QIWidgetValidator (mPageReg, this);
    connect (mWvalReg, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWvalReg, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (vboxGlobal().mainWindow(), SIGNAL (closing()), this, SLOT (reject()));

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
    /* Erase dialog handle in config file. */
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_RegistrationDlgWinID,
                                            QString::null);

    /* Erase external pointer to this dialog. */
    *mSelf = 0;
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
    QTimer::singleShot (0, this, SLOT (handshakeStart()));
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

void VBoxRegistrationDlg::handshakeStart()
{
    /* Compose query */
    QUrl url (mUrl);
    url.addQueryItem ("version", vboxGlobal().virtualBox().GetVersion());

    /* Handshake */
    mHttp->disconnect (this);
    connect (mHttp, SIGNAL (allIsDone (bool)), this, SLOT (handshakeResponse (bool)));
    mHttp->post (url.toEncoded());
}

void VBoxRegistrationDlg::handshakeResponse (bool aError)
{
    /* Block all the other incoming signals */
    mHttp->disconnect (this);

    /* Process error if present */
    if (aError)
        return abortRequest (mHttp->errorString());

    /* Read received data */
    mKey = mHttp->readAll();

    /* Verifying key correctness */
    if (QString (mKey).indexOf (QRegExp ("^[a-zA-Z0-9]{32}$")))
        return abortRequest (tr ("Could not perform connection handshake."));

    /* Perform registration */
    QTimer::singleShot (0, this, SLOT (registrationStart()));
}

void VBoxRegistrationDlg::registrationStart()
{
    /* Compose query */
    QUrl url (mUrl);
    url.addQueryItem ("version", vboxGlobal().virtualBox().GetVersion());
    url.addQueryItem ("key", mKey);
    url.addQueryItem ("platform", vboxGlobal().platformInfo());
    url.addQueryItem ("name", mLeName->text());
    url.addQueryItem ("email", mLeEmail->text());
    url.addQueryItem ("private", mCbUse->isChecked() ? "1" : "0");

    /* Registration */
    mHttp->disconnect (this);
    connect (mHttp, SIGNAL (allIsDone (bool)), this, SLOT (registrationResponse (bool)));
    mHttp->post (url.toEncoded());
}

void VBoxRegistrationDlg::registrationResponse (bool aError)
{
    /* Block all the other incoming signals */
    mHttp->disconnect (this);

    /* Process error if present */
    if (aError)
        return abortRequest (mHttp->errorString());

    /* Read received data */
    QString data (mHttp->readAll());

    /* Show registration result */
    vboxProblem().showRegisterResult (this, data);

    /* Close the dialog */
    data == "OK" ? finish() : reject();
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

/* This wrapper displays an error message box (unless aReason is QString::null)
 * with the cause of the request-send procedure termination. After the message
 * box dismissed, the registration dialog signals to close itself on the next
 * event loop iteration. */
void VBoxRegistrationDlg::abortRequest (const QString &aReason)
{
    if (!aReason.isNull())
        vboxProblem().cannotConnectRegister (this, mUrl.toString(), aReason);

    /* Allows all the queued signals to be processed before quit. */
    QTimer::singleShot (0, this, SLOT (reject()));
}

void VBoxRegistrationDlg::finish()
{
    VBoxRegistrationData regData (mLeName->text(), mLeEmail->text(),
                                  mCbUse->isChecked() ? "yes" : "no");
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_RegistrationData,
                                            regData.data());

    QIAbstractWizard::accept();
}

