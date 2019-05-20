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
    void setWidth(int iWidth);
    int width() const;
    void updateFontSize(float multiplier);

protected:

private:

    int m_iWidth;
    int m_iDefaultPixelSize;
    int m_iDefaultPointSize;
};

/*********************************************************************************************************************************
*   UISoftKeyboardRow definition.                                                                                  *
*********************************************************************************************************************************/

class UISoftKeyboardRow : public QWidget
{
    Q_OBJECT;

public:

    UISoftKeyboardRow(QWidget *pParent = 0);
    void updateLayout();
    int m_iWidth;
    int m_iHeight;
    QVector<UISoftKeyboardKey*> m_keys;
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
    , m_iWidth(1)
{
    m_iDefaultPointSize = font().pointSize();
    m_iDefaultPixelSize = font().pixelSize();
}

void UISoftKeyboardKey::setWidth(int iWidth)
{
    m_iWidth = iWidth;
}

int UISoftKeyboardKey::width() const
{
    return m_iWidth;
}

void UISoftKeyboardKey::updateFontSize(float multiplier)
{
    if (m_iDefaultPointSize != -1)
    {
        QFont newFont = font();
        newFont.setPointSize(multiplier * m_iDefaultPointSize);
        setFont(newFont);
    }
    else
    {
        QFont newFont = font();
        newFont.setPixelSize(multiplier * m_iDefaultPixelSize);
        setFont(newFont);
    }
}

/*********************************************************************************************************************************
*   UISoftKeyboardRow implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardRow::UISoftKeyboardRow(QWidget *pParent /* = 0 */)
    :QWidget(pParent)
    , m_iWidth(0)
    , m_iHeight(0)

{
}

void UISoftKeyboardRow::updateLayout()
{
    if (m_iHeight == 0)
        return;

    float fMultiplier = height() / (float)m_iHeight;
    int iX = 0;
    for (int i = 0; i < m_keys.size(); ++i)
    {
        UISoftKeyboardKey *pKey = m_keys[i];
        if (!pKey)
            continue;
        pKey->setVisible(true);
        pKey->updateFontSize(fMultiplier);
        pKey->setGeometry(iX, 0, fMultiplier * pKey->width(), height());
        iX += fMultiplier * pKey->width();
    }
}


/*********************************************************************************************************************************
*   UISoftKeyboard implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboard::UISoftKeyboard(EmbedTo /* enmEmbedding */,
                               QWidget *pParent, QString strMachineName /* = QString()*/,
                               bool fShowToolbar /* = false */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pContainerWidget(0)
    , m_pToolBar(0)
    , m_fShowToolbar(fShowToolbar)
    , m_strMachineName(strMachineName)
    , m_iTotalRowHeight(0)
    , m_iMaxRowWidth(0)
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
    updateLayout();
}

void UISoftKeyboard::prepareObjects()
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    /* Create layout: */

    m_pMainLayout = new QHBoxLayout(this);
    if (!m_pMainLayout)
        return;
    m_pContainerWidget = new QWidget;
    if (!m_pContainerWidget)
        return;
    m_pContainerWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    //m_pContainerWidget->setStyleSheet("background-color:red;");
    //m_pContainerWidget->setAutoFillBackground(false);

    m_pMainLayout->addWidget(m_pContainerWidget);
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
    UIKeyboardLayoutReader reader;
    SoftKeyboardLayout layout;
    if (!reader.parseXMLFile(":/us_layout.xml", layout))
        return;

    m_iTotalRowHeight = 0;
    m_iMaxRowWidth = 0;
    qDeleteAll(m_rows);
    m_rows.clear();

    for (int i = 0; i < layout.m_rows.size(); ++i)
    {
        UISoftKeyboardRow *pNewRow = new UISoftKeyboardRow(m_pContainerWidget);
        if ( i == 0)
            pNewRow->setStyleSheet("background-color:yellow;");
        else
            pNewRow->setStyleSheet("background-color:green;");

        m_rows.push_back(pNewRow);
        pNewRow->m_iHeight = layout.m_rows[i].m_iHeight;
        m_iTotalRowHeight += layout.m_rows[i].m_iHeight;
        pNewRow->m_iWidth = 0;
        for (int j = 0; j < layout.m_rows[i].m_keys.size(); ++j)
        {
            pNewRow->m_iWidth += layout.m_rows[i].m_keys[j].m_iWidth;
            UISoftKeyboardKey *pKey = new UISoftKeyboardKey(pNewRow);
            pNewRow->m_keys.append(pKey);
            pKey->setText(layout.m_rows[i].m_keys[j].m_strLabel);
            pKey->setWidth(layout.m_rows[i].m_keys[j].m_iWidth);
            pKey->hide();
        }
        m_iMaxRowWidth = qMax(m_iMaxRowWidth, pNewRow->m_iWidth);
    }
    printf("total height %d %d\n", m_iTotalRowHeight, m_iMaxRowWidth);


//     return;
//     for (int i = 0; i < layout.m_rows.size(); ++i)
//     {
//         QHBoxLayout *pRowLayout = new QHBoxLayout;
//         pRowLayout->setSpacing(0);
//         for (int j = 0; j < layout.m_rows[i].m_keys.size(); ++j)
//         {
//             UISoftKeyboardKey *pKey = new UISoftKeyboardKey;
//             pRowLayout->addWidget(pKey);

//             //m_pContainerLayout->addWidget(pKey, i, j);
//             //pKey->setFixedSize(layout.m_rows[i].m_keys[j].m_iWidth, layout.m_rows[i].m_iHeight);
//             pKey->setText(layout.m_rows[i].m_keys[j].m_strLabel);
//             //if (j != 3)


//             // else
//             //m_pContainerLayout->setColumnStretch(j, 0);
//             //m_pContainerLayout->addSpacing(2);
//             // QSpacerItem *pSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
//             // m_pContainerLayout->addItem(pSpacer);
//         }
//         //pRowLayout->addStretch(1);
//         //m_pContainerLayout->setColumnStretch(layout.m_rows[i].m_keys.size()-1, 1);
//         m_pContainerLayout->addLayout(pRowLayout);
//     }
}

void UISoftKeyboard::updateLayout()
{
    if (!m_pContainerWidget)
        return;

    QSize containerSize(m_pContainerWidget->size());
    if (containerSize.width() == 0 || containerSize.height() == 0)
        return;
    float fMultiplier = containerSize.width() / (float) m_iMaxRowWidth;

    if (fMultiplier * m_iTotalRowHeight > containerSize.height())
        fMultiplier = containerSize.height() / (float) m_iTotalRowHeight;

    int y = 0;
    for (int i = 0; i < m_rows.size(); ++i)
    {
        UISoftKeyboardRow *pRow = m_rows[i];
        if (!pRow)
            continue;
        pRow->setGeometry(0, y, fMultiplier * pRow->m_iWidth, fMultiplier * pRow->m_iHeight);
        pRow->setVisible(true);
        y += fMultiplier * pRow->m_iHeight;
        // pRow->setAutoFillBackground(true);
        // pRow->raise();

        pRow->updateLayout();

        // m_pContainerWidget->lower();
        // pRow->update();

    }
    update();
    /* Compute the size multiplier: */

}

#include "UISoftKeyboard.moc"
