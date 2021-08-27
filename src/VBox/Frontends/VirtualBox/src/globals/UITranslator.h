/* $Id$ */
/** @file
 * VBox Qt GUI - UITranslator class declaration.
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

#ifndef FEQT_INCLUDED_SRC_globals_UITranslator_h
#define FEQT_INCLUDED_SRC_globals_UITranslator_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QTranslator>

/** QTranslator subclass for VBox needs. */
class UITranslator : public QTranslator
{
    Q_OBJECT;

public:

    /** Loads the language by language ID.
      * @param  strLangId  Brings the language ID in in form of xx_YY.
      *                    QString() means the system default language. */
    static void loadLanguage(const QString &strLangId = QString());

    /** Returns VBox language sub-directory. */
    static QString vboxLanguageSubDirectory();
    /** Returns VBox language file-base. */
    static QString vboxLanguageFileBase();
    /** Returns VBox language file-extension. */
    static QString vboxLanguageFileExtension();
    /** Returns VBox language ID reg-exp. */
    static QString vboxLanguageIdRegExp();
    /** Returns built in language name. */
    static QString vboxBuiltInLanguageName();

    /** Returns the loaded (active) language ID. */
    static QString languageId();

private:

    /** Constructs translator passing @a pParent to the base-class. */
    UITranslator(QObject *pParent = 0);

    /** Loads language file with gained @a strFileName. */
    bool loadFile(const QString &strFileName);

    /** Native language name of the currently installed translation. */
    static QString languageName();
    /** Native language country name of the currently installed translation. */
    static QString languageCountry();
    /** Language name of the currently installed translation, in English. */
    static QString languageNameEnglish();
    /** Language country name of the currently installed translation, in English. */
    static QString languageCountryEnglish();
    /** Comma-separated list of authors of the currently installed translation. */
    static QString languageTranslators();

    /** Returns the system language ID. */
    static QString systemLanguageId();

    /** Holds the singleton instance. */
    static UITranslator *s_pTranslator;
    /** Holds the currently loaded language ID. */
    static QString       s_strLoadedLanguageId;

    /** Holds the loaded data. */
    QByteArray  m_data;
};

#endif /* !FEQT_INCLUDED_SRC_globals_UITranslator_h */

