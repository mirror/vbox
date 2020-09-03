/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsLanguage class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QDir>
#include <QHeaderView>
#include <QGridLayout>
#include <QPainter>
#include <QTranslator>

/* GUI includes: */
#include "QILabelSeparator.h"
#include "QIRichTextLabel.h"
#include "QITreeWidget.h"
#include "UIGlobalSettingsLanguage.h"
#include "UIExtraDataManager.h"
#include "UIMessageCenter.h"
#include "UICommon.h"

/* Other VBox includes: */
#include <iprt/errcore.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <VBox/version.h>


/** Global settings: Language page data structure. */
struct UIDataSettingsGlobalLanguage
{
    /** Constructs data. */
    UIDataSettingsGlobalLanguage()
        : m_strLanguageId(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalLanguage &other) const
    {
        return true
               && (m_strLanguageId == other.m_strLanguageId)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalLanguage &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalLanguage &other) const { return !equal(other); }

    /** Holds the current language id. */
    QString m_strLanguageId;
};


/* Language item: */
class UILanguageItem : public QITreeWidgetItem
{
    Q_OBJECT;

public:

    /* Language item constructor: */
    UILanguageItem(QITreeWidget *pParent, const QTranslator &translator,
                   const QString &strId, bool fBuiltIn = false)
        : QITreeWidgetItem(pParent), m_fBuiltIn(fBuiltIn)
    {
        Assert (!strId.isEmpty());

        /* Note: context/source/comment arguments below must match strings
         * used in UICommon::languageName() and friends (the latter are the
         * source of information for the lupdate tool that generates
         * translation files) */

        QString strNativeLanguage = tratra(translator, "@@@", "English", "Native language name");
        QString strNativeCountry = tratra(translator, "@@@", "--", "Native language country name "
                                                                   "(empty if this language is for all countries)");

        QString strEnglishLanguage = tratra(translator, "@@@", "English", "Language name, in English");
        QString strEnglishCountry = tratra(translator, "@@@", "--", "Language country name, in English "
                                                                    "(empty if native country name is empty)");

        QString strTranslatorsName = tratra(translator, "@@@", "Oracle Corporation", "Comma-separated list of translators");

        QString strItemName = strNativeLanguage;
        QString strLanguageName = strEnglishLanguage;

        if (!m_fBuiltIn)
        {
            if (strNativeCountry != "--")
                strItemName += " (" + strNativeCountry + ")";

            if (strEnglishCountry != "--")
                strLanguageName += " (" + strEnglishCountry + ")";

            if (strItemName != strLanguageName)
                strLanguageName = strItemName + " / " + strLanguageName;
        }
        else
        {
            strItemName += tr(" (built-in)", "Language");
            strLanguageName += tr(" (built-in)", "Language");
        }

        setText(0, strItemName);
        setText(1, strId);
        setText(2, strLanguageName);
        setText(3, strTranslatorsName);

        /* Current language appears in bold: */
        if (text(1) == UICommon::languageId())
        {
            QFont fnt = font(0);
            fnt.setBold(true);
            setFont(0, fnt);
        }
    }

    /* Constructs an item for an invalid language ID
     * (i.e. when a language file is missing or corrupt): */
    UILanguageItem(QITreeWidget *pParent, const QString &strId)
        : QITreeWidgetItem(pParent), m_fBuiltIn(false)
    {
        Assert(!strId.isEmpty());

        setText(0, QString("<%1>").arg(strId));
        setText(1, strId);
        setText(2, tr("<unavailable>", "Language"));
        setText(3, tr("<unknown>", "Author(s)"));

        /* Invalid language appears in italic: */
        QFont fnt = font(0);
        fnt.setItalic(true);
        setFont(0, fnt);
    }

    /* Constructs an item for the default language ID
     * (column 1 will be set to QString::null): */
    UILanguageItem(QITreeWidget *pParent)
        : QITreeWidgetItem(pParent), m_fBuiltIn(false)
    {
        setText(0, tr("Default", "Language"));
        setText(1, QString::null);
        /* Empty strings of some reasonable length to prevent the info part
         * from being shrinked too much when the list wants to be wider */
        setText(2, "                ");
        setText(3, "                ");

        /* Default language item appears in italic: */
        QFont fnt = font(0);
        fnt.setItalic(true);
        setFont(0, fnt);
    }

    bool isBuiltIn() const { return m_fBuiltIn; }

    bool operator<(const QTreeWidgetItem &other) const
    {
        QString thisId = text(1);
        QString thatId = other.text(1);
        if (thisId.isNull())
            return true;
        if (thatId.isNull())
            return false;
        if (m_fBuiltIn)
            return true;
        if (other.type() == ItemType && ((UILanguageItem*)&other)->m_fBuiltIn)
            return false;
        return QITreeWidgetItem::operator<(other);
    }

private:

    QString tratra(const QTranslator &translator, const char *pCtxt,
                   const char *pSrc, const char *pCmnt)
    {
        QString strMsg = translator.translate(pCtxt, pSrc, pCmnt);
        /* Return the source text if no translation is found: */
        if (strMsg.isEmpty())
            strMsg = QString(pSrc);
        return strMsg;
    }

    bool m_fBuiltIn : 1;
};


UIGlobalSettingsLanguage::UIGlobalSettingsLanguage()
    : m_fPolished(false)
    , m_pCache(0)
    , m_pTreeWidget(0)
    , m_pLabelSeparator(0)
    , m_pLabelInfo(0)
{
    /* Prepare: */
    prepare();
}

UIGlobalSettingsLanguage::~UIGlobalSettingsLanguage()
{
    /* Cleanup: */
    cleanup();
}

void UIGlobalSettingsLanguage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old language data: */
    UIDataSettingsGlobalLanguage oldLanguageData;

    /* Gather old language data: */
    oldLanguageData.m_strLanguageId = gEDataManager->languageId();

    /* Cache old language data: */
    m_pCache->cacheInitialData(oldLanguageData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsLanguage::getFromCache()
{
    /* Get old language data from the cache: */
    const UIDataSettingsGlobalLanguage &oldLanguageData = m_pCache->base();

    /* Load old language data from the cache: */
    reloadLanguageTree(oldLanguageData.m_strLanguageId);
}

void UIGlobalSettingsLanguage::putToCache()
{
    /* Prepare new language data: */
    UIDataSettingsGlobalLanguage newInputData = m_pCache->base();

    /* Gather new language data: */
    QTreeWidgetItem *pCurrentItem = m_pTreeWidget->currentItem();
    Assert(pCurrentItem);
    if (pCurrentItem)
        newInputData.m_strLanguageId = pCurrentItem->text(1);

    /* Cache new language data: */
    m_pCache->cacheCurrentData(newInputData);
}

void UIGlobalSettingsLanguage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update language data and failing state: */
    setFailed(!saveLanguageData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsLanguage::retranslateUi()
{
    m_pLabelSeparator->setText(tr("&Interface Languages"));
    QTreeWidgetItem *pTreeWidgetItem = m_pTreeWidget->headerItem();
    if (pTreeWidgetItem)
    {
        pTreeWidgetItem->setText(3, tr("Author"));
        pTreeWidgetItem->setText(2, tr("Language"));
        pTreeWidgetItem->setText(1, tr("Id"));
        pTreeWidgetItem->setText(0, tr("Name"));
    }
    m_pTreeWidget->setWhatsThis(tr("Lists all available user interface "
                                     "languages. The effective language is written in <b>bold</b>. "
                                     "Select <i>Default</i> to reset to the system default language."));

    /* Reload language tree: */
    reloadLanguageTree(UICommon::languageId());
}

void UIGlobalSettingsLanguage::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    UISettingsPageGlobal::showEvent(pEvent);

    /* Polishing border: */
    if (m_fPolished)
        return;
    m_fPolished = true;

    /* Call for polish event: */
    polishEvent(pEvent);
}

void UIGlobalSettingsLanguage::polishEvent(QShowEvent * /* pEvent */)
{
    /* Remember current info-label width: */
    m_pLabelInfo->setMinimumTextWidth(m_pLabelInfo->width());
}

void UIGlobalSettingsLanguage::sltHandleItemPainting(QTreeWidgetItem *pItem, QPainter *pPainter)
{
    if (pItem && pItem->type() == QITreeWidgetItem::ItemType)
    {
        UILanguageItem *pLanguageItem = static_cast<UILanguageItem*>(pItem);
        if (pLanguageItem->isBuiltIn())
        {
            const QRect rect = m_pTreeWidget->visualItemRect(pLanguageItem);
            pPainter->setPen(m_pTreeWidget->palette().color(QPalette::Mid));
            pPainter->drawLine(rect.x(), rect.y() + rect.height() - 1,
                               rect.x() + rect.width(), rect.y() + rect.height() - 1);
        }
    }
}

void UIGlobalSettingsLanguage::sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem)
{
    if (!pCurrentItem)
        return;

    /* Disable labels for the Default language item: */
    const bool fEnabled = !pCurrentItem->text (1).isNull();

    m_pLabelInfo->setEnabled(fEnabled);
    m_pLabelInfo->setText(QString("<table>"
                             "<tr><td>%1&nbsp;</td><td>%2</td></tr>"
                             "<tr><td>%3&nbsp;</td><td>%4</td></tr>"
                             "</table>")
                             .arg(tr("Language:"))
                             .arg(pCurrentItem->text(2))
                             .arg(tr("Author(s):"))
                             .arg(pCurrentItem->text(3)));
}

void UIGlobalSettingsLanguage::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalLanguage;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();
    prepareConnection();

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsLanguage::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        /* Prepare separator: */
        m_pLabelSeparator = new QILabelSeparator(this);
        if (m_pLabelSeparator)
            pLayoutMain->addWidget(m_pLabelSeparator, 0, 0);

        /* Prepare tree-widget: */
        m_pTreeWidget = new QITreeWidget(this);
        if (m_pTreeWidget)
        {
            if (m_pLabelSeparator)
                m_pLabelSeparator->setBuddy(m_pTreeWidget);
            m_pTreeWidget->header()->hide();
            m_pTreeWidget->setColumnCount(4);
            m_pTreeWidget->hideColumn(1);
            m_pTreeWidget->hideColumn(2);
            m_pTreeWidget->hideColumn(3);
            m_pTreeWidget->setRootIsDecorated(false);

            pLayoutMain->addWidget(m_pTreeWidget, 1, 0);
        }

        /* Prepare info label: */
        m_pLabelInfo = new QIRichTextLabel(this);
        if (m_pLabelInfo)
        {
            m_pLabelInfo->setWordWrapMode(QTextOption::WordWrap);
            m_pLabelInfo->setMinimumHeight(QFontMetrics(m_pLabelInfo->font(), m_pLabelInfo).height() * 5);

            pLayoutMain->addWidget(m_pLabelInfo, 2, 0);
        }
    }
}

void UIGlobalSettingsLanguage::prepareConnection()
{
    connect(m_pTreeWidget, &QITreeWidget::painted, this, &UIGlobalSettingsLanguage::sltHandleItemPainting);
    connect(m_pTreeWidget, &QITreeWidget::currentItemChanged, this, &UIGlobalSettingsLanguage::sltHandleCurrentItemChange);
}

void UIGlobalSettingsLanguage::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

void UIGlobalSettingsLanguage::reloadLanguageTree(const QString &strLanguageId)
{
    /* Clear languages tree: */
    m_pTreeWidget->clear();

    /* Load languages tree: */
    char szNlsPath[RTPATH_MAX];
    int rc = RTPathAppPrivateNoArch(szNlsPath, sizeof(szNlsPath));
    AssertRC(rc);
    QString strNlsPath = QString(szNlsPath) + UICommon::vboxLanguageSubDirectory();
    QDir nlsDir(strNlsPath);
    QStringList files = nlsDir.entryList(QStringList(QString("%1*%2").arg(UICommon::vboxLanguageFileBase(),
                                                                          UICommon::vboxLanguageFileExtension())),
                                         QDir::Files);

    QTranslator translator;
    /* Add the default language: */
    new UILanguageItem(m_pTreeWidget);
    /* Add the built-in language: */
    new UILanguageItem(m_pTreeWidget, translator, UICommon::vboxBuiltInLanguageName(), true /* built-in */);
    /* Add all existing languages */
    for (QStringList::Iterator it = files.begin(); it != files.end(); ++it)
    {
        QString strFileName = *it;
        QRegExp regExp(UICommon::vboxLanguageFileBase() + UICommon::vboxLanguageIdRegExp());
        int iPos = regExp.indexIn(strFileName);
        if (iPos == -1)
            continue;

        /* Skip any English version, cause this is extra handled: */
        QString strLanguage = regExp.cap(2);
        if (strLanguage.toLower() == "en")
            continue;

        bool fLoadOk = translator.load(strFileName, strNlsPath);
        if (!fLoadOk)
            continue;

        new UILanguageItem(m_pTreeWidget, translator, regExp.cap(1));
    }

    /* Adjust selector list: */
    m_pTreeWidget->resizeColumnToContents(0);

    /* Search for necessary language: */
    QList<QTreeWidgetItem*> itemsList = m_pTreeWidget->findItems(strLanguageId, Qt::MatchExactly, 1);
    QTreeWidgetItem *pItem = itemsList.isEmpty() ? 0 : itemsList[0];
    if (!pItem)
    {
        /* Add an pItem for an invalid language to represent it in the list: */
        pItem = new UILanguageItem(m_pTreeWidget, strLanguageId);
        m_pTreeWidget->resizeColumnToContents(0);
    }
    Assert(pItem);
    if (pItem)
        m_pTreeWidget->setCurrentItem(pItem);

    m_pTreeWidget->sortItems(0, Qt::AscendingOrder);
    m_pTreeWidget->scrollToItem(pItem);
}

bool UIGlobalSettingsLanguage::saveLanguageData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save language settings from the cache: */
    if (fSuccess && m_pCache->wasChanged())
    {
        /* Get old language data from the cache: */
        const UIDataSettingsGlobalLanguage &oldLanguageData = m_pCache->base();
        /* Get new language data from the cache: */
        const UIDataSettingsGlobalLanguage &newLanguageData = m_pCache->data();

        /* Save new language data from the cache: */
        if (newLanguageData.m_strLanguageId != oldLanguageData.m_strLanguageId)
            gEDataManager->setLanguageId(newLanguageData.m_strLanguageId);
    }
    /* Return result: */
    return fSuccess;
}


#include "UIGlobalSettingsLanguage.moc"
