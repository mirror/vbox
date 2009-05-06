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

    VBoxRegistrationData (const QString &aString, bool aEncode)
        : mIsValid (aEncode), mIsRegistered (aEncode)
        , mTriesLeft (3 /* the initial tries value */)
    {
        aEncode ? encode (aString) : decode (aString);
    }

    bool isValid() const { return mIsValid; }
    bool isRegistered() const { return mIsRegistered; }

    const QString &data() const { return mData; }
    const QString &account() const { return mAccount; }

    uint triesLeft() const { return mTriesLeft; }

private:

    void decode (const QString &aData)
    {
        if (aData.isEmpty())
            return;

        mData = aData;

        /* Decoding number of left tries */
        if (mData.startsWith ("triesLeft="))
        {
            bool ok = false;
            uint triesLeft = mData.section ('=', 1, 1).toUInt (&ok);
            if (!ok)
                return;
            mTriesLeft = triesLeft;
            mIsValid = true;
            return;
        }

        /* Decoding CRC32 */
        QString data = mData;
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

        /* Split values list */
        QStringList list (result.split ("|"));

        /* Ignore the old format */
        if (list.size() > 1)
            return;

        /* Result value */
        mIsValid = true;
        mIsRegistered = true;
        mAccount = list [0];
    }

    void encode (const QString &aAccount)
    {
        if (aAccount.isEmpty())
            return;

        mAccount = aAccount;

        /* Encoding data */
        QString data = QString ("%1").arg (mAccount);
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

    uint mTriesLeft;

    QString mAccount;
    QString mData;
};

/* Static member to check if registration dialog necessary. */
bool VBoxRegistrationDlg::hasToBeShown()
{
    VBoxRegistrationData data (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_RegistrationData), false);

    return (!data.isValid()) ||
           (!data.isRegistered() && data.triesLeft() > 0);
}

VBoxRegistrationDlg::VBoxRegistrationDlg (VBoxRegistrationDlg **aSelf, QWidget * /* aParent */)
    : QIWithRetranslateUI <QIAbstractWizard> (0)
    , mSelf (aSelf)
    , mWvalReg (0)
    , mUrl ("http://registration.virtualbox.org/register763.php")
    , mHttp (new QIHttp (this, mUrl.host()))
{
    /* Store external pointer to this dialog */
    *mSelf = this;

    /* Apply UI decorations */
    Ui::VBoxRegistrationDlg::setupUi (this);

    /* Apply window icons */
    setWindowIcon (vboxGlobal().iconSetFull (QSize (32, 32), QSize (16, 16),
                                             ":/register_32px.png", ":/register_16px.png"));

    /* Setup widgets */
    QSizePolicy sp (mTextRegInfo->sizePolicy());
    sp.setHeightForWidth (true);
    mTextRegInfo->setSizePolicy (sp);

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
    QRegExp passwordExp ("[a-zA-Z0-9_\\-\\+=`~!@#$%^&\\*\\(\\)?\\[\\]:;,\\./]+");

    mLeOldEmail->setMaxLength (50);
    mLeOldEmail->setValidator (new QRegExpValidator (emailExp, this));

    mLeOldPassword->setMaxLength (20);
    mLeOldPassword->setValidator (new QRegExpValidator (passwordExp, this));

    mLeNewFirstName->setMaxLength (50);

   mLeNewFirstName->setValidator (new QRegExpValidator (nameExp, this));

    mLeNewLastName->setMaxLength (50);
    mLeNewLastName->setValidator (new QRegExpValidator (nameExp, this));

    mLeNewCompany->setMaxLength (50);
    mLeNewCompany->setValidator (new QRegExpValidator (nameExp, this));

    mLeNewCountry->setMaxLength (50);
    mLeNewCountry->setValidator (new QRegExpValidator (nameExp, this));

    mLeNewEmail->setMaxLength (50);
    mLeNewEmail->setValidator (new QRegExpValidator (emailExp, this));

    mLeNewPassword->setMaxLength (20);
    mLeNewPassword->setValidator (new QRegExpValidator (passwordExp, this));

    mLeNewPassword2->setMaxLength (20);
    mLeNewPassword2->setValidator (new QRegExpValidator (passwordExp, this));

    /* Setup validation */
    mWvalReg = new QIWidgetValidator (mPageReg, this);
    connect (mWvalReg, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWvalReg, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));

    connect (mRbOld, SIGNAL (toggled (bool)), mWvalReg, SLOT (revalidate()));
    connect (mRbNew, SIGNAL (toggled (bool)), mWvalReg, SLOT (revalidate()));

    connect (mLeOldEmail, SIGNAL (textEdited (const QString &)),
             this, SLOT (tuneRadioButton (const QString &)));
    connect (mLeOldPassword, SIGNAL (textEdited (const QString &)),
             this, SLOT (tuneRadioButton (const QString &)));
    connect (mLeNewFirstName, SIGNAL (textEdited (const QString &)),
             this, SLOT (tuneRadioButton (const QString &)));
    connect (mLeNewLastName, SIGNAL (textEdited (const QString &)),
             this, SLOT (tuneRadioButton (const QString &)));
    connect (mLeNewCompany, SIGNAL (textEdited (const QString &)),
             this, SLOT (tuneRadioButton (const QString &)));
    connect (mLeNewCountry, SIGNAL (textEdited (const QString &)),
             this, SLOT (tuneRadioButton (const QString &)));
    connect (mLeNewEmail, SIGNAL (textEdited (const QString &)),
             this, SLOT (tuneRadioButton (const QString &)));
    connect (mLeNewPassword, SIGNAL (textEdited (const QString &)),
             this, SLOT (tuneRadioButton (const QString &)));
    connect (mLeNewPassword2, SIGNAL (textEdited (const QString &)),
             this, SLOT (tuneRadioButton (const QString &)));

    /* Setup other connections */
    connect (vboxGlobal().mainWindow(), SIGNAL (closing()), this, SLOT (reject()));

    /* Setup initial dialog parameters. */
    VBoxRegistrationData data (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_RegistrationData), false);
    mLeOldEmail->setText (data.account());

    /* Update the next button state for pages with validation
     * (validityChanged() connected to enableNext() will do the job) */
    mWvalReg->revalidate();

    /* Initialize wizard hdr */
    initializeWizardFtr();

    retranslateUi();

    /* Fix minimum possible size */
    resize (minimumSizeHint());
    qApp->processEvents();
    setFixedSize (size());
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

void VBoxRegistrationDlg::tuneRadioButton (const QString &aNewText)
{
    QList <QLineEdit*> oldSet;
    oldSet << mLeOldEmail << mLeOldPassword;

    QList <QLineEdit*> newSet;
    newSet << mLeNewFirstName << mLeNewLastName
           << mLeNewCompany << mLeNewCountry
           << mLeNewEmail << mLeNewPassword << mLeNewPassword2;

    QLineEdit *le = qobject_cast <QLineEdit*> (sender());

    if (le && !aNewText.isEmpty() && oldSet.contains (le))
        mRbOld->setChecked (true);
    else if (le && !aNewText.isEmpty() && newSet.contains (le))
        mRbNew->setChecked (true);

    mWvalReg->revalidate();
}

/* Post the handshake request into the register site. */
void VBoxRegistrationDlg::accept()
{
    /* Disable control elements */
    mLeOldEmail->setEnabled (false);
    mLeOldPassword->setEnabled (false);
    mLeNewFirstName->setEnabled (false);
    mLeNewLastName->setEnabled (false);
    mLeNewCompany->setEnabled (false);
    mLeNewCountry->setEnabled (false);
    mLeNewEmail->setEnabled (false);
    mLeNewPassword->setEnabled (false);
    mLeNewPassword2->setEnabled (false);
    finishButton()->setEnabled (false);
    cancelButton()->setEnabled (false);

    /* Set busy cursor */
    setCursor (QCursor (Qt::BusyCursor));

    /* Perform connection handshake */
    QTimer::singleShot (0, this, SLOT (handshakeStart()));
}

void VBoxRegistrationDlg::reject()
{
    /* Decrement the triesLeft. */
    VBoxRegistrationData data (vboxGlobal().virtualBox().
                               GetExtraData (VBoxDefs::GUI_RegistrationData), false);
    if (!data.isValid() || !data.isRegistered())
    {
        uint triesLeft = data.triesLeft();
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

void VBoxRegistrationDlg::reinit()
{
    /* Read all the dirty data */
    mHttp->disconnect (this);
    mHttp->readAll();

    /* Enable control elements */
    mLeOldEmail->setEnabled (true);
    mLeOldPassword->setEnabled (true);
    mLeNewFirstName->setEnabled (true);
    mLeNewLastName->setEnabled (true);
    mLeNewCompany->setEnabled (true);
    mLeNewCountry->setEnabled (true);
    mLeNewEmail->setEnabled (true);
    mLeNewPassword->setEnabled (true);
    mLeNewPassword2->setEnabled (true);
    finishButton()->setEnabled (true);
    cancelButton()->setEnabled (true);

    /* Unset busy cursor */
    unsetCursor();
}

void VBoxRegistrationDlg::handshakeStart()
{
    /* Compose query */
    QUrl url (mUrl);
    url.addQueryItem ("version", vboxGlobal().virtualBox().GetVersion());

    /* Handshake */
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

    /* Basic set */
    url.addQueryItem ("version", vboxGlobal().virtualBox().GetVersion());
    url.addQueryItem ("key", mKey);
    url.addQueryItem ("platform", vboxGlobal().platformInfo());

    if (mRbOld->isChecked())
    {
        /* Light set */
        url.addQueryItem ("email", mLeOldEmail->text());
        url.addQueryItem ("password", mLeOldPassword->text());
    }
    else if (mRbNew->isChecked())
    {
        /* Full set */
        url.addQueryItem ("email", mLeNewEmail->text());
        url.addQueryItem ("password", mLeNewPassword->text());
        url.addQueryItem ("firstname", mLeNewFirstName->text());
        url.addQueryItem ("lastname", mLeNewLastName->text());
        url.addQueryItem ("company", mLeNewCompany->text());
        url.addQueryItem ("country", mLeNewCountry->text());
    }

    /* Registration */
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
    data == "OK" ? finish() : reinit();
}

void VBoxRegistrationDlg::revalidate (QIWidgetValidator *aWval)
{
    bool valid = true;

    if (mRbOld->isChecked())
    {
        /* Check for fields correctness */
        if (!isFieldValid (mLeOldEmail) || !isFieldValid (mLeOldPassword))
            valid = false;
    }
    if (mRbNew->isChecked())
    {
        /* Check for fields correctness */
        if (!isFieldValid (mLeNewFirstName) || !isFieldValid (mLeNewLastName) ||
            !isFieldValid (mLeNewCompany) || !isFieldValid (mLeNewCountry) ||
            !isFieldValid (mLeNewEmail) ||
            !isFieldValid (mLeNewPassword) || !isFieldValid (mLeNewPassword2))
            valid = false;

        /* Check for password correctness */
        if (mLeNewPassword->text() != mLeNewPassword2->text())
            valid = false;
    }

    aWval->setOtherValid (valid);
}

void VBoxRegistrationDlg::enableNext (const QIWidgetValidator *aWval)
{
    /* Validate all the subscribed widgets */
    aWval->isValid();
    /* But control dialog only with necessary */
    finishButton()->setEnabled (aWval->isOtherValid());
}

void VBoxRegistrationDlg::onPageShow()
{
    Assert (mPageStack->currentWidget() == mPageReg);
    mLeOldEmail->setFocus();
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
    QTimer::singleShot (0, this, SLOT (reinit()));
}

void VBoxRegistrationDlg::finish()
{
    QString acc (mRbOld->isChecked() ? mLeOldEmail->text() :
                 mRbNew->isChecked() ? mLeNewEmail->text() : QString::null);

    VBoxRegistrationData data (acc, true);
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_RegistrationData,
                                            data.data());

    QIAbstractWizard::accept();
}

bool VBoxRegistrationDlg::isFieldValid (QLineEdit *aLe) const
{
    QString text (aLe->text());
    int position;
    return aLe->validator()->validate (text, position) == QValidator::Acceptable;
}

