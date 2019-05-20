/* $Id$ */
/** @file
 * VBox Qt GUI - UISoftKeyboard class implementation.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
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
#include <QFile>
#include <QPushButton>
#include <QXmlStreamReader>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIExtraDataManager.h"
#include "UISoftKeyboard.h"
#include "VBoxGlobal.h"

/* COM includes: */
#include "CGuest.h"
#include "CEventSource.h"

struct SoftKeyboardKey
{
    int      m_iWidth;
    QString  m_strLabel;
    LONG     m_scanCode;
};

struct SoftKeyboardRow
{
    int m_iHeight;
    QList<SoftKeyboardKey> m_keys;
};

struct SoftKeyboardLayout
{
    QList<SoftKeyboardRow> m_rows;
};

/*********************************************************************************************************************************
*   UIKeyboardLayoutReader definition.                                                                                  *
*********************************************************************************************************************************/

class UIKeyboardLayoutReader
{

public:

    bool parseXMLFile(const QString &strFileName, SoftKeyboardLayout &layout);

private:

    bool parseRow(SoftKeyboardLayout &layout);
    bool parseKey(SoftKeyboardRow &row);
    QXmlStreamReader m_xmlReader;
};


/*********************************************************************************************************************************
*   UISoftKeyboardKey definition.                                                                                  *
*********************************************************************************************************************************/

class UISoftKeyboardKey : public QPushButton
{
    Q_OBJECT;

public:

    UISoftKeyboardKey(QWidget *pParent = 0);
    void setAspectRatio(float fRatio);

protected:

    virtual void resizeEvent(QResizeEvent *pEvent);

private:

    float m_fAspectRatio;
};

/*********************************************************************************************************************************
*   UIKeyboardLayoutReader implementation.                                                                                  *
*********************************************************************************************************************************/

bool UIKeyboardLayoutReader::parseXMLFile(const QString &strFileName, SoftKeyboardLayout &layout)
{
    QFile xmlFile(strFileName);
    if (!xmlFile.exists())
        return false;

    if (!xmlFile.open(QIODevice::ReadOnly))
        return false;

    m_xmlReader.setDevice(&xmlFile);

    if (!m_xmlReader.readNextStartElement() || m_xmlReader.name() != "layout")
        return false;

    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "row")
            parseRow(layout);
        else
            m_xmlReader.skipCurrentElement();
    }

    return true;
}

bool UIKeyboardLayoutReader::parseRow(SoftKeyboardLayout &layout)
{
    layout.m_rows.append(SoftKeyboardRow());
    SoftKeyboardRow &row = layout.m_rows.last();
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "key")
            parseKey(row);
        else if (m_xmlReader.name() == "height")
            row.m_iHeight = m_xmlReader.readElementText().toInt();
        else
            m_xmlReader.skipCurrentElement();
    }
    return true;
}

bool UIKeyboardLayoutReader::parseKey(SoftKeyboardRow &row)
{
    row.m_keys.append(SoftKeyboardKey());
    SoftKeyboardKey &key = row.m_keys.last();
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "width")
            key.m_iWidth = m_xmlReader.readElementText().toInt();
        else if (m_xmlReader.name() == "label")
            key.m_strLabel = m_xmlReader.readElementText();
        else if (m_xmlReader.name() == "scancode")
            key.m_scanCode = m_xmlReader.readElementText().toInt();
        else
            m_xmlReader.skipCurrentElement();
    }
    return true;
}

/*********************************************************************************************************************************
*   UISoftKeyboardKey implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardKey::UISoftKeyboardKey(QWidget *pParent /* = 0 */)
    :QPushButton(pParent)
    , m_fAspectRatio(1.f)
{
}

void UISoftKeyboardKey::setAspectRatio(float fRatio)
{
    m_fAspectRatio = fRatio;
}

void UISoftKeyboardKey::resizeEvent(QResizeEvent *pEvent)
{
    // QWidget::resize(qMin(pEvent->size().width(),pEvent->size().height()),
    //                 qMin(pEvent->size().width(),pEvent->size().height()));

    QPushButton::resizeEvent(pEvent);
}

/*********************************************************************************************************************************
*   UISoftKeyboard implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboard::UISoftKeyboard(EmbedTo /* enmEmbedding */,
                               QWidget *pParent, QString strMachineName /* = QString()*/,
                               bool fShowToolbar /* = false */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pContainerLayout(0)
    , m_pToolBar(0)
    , m_fShowToolbar(fShowToolbar)
    , m_strMachineName(strMachineName)
{
    prepareObjects();
    parseLayout();
    prepareConnections();
    prepareToolBar();
    loadSettings();
    retranslateUi();
}

UISoftKeyboard::~UISoftKeyboard()
{
    saveSettings();
}

void UISoftKeyboard::retranslateUi()
{
}

void UISoftKeyboard::resizeEvent(QResizeEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::resizeEvent(pEvent);
}

void UISoftKeyboard::prepareObjects()
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    /* Create layout: */

    m_pMainLayout = new QHBoxLayout(this);
    if (!m_pMainLayout)
        return;
    QWidget *pContainerWidget = new QWidget;
    if (!pContainerWidget)
        return;
    pContainerWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    pContainerWidget->setStyleSheet("background-color:red;");

    m_pMainLayout->addWidget(pContainerWidget);

    m_pContainerLayout = new QVBoxLayout;
    m_pContainerLayout->setSpacing(0);
    pContainerWidget->setLayout(m_pContainerLayout);

    /* Configure layout: */

}

void UISoftKeyboard::prepareConnections()
{
}

void UISoftKeyboard::prepareToolBar()
{
}

void UISoftKeyboard::saveSettings()
{
}

void UISoftKeyboard::loadSettings()
{
}

void UISoftKeyboard::parseLayout()
{
    if (!m_pContainerLayout)
        return;

    UIKeyboardLayoutReader reader;
    SoftKeyboardLayout layout;
    if (!reader.parseXMLFile(":/us_layout.xml", layout))
        return;

    for (int i = 0; i < layout.m_rows.size(); ++i)
    {
        QHBoxLayout *pRowLayout = new QHBoxLayout;
        pRowLayout->setSpacing(0);
        for (int j = 0; j < layout.m_rows[i].m_keys.size(); ++j)
        {
            UISoftKeyboardKey *pKey = new UISoftKeyboardKey;
            pRowLayout->addWidget(pKey);
            //m_pContainerLayout->addWidget(pKey, i, j);
            //pKey->setFixedSize(layout.m_rows[i].m_keys[j].m_iWidth, layout.m_rows[i].m_iHeight);
            pKey->setText(layout.m_rows[i].m_keys[j].m_strLabel);
            //if (j != 3)


            // else
            //m_pContainerLayout->setColumnStretch(j, 0);
            //m_pContainerLayout->addSpacing(2);
            // QSpacerItem *pSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
            // m_pContainerLayout->addItem(pSpacer);
        }
        //pRowLayout->addStretch(1);
        //m_pContainerLayout->setColumnStretch(layout.m_rows[i].m_keys.size()-1, 1);
        m_pContainerLayout->addLayout(pRowLayout);
    }
}

#include "UISoftKeyboard.moc"
