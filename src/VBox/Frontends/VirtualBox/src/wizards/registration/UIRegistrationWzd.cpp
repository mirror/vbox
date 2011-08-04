/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIRegistrationWzd class implementation
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QTimer>
#include <QUrl>

/* Local includes */
#include "UIRegistrationWzd.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "QIHttp.h"

/**
 *  This class is used to encode/decode the registration data.
 */
class RegistrationData
{
public:

    RegistrationData(const QString &strData, bool bEncode)
        : m_bValid(bEncode)
        , m_bRegistered(bEncode)
        , m_cTriesLeft(3 /* the initial tries value */)
    {
        bEncode ? encode(strData) : decode(strData);
    }

    bool isValid() const { return m_bValid; }
    bool isRegistered() const { return m_bRegistered; }

    const QString &data() const { return m_strData; }
    const QString &account() const { return m_strAccount; }

    uint triesLeft() const { return m_cTriesLeft; }

private:

    void decode(const QString &strData)
    {
        if (strData.isEmpty())
            return;

        m_strData = strData;

        /* Decoding number of left tries */
        if (m_strData.startsWith("triesLeft="))
        {
            bool ok = false;
            uint cTriesLeft = m_strData.section('=', 1, 1).toUInt(&ok);
            if (!ok)
                return;
            m_cTriesLeft = cTriesLeft;
            m_bValid = true;
            return;
        }

        /* Decoding CRC32 */
        QString strDataCopy = m_strData;
        QString strCrcData = strDataCopy.right(2 * sizeof(ulong));
        ulong uCrcNeed = 0;
        for (long i = 0; i < strCrcData.length(); i += 2)
        {
            uCrcNeed <<= 8;
            uchar uCurByte = (uchar)strCrcData.mid(i, 2).toUShort(0, 16);
            uCrcNeed += uCurByte;
        }
        strDataCopy.truncate(strDataCopy.length() - 2 * sizeof(ulong));

        /* Decoding data */
        QString strResult;
        for (long i = 0; i < strDataCopy.length(); i += 4)
            strResult += QChar(strDataCopy.mid(i, 4).toUShort(0, 16));
        ulong uCrcNow = crc32((uchar*)strResult.toAscii().constData(), strResult.length());

        /* Check the CRC32 */
        if (uCrcNeed != uCrcNow)
            return;

        /* Split values list */
        QStringList list(strResult.split('|'));

        /* Ignore the old format */
        if (list.size() > 1)
            return;

        /* Result value */
        m_bValid = true;
        m_bRegistered = true;
        m_strAccount = list[0];
    }

    void encode(const QString &strAccount)
    {
        if (strAccount.isEmpty())
            return;

        m_strAccount = strAccount;

        /* Encoding data */
        QString strData = m_strAccount;
        m_strData = QString::null;
        for (long i = 0; i < strData.length(); ++ i)
        {
            QString strCurPair = QString::number(strData.at(i).unicode(), 16);
            while (strCurPair.length() < 4)
                strCurPair.prepend('0');
            m_strData += strCurPair;
        }

        /* Enconding CRC32 */
        ulong uCrcNow = crc32((uchar*)strData.toAscii().constData(), strData.length());
        QString strCrcData;
        for (ulong i = 0; i < sizeof(ulong); ++ i)
        {
            ushort uCurByte = uCrcNow & 0xFF;
            QString strCurPair = QString::number(uCurByte, 16);
            if (strCurPair.length() == 1)
                strCurPair.prepend('0');
            strCrcData = strCurPair + strCrcData;
            uCrcNow >>= 8;
        }

        m_strData += strCrcData;
    }

    ulong crc32(unsigned char *strBufer, int iSize)
    {
        /* Filling CRC32 table */
        ulong uCrc32;
        ulong aCrcTable[256];
        ulong uTemp;
        for (int i = 0; i < 256; ++ i)
        {
            uTemp = i;
            for (int j = 8; j > 0; -- j)
            {
                if (uTemp & 1)
                    uTemp = (uTemp >> 1) ^ 0xedb88320;
                else
                    uTemp >>= 1;
            };
            aCrcTable[i] = uTemp;
        }

        /* CRC32 calculation */
        uCrc32 = 0xffffffff;
        for (int i = 0; i < iSize; ++ i)
        {
            uCrc32 = aCrcTable[(uCrc32 ^ (*strBufer ++)) & 0xff] ^ (uCrc32 >> 8);
        }
        uCrc32 = uCrc32 ^ 0xffffffff;
        return uCrc32;
    };

    bool m_bValid : 1;
    bool m_bRegistered : 1;

    uint m_cTriesLeft;

    QString m_strAccount;
    QString m_strData;
};

/**
 *  This class is used to perform the registration actions.
 */
class RegistrationEngine : public QEventLoop
{
    Q_OBJECT;

public:

    RegistrationEngine(QWidget *pParent) : QEventLoop(pParent)
    {
        /* Registration address */
        m_Url = QUrl("http://registration.virtualbox.org/register763.php");

        /* HTTP Engine */
        m_pHttp = new QIHttp(this, m_Url.host());

        /* Initial values */
        m_bPresent = false;
        m_bResult = false;
    }

    void setOldData(const QString &strVersion, const QString &strPlatform,
                    const QString &strOldEmail, const QString &strOldPassword)
    {
        /* Case for present account */
        resetFields();
        m_bPresent = true;
        m_strVersion = strVersion;
        m_strPlatform = strPlatform;
        m_strOldEmail = strOldEmail;
        m_strOldPassword = strOldPassword;
    }

    void setNewData(const QString &strVersion, const QString &strPlatform,
                    const QString &strNewEmail, const QString &strNewPassword,
                    const QString &strNewFirstName, const QString &strNewLastName,
                    const QString &strNewCompany, const QString &strNewCountry)
    {
        /* Case for account creation */
        resetFields();
        m_bPresent = false;
        m_strVersion = strVersion;
        m_strPlatform = strPlatform;
        m_strNewEmail = strNewEmail;
        m_strNewPassword = strNewPassword;
        m_strNewFirstName = strNewFirstName;
        m_strNewLastName = strNewLastName;
        m_strNewCompany = strNewCompany;
        m_strNewCountry = strNewCountry;
    }

    int start()
    {
        /* Request handshake */
        QTimer::singleShot(0, this, SLOT(sltHandshakeStart()));
        return QEventLoop::exec(QEventLoop::AllEvents);
    }

    bool result() const
    {
        return m_bResult;
    }

private slots:

    void sltHandshakeStart()
    {
        /* Compose query */
        QUrl url(m_Url);
        url.addQueryItem("version", m_strVersion);

        /* Handshake */
        connect(m_pHttp, SIGNAL(allIsDone(bool)), this, SLOT(sltHandshakeResponse(bool)));
        m_pHttp->post(url.toEncoded());
    }

    void sltHandshakeResponse(bool bError)
    {
        /* Block all the other incoming signals */
        m_pHttp->disconnect(this);

        /* Process error if present */
        if (bError)
            return abortRequest(m_pHttp->errorString());

        /* Read received key */
        m_strKey = m_pHttp->readAll();

        /* Verifying key correctness */
        if (QString(m_strKey).indexOf(QRegExp("^[a-zA-Z0-9]{32}$")))
            return abortRequest(tr("Could not perform connection handshake."));

        /* Request registration */
        QTimer::singleShot(0, this, SLOT(sltRegistrationStart()));
    }

    void sltRegistrationStart()
    {
        /* Compose query */
        QUrl url(m_Url);

        /* Basic set */
        url.addQueryItem("version", m_strVersion);
        url.addQueryItem("key", m_strKey);
        url.addQueryItem("platform", m_strPlatform);

        if (m_bPresent)
        {
            /* Set for present account */
            url.addQueryItem("email", m_strOldEmail);
            url.addQueryItem("password", m_strOldPassword);
        }
        else
        {
            /* Set for account creation */
            url.addQueryItem("email", m_strNewEmail);
            url.addQueryItem("password", m_strNewPassword);
            url.addQueryItem("firstname", m_strNewFirstName);
            url.addQueryItem("lastname", m_strNewLastName);
            url.addQueryItem("company", m_strNewCompany);
            url.addQueryItem("country", m_strNewCountry);
        }

        /* Registration */
        connect(m_pHttp, SIGNAL(allIsDone(bool)), this, SLOT(sltRegistrationResponse(bool)));
        m_pHttp->post(url.toEncoded());
    }

    void sltRegistrationResponse(bool bError)
    {
        /* Block all the other incoming signals */
        m_pHttp->disconnect(this);

        /* Process error if present */
        if (bError)
            return abortRequest(m_pHttp->errorString());

        /* Read received result */
        m_strData = m_pHttp->readAll();

        /* Show registration result */
        msgCenter().showRegisterResult(qobject_cast<QWidget*>(parent()), m_strData);

        /* Set result */
        m_bResult = m_strData == "OK";

        /* Allows all the queued events to be processed before leaving loop */
        QTimer::singleShot(0, this, SLOT(quit()));
    }

private:

    void resetFields()
    {
        m_bResult = false;
        m_bPresent = false;
        m_strVersion = QString();
        m_strKey = QString();
        m_strPlatform = QString();
        m_strOldEmail = QString();
        m_strOldPassword = QString();
        m_strNewEmail = QString();
        m_strNewPassword = QString();
        m_strNewFirstName = QString();
        m_strNewLastName = QString();
        m_strNewCompany = QString();
        m_strNewCountry = QString();
        m_strData = QString();
    }

    void abortRequest(const QString &strReason)
    {
        /* Show reason if present */
        if (!strReason.isNull())
            msgCenter().cannotConnectRegister(qobject_cast<QWidget*>(parent()), m_Url.toString(), strReason);

        /* Result = FAILED */
        m_bResult = false;

        /* Allows all the queued events to be processed before leaving loop */
        QTimer::singleShot(0, this, SLOT(quit()));
    }

    QUrl m_Url;
    QIHttp *m_pHttp;

    bool m_bResult;
    bool m_bPresent;

    QString m_strVersion;
    QString m_strKey;
    QString m_strPlatform;
    QString m_strOldEmail;
    QString m_strOldPassword;
    QString m_strNewEmail;
    QString m_strNewPassword;
    QString m_strNewFirstName;
    QString m_strNewLastName;
    QString m_strNewCompany;
    QString m_strNewCountry;
    QString m_strData;
};

/* Check if registration dialog necessary */
bool UIRegistrationWzd::hasToBeShown()
{
    RegistrationData data(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_RegistrationData), false);
    return (!data.isValid()) || (!data.isRegistered() && data.triesLeft() > 0);
}

UIRegistrationWzd::UIRegistrationWzd(UIRegistrationWzd **ppSelf)
    : QIWizard(0)
    , m_ppThis(ppSelf)
{
    /* Store external pointer to this dialog */
    *m_ppThis = this;

    /* Apply window icons */
    setWindowIcon(vboxGlobal().iconSetFull(QSize(32, 32), QSize(16, 16),
                                           ":/register_32px.png", ":/register_16px.png"));

    /* Create & add page */
    addPage(new UIRegistrationWzdPage1);

    /* Initial translate */
    retranslateUi();

    /* Initial translate all pages */
    retranslateAllPages();

    /* Resize to 'golden ratio' */
    resizeToGoldenRatio();

#ifdef Q_WS_MAC
    /* Assign background image */
    assignBackground(":/vmw_new_user_bg.png");
#else /* Q_WS_MAC */
    /* Assign watermark */
    assignWatermark(":/vmw_new_user.png");
#endif /* Q_WS_MAC */

    /* Setup 'closing' connection if main window is VBoxSelectorWnd: */
    if (vboxGlobal().mainWindow() && vboxGlobal().mainWindow()->inherits("VBoxSelectorWnd"))
        connect(vboxGlobal().mainWindow(), SIGNAL(closing()), this, SLOT(reject()));
}

UIRegistrationWzd::~UIRegistrationWzd()
{
    /* Erase dialog handle in config file */
    vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_RegistrationDlgWinID, QString::null);

    /* Erase external pointer to this dialog */
    *m_ppThis = 0;
}

void UIRegistrationWzd::retranslateUi()
{
    /* Wizard title */
    setWindowTitle(tr("VirtualBox Registration Dialog"));
}

void UIRegistrationWzd::reject()
{
    /* Decrement the triesLeft */
    RegistrationData data(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_RegistrationData), false);
    if (!data.isValid() || !data.isRegistered())
    {
        uint cTriesLeft = data.triesLeft();
        if (cTriesLeft)
        {
            -- cTriesLeft;
            vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_RegistrationData, QString("triesLeft=%1").arg(cTriesLeft));
        }
    }
    /* And close the dialog */
    QWizard::reject();
}

UIRegistrationWzdPage1::UIRegistrationWzdPage1()
{
    /* Decorate page */
    Ui::UIRegistrationWzdPage1::setupUi(this);

    /* Setup fields validations */
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

    m_pLeOldEmail->setMaxLength(50);
    /* New accounts *must* have a valid email as user name. This not the case
     * for old existing accounts. So we don't force the email format, so that
     * old accounts could be used for registration also. */
    m_pLeOldEmail->setValidator(new QRegExpValidator(nameExp, this));

    m_pLeOldPassword->setMaxLength(20);
    m_pLeOldPassword->setValidator(new QRegExpValidator(passwordExp, this));

    m_pLeNewFirstName->setMaxLength(50);
    m_pLeNewFirstName->setValidator(new QRegExpValidator (nameExp, this));

    m_pLeNewLastName->setMaxLength(50);
    m_pLeNewLastName->setValidator(new QRegExpValidator(nameExp, this));

    m_pLeNewCompany->setMaxLength(50);
    m_pLeNewCompany->setValidator(new QRegExpValidator(nameExp, this));

    m_pLeNewEmail->setMaxLength(50);
    m_pLeNewEmail->setValidator(new QRegExpValidator(emailExp, this));

    m_pLeNewPassword->setMaxLength(20);
    m_pLeNewPassword->setValidator(new QRegExpValidator(passwordExp, this));

    m_pLeNewConfirmPassword->setMaxLength(20);
    m_pLeNewConfirmPassword->setValidator(new QRegExpValidator(passwordExp, this));

    /* Populate countries list */
    populateCountries();

    /* Setup connections */
    connect(m_pRbPresent, SIGNAL(toggled(bool)), this, SLOT(sltAccountTypeChanged()));
    connect(m_pRbCreate, SIGNAL(toggled(bool)), this, SLOT(sltAccountTypeChanged()));
    connect(m_pLeOldEmail, SIGNAL(textEdited(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pLeOldPassword, SIGNAL(textEdited(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pLeNewFirstName, SIGNAL(textEdited(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pLeNewLastName, SIGNAL(textEdited(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pLeNewCompany, SIGNAL(textEdited(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pCbNewCountry, SIGNAL(currentIndexChanged(int)), this, SIGNAL(completeChanged()));
    connect(m_pLeNewEmail, SIGNAL(textEdited(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pLeNewPassword, SIGNAL(textEdited(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pLeNewConfirmPassword, SIGNAL(textEdited(const QString &)), this, SIGNAL(completeChanged()));

    /* Initial values */
    m_pRbPresent->click();
    RegistrationData data(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_RegistrationData), false);
    m_pLeOldEmail->setText(data.account());
    m_pLeOldEmail->setFocus();

    /* Fill and translate */
    retranslateUi();
}

void UIRegistrationWzdPage1::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIRegistrationWzdPage1::retranslateUi(this);

    /* Wizard page 1 title */
    setTitle(tr("Welcome to the VirtualBox Registration Form!"));

    /* Translate the first element of countries list */
    m_pCbNewCountry->setItemText(0, tr("Select Country/Territory"));
}

bool UIRegistrationWzdPage1::isComplete() const
{
    if (m_pRbPresent->isChecked())
    {
        /* Check for fields correctness */
        if (!isFieldValid(m_pLeOldEmail) || !isFieldValid(m_pLeOldPassword))
            return false;
    }
    if (m_pRbCreate->isChecked())
    {
        /* Check for fields correctness */
        if (!isFieldValid (m_pLeNewFirstName) || !isFieldValid (m_pLeNewLastName) ||
            !isFieldValid (m_pLeNewCompany) || !isFieldValid (m_pCbNewCountry) ||
            !isFieldValid (m_pLeNewEmail) ||
            !isFieldValid (m_pLeNewPassword) || !isFieldValid (m_pLeNewConfirmPassword))
            return false;

        /* Check for password correctness */
        if (m_pLeNewPassword->text() != m_pLeNewConfirmPassword->text())
            return false;
    }
    return true;
}

bool UIRegistrationWzdPage1::validatePage()
{
    startProcessing();
    bool fResult = registration();
    endProcessing();
    return fResult;
}

bool UIRegistrationWzdPage1::registration()
{
    /* Create registration engine */
    RegistrationEngine engine(this);

    /* Initialize registration engine */
    if (m_pRbPresent->isChecked())
    {
        engine.setOldData(vboxGlobal().virtualBox().GetVersion(), vboxGlobal().platformInfo(),
                          m_pLeOldEmail->text(), m_pLeOldPassword->text());
    }
    if (m_pRbCreate->isChecked())
    {
        engine.setNewData(vboxGlobal().virtualBox().GetVersion(), vboxGlobal().platformInfo(),
                          m_pLeNewEmail->text(), m_pLeNewPassword->text(),
                          m_pLeNewFirstName->text(), m_pLeNewLastName->text(),
                          m_pLeNewCompany->text(), m_pCbNewCountry->currentText());
    }

    /* Disable wizard */
    wizard()->setEnabled(false);

    /* Set busy cursor */
    setCursor(QCursor(Qt::BusyCursor));

    /* Perform registration routine */
    engine.start();

    /* Restore cursor */
    unsetCursor();

    /* Enable wizard */
    wizard()->setEnabled(true);

    /* Save accepted account name */
    if (engine.result())
    {
        /* Store registration data */
        QString strAccount = m_pRbPresent->isChecked() ? m_pLeOldEmail->text() :
                             m_pRbCreate->isChecked() ? m_pLeNewEmail->text() : QString();
        RegistrationData data(strAccount, true);
        vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_RegistrationData, data.data());
    }

    return engine.result();
}

void UIRegistrationWzdPage1::sltAccountTypeChanged()
{
    bool bPresentChosen = m_pRbPresent->isChecked();
    bool bCreateChosen = m_pRbCreate->isChecked();

    m_pLeOldEmail->setEnabled(bPresentChosen);
    m_pLeOldPassword->setEnabled(bPresentChosen);
    m_pLeNewFirstName->setEnabled(bCreateChosen);
    m_pLeNewLastName->setEnabled(bCreateChosen);
    m_pLeNewCompany->setEnabled(bCreateChosen);
    m_pCbNewCountry->setEnabled(bCreateChosen);
    m_pLeNewEmail->setEnabled(bCreateChosen);
    m_pLeNewPassword->setEnabled(bCreateChosen);
    m_pLeNewConfirmPassword->setEnabled(bCreateChosen);

    emit completeChanged();
}

void UIRegistrationWzdPage1::populateCountries()
{
    QStringList list("Empty");
    list << "Afghanistan"
         << "Albania"
         << "Algeria"
         << "American Samoa"
         << "Andorra"
         << "Angola"
         << "Anguilla"
         << "Antarctica"
         << "Antigua & Barbuda"
         << "Argentina"
         << "Armenia"
         << "Aruba"
         << "Ascension Island"
         << "Australia"
         << "Austria"
         << "Azerbaijan"
         << "Bahamas"
         << "Bahrain"
         << "Bangladesh"
         << "Barbados"
         << "Belarus"
         << "Belgium"
         << "Belize"
         << "Benin"
         << "Bermuda"
         << "Bhutan"
         << "Bolivia"
         << "Bosnia and Herzegovina"
         << "Botswana"
         << "Bouvet Island"
         << "Brazil"
         << "British Indian Ocean Territory"
         << "Brunei Darussalam"
         << "Bulgaria"
         << "Burkina Faso"
         << "Burundi"
         << "Cambodia"
         << "Cameroon"
         << "Canada"
         << "Cape Verde"
         << "Cayman Islands"
         << "Central African Republic"
         << "Chad"
         << "Chile"
         << "China"
         << "Christmas Island"
         << "Cocos (Keeling) Islands"
         << "Colombia"
         << "Comoros"
         << "Congo, Democratic People's Republic"
         << "Congo, Republic of"
         << "Cook Islands"
         << "Costa Rica"
         << "Cote d'Ivoire"
         << "Croatia/Hrvatska"
         << "Cyprus"
         << "Czech Republic"
         << "Denmark"
         << "Djibouti"
         << "Dominica"
         << "Dominican Republic"
         << "East Timor"
         << "Ecuador"
         << "Egypt"
         << "El Salvador"
         << "Equatorial Guinea"
         << "Eritrea"
         << "Estonia"
         << "Ethiopia"
         << "Falkland Islands (Malvina)"
         << "Faroe Islands"
         << "Fiji"
         << "Finland"
         << "France"
         << "French Guiana"
         << "French Polynesia"
         << "French Southern Territories"
         << "Gabon"
         << "Gambia"
         << "Georgia"
         << "Germany"
         << "Ghana"
         << "Gibraltar"
         << "Greece"
         << "Greenland"
         << "Grenada"
         << "Guadeloupe"
         << "Guam"
         << "Guatemala"
         << "Guernsey"
         << "Guinea"
         << "Guinea-Bissau"
         << "Guyana"
         << "Haiti"
         << "Heard and McDonald Islands"
         << "Holy See (City Vatican State)"
         << "Honduras"
         << "Hong Kong"
         << "Hungary"
         << "Iceland"
         << "India"
         << "Indonesia"
         << "Iraq"
         << "Ireland"
         << "Isle of Man"
         << "Israel"
         << "Italy"
         << "Jamaica"
         << "Japan"
         << "Jersey"
         << "Jordan"
         << "Kazakhstan"
         << "Kenya"
         << "Kiribati"
         << "Korea, Republic of"
         << "Kuwait"
         << "Kyrgyzstan"
         << "Lao People's Democratic Republic"
         << "Latvia"
         << "Lebanon"
         << "Lesotho"
         << "Liberia"
         << "Libyan Arab Jamahiriya"
         << "Liechtenstein"
         << "Lithuania"
         << "Luxembourg"
         << "Macau"
         << "Macedonia, Former Yugoslav Republic"
         << "Madagascar"
         << "Malawi"
         << "Malaysia"
         << "Maldives"
         << "Mali"
         << "Malta"
         << "Marshall Islands"
         << "Martinique"
         << "Mauritania"
         << "Mauritius"
         << "Mayotte"
         << "Mexico"
         << "Micronesia, Federal State of"
         << "Moldova, Republic of"
         << "Monaco"
         << "Mongolia"
         << "Montenegro"
         << "Montserrat"
         << "Morocco"
         << "Mozambique"
         << "Namibia"
         << "Nauru"
         << "Nepal"
         << "Netherlands"
         << "Netherlands Antilles"
         << "New Caledonia"
         << "New Zealand"
         << "Nicaragua"
         << "Niger"
         << "Nigeria"
         << "Niue"
         << "Norfolk Island"
         << "Northern Mariana Island"
         << "Norway"
         << "Oman"
         << "Pakistan"
         << "Palau"
         << "Panama"
         << "Papua New Guinea"
         << "Paraguay"
         << "Peru"
         << "Philippines"
         << "Pitcairn Island"
         << "Poland"
         << "Portugal"
         << "Puerto Rico"
         << "Qatar"
         << "Reunion Island"
         << "Romania"
         << "Russian Federation"
         << "Rwanda"
         << "Saint Kitts and Nevis"
         << "Saint Lucia"
         << "Saint Vincent and the Grenadines"
         << "San Marino"
         << "Sao Tome & Principe"
         << "Saudi Arabia"
         << "Senegal"
         << "Serbia"
         << "Seychelles"
         << "Sierra Leone"
         << "Singapore"
         << "Slovak Republic"
         << "Slovenia"
         << "Solomon Islands"
         << "Somalia"
         << "South Africa"
         << "South Georgia and the South Sandwich Islands"
         << "Spain"
         << "Sri Lanka"
         << "St Pierre and Miquelon"
         << "St. Helena"
         << "Suriname"
         << "Svalbard And Jan Mayen Island"
         << "Swaziland"
         << "Sweden"
         << "Switzerland"
         << "Taiwan"
         << "Tajikistan"
         << "Tanzania"
         << "Thailand"
         << "Togo"
         << "Tokelau"
         << "Tonga"
         << "Trinidad and Tobago"
         << "Tunisia"
         << "Turkey"
         << "Turkmenistan"
         << "Turks and Ciacos Islands"
         << "Tuvalu"
         << "US Minor Outlying Islands"
         << "Uganda"
         << "Ukraine"
         << "United Arab Emirates"
         << "United Kingdom"
         << "United States"
         << "Uruguay"
         << "Uzbekistan"
         << "Vanuatu"
         << "Venezuela"
         << "Vietnam"
         << "Virgin Island (British)"
         << "Virgin Islands (USA)"
         << "Wallis And Futuna Islands"
         << "Western Sahara"
         << "Western Samoa"
         << "Yemen"
         << "Zambia"
         << "Zimbabwe";
    m_pCbNewCountry->addItems(list);
}

bool UIRegistrationWzdPage1::isFieldValid(QWidget *pWidget) const
{
    if (QLineEdit *pLineEdit = qobject_cast<QLineEdit*>(pWidget))
    {
        QString strText(pLineEdit->text());
        int iPosition;
        return pLineEdit->validator()->validate(strText, iPosition) == QValidator::Acceptable;
    }
    else if (QComboBox *pComboBox = qobject_cast<QComboBox*>(pWidget))
    {
        return pComboBox->currentIndex() > 0;
    }
    return false;
}

#include "UIRegistrationWzd.moc"

