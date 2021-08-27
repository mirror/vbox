/* $Id$ */
/** @file
 * VBox Qt GUI - UITranslator class implementation.
 */

/*
 * Copyright (C) 2006-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QApplication>
#include <QDir>

/* GUI includes: */
#include "UIMessageCenter.h"
#include "UITranslator.h"

/* Other VBox includes: */
#include <iprt/assert.h>
#include <iprt/path.h>

/* static */
UITranslator *UITranslator::s_pTranslator = 0;
QString UITranslator::s_strLoadedLanguageId = UITranslator::vboxBuiltInLanguageName();

/* static */
void UITranslator::loadLanguage(const QString &strLangId /* = QString() */)
{
    QString strEffectiveLangId = strLangId.isEmpty()
                               ? systemLanguageId()
                               : strLangId;
    QString strLanguageFileName;
    QString strSelectedLangId = vboxBuiltInLanguageName();

    /* If C is selected we change it temporary to en. This makes sure any extra
     * "en" translation file will be loaded. This is necessary for loading the
     * plural forms of some of our translations. */
    bool fResetToC = false;
    if (strEffectiveLangId == "C")
    {
        strEffectiveLangId = "en";
        fResetToC = true;
    }

    char szNlsPath[RTPATH_MAX];
    int rc;

    rc = RTPathAppPrivateNoArch(szNlsPath, sizeof(szNlsPath));
    AssertRC(rc);

    QString strNlsPath = QString(szNlsPath) + vboxLanguageSubDirectory();
    QDir nlsDir(strNlsPath);

    Assert(!strEffectiveLangId.isEmpty());
    if (!strEffectiveLangId.isEmpty() && strEffectiveLangId != vboxBuiltInLanguageName())
    {
        QRegExp regExp(vboxLanguageIdRegExp());
        int iPos = regExp.indexIn(strEffectiveLangId);
        /* The language ID should match the regexp completely: */
        AssertReturnVoid(iPos == 0);

        QString strStrippedLangId = regExp.cap(2);

        if (nlsDir.exists(vboxLanguageFileBase() + strEffectiveLangId + vboxLanguageFileExtension()))
        {
            strLanguageFileName = nlsDir.absoluteFilePath(vboxLanguageFileBase() +
                                                          strEffectiveLangId +
                                                          vboxLanguageFileExtension());
            strSelectedLangId = strEffectiveLangId;
        }
        else if (nlsDir.exists(vboxLanguageFileBase() + strStrippedLangId + vboxLanguageFileExtension()))
        {
            strLanguageFileName = nlsDir.absoluteFilePath(vboxLanguageFileBase() +
                                                          strStrippedLangId +
                                                          vboxLanguageFileExtension());
            strSelectedLangId = strStrippedLangId;
        }
        else
        {
            /* Never complain when the default language is requested.  In any
             * case, if no explicit language file exists, we will simply
             * fall-back to English (built-in). */
            if (!strLangId.isNull() && strEffectiveLangId != "en")
                msgCenter().cannotFindLanguage(strEffectiveLangId, strNlsPath);
            /* strSelectedLangId remains built-in here: */
            AssertReturnVoid(strSelectedLangId == vboxBuiltInLanguageName());
        }
    }

    /* Delete the old translator if there is one: */
    if (s_pTranslator)
    {
        /* QTranslator destructor will call qApp->removeTranslator() for
         * us. It will also delete all its child translations we attach to it
         * below, so we don't have to care about them specially. */
        delete s_pTranslator;
    }

    /* Load new language files: */
    s_pTranslator = new UITranslator(qApp);
    Assert(s_pTranslator);
    bool fLoadOk = true;
    if (s_pTranslator)
    {
        if (strSelectedLangId != vboxBuiltInLanguageName())
        {
            Assert(!strLanguageFileName.isNull());
            fLoadOk = s_pTranslator->loadFile(strLanguageFileName);
        }
        /* We install the translator in any case: on failure, this will
         * activate an empty translator that will give us English (built-in): */
        qApp->installTranslator(s_pTranslator);
    }
    else
        fLoadOk = false;

    if (fLoadOk)
        s_strLoadedLanguageId = strSelectedLangId;
    else
    {
        msgCenter().cannotLoadLanguage(strLanguageFileName);
        s_strLoadedLanguageId = vboxBuiltInLanguageName();
    }

    /* Try to load the corresponding Qt translation: */
    if (languageId() != vboxBuiltInLanguageName() && languageId() != "en")
    {
#ifdef Q_OS_UNIX
        // We use system installations of Qt on Linux systems, so first, try
        // to load the Qt translation from the system location.
        strLanguageFileName = QLibraryInfo::location(QLibraryInfo::TranslationsPath) + "/qt_" +
                              languageId() + vboxLanguageFileExtension();
        QTranslator *pQtSysTr = new QTranslator(s_pTranslator);
        Assert(pQtSysTr);
        if (pQtSysTr && pQtSysTr->load(strLanguageFileName))
            qApp->installTranslator(pQtSysTr);
        // Note that the Qt translation supplied by Oracle is always loaded
        // afterwards to make sure it will take precedence over the system
        // translation (it may contain more decent variants of translation
        // that better correspond to VirtualBox UI). We need to load both
        // because a newer version of Qt may be installed on the user computer
        // and the Oracle version may not fully support it. We don't do it on
        // Win32 because we supply a Qt library there and therefore the
        // Oracle translation is always the best one. */
#endif
        strLanguageFileName = nlsDir.absoluteFilePath(QString("qt_") +
                                                      languageId() +
                                                      vboxLanguageFileExtension());
        QTranslator *pQtTr = new QTranslator(s_pTranslator);
        Assert(pQtTr);
        if (pQtTr && (fLoadOk = pQtTr->load(strLanguageFileName)))
            qApp->installTranslator(pQtTr);
        /* The below message doesn't fit 100% (because it's an additional
         * language and the main one won't be reset to built-in on failure)
         * but the load failure is so rare here that it's not worth a separate
         * message (but still, having something is better than having none) */
        if (!fLoadOk && !strLangId.isNull())
            msgCenter().cannotLoadLanguage(strLanguageFileName);
    }
    if (fResetToC)
        s_strLoadedLanguageId = vboxBuiltInLanguageName();
#ifdef VBOX_WS_MAC
    // Qt doesn't translate the items in the Application menu initially.
    // Manually trigger an update.
    ::darwinRetranslateAppMenu();
#endif
}

/* static */
QString UITranslator::vboxLanguageSubDirectory()
{
    return "/nls";
}

/* static */
QString UITranslator::vboxLanguageFileBase()
{
    return "VirtualBox_";
}

/* static */
QString UITranslator::vboxLanguageFileExtension()
{
    return ".qm";
}

/* static */
QString UITranslator::vboxLanguageIdRegExp()
{
    return "(([a-z]{2})(?:_([A-Z]{2}))?)|(C)";
}

/* static */
QString UITranslator::vboxBuiltInLanguageName()
{
    return "C";
}

/* static */
QString UITranslator::languageId()
{
    /* Note that it may not match with UIExtraDataManager::languageId() if the specified language cannot be loaded.
     *
     * If the built-in language is active, this method returns "C". "C" is treated as the built-in language for
     * simplicity -- the C locale is used in unix environments as a fallback when the requested locale is invalid.
     * This way we don't need to process both the "built_in" language and the "C" language (which is a valid
     * environment setting) separately. */

    return s_strLoadedLanguageId;
}

UITranslator::UITranslator(QObject *pParent /* = 0 */)
    : QTranslator(pParent)
{
}

bool UITranslator::loadFile(const QString &strFileName)
{
    QFile file(strFileName);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    m_data = file.readAll();
    return load((uchar*)m_data.data(), m_data.size());
}

/* static */
QString UITranslator::languageName()
{
    /* Returns "English" if no translation is installed
     * or if the translation file is invalid. */
    return QApplication::translate("@@@", "English",
                                   "Native language name");
}

/* static */
QString UITranslator::languageCountry()
{
    /* Returns "--" if no translation is installed or if the translation file
     * is invalid, or if the language is independent on the country. */
    return QApplication::translate("@@@", "--",
                                   "Native language country name "
                                   "(empty if this language is for all countries)");
}

/* static */
QString UITranslator::languageNameEnglish()
{
    /* Returns "English" if no translation is installed
     * or if the translation file is invalid. */
    return QApplication::translate("@@@", "English",
                                   "Language name, in English");
}

/* static */
QString UITranslator::languageCountryEnglish()
{
    /* Returns "--" if no translation is installed or if the translation file
     * is invalid, or if the language is independent on the country. */
    return QApplication::translate("@@@", "--",
                                   "Language country name, in English "
                                   "(empty if native country name is empty)");
}

/* static */
QString UITranslator::languageTranslators()
{
    /* Returns "Oracle Corporation" if no translation is installed or if the translation file
     * is invalid, or if the translation is supplied by Oracle Corporation. */
    return QApplication::translate("@@@", "Oracle Corporation",
                                   "Comma-separated list of translators");
}

/* static */
QString UITranslator::systemLanguageId()
{
    /* This does exactly the same as QLocale::system().name() but corrects its wrong behavior on Linux systems
     * (LC_NUMERIC for some strange reason takes precedence over any other locale setting in the QLocale::system()
     * implementation). This implementation first looks at LC_ALL (as defined by SUS), then looks at LC_MESSAGES
     * which is designed to define a language for program messages in case if it differs from the language for
     * other locale categories. Then it looks for LANG and finally falls back to QLocale::system().name().
     *
     * The order of precedence is well defined here:
     * http://opengroup.org/onlinepubs/007908799/xbd/envvar.html
     *
     * This method will return "C" when the requested locale is invalid or when the "C" locale is set explicitly. */

#if defined(VBOX_WS_MAC)
    // QLocale return the right id only if the user select the format
    // of the language also. So we use our own implementation */
    return ::darwinSystemLanguage();
#elif defined(Q_OS_UNIX)
    const char *pszValue = RTEnvGet("LC_ALL");
    if (pszValue == 0)
        pszValue = RTEnvGet("LC_MESSAGES");
    if (pszValue == 0)
        pszValue = RTEnvGet("LANG");
    if (pszValue != 0)
        return QLocale(pszValue).name();
#endif
    return QLocale::system().name();
}
