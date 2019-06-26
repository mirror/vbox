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
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPicture>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QStackedWidget>
#include <QToolButton>
#include <QXmlStreamReader>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIDesktopWidgetWatchdog.h"
#include "QIDialogButtonBox.h"
#include "QIFileDialog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UISession.h"
#include "UISoftKeyboard.h"
#include "QIToolButton.h"
#include "VBoxGlobal.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/* COM includes: */
#include "CGuest.h"
#include "CEventSource.h"

/* Forward declarations: */
class UISoftKeyboardWidget;
class UISoftKeyboardLayout;
class UISoftKeyboardRow;

const int iMessageTimeout = 3000;
const QString strSubDirectorName("keyboardLayouts");

enum UIKeyState
{
    UIKeyState_NotPressed,
    UIKeyState_Pressed,
    UIKeyState_Locked,
    UIKeyState_Max
};

enum UIKeyType
{
    /** Can be in UIKeyState_NotPressed and UIKeyState_Pressed states. */
    UIKeyType_Ordinary,
    /** e.g. CapsLock. Can be only in UIKeyState_NotPressed, UIKeyState_Locked */
    UIKeyType_Toggleable,
    /** e.g. Shift Can be in all 3 states*/
    UIKeyType_Modifier,
    UIKeyType_Max
};

struct KeyCaptions
{
    QString m_strBase;
    QString m_strShift;
    QString m_strAltGr;
    QString m_strShiftAltGr;
};


/*********************************************************************************************************************************
*   UISoftKeyboardPhysicalLayout definition.                                                                                     *
*********************************************************************************************************************************/

class UISoftKeyboardPhysicalLayout
{

public:

    void setName(const QString &strName)
    {
        m_strName = strName;
    }

    const QString &name() const
    {
        return m_strName;
    }

    QString  m_strFileName;
    QUuid    m_uId;

    const QVector<UISoftKeyboardRow> &rows() const
    {
        return m_rows;
    }

    QVector<UISoftKeyboardRow> &rows()
    {
        return m_rows;
    }

private:

    QString  m_strName;
    QVector<UISoftKeyboardRow>  m_rows;
};

/*********************************************************************************************************************************
*   UILayoutEditor definition.                                                                                  *
*********************************************************************************************************************************/

class UILayoutEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigLayoutEdited();
    void sigKeyCaptionsEdited(UISoftKeyboardKey* pKey);
    void sigGoBackButton();

public:

    UILayoutEditor(QWidget *pParent = 0);
    void setKey(UISoftKeyboardKey *pKey);

    void setLayoutToEdit(UISoftKeyboardLayout *pLayout);
    void setPhysicalLayoutList(const QVector<UISoftKeyboardPhysicalLayout> &physicalLayouts);

protected:

    virtual void retranslateUi() /* override */;

private slots:

    void sltKeyBaseCaptionChange(const QString &strCaption);
    void sltKeyShiftCaptionChange(const QString &strCaption);
    void sltKeyAltGrCaptionChange(const QString &strCaption);
    void sltKeyShiftAltGrCaptionChange(const QString &strCaption);
    void sltPhysicalLayoutChanged();
    void sltLayoutNameChanged(const QString &strCaption);

private:

    void prepareObjects();
    void prepareConnections();
    QWidget *prepareKeyCaptionEditWidgets();
    void reset();
    void resetKeyWidgets();
    QGridLayout *m_pEditorLayout;
    QToolButton *m_pGoBackButton;
    QGroupBox *m_pSelectedKeyGroupBox;
    QGroupBox *m_pCaptionEditGroupBox;

    QComboBox *m_pPhysicalLayoutCombo;
    QLabel *m_pTitleLabel;
    QLabel *m_pPhysicalLayoutLabel;
    QLabel *m_pLayoutNameLabel;
    QLabel *m_pScanCodeLabel;
    QLabel *m_pPositionLabel;
    QLabel *m_pBaseCaptionLabel;
    QLabel *m_pShiftCaptionLabel;
    QLabel *m_pAltGrCaptionLabel;
    QLabel *m_pShiftAltGrCaptionLabel;

    QLineEdit *m_pLayoutNameEdit;
    QLineEdit *m_pScanCodeEdit;
    QLineEdit *m_pPositionEdit;
    QLineEdit *m_pBaseCaptionEdit;
    QLineEdit *m_pShiftCaptionEdit;
    QLineEdit *m_pAltGrCaptionEdit;
    QLineEdit *m_pShiftAltGrCaptionEdit;

    /** The key which is being currently edited. Might be Null. */
    UISoftKeyboardKey  *m_pKey;
    /** The layout which is being currently edited. */
    UISoftKeyboardLayout *m_pLayout;
};

/*********************************************************************************************************************************
*   UILayoutSelector definition.                                                                                  *
*********************************************************************************************************************************/

class UILayoutSelector : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigSaveLayout();
    void sigCopyLayout();
    void sigDeleteLayout();
    void sigLayoutSelectionChanged(const QString &strSelectedLayoutName);
    void sigShowLayoutEditor();
    void sigCloseLayoutList();

public:

    UILayoutSelector(QWidget *pParent = 0);
    void setLayoutList(const QStringList &layoutNames);
    void setCurrentLayout(const QString &strLayoutName);
    void setCurrentLayoutIsEditable(bool fEditable);

protected:

    virtual void retranslateUi() /* override */;

private slots:


private:

    void prepareObjects();

    QListWidget *m_pLayoutListWidget;
    QToolButton *m_pApplyLayoutButton;
    QToolButton *m_pEditLayoutButton;
    QToolButton *m_pCopyLayoutButton;
    QToolButton *m_pSaveLayoutButton;
    QToolButton *m_pDeleteLayoutButton;
    QLabel      *m_pTitleLabel;
    QToolButton *m_pCloseButton;
};

/*********************************************************************************************************************************
*   UISoftKeyboardRow definition.                                                                                  *
*********************************************************************************************************************************/

/** UISoftKeyboardRow represents a row in the physical keyboard. */
class UISoftKeyboardRow
{
public:

    UISoftKeyboardRow();

    void setDefaultWidth(int iWidth);
    int defaultWidth() const;

    void setDefaultHeight(int iHeight);
    int defaultHeight() const;

    QVector<UISoftKeyboardKey> &keys();
    const QVector<UISoftKeyboardKey> &keys() const;

    void setSpaceHeightAfter(int iSpace);
    int spaceHeightAfter() const;

private:

    /** Default width and height might be inherited from the layout and overwritten in row settings. */
    int m_iDefaultWidth;
    int m_iDefaultHeight;
    QVector<UISoftKeyboardKey> m_keys;
    int m_iSpaceHeightAfter;
};

/*********************************************************************************************************************************
*   UISoftKeyboardKey definition.                                                                                  *
*********************************************************************************************************************************/

/** UISoftKeyboardKey is a place holder for a keyboard key. Graphical key represantations are drawn according to this class. */
class UISoftKeyboardKey
{
public:

    UISoftKeyboardKey();

    const QRect keyGeometry() const;
    void setKeyGeometry(const QRect &rect);

    const QString baseCaption() const;
    void setBaseCaption(const QString &strBaseCaption);

    const QString shiftCaption() const;
    void setShiftCaption(const QString &strShiftCaption);

    const QString altGrCaption() const;
    void  setAltGrCaption(const QString &strAltGrCaption);

    const QString shiftAltGrCaption() const;
    void  setShiftAltGrCaption(const QString &strAltGrCaption);

    void resetCaptions();
    const QString text() const;

    void setWidth(int iWidth);
    int width() const;

    void setHeight(int iHeight);
    int height() const;

    void setScanCode(LONG scanCode);
    LONG scanCode() const;

    void setScanCodePrefix(LONG scanCode);
    LONG scanCodePrefix() const;

    void setSpaceWidthAfter(int iSpace);
    int spaceWidthAfter() const;

    void setPosition(int iPosition);
    int position() const;

    void setType(UIKeyType enmType);
    UIKeyType type() const;

    void setIsNumPadKey(bool fIsNumPadKey);
    bool isNumPadKey() const;

    void setIsOSMenuKey(bool fIsOSMenuKey);
    bool isOSMenuKey() const;

    void setCutout(int iCorner, int iWidth, int iHeight);

    void setParentWidget(UISoftKeyboardWidget* pParent);
    UIKeyState state() const;
    QVector<LONG> scanCodeWithPrefix() const;

    void release();
    void press();

    void setPolygon(const QPolygon &polygon);
    const QPolygon &polygon() const;

    QPolygon polygonInGlobal() const;

    int cutoutCorner() const;
    int cutoutWidth() const;
    int cutoutHeight() const;
    QPicture m_textPixmap;

private:

    void updateState(bool fPressed);
    void updateText();

    QRect      m_keyGeometry;
    QString    m_strBaseCaption;
    QString    m_strShiftCaption;
    QString    m_strAltGrCaption;
    QString    m_strShiftAltGrCaption;
    /** m_strText is concatenation of base, shift, and altgr captions. */
    QString    m_strText;
    /** Stores the key polygon in local coordinates. */
    QPolygon   m_polygon;
    UIKeyType  m_enmType;
    UIKeyState m_enmState;
    /** Key width as it is read from the xml file. */
    int        m_iWidth;
    /** Key height as it is read from the xml file. */
    int        m_iHeight;
    int        m_iSpaceWidthAfter;
    LONG       m_scanCode;
    LONG       m_scanCodePrefix;

    /** @name Cutouts are used to create non-rectangle keys polygons.
      * @{ */
        int  m_iCutoutWidth;
        int  m_iCutoutHeight;
        /** -1 is for no cutout. 0 is the topleft, 2 is the top right and so on. */
        int  m_iCutoutCorner;
    /** @} */

    /** Key's position in the layout. */
    int        m_iPosition;
    UISoftKeyboardWidget  *m_pParentWidget;
    bool m_fIsNumPadKey;
    bool m_fIsOSMenuKey;
};


/*********************************************************************************************************************************
*   UISoftKeyboardLayout definition.                                                                                  *
*********************************************************************************************************************************/

class UISoftKeyboardLayout
{

public:

    UISoftKeyboardLayout();

    void setName(const QString &strName);
    const QString &name() const;

    void setSourceFilePath(const QString& strSourceFilePath);
    const QString& sourceFilePath() const;

    void setIsFromResources(bool fIsFromResources);
    bool isFromResources() const;

    void updateKeyCaptions(int iKeyPosition, KeyCaptions &newCaptions);

    void setEditable(bool fEditable);
    bool editable() const;

    void setPhysicalLayoutUuid(const QUuid &uuid);
    const QUuid &physicalLayoutUuid() const;

    void setKeyCapMap(const QMap<int, KeyCaptions> &keyCapMap);
    QMap<int, KeyCaptions> &keyCapMap();
    const QMap<int, KeyCaptions> &keyCapMap() const;
    bool operator==(const UISoftKeyboardLayout &otherLayout);

private:

    /** The UUID of the physical layout used by this layout. */
    QUuid m_physicalLayoutUuid;

    /** We cache the key caps here instead of reading the layout files each time layout changes.
      * Map key is the key position and the value is the captions of the key. */
    QMap<int, KeyCaptions> m_keyCapMap;

    QString m_strName;
    QString m_strSourceFilePath;
    bool    m_fEditable;
    bool    m_fIsFromResources;
};

/*********************************************************************************************************************************
*   UISoftKeyboardWidget definition.                                                                                  *
*********************************************************************************************************************************/

/** The container widget for keyboard keys. It also handles all the keyboard related events. */
class UISoftKeyboardWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    enum Mode
    {
        Mode_LayoutEdit,
        Mode_Keyboard,
        Mode_Max
    };

signals:

    void sigStatusBarMessage(const QString &strMessage);
    void sigPutKeyboardSequence(QVector<LONG> sequence);
    void sigCurrentLayoutChange();
    void sigKeyToEdit(UISoftKeyboardKey* pKey);

public:

    UISoftKeyboardWidget(QWidget *pParent = 0);

    virtual QSize minimumSizeHint() const /* override */;
    virtual QSize sizeHint() const  /* override */;
    void keyStateChange(UISoftKeyboardKey* pKey);
    void loadLayouts();
    void showContextMenu(const QPoint &globalPoint);

    void setCurrentLayout(const QString &strLayoutName);
    UISoftKeyboardLayout *currentLayout();

    QStringList layoutNameList() const;
    const QVector<UISoftKeyboardPhysicalLayout> &physicalLayouts() const;
    void deleteCurrentLayout();
    void toggleEditMode(bool fIsEditMode);
    /** Is called when the captions in UISoftKeyboardKey is changed and forward this changes to
      * corresponding UISoftKeyboardLayout */
    void updateKeyCaptionsInLayout(UISoftKeyboardKey *pKey);
    void saveCurentLayoutToFile();
    void copyCurentLayout();
    float layoutAspectRatio();

    bool showOSMenuKeys();
    void setShowOSMenuKeys(bool fShow);

    bool showNumPad();
    void setShowNumPad(bool fShow);

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
    virtual void mouseReleaseEvent(QMouseEvent *pEvent) /* override */;
    virtual void mouseMoveEvent(QMouseEvent *pEvent) /* override */;
    virtual void retranslateUi() /* override */;

private slots:

    void sltContextMenuRequest(const QPoint &point);
    void sltPhysicalLayoutForLayoutChanged(int iIndex);

private:

    void    addLayout(const UISoftKeyboardLayout &newLayout);
    void    setNewMinimumSize(const QSize &size);
    void    setInitialSize(int iWidth, int iHeight);
    /** Searches for the key which contains the position of the @p pEvent and returns it if found. */
    UISoftKeyboardKey *keyUnderMouse(QMouseEvent *pEvent);
    UISoftKeyboardKey *keyUnderMouse(const QPoint &point);
    void               handleKeyPress(UISoftKeyboardKey *pKey);
    void               handleKeyRelease(UISoftKeyboardKey *pKey);
    bool               loadPhysicalLayout(const QString &strLayoutFileName, bool isNumPad = false);
    bool               loadKeyboardLayout(const QString &strLayoutName);
    void               reset();
    void               prepareObjects();
    UISoftKeyboardPhysicalLayout *findPhysicalLayout(const QUuid &uuid);
    /** Sets m_pKeyBeingEdited. */
    void               setKeyBeingEdited(UISoftKeyboardKey *pKey);
    void               setCurrentLayout(UISoftKeyboardLayout *pLayout);
    UISoftKeyboardLayout *findLayoutByName(const QString &strName);
    /** Looks under the default keyboard layout folder and add the file names to the fileList. */
    void               lookAtDefaultLayoutFolder(QStringList &fileList);

    UISoftKeyboardKey *m_pKeyUnderMouse;
    UISoftKeyboardKey *m_pKeyBeingEdited;

    UISoftKeyboardKey *m_pKeyPressed;
    QColor m_keyDefaultColor;
    QColor m_keyHoverColor;
    QColor m_textDefaultColor;
    QColor m_textPressedColor;
    QColor m_keyEditColor;
    QVector<UISoftKeyboardKey*> m_pressedModifiers;
    QVector<UISoftKeyboardPhysicalLayout> m_physicalLayouts;
    UISoftKeyboardPhysicalLayout          m_numPadLayout;
    QVector<UISoftKeyboardLayout>         m_layouts;
    UISoftKeyboardLayout *m_pCurrentKeyboardLayout;

    QSize m_minimumSize;
    float m_fScaleFactorX;
    float m_fScaleFactorY;
    int   m_iInitialHeight;
    int   m_iInitialWidth;
    int   m_iInitialWidthNoNumPad;
    int   m_iXSpacing;
    int   m_iYSpacing;
    int   m_iLeftMargin;
    int   m_iTopMargin;
    int   m_iRightMargin;
    int   m_iBottomMargin;

    QMenu   *m_pContextMenu;
    Mode     m_enmMode;

    bool m_fShowOSMenuKeys;
    bool m_fShowNumPad;
};

/*********************************************************************************************************************************
*   UIPhysicalLayoutReader definition.                                                                                  *
*********************************************************************************************************************************/

class UIPhysicalLayoutReader
{

public:

    bool parseXMLFile(const QString &strFileName, UISoftKeyboardPhysicalLayout &physicalLayout);
    static QVector<QPoint> computeKeyVertices(const UISoftKeyboardKey &key);
private:

    void  parseKey(UISoftKeyboardRow &row);
    void  parseRow(int iDefaultWidth, int iDefaultHeight, QVector<UISoftKeyboardRow> &rows);
    /**  Parses the verticel space between the rows. */
    void  parseRowSpace(QVector<UISoftKeyboardRow> &rows);
    /** Parses the horizontal space between keys. */
    void  parseKeySpace(UISoftKeyboardRow &row);
    void  parseCutout(UISoftKeyboardKey &key);

    QXmlStreamReader m_xmlReader;
};

/*********************************************************************************************************************************
*   UIKeyboardLayoutReader definition.                                                                                  *
*********************************************************************************************************************************/

class UIKeyboardLayoutReader
{

public:

    bool  parseFile(const QString &strFileName);
    const QUuid &physicalLayoutUUID() const;
    const QString &name() const;
    const QMap<int, KeyCaptions> &keyCapMap() const;

private:

    void  parseKey();
    QXmlStreamReader m_xmlReader;
    /** Map key is the key position and the value is the captions of the key. */
    QMap<int, KeyCaptions> m_keyCapMap;
    QUuid m_physicalLayoutUid;
    QString m_strName;
};


/*********************************************************************************************************************************
*   UISoftKeyboardStatusBarWidget  definition.                                                                                   *
*********************************************************************************************************************************/

class UISoftKeyboardStatusBarWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigShowHideSidePanel();
    void sigShowSettingWidget();

public:

    UISoftKeyboardStatusBarWidget(QWidget *pParent = 0);
    void updateMessage(const QString &strMessage);

protected:

    virtual void retranslateUi() /* override */;

private slots:


private:

    void prepareObjects();
    QToolButton  *m_pLayoutListButton;
    QToolButton  *m_pSettingsButton;
    QLabel       *m_pMessageLabel;
};


/*********************************************************************************************************************************
*   UISoftKeyboardSettingsWidget  definition.                                                                                    *
*********************************************************************************************************************************/

class UISoftKeyboardSettingsWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigShowNumPad(bool fShow);
    void sigShowOSMenuKeys(bool fShow);

public:

    UISoftKeyboardSettingsWidget(QWidget *pParent = 0);
    void setShowOSMenuKeys(bool fShow);
    void setShowNumPad(bool fShow);

protected:

    virtual void retranslateUi() /* override */;

private slots:


private:

    void prepareObjects();

    QCheckBox  *m_pShowNumPadCheckBox;
    QCheckBox  *m_pShowOsMenuButtonsCheckBox;

};

/*********************************************************************************************************************************
*   UILayoutEditor implementation.                                                                                  *
*********************************************************************************************************************************/

UILayoutEditor::UILayoutEditor(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pEditorLayout(0)
    , m_pGoBackButton(0)
    , m_pSelectedKeyGroupBox(0)
    , m_pCaptionEditGroupBox(0)
    , m_pPhysicalLayoutCombo(0)
    , m_pTitleLabel(0)
    , m_pPhysicalLayoutLabel(0)
    , m_pLayoutNameLabel(0)
    , m_pScanCodeLabel(0)
    , m_pPositionLabel(0)
    , m_pBaseCaptionLabel(0)
    , m_pShiftCaptionLabel(0)
    , m_pAltGrCaptionLabel(0)
    , m_pShiftAltGrCaptionLabel(0)
    , m_pLayoutNameEdit(0)
    , m_pScanCodeEdit(0)
    , m_pPositionEdit(0)
    , m_pBaseCaptionEdit(0)
    , m_pShiftCaptionEdit(0)
    , m_pAltGrCaptionEdit(0)
    , m_pShiftAltGrCaptionEdit(0)
    , m_pKey(0)
    , m_pLayout(0)
{
    setAutoFillBackground(true);
    prepareObjects();
}

void UILayoutEditor::setKey(UISoftKeyboardKey *pKey)
{
    if (m_pKey == pKey)
        return;
    /* First apply the pending changes to the key that has been edited: */
    if (m_pKey)
    {
        if (m_pKey->shiftCaption() != m_pShiftCaptionEdit->text())
            m_pKey->setShiftCaption(m_pShiftCaptionEdit->text());
        if (m_pKey->baseCaption() != m_pBaseCaptionEdit->text())
            m_pKey->setBaseCaption(m_pBaseCaptionEdit->text());
        if (m_pKey->altGrCaption() != m_pAltGrCaptionEdit->text())
            m_pKey->setAltGrCaption(m_pAltGrCaptionEdit->text());
        if (m_pKey->shiftAltGrCaption() != m_pShiftAltGrCaptionEdit->text())
            m_pKey->setShiftAltGrCaption(m_pShiftAltGrCaptionEdit->text());
    }

    m_pKey = pKey;
    if (m_pSelectedKeyGroupBox)
        m_pSelectedKeyGroupBox->setEnabled(m_pKey);
    if (!m_pKey)
    {
        resetKeyWidgets();
        return;
    }
    if (m_pScanCodeEdit)
        m_pScanCodeEdit->setText(QString::number(m_pKey->scanCode(), 16));
    if (m_pPositionEdit)
        m_pPositionEdit->setText(QString::number(m_pKey->position()));
    if (m_pBaseCaptionEdit)
        m_pBaseCaptionEdit->setText(m_pKey->baseCaption());
    if (m_pShiftCaptionEdit)
        m_pShiftCaptionEdit->setText(m_pKey->shiftCaption());
    if (m_pAltGrCaptionEdit)
        m_pAltGrCaptionEdit->setText(m_pKey->altGrCaption());
    if (m_pShiftAltGrCaptionEdit)
        m_pShiftAltGrCaptionEdit->setText(m_pKey->shiftAltGrCaption());
}

void UILayoutEditor::setLayoutToEdit(UISoftKeyboardLayout *pLayout)
{
    if (m_pLayout == pLayout)
        return;

    m_pLayout = pLayout;
    if (!m_pLayout)
        reset();

    if (m_pLayoutNameEdit)
        m_pLayoutNameEdit->setText(m_pLayout ? m_pLayout->name() : QString());

    if (m_pPhysicalLayoutCombo && m_pLayout)
    {
        int iIndex = m_pPhysicalLayoutCombo->findData(m_pLayout->physicalLayoutUuid());
        if (iIndex != -1)
            m_pPhysicalLayoutCombo->setCurrentIndex(iIndex);
    }
    update();
}

void UILayoutEditor::setPhysicalLayoutList(const QVector<UISoftKeyboardPhysicalLayout> &physicalLayouts)
{
    if (!m_pPhysicalLayoutCombo)
        return;
    m_pPhysicalLayoutCombo->clear();
    foreach (const UISoftKeyboardPhysicalLayout &physicalLayout, physicalLayouts)
        m_pPhysicalLayoutCombo->addItem(physicalLayout.name(), physicalLayout.m_uId);
}

void UILayoutEditor::retranslateUi()
{
    if (m_pTitleLabel)
        m_pTitleLabel->setText(UISoftKeyboard::tr("Layout Editor"));
    if (m_pGoBackButton)
    {
        m_pGoBackButton->setToolTip(UISoftKeyboard::tr("Return Back to Layout List"));
        m_pGoBackButton->setText(UISoftKeyboard::tr("Back to Layout List"));
    }
    if (m_pPhysicalLayoutLabel)
        m_pPhysicalLayoutLabel->setText(UISoftKeyboard::tr("Physical Layout"));
    if (m_pLayoutNameLabel)
        m_pLayoutNameLabel->setText(UISoftKeyboard::tr("Layout Name"));
    if (m_pScanCodeLabel)
        m_pScanCodeLabel->setText(UISoftKeyboard::tr("Scan Code"));
    if (m_pPositionLabel)
        m_pPositionLabel->setText(UISoftKeyboard::tr("Position"));
    if (m_pBaseCaptionLabel)
        m_pBaseCaptionLabel->setText(UISoftKeyboard::tr("Base"));
    if (m_pShiftCaptionLabel)
        m_pShiftCaptionLabel->setText(UISoftKeyboard::tr("Shift"));
    if (m_pAltGrCaptionLabel)
        m_pAltGrCaptionLabel->setText(UISoftKeyboard::tr("AltGr"));
   if (m_pShiftAltGrCaptionLabel)
        m_pShiftAltGrCaptionLabel->setText(UISoftKeyboard::tr("ShiftAltGr"));
    if (m_pCaptionEditGroupBox)
        m_pCaptionEditGroupBox->setTitle(UISoftKeyboard::tr("Captions"));
    if (m_pSelectedKeyGroupBox)
        m_pSelectedKeyGroupBox->setTitle(UISoftKeyboard::tr("Selected Key"));
}

void UILayoutEditor::sltKeyBaseCaptionChange(const QString &strCaption)
{
    if (!m_pKey || m_pKey->baseCaption() == strCaption)
        return;
    m_pKey->setBaseCaption(strCaption);
    emit sigKeyCaptionsEdited(m_pKey);
}

void UILayoutEditor::sltKeyShiftCaptionChange(const QString &strCaption)
{
    if (!m_pKey || m_pKey->shiftCaption() == strCaption)
        return;
    m_pKey->setShiftCaption(strCaption);
    emit sigKeyCaptionsEdited(m_pKey);
}

void UILayoutEditor::sltKeyAltGrCaptionChange(const QString &strCaption)
{
    if (!m_pKey  || m_pKey->altGrCaption() == strCaption)
        return;
    m_pKey->setAltGrCaption(strCaption);
    emit sigKeyCaptionsEdited(m_pKey);
}

void UILayoutEditor::sltKeyShiftAltGrCaptionChange(const QString &strCaption)
{
    if (!m_pKey  || m_pKey->shiftAltGrCaption() == strCaption)
        return;
    m_pKey->setShiftAltGrCaption(strCaption);
    emit sigKeyCaptionsEdited(m_pKey);
}

void UILayoutEditor::sltPhysicalLayoutChanged()
{
    if (!m_pPhysicalLayoutCombo || !m_pLayout)
        return;
    QUuid currentData = m_pPhysicalLayoutCombo->currentData().toUuid();
    if (!currentData.isNull())
        m_pLayout->setPhysicalLayoutUuid(currentData);
    emit sigLayoutEdited();
}

void UILayoutEditor::sltLayoutNameChanged(const QString &strName)
{
    if (!m_pLayout || m_pLayout->name() == strName)
        return;
    m_pLayout->setName(strName);
    emit sigLayoutEdited();
}

void UILayoutEditor::prepareObjects()
{
    m_pEditorLayout = new QGridLayout;
    if (!m_pEditorLayout)
        return;
    setLayout(m_pEditorLayout);

    QHBoxLayout *pTitleLayout = new QHBoxLayout;
    m_pGoBackButton = new QToolButton;
    m_pGoBackButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_pGoBackButton->setIcon(UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_ArrowBack));
    //m_pGoBackButton->setStyleSheet("QToolButton { border: 0px none black; margin: 0px 0px 0px 0px; } QToolButton::menu-indicator {image: none;}");
    m_pGoBackButton->setAutoRaise(true);
    m_pEditorLayout->addWidget(m_pGoBackButton, 0, 0, 1, 1);
    connect(m_pGoBackButton, &QToolButton::clicked, this, &UILayoutEditor::sigGoBackButton);
    pTitleLayout->addWidget(m_pGoBackButton);
    pTitleLayout->addStretch(2);
    m_pTitleLabel = new QLabel;
    pTitleLayout->addWidget(m_pTitleLabel);
    m_pEditorLayout->addLayout(pTitleLayout, 0, 0, 1, 2);


    m_pLayoutNameLabel = new QLabel;
    m_pLayoutNameEdit = new QLineEdit;
    m_pLayoutNameLabel->setBuddy(m_pLayoutNameEdit);
    m_pEditorLayout->addWidget(m_pLayoutNameLabel, 2, 0, 1, 1);
    m_pEditorLayout->addWidget(m_pLayoutNameEdit, 2, 1, 1, 1);
    connect(m_pLayoutNameEdit, &QLineEdit::textChanged, this, &UILayoutEditor::sltLayoutNameChanged);


    m_pPhysicalLayoutLabel = new QLabel;
    m_pPhysicalLayoutCombo = new QComboBox;
    m_pPhysicalLayoutLabel->setBuddy(m_pPhysicalLayoutCombo);
    m_pEditorLayout->addWidget(m_pPhysicalLayoutLabel, 3, 0, 1, 1);
    m_pEditorLayout->addWidget(m_pPhysicalLayoutCombo, 3, 1, 1, 1);
    connect(m_pPhysicalLayoutCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UILayoutEditor::sltPhysicalLayoutChanged);

    m_pSelectedKeyGroupBox = new QGroupBox;
    m_pSelectedKeyGroupBox->setEnabled(false);

    m_pEditorLayout->addWidget(m_pSelectedKeyGroupBox, 4, 0, 1, 2);
    QGridLayout *pSelectedKeyLayout = new QGridLayout(m_pSelectedKeyGroupBox);
    pSelectedKeyLayout->setSpacing(0);
    pSelectedKeyLayout->setContentsMargins(0, 0, 0, 0);

    m_pScanCodeLabel = new QLabel;
    m_pScanCodeEdit = new QLineEdit;
    m_pScanCodeLabel->setBuddy(m_pScanCodeEdit);
    m_pScanCodeEdit->setEnabled(false);
    pSelectedKeyLayout->addWidget(m_pScanCodeLabel, 0, 0);
    pSelectedKeyLayout->addWidget(m_pScanCodeEdit, 0, 1);

    m_pPositionLabel= new QLabel;
    m_pPositionEdit = new QLineEdit;
    m_pPositionEdit->setEnabled(false);
    m_pPositionLabel->setBuddy(m_pPositionEdit);
    pSelectedKeyLayout->addWidget(m_pPositionLabel, 1, 0);
    pSelectedKeyLayout->addWidget(m_pPositionEdit, 1, 1);

    QWidget *pCaptionEditor = prepareKeyCaptionEditWidgets();
    if (pCaptionEditor)
        pSelectedKeyLayout->addWidget(pCaptionEditor, 2, 0, 2, 2);

    QSpacerItem *pSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (pSpacer)
        pSelectedKeyLayout->addItem(pSpacer, 4, 1);

    retranslateUi();
}

QWidget *UILayoutEditor::prepareKeyCaptionEditWidgets()
{
    m_pCaptionEditGroupBox = new QGroupBox;
    if (!m_pCaptionEditGroupBox)
        return 0;
    m_pCaptionEditGroupBox->setFlat(false);
    QGridLayout *pCaptionEditorLayout = new QGridLayout(m_pCaptionEditGroupBox);
    pCaptionEditorLayout->setSpacing(0);
    pCaptionEditorLayout->setContentsMargins(0, 0, 0, 0);

    if (!pCaptionEditorLayout)
        return 0;

    m_pBaseCaptionLabel = new QLabel;
    m_pBaseCaptionEdit = new QLineEdit;
    m_pBaseCaptionLabel->setBuddy(m_pBaseCaptionEdit);
    pCaptionEditorLayout->addWidget(m_pBaseCaptionLabel, 0, 0);
    pCaptionEditorLayout->addWidget(m_pBaseCaptionEdit, 0, 1);
    connect(m_pBaseCaptionEdit, &QLineEdit::textChanged, this, &UILayoutEditor::sltKeyBaseCaptionChange);

    m_pShiftCaptionLabel = new QLabel;
    m_pShiftCaptionEdit = new QLineEdit;
    m_pShiftCaptionLabel->setBuddy(m_pShiftCaptionEdit);
    pCaptionEditorLayout->addWidget(m_pShiftCaptionLabel, 1, 0);
    pCaptionEditorLayout->addWidget(m_pShiftCaptionEdit, 1, 1);
    connect(m_pShiftCaptionEdit, &QLineEdit::textChanged, this, &UILayoutEditor::sltKeyShiftCaptionChange);

    m_pAltGrCaptionLabel = new QLabel;
    m_pAltGrCaptionEdit = new QLineEdit;
    m_pAltGrCaptionLabel->setBuddy(m_pAltGrCaptionEdit);
    pCaptionEditorLayout->addWidget(m_pAltGrCaptionLabel, 2, 0);
    pCaptionEditorLayout->addWidget(m_pAltGrCaptionEdit, 2, 1);
    connect(m_pAltGrCaptionEdit, &QLineEdit::textChanged, this, &UILayoutEditor::sltKeyAltGrCaptionChange);

    m_pShiftAltGrCaptionLabel = new QLabel;
    m_pShiftAltGrCaptionEdit = new QLineEdit;
    m_pShiftAltGrCaptionLabel->setBuddy(m_pShiftAltGrCaptionEdit);
    pCaptionEditorLayout->addWidget(m_pShiftAltGrCaptionLabel, 3, 0);
    pCaptionEditorLayout->addWidget(m_pShiftAltGrCaptionEdit, 3, 1);
    connect(m_pShiftAltGrCaptionEdit, &QLineEdit::textChanged, this, &UILayoutEditor::sltKeyShiftAltGrCaptionChange);


    QSpacerItem *pSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (pSpacer)
        pCaptionEditorLayout->addItem(pSpacer, 4, 1);
    return m_pCaptionEditGroupBox;
}

void UILayoutEditor::reset()
{
    if (m_pLayoutNameEdit)
        m_pLayoutNameEdit->clear();
    resetKeyWidgets();
}

void UILayoutEditor::resetKeyWidgets()
{
    if (m_pScanCodeEdit)
        m_pScanCodeEdit->clear();
    if (m_pPositionEdit)
        m_pPositionEdit->clear();
    if (m_pBaseCaptionEdit)
        m_pBaseCaptionEdit->clear();
    if (m_pShiftCaptionEdit)
        m_pShiftCaptionEdit->clear();
    if (m_pAltGrCaptionEdit)
        m_pAltGrCaptionEdit->clear();
    if (m_pShiftAltGrCaptionEdit)
        m_pShiftAltGrCaptionEdit->clear();
}

/*********************************************************************************************************************************
*   UILayoutSelector implementation.                                                                                  *
*********************************************************************************************************************************/

UILayoutSelector::UILayoutSelector(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pLayoutListWidget(0)
    , m_pApplyLayoutButton(0)
    , m_pEditLayoutButton(0)
    , m_pCopyLayoutButton(0)
    , m_pSaveLayoutButton(0)
    , m_pDeleteLayoutButton(0)
    , m_pTitleLabel(0)
    , m_pCloseButton(0)
{
    prepareObjects();
}

void UILayoutSelector::setCurrentLayout(const QString &strLayoutName)
{
    if (!m_pLayoutListWidget)
        return;
    if (strLayoutName.isEmpty())
    {
        m_pLayoutListWidget->selectionModel()->clear();
        return;
    }
    QList<QListWidgetItem *> items = m_pLayoutListWidget->findItems(strLayoutName, Qt::MatchFixedString |Qt::MatchCaseSensitive);
    if (items.isEmpty())
        return;
    QListWidgetItem *pItem = items[0];
    if (!pItem || pItem == m_pLayoutListWidget->currentItem())
        return;
    m_pLayoutListWidget->setCurrentItem(pItem);
}

void UILayoutSelector::setCurrentLayoutIsEditable(bool fEditable)
{
    if (m_pEditLayoutButton)
        m_pEditLayoutButton->setEnabled(fEditable);
    if (m_pSaveLayoutButton)
        m_pSaveLayoutButton->setEnabled(fEditable);
    if (m_pDeleteLayoutButton)
        m_pDeleteLayoutButton->setEnabled(fEditable);
}

void UILayoutSelector::setLayoutList(const QStringList &layoutNames)
{
    if (!m_pLayoutListWidget)
        return;
    m_pLayoutListWidget->clear();
    foreach (const QString &strLayoutName, layoutNames)
        m_pLayoutListWidget->addItem(strLayoutName);
}

void UILayoutSelector::retranslateUi()
{
    if (m_pApplyLayoutButton)
        m_pApplyLayoutButton->setToolTip(UISoftKeyboard::tr("Use the selected layout"));
    if (m_pEditLayoutButton)
        m_pEditLayoutButton->setToolTip(UISoftKeyboard::tr("Edit the selected layout"));
    if (m_pDeleteLayoutButton)
        m_pDeleteLayoutButton->setToolTip(UISoftKeyboard::tr("Delete the selected layout"));
    if (m_pCopyLayoutButton)
        m_pCopyLayoutButton->setToolTip(UISoftKeyboard::tr("Copy the selected layout"));
    if (m_pSaveLayoutButton)
        m_pSaveLayoutButton->setToolTip(UISoftKeyboard::tr("Save the selected layout into File"));
    if (m_pTitleLabel)
        m_pTitleLabel->setText(UISoftKeyboard::tr("Layout List"));
    if (m_pCloseButton)
    {
        m_pCloseButton->setToolTip(UISoftKeyboard::tr("Close the layout list"));
        m_pCloseButton->setText("Close");
    }
}

void UILayoutSelector::prepareObjects()
{
    QVBoxLayout *pLayout = new QVBoxLayout;
    if (!pLayout)
        return;
    pLayout->setSpacing(0);
    setLayout(pLayout);

    QHBoxLayout *pTitleLayout = new QHBoxLayout;
    m_pCloseButton = new QToolButton;
    m_pCloseButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_pCloseButton->setIcon(UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_DialogCancel));
    //m_pCloseButton->setStyleSheet("QToolButton { border: 0px none black; margin: 0px 0px 0px 0px; } QToolButton::menu-indicator {image: none;}");
    m_pCloseButton->setAutoRaise(true);
    connect(m_pCloseButton, &QToolButton::clicked, this, &UILayoutSelector::sigCloseLayoutList);
    pTitleLayout->addWidget(m_pCloseButton);
    pTitleLayout->addStretch(2);
    m_pTitleLabel = new QLabel;
    pTitleLayout->addWidget(m_pTitleLabel);
    pLayout->addLayout(pTitleLayout);

    m_pLayoutListWidget = new QListWidget;
    pLayout->addWidget(m_pLayoutListWidget);
    connect(m_pLayoutListWidget, &QListWidget::currentTextChanged, this, &UILayoutSelector::sigLayoutSelectionChanged);

    QHBoxLayout *pButtonsLayout = new QHBoxLayout;
    pLayout->addLayout(pButtonsLayout);

    m_pEditLayoutButton = new QToolButton;
    m_pEditLayoutButton->setIcon(UIIconPool::iconSet(":/keyboard_settings_16px.png"));
    pButtonsLayout->addWidget(m_pEditLayoutButton);
    connect(m_pEditLayoutButton, &QToolButton::clicked, this, &UILayoutSelector::sigShowLayoutEditor);

    m_pCopyLayoutButton = new QToolButton;
    m_pCopyLayoutButton->setIcon(UIIconPool::iconSet(":/vm_clone_16px.png"));
    pButtonsLayout->addWidget(m_pCopyLayoutButton);
    connect(m_pCopyLayoutButton, &QToolButton::clicked, this, &UILayoutSelector::sigCopyLayout);

    m_pSaveLayoutButton = new QToolButton;
    m_pSaveLayoutButton->setIcon(UIIconPool::iconSet(":/vm_save_state_16px.png"));
    pButtonsLayout->addWidget(m_pSaveLayoutButton);
    connect(m_pSaveLayoutButton, &QToolButton::clicked, this, &UILayoutSelector::sigSaveLayout);

    m_pDeleteLayoutButton = new QToolButton;
    m_pDeleteLayoutButton->setIcon(UIIconPool::iconSet(":/file_manager_delete_16px.png"));
    pButtonsLayout->addWidget(m_pDeleteLayoutButton);
    connect(m_pDeleteLayoutButton, &QToolButton::clicked, this, &UILayoutSelector::sigDeleteLayout);

    pButtonsLayout->addStretch(2);

    retranslateUi();
}

/*********************************************************************************************************************************
*   UISoftKeyboardRow implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardRow::UISoftKeyboardRow()
    : m_iDefaultWidth(0)
    , m_iDefaultHeight(0)
    , m_iSpaceHeightAfter(0)
{
}

void UISoftKeyboardRow::setDefaultWidth(int iWidth)
{
    m_iDefaultWidth = iWidth;
}

int UISoftKeyboardRow::defaultWidth() const
{
    return m_iDefaultWidth;
}

void UISoftKeyboardRow::setDefaultHeight(int iHeight)
{
    m_iDefaultHeight = iHeight;
}

int UISoftKeyboardRow::defaultHeight() const
{
    return m_iDefaultHeight;
}

QVector<UISoftKeyboardKey> &UISoftKeyboardRow::keys()
{
    return m_keys;
}

const QVector<UISoftKeyboardKey> &UISoftKeyboardRow::keys() const
{
    return m_keys;
}

void UISoftKeyboardRow::setSpaceHeightAfter(int iSpace)
{
    m_iSpaceHeightAfter = iSpace;
}

int UISoftKeyboardRow::spaceHeightAfter() const
{
    return m_iSpaceHeightAfter;
}

/*********************************************************************************************************************************
*   UISoftKeyboardKey implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardKey::UISoftKeyboardKey()
    : m_enmType(UIKeyType_Ordinary)
    , m_enmState(UIKeyState_NotPressed)
    , m_iWidth(0)
    , m_iHeight(0)
    , m_iSpaceWidthAfter(0)
    , m_scanCode(0)
    , m_scanCodePrefix(0)
    , m_iCutoutWidth(0)
    , m_iCutoutHeight(0)
    , m_iCutoutCorner(-1)
    , m_iPosition(0)
    , m_pParentWidget(0)
    , m_fIsNumPadKey(false)
    , m_fIsOSMenuKey(false)

{
}

const QRect UISoftKeyboardKey::keyGeometry() const
{
    return m_keyGeometry;
}

void UISoftKeyboardKey::setKeyGeometry(const QRect &rect)
{
    m_keyGeometry = rect;
}

const QString UISoftKeyboardKey::baseCaption() const
{
    return m_strBaseCaption;
}

void UISoftKeyboardKey::setBaseCaption(const QString &strBaseCaption)
{
    m_strBaseCaption = strBaseCaption;
    updateText();
}

const QString UISoftKeyboardKey::shiftCaption() const
{
    return m_strShiftCaption;
}

void UISoftKeyboardKey::setShiftCaption(const QString &strShiftCaption)
{
    m_strShiftCaption = strShiftCaption;
    updateText();
}

const QString UISoftKeyboardKey::altGrCaption() const
{
    return m_strAltGrCaption;
}

void UISoftKeyboardKey::setAltGrCaption(const QString &strAltGrCaption)
{
    m_strAltGrCaption = strAltGrCaption;
    updateText();
}

const QString UISoftKeyboardKey::shiftAltGrCaption() const
{
    return m_strShiftAltGrCaption;
}

void UISoftKeyboardKey::setShiftAltGrCaption(const QString &strShiftAltGrCaption)
{
    m_strShiftAltGrCaption = strShiftAltGrCaption;
    updateText();
}

void UISoftKeyboardKey::resetCaptions()
{
    m_strBaseCaption.clear();
    m_strShiftCaption.clear();
    m_strAltGrCaption.clear();
    m_strShiftAltGrCaption.clear();
    updateText();
}

const QString UISoftKeyboardKey::text() const
{
    return m_strText;
}

void UISoftKeyboardKey::setWidth(int iWidth)
{
    m_iWidth = iWidth;
}

int UISoftKeyboardKey::width() const
{
    return m_iWidth;
}

void UISoftKeyboardKey::setHeight(int iHeight)
{
    m_iHeight = iHeight;
}

int UISoftKeyboardKey::height() const
{
    return m_iHeight;
}

void UISoftKeyboardKey::setScanCode(LONG scanCode)
{
    m_scanCode = scanCode;
}

LONG UISoftKeyboardKey::scanCode() const
{
    return m_scanCode;
}

void UISoftKeyboardKey::setScanCodePrefix(LONG scanCodePrefix)
{
    m_scanCodePrefix = scanCodePrefix;
}

LONG UISoftKeyboardKey::scanCodePrefix() const
{
    return m_scanCodePrefix;
}

void UISoftKeyboardKey::setSpaceWidthAfter(int iSpace)
{
    m_iSpaceWidthAfter = iSpace;
}

int UISoftKeyboardKey::spaceWidthAfter() const
{
    return m_iSpaceWidthAfter;
}

void UISoftKeyboardKey::setPosition(int iPosition)
{
    m_iPosition = iPosition;
}

int UISoftKeyboardKey::position() const
{
    return m_iPosition;
}

void UISoftKeyboardKey::setType(UIKeyType enmType)
{
    m_enmType = enmType;
}

UIKeyType UISoftKeyboardKey::type() const
{
    return m_enmType;
}

void UISoftKeyboardKey::setIsNumPadKey(bool fIsNumPadKey)
{
    m_fIsNumPadKey = fIsNumPadKey;
}

bool UISoftKeyboardKey::isNumPadKey() const
{
    return m_fIsNumPadKey;
}

void UISoftKeyboardKey::setIsOSMenuKey(bool fIsOSMenuKey)
{
    m_fIsOSMenuKey = fIsOSMenuKey;
}

bool UISoftKeyboardKey::isOSMenuKey() const
{
    return m_fIsOSMenuKey;
}

void UISoftKeyboardKey::setCutout(int iCorner, int iWidth, int iHeight)
{
    m_iCutoutCorner = iCorner;
    m_iCutoutWidth = iWidth;
    m_iCutoutHeight = iHeight;
}

UIKeyState UISoftKeyboardKey::state() const
{
    return m_enmState;
}

void UISoftKeyboardKey::setParentWidget(UISoftKeyboardWidget* pParent)
{
    m_pParentWidget = pParent;
}

void UISoftKeyboardKey::release()
{
    updateState(false);
}

void UISoftKeyboardKey::press()
{
    updateState(true);
}

void UISoftKeyboardKey::setPolygon(const QPolygon &polygon)
{
    m_polygon = polygon;
}

const QPolygon &UISoftKeyboardKey::polygon() const
{
    return m_polygon;
}

QPolygon UISoftKeyboardKey::polygonInGlobal() const
{
    QPolygon globalPolygon(m_polygon);
    globalPolygon.translate(m_keyGeometry.x(), m_keyGeometry.y());
    return globalPolygon;
}

int UISoftKeyboardKey::cutoutCorner() const
{
    return m_iCutoutCorner;
}

int UISoftKeyboardKey::cutoutWidth() const
{
    return m_iCutoutWidth;
}

int UISoftKeyboardKey::cutoutHeight() const
{
    return m_iCutoutHeight;
}

void UISoftKeyboardKey::updateState(bool fPressed)
{
    UIKeyState enmPreviousState = state();
    if (m_enmType == UIKeyType_Modifier)
    {
        if (fPressed)
        {
            if (m_enmState == UIKeyState_NotPressed)
                m_enmState = UIKeyState_Pressed;
            else if(m_enmState == UIKeyState_Pressed)
                m_enmState = UIKeyState_Locked;
            else
                m_enmState = UIKeyState_NotPressed;
        }
        else
        {
            if(m_enmState == UIKeyState_Pressed)
                m_enmState = UIKeyState_NotPressed;
        }
    }
    else if (m_enmType == UIKeyType_Toggleable)
    {
        if (fPressed)
        {
            if (m_enmState == UIKeyState_NotPressed)
                 m_enmState = UIKeyState_Locked;
            else
                m_enmState = UIKeyState_NotPressed;
        }
    }
    else if (m_enmType == UIKeyType_Ordinary)
    {
        if (m_enmState == UIKeyState_NotPressed)
            m_enmState = UIKeyState_Pressed;
        else
            m_enmState = UIKeyState_NotPressed;
    }
    if (enmPreviousState != state() && m_pParentWidget)
        m_pParentWidget->keyStateChange(this);
}

void UISoftKeyboardKey::updateText()
{
    QPainter painter(&m_textPixmap);
    painter.setPen(QColor(0,0,0));
    painter.setRenderHint(QPainter::Antialiasing);
    QFont painterFont(painter.font());

    painterFont.setPixelSize(15);
    painterFont.setBold(true);
    painter.setFont(painterFont);
    QFontMetrics fontMetric = painter.fontMetrics();
    int iSideMargin = 0.5 * fontMetric.width('X');

    int iX = 0;
    int iY = fontMetric.height();
#if 0
    painter.drawText(iX + iSideMargin, iY, QString::number(m_iPosition));
#else
    if (!m_strShiftCaption.isEmpty())
    {
        painter.drawText(iX + iSideMargin, iY, m_strShiftCaption);
        painter.drawText(iX + iSideMargin, 2 * iY, m_strBaseCaption);
    }
    else
    {
        int iSpaceIndex = m_strBaseCaption.indexOf(" " );
        if (iSpaceIndex == -1)
            painter.drawText(iX + iSideMargin, iY, m_strBaseCaption);
        else
        {
            painter.drawText(iX + iSideMargin, iY, m_strBaseCaption.left(iSpaceIndex));
            painter.drawText(iX + iSideMargin, 2 * iY, m_strBaseCaption.right(m_strBaseCaption.length() - iSpaceIndex - 1));
        }
    }

    if (!m_strShiftAltGrCaption.isEmpty())
    {
        painter.drawText(keyGeometry().width() - fontMetric.width('X') - iSideMargin, iY, m_strShiftAltGrCaption);
        painter.drawText(keyGeometry().width() - fontMetric.width('X') - iSideMargin, 2 * iY, m_strAltGrCaption);
    }
    else
        painter.drawText(keyGeometry().width() - fontMetric.width('X') - iSideMargin, 2 * iY, m_strAltGrCaption);
#endif
    painter.end();
}

/*********************************************************************************************************************************
*   UISoftKeyboardLayout implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardLayout::UISoftKeyboardLayout()
    : m_fEditable(true)
    , m_fIsFromResources(false)
{
}

void UISoftKeyboardLayout::updateKeyCaptions(int iKeyPosition, KeyCaptions &newCaptions)
{
    if (!m_keyCapMap.contains(iKeyPosition))
        return;
    m_keyCapMap[iKeyPosition] = newCaptions;
}

void UISoftKeyboardLayout::setSourceFilePath(const QString& strSourceFilePath)
{
    m_strSourceFilePath = strSourceFilePath;
}

const QString& UISoftKeyboardLayout::sourceFilePath() const
{
    return m_strSourceFilePath;
}

void UISoftKeyboardLayout::setIsFromResources(bool fIsFromResources)
{
    m_fIsFromResources = fIsFromResources;
}

bool UISoftKeyboardLayout::isFromResources() const
{
    return m_fIsFromResources;
}

void UISoftKeyboardLayout::setName(const QString &strName)
{
    m_strName = strName;
}

const QString &UISoftKeyboardLayout::name() const
{
    return m_strName;
}

void UISoftKeyboardLayout::setEditable(bool fEditable)
{
    m_fEditable = fEditable;
}

bool UISoftKeyboardLayout::editable() const
{
    return m_fEditable;
}

void UISoftKeyboardLayout::setPhysicalLayoutUuid(const QUuid &uuid)
{
    m_physicalLayoutUuid = uuid;
}

const QUuid &UISoftKeyboardLayout::physicalLayoutUuid() const
{
    return m_physicalLayoutUuid;
}

void UISoftKeyboardLayout::setKeyCapMap(const QMap<int, KeyCaptions> &keyCapMap)
{
    m_keyCapMap = keyCapMap;
}

QMap<int, KeyCaptions> &UISoftKeyboardLayout::keyCapMap()
{
    return m_keyCapMap;
}

const QMap<int, KeyCaptions> &UISoftKeyboardLayout::keyCapMap() const
{
    return m_keyCapMap;
}

bool UISoftKeyboardLayout::operator==(const UISoftKeyboardLayout &otherLayout)
{
    if (m_strName != otherLayout.m_strName)
        return false;
    if (m_physicalLayoutUuid != otherLayout.m_physicalLayoutUuid)
        return false;
    if (m_fEditable != otherLayout.m_fEditable)
        return false;
    if (m_strSourceFilePath != otherLayout.m_strSourceFilePath)
        return false;
    if (m_fIsFromResources != otherLayout.m_fIsFromResources)
        return false;
    return true;
}


/*********************************************************************************************************************************
*   UISoftKeyboardWidget implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardWidget::UISoftKeyboardWidget(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pKeyUnderMouse(0)
    , m_pKeyBeingEdited(0)
    , m_pKeyPressed(0)
    , m_keyDefaultColor(QColor(103, 128, 159))
    , m_keyHoverColor(QColor(108, 122, 137))
    , m_textDefaultColor(QColor(46, 49, 49))
    , m_textPressedColor(QColor(149, 165, 166))
    , m_keyEditColor(QColor(249, 165, 166))
    , m_pCurrentKeyboardLayout(0)
    , m_iInitialHeight(0)
    , m_iInitialWidth(0)
    , m_iInitialWidthNoNumPad(0)
    , m_iXSpacing(5)
    , m_iYSpacing(5)
    , m_iLeftMargin(10)
    , m_iTopMargin(10)
    , m_iRightMargin(10)
    , m_iBottomMargin(10)
    , m_pContextMenu(0)
    , m_enmMode(Mode_Keyboard)
    , m_fShowOSMenuKeys(true)
    , m_fShowNumPad(true)
{
    prepareObjects();
}

QSize UISoftKeyboardWidget::minimumSizeHint() const
{
    float fScale = 0.5f;
    return QSize(fScale * m_minimumSize.width(), fScale * m_minimumSize.height());
}

QSize UISoftKeyboardWidget::sizeHint() const
{
    float fScale = 0.5f;
    return QSize(fScale * m_minimumSize.width(), fScale * m_minimumSize.height());
}

void UISoftKeyboardWidget::paintEvent(QPaintEvent *pEvent) /* override */
{
    Q_UNUSED(pEvent);
    if (!m_pCurrentKeyboardLayout || m_iInitialWidth == 0 || m_iInitialWidthNoNumPad == 0 || m_iInitialHeight == 0)
        return;

    if (m_fShowNumPad)
        m_fScaleFactorX = width() / (float) m_iInitialWidth;
    else
        m_fScaleFactorX = width() / (float) m_iInitialWidthNoNumPad;
    m_fScaleFactorY = height() / (float) m_iInitialHeight;

    QPainter painter(this);
    QFont painterFont(font());
    painterFont.setPixelSize(15);
    painterFont.setBold(true);
    painter.setFont(painterFont);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(m_fScaleFactorX, m_fScaleFactorY);
    int unitSize = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    float fLedRadius =  0.8 * unitSize;
    float fLedMargin =  0.6 * unitSize;


    UISoftKeyboardPhysicalLayout *pPhysicalLayout = findPhysicalLayout(m_pCurrentKeyboardLayout->physicalLayoutUuid());
    if (!pPhysicalLayout)
        return;

    QVector<UISoftKeyboardRow> &rows = pPhysicalLayout->rows();
    for (int i = 0; i < rows.size(); ++i)
    {
        QVector<UISoftKeyboardKey> &keys = rows[i].keys();
        for (int j = 0; j < keys.size(); ++j)
        {
            UISoftKeyboardKey &key = keys[j];
            if (!m_fShowOSMenuKeys && key.isOSMenuKey())
                continue;

            if (!m_fShowNumPad &&key.isNumPadKey())
                continue;

            painter.translate(key.keyGeometry().x(), key.keyGeometry().y());

            if(&key  == m_pKeyBeingEdited)
                painter.setBrush(QBrush(m_keyEditColor));
            else if (&key  == m_pKeyUnderMouse)
                painter.setBrush(QBrush(m_keyHoverColor));
            else
                painter.setBrush(QBrush(m_keyDefaultColor));

            if (&key  == m_pKeyPressed)
                painter.setPen(QPen(QColor(m_textPressedColor), 2));
            else
                painter.setPen(QPen(QColor(m_textDefaultColor), 2));

            painter.drawPolygon(key.polygon());

            key.m_textPixmap.play(&painter);

            if (key.type() != UIKeyType_Ordinary)
            {
                QColor ledColor;
                if (key.state() == UIKeyState_NotPressed)
                    ledColor = m_textDefaultColor;
                else if (key.state() == UIKeyState_Pressed)
                    ledColor = QColor(0, 255, 0);
                else
                    ledColor = QColor(255, 0, 0);
                painter.setBrush(ledColor);
                painter.setPen(ledColor);
                QRectF rectangle(key.keyGeometry().width() - 2 * fLedMargin, fLedMargin, fLedRadius, fLedRadius);
                painter.drawEllipse(rectangle);
            }
            painter.translate(-key.keyGeometry().x(), -key.keyGeometry().y());

        }
    }
}

void UISoftKeyboardWidget::mousePressEvent(QMouseEvent *pEvent)
{
    QWidget::mousePressEvent(pEvent);
    if (pEvent->button() != Qt::LeftButton)
        return;
    m_pKeyPressed = keyUnderMouse(pEvent);

    if (m_enmMode == Mode_Keyboard)
        handleKeyPress(m_pKeyPressed);
    else if (m_enmMode == Mode_LayoutEdit)
        setKeyBeingEdited(m_pKeyUnderMouse);
    update();
}

void UISoftKeyboardWidget::mouseReleaseEvent(QMouseEvent *pEvent)
{
    QWidget::mouseReleaseEvent(pEvent);
    if (pEvent->button() != Qt::LeftButton)
        return;
    if (!m_pKeyPressed)
        return;

    if (m_enmMode == Mode_Keyboard)
        handleKeyRelease(m_pKeyPressed);

    update();
    m_pKeyPressed = 0;
}

void UISoftKeyboardWidget::mouseMoveEvent(QMouseEvent *pEvent)
{
    QWidget::mouseMoveEvent(pEvent);
    keyUnderMouse(pEvent);
}

void UISoftKeyboardWidget::retranslateUi()
{
}

void UISoftKeyboardWidget::sltContextMenuRequest(const QPoint &point)
{
    showContextMenu(mapToGlobal(point));
}

void UISoftKeyboardWidget::saveCurentLayoutToFile()
{
    if (!m_pCurrentKeyboardLayout)
        return;

    QString strHomeFolder = vboxGlobal().homeFolder();
    QDir dir(strHomeFolder);
    if (!dir.exists(strSubDirectorName))
    {
        if (!dir.mkdir(strSubDirectorName))
        {
            sigStatusBarMessage(QString("%1 %2").arg(UISoftKeyboard::tr("Error! Could not create folder under").arg(strHomeFolder)));
            return;
        }
    }

    strHomeFolder += QString(QDir::separator()) + strSubDirectorName;
    QInputDialog dialog(this);
    dialog.setInputMode(QInputDialog::TextInput);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setWindowTitle(UISoftKeyboard::tr("Provide a file name"));
    dialog.setTextValue(m_pCurrentKeyboardLayout->name());
    dialog.setLabelText(QString("%1 %2").arg(UISoftKeyboard::tr("The file will be saved under:\n")).arg(strHomeFolder));
    if (dialog.exec() == QDialog::Rejected)
        return;
    QString strFileName(dialog.textValue());
    if (strFileName.isEmpty() || strFileName.contains("..") || strFileName.contains(QDir::separator()))
    {
        sigStatusBarMessage(QString("%1 %2").arg(strFileName).arg(UISoftKeyboard::tr(" is an invalid file name")));
        return;
    }

    UISoftKeyboardPhysicalLayout *pPhysicalLayout = findPhysicalLayout(m_pCurrentKeyboardLayout->physicalLayoutUuid());
    if (!pPhysicalLayout)
    {
        sigStatusBarMessage("The layout file could not be saved");
        return;
    }

    QFileInfo fileInfo(strFileName);
    if (fileInfo.suffix().compare("xml", Qt::CaseInsensitive) != 0)
        strFileName += ".xml";
    strFileName = strHomeFolder + QString(QDir::separator()) + strFileName;
    QFile xmlFile(strFileName);
    if (!xmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        sigStatusBarMessage("The layout file could not be saved");
        return;
    }

    QXmlStreamWriter xmlWriter;
    xmlWriter.setDevice(&xmlFile);

    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument("1.0");
    xmlWriter.writeStartElement("layout");
    xmlWriter.writeTextElement("name", m_pCurrentKeyboardLayout->name());
    xmlWriter.writeTextElement("physicallayoutid", pPhysicalLayout->m_uId.toString());

    QVector<UISoftKeyboardRow> &rows = pPhysicalLayout->rows();
    for (int i = 0; i < rows.size(); ++i)
    {
        QVector<UISoftKeyboardKey> &keys = rows[i].keys();

       for (int j = 0; j < keys.size(); ++j)
       {
           xmlWriter.writeStartElement("key");

           UISoftKeyboardKey &key = keys[j];
           xmlWriter.writeTextElement("position", QString::number(key.position()));
           xmlWriter.writeTextElement("basecaption", key.baseCaption());
           xmlWriter.writeTextElement("shiftcaption", key.shiftCaption());
           xmlWriter.writeTextElement("altgrcaption", key.altGrCaption());
           xmlWriter.writeTextElement("shiftaltgrcaption", key.shiftAltGrCaption());
           xmlWriter.writeEndElement();
       }
   }
   xmlWriter.writeEndElement();
   xmlWriter.writeEndDocument();

   xmlFile.close();
   m_pCurrentKeyboardLayout->setSourceFilePath(strFileName);
   sigStatusBarMessage(QString("%1 %2").arg(strFileName).arg(UISoftKeyboard::tr(" is saved")));
}

void UISoftKeyboardWidget::copyCurentLayout()
{
    UISoftKeyboardLayout newLayout(*(m_pCurrentKeyboardLayout));
    QString strNewName = QString("%1-%2").arg(newLayout.name()).arg(tr("Copy"));
    newLayout.setName(strNewName);
    newLayout.setEditable(true);
    newLayout.setIsFromResources(false);
    newLayout.setSourceFilePath(QString());
    addLayout(newLayout);
}

float UISoftKeyboardWidget::layoutAspectRatio()
{
    if (m_iInitialWidth == 0)
        return 1.f;
    return  m_iInitialHeight / (float) m_iInitialWidth;
}

bool UISoftKeyboardWidget::showOSMenuKeys()
{
    return m_fShowOSMenuKeys;
}

void UISoftKeyboardWidget::setShowOSMenuKeys(bool fShow)
{
    if (m_fShowOSMenuKeys == fShow)
        return;
    m_fShowOSMenuKeys = fShow;
    update();
}

bool UISoftKeyboardWidget::showNumPad()
{
    return m_fShowNumPad;
}

void UISoftKeyboardWidget::setShowNumPad(bool fShow)
{
    if (m_fShowNumPad == fShow)
        return;
    m_fShowNumPad = fShow;
    update();
}

void UISoftKeyboardWidget::deleteCurrentLayout()
{
    if (!m_pCurrentKeyboardLayout || !m_pCurrentKeyboardLayout->editable() || m_pCurrentKeyboardLayout->isFromResources())
        return;
    /* Make sure we have at least one layout. */
    if (m_layouts.size() <= 1)
        return;

    int iIndex = m_layouts.indexOf(*(m_pCurrentKeyboardLayout));
    if (iIndex == -1)
        return;

    QDir fileToDelete;
    QString strFilePath(m_pCurrentKeyboardLayout->sourceFilePath());
    bool fFileExists = false;
    if (!strFilePath.isEmpty())
        fFileExists = fileToDelete.exists(strFilePath);

    if (fFileExists)
    {
        if (!msgCenter().questionBinary(this, MessageType_Question,
                                        QString(UISoftKeyboard::tr("This will delete the keyboard layout file as well, Proceed?")),
                                        0 /* auto-confirm id */,
                                        QString("Delete") /* ok button text */,
                                        QString() /* cancel button text */,
                                        false /* ok button by default? */))
            return;
    }

    m_layouts.removeAt(iIndex);
    setCurrentLayout(&(m_layouts[0]));
    /* It might be that the layout copied but not yet saved into a file: */
    if (fFileExists)
    {
        if (fileToDelete.remove(strFilePath))
            sigStatusBarMessage(QString("%1 %2 %3").arg(UISoftKeyboard::tr("The file ")).arg(strFilePath).arg(UISoftKeyboard::tr(" has been deleted")));
        else
            sigStatusBarMessage(QString("%1 %2 %3").arg(UISoftKeyboard::tr("Deleting the file ")).arg(strFilePath).arg(UISoftKeyboard::tr(" has failed")));
    }
}

void UISoftKeyboardWidget::toggleEditMode(bool fIsEditMode)
{
    if (fIsEditMode)
        m_enmMode = Mode_LayoutEdit;
    else
    {
        m_enmMode = Mode_Keyboard;
        m_pKeyBeingEdited = 0;
    }
    update();
}

void UISoftKeyboardWidget::updateKeyCaptionsInLayout(UISoftKeyboardKey *pKey)
{
    if (!m_pCurrentKeyboardLayout || !pKey)
        return;

    /* Assuming the key captions are changed for the current layout: */
    KeyCaptions newCaptions;
    newCaptions.m_strBase = pKey->baseCaption();
    newCaptions.m_strShift = pKey->shiftCaption();
    newCaptions.m_strAltGr = pKey->altGrCaption();
    newCaptions.m_strShiftAltGr = pKey->shiftAltGrCaption();
    m_pCurrentKeyboardLayout->updateKeyCaptions(pKey->position(), newCaptions);
}

void UISoftKeyboardWidget::addLayout(const UISoftKeyboardLayout &newLayout)
{
    m_layouts.append(newLayout);
}

void UISoftKeyboardWidget::sltPhysicalLayoutForLayoutChanged(int iIndex)
{
    if (!m_pCurrentKeyboardLayout)
        return;
    if (iIndex < 0 || iIndex >= m_physicalLayouts.size())
        return;

    if (m_pCurrentKeyboardLayout->physicalLayoutUuid() == m_physicalLayouts[iIndex].m_uId)
        return;
    m_pCurrentKeyboardLayout->setPhysicalLayoutUuid(m_physicalLayouts[iIndex].m_uId);
    update();
}

void UISoftKeyboardWidget::setNewMinimumSize(const QSize &size)
{
    m_minimumSize = size;
    updateGeometry();
}

void UISoftKeyboardWidget::setInitialSize(int iWidth, int iHeight)
{
    m_iInitialWidth = iWidth;
    m_iInitialHeight = iHeight;
}

UISoftKeyboardKey *UISoftKeyboardWidget::keyUnderMouse(QMouseEvent *pEvent)
{
    QPoint eventPosition(pEvent->pos().x() / m_fScaleFactorX, pEvent->pos().y() / m_fScaleFactorY);
    return keyUnderMouse(eventPosition);
}

UISoftKeyboardKey *UISoftKeyboardWidget::keyUnderMouse(const QPoint &eventPosition)
{
    if (!m_pCurrentKeyboardLayout)
        return 0;
    UISoftKeyboardPhysicalLayout *pPhysicalLayout = findPhysicalLayout(m_pCurrentKeyboardLayout->physicalLayoutUuid());
    if (!pPhysicalLayout)
        return 0;

    UISoftKeyboardKey *pKey = 0;
    QVector<UISoftKeyboardRow> &rows = pPhysicalLayout->rows();
    for (int i = 0; i < rows.size(); ++i)
    {
        QVector<UISoftKeyboardKey> &keys = rows[i].keys();
        for (int j = 0; j < keys.size(); ++j)
        {
            UISoftKeyboardKey &key = keys[j];
            if (key.polygonInGlobal().containsPoint(eventPosition, Qt::OddEvenFill))
            {
                pKey = &key;
                break;
            }
        }
    }
    if (m_pKeyUnderMouse != pKey)
    {
        m_pKeyUnderMouse = pKey;
        update();
    }
    return pKey;
}

void UISoftKeyboardWidget::handleKeyPress(UISoftKeyboardKey *pKey)
{
    if (!pKey)
        return;
    pKey->press();

    if (pKey->type() == UIKeyType_Modifier)
        return;

    QVector<LONG> sequence;
     /* Add the pressed modifiers first: */
    for (int i = 0; i < m_pressedModifiers.size(); ++i)
    {
        UISoftKeyboardKey *pModifier = m_pressedModifiers[i];
        if (pModifier->scanCodePrefix() != 0)
            sequence << pModifier->scanCodePrefix();
        sequence << pModifier->scanCode();
    }

    if (pKey->scanCodePrefix() != 0)
        sequence << pKey->scanCodePrefix();
    sequence << pKey->scanCode();
    emit sigPutKeyboardSequence(sequence);
}

void UISoftKeyboardWidget::keyStateChange(UISoftKeyboardKey* pKey)
{
    if (!pKey)
        return;
    if (pKey->type() == UIKeyType_Modifier)
    {
        if (pKey->state() == UIKeyState_NotPressed)
            m_pressedModifiers.removeOne(pKey);
        else
            if (!m_pressedModifiers.contains(pKey))
                m_pressedModifiers.append(pKey);
    }
}

void UISoftKeyboardWidget::showContextMenu(const QPoint &globalPoint)
{
    m_pContextMenu->exec(globalPoint);
    update();
}

void UISoftKeyboardWidget::setCurrentLayout(const QString &strLayoutName)
{
    UISoftKeyboardLayout *pLayout = findLayoutByName(strLayoutName);
    if (!pLayout)
        return;
    setCurrentLayout(pLayout);
}

UISoftKeyboardLayout *UISoftKeyboardWidget::currentLayout()
{
    return m_pCurrentKeyboardLayout;
}

void UISoftKeyboardWidget::handleKeyRelease(UISoftKeyboardKey *pKey)
{
    if (!pKey)
        return;
    if (pKey->type() == UIKeyType_Ordinary)
        pKey->release();
    /* We only send the scan codes of Ordinary keys: */
    if (pKey->type() == UIKeyType_Modifier)
        return;

    QVector<LONG> sequence;
    if (pKey->scanCodePrefix() != 0)
        sequence <<  pKey->scanCodePrefix();
    sequence << (pKey->scanCode() | 0x80);

    /* Add the pressed modifiers in the reverse order: */
    for (int i = m_pressedModifiers.size() - 1; i >= 0; --i)
    {
        UISoftKeyboardKey *pModifier = m_pressedModifiers[i];
        if (pModifier->scanCodePrefix() != 0)
            sequence << pModifier->scanCodePrefix();
        sequence << (pModifier->scanCode() | 0x80);
        /* Release the pressed modifiers (if there are not locked): */
        pModifier->release();
    }
    emit sigPutKeyboardSequence(sequence);
}

bool UISoftKeyboardWidget::loadPhysicalLayout(const QString &strLayoutFileName, bool isNumPad /* = false */)
{
    if (strLayoutFileName.isEmpty())
        return false;
    UIPhysicalLayoutReader reader;
    UISoftKeyboardPhysicalLayout *newPhysicalLayout = 0;
    if (!isNumPad)
    {
        m_physicalLayouts.append(UISoftKeyboardPhysicalLayout());
        newPhysicalLayout = &(m_physicalLayouts.back());
    }
    else
        newPhysicalLayout = &(m_numPadLayout);

    if (!reader.parseXMLFile(strLayoutFileName, *newPhysicalLayout))
    {
        m_physicalLayouts.removeLast();
        return false;
    }

    if (isNumPad)
    {
        /* Mark all the key as numpad keys: */
        for (int i = 0; i < m_numPadLayout.rows().size(); ++i)
        {
            UISoftKeyboardRow &row = m_numPadLayout.rows()[i];
            for (int j = 0; j < row.keys().size(); ++j)
            {
                row.keys()[j].setIsNumPadKey(true);
            }
        }
        return true;
    }

    int iY = m_iTopMargin;
    int iMaxWidth = 0;
    int iMaxWidthNoNumPad = 0;
    const QVector<UISoftKeyboardRow> &numPadRows = m_numPadLayout.rows();
    QVector<UISoftKeyboardRow> &rows = newPhysicalLayout->rows();
    int iKeyCount = 0;
    for (int i = 0; i < rows.size(); ++i)
    {
        UISoftKeyboardRow &row = rows[i];
        /* Start adding the numpad keys after the 0th row: */
        if (i > 0)
        {
            int iNumPadRowIndex = i - 1;
            if (iNumPadRowIndex < numPadRows.size())
            {
                for (int m = 0; m < numPadRows[iNumPadRowIndex].keys().size(); ++m)
                    row.keys().append(numPadRows[iNumPadRowIndex].keys()[m]);
            }
        }

        int iX = m_iLeftMargin;
        int iXNoNumPad = m_iLeftMargin;
        int iRowHeight = row.defaultHeight();
        for (int j = 0; j < row.keys().size(); ++j)
        {
            ++iKeyCount;
            UISoftKeyboardKey &key = (row.keys())[j];
            key.setKeyGeometry(QRect(iX, iY, key.width(), key.height()));
            key.setPolygon(QPolygon(UIPhysicalLayoutReader::computeKeyVertices(key)));
            key.setParentWidget(this);
            iX += key.width();
            if (j < row.keys().size() - 1)
                iX += m_iXSpacing;
            if (key.spaceWidthAfter() != 0)
                iX += (m_iXSpacing + key.spaceWidthAfter());

            if (!key.isNumPadKey())
            {
                iXNoNumPad += key.width();
                if (j < row.keys().size() - 1)
                    iXNoNumPad += m_iXSpacing;
                if (key.spaceWidthAfter() != 0)
                    iXNoNumPad += (m_iXSpacing + key.spaceWidthAfter());
            }
        }
        if (row.spaceHeightAfter() != 0)
            iY += row.spaceHeightAfter() + m_iYSpacing;
        iMaxWidth = qMax(iMaxWidth, iX);
        iMaxWidthNoNumPad = qMax(iMaxWidthNoNumPad, iXNoNumPad);
        iY += iRowHeight;
        if (i < rows.size() - 1)
            iY += m_iYSpacing;
    }
    int iInitialWidth = iMaxWidth + m_iRightMargin;
    int iInitialWidthNoNumPad = iMaxWidthNoNumPad + m_iRightMargin;
    int iInitialHeight = iY + m_iBottomMargin;
    m_iInitialWidth = qMax(m_iInitialWidth, iInitialWidth);
    m_iInitialWidthNoNumPad = qMax(m_iInitialWidthNoNumPad, iInitialWidthNoNumPad);
    m_iInitialHeight = qMax(m_iInitialHeight, iInitialHeight);
    //printf("%s total key count: %d\n", qPrintable(strLayoutFileName), iKeyCount - 3 /* substract OS an menu keys */);
    return true;
}

bool UISoftKeyboardWidget::loadKeyboardLayout(const QString &strLayoutFileName)
{
    if (strLayoutFileName.isEmpty())
        return false;

    UIKeyboardLayoutReader keyboardLayoutReader;

    if (!keyboardLayoutReader.parseFile(strLayoutFileName))
        return false;

    UISoftKeyboardPhysicalLayout *pPhysicalLayout = findPhysicalLayout(keyboardLayoutReader.physicalLayoutUUID());
    /* If no pyhsical layout with the UUID the keyboard layout refers is found then cancel loading the keyboard layout: */
    if (!pPhysicalLayout)
        return false;

    m_layouts.append(UISoftKeyboardLayout());
    UISoftKeyboardLayout &newLayout = m_layouts.back();
    newLayout.setPhysicalLayoutUuid(pPhysicalLayout->m_uId);
    newLayout.setName(keyboardLayoutReader.name());
    newLayout.setSourceFilePath(strLayoutFileName);
    newLayout.setKeyCapMap(keyboardLayoutReader.keyCapMap());
    return true;
}

UISoftKeyboardPhysicalLayout *UISoftKeyboardWidget::findPhysicalLayout(const QUuid &uuid)
{
    for (int i = 0; i < m_physicalLayouts.size(); ++i)
    {
        if (m_physicalLayouts[i].m_uId == uuid)
            return &(m_physicalLayouts[i]);
    }
    return 0;
}

void UISoftKeyboardWidget::reset()
{
    m_pressedModifiers.clear();
}

void UISoftKeyboardWidget::loadLayouts()
{
    /* Load physical layouts from resources: */
    loadPhysicalLayout(":/numpad.xml", true);
    QStringList physicalLayoutNames;
    physicalLayoutNames << ":/101_ansi.xml"
                        << ":/102_iso.xml"
                        << ":/106_japanese.xml";
    foreach (const QString &strName, physicalLayoutNames)
        loadPhysicalLayout(strName);

    setNewMinimumSize(QSize(m_iInitialWidth, m_iInitialHeight));
    setInitialSize(m_iInitialWidth, m_iInitialHeight);

    /* Add keyboard layouts from resources: */
    QStringList keyboardLayoutNames;
    keyboardLayoutNames << ":/us_international.xml"
                        << ":/german.xml"
                        << ":/us.xml"
                        << ":/greek.xml";

    foreach (const QString &strName, keyboardLayoutNames)
        loadKeyboardLayout(strName);
    /* Mark the layouts we load from the resources as non-editable: */
    for (int i = 0; i < m_layouts.size(); ++i)
    {
        m_layouts[i].setEditable(false);
        m_layouts[i].setIsFromResources(true);
    }
    keyboardLayoutNames.clear();
    /* Add keyboard layouts from the defalt keyboard layout folder: */
    lookAtDefaultLayoutFolder(keyboardLayoutNames);
    foreach (const QString &strName, keyboardLayoutNames)
        loadKeyboardLayout(strName);

    if (m_layouts.isEmpty())
        return;
    setCurrentLayout(&(m_layouts[0]));
}

void UISoftKeyboardWidget::prepareObjects()
{
    m_pContextMenu = new QMenu(this);

    setMouseTracking(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &UISoftKeyboardWidget::customContextMenuRequested,
            this, &UISoftKeyboardWidget::sltContextMenuRequest);
}

void UISoftKeyboardWidget::setKeyBeingEdited(UISoftKeyboardKey* pKey)
{
    if (m_pKeyBeingEdited == pKey)
        return;
    m_pKeyBeingEdited = pKey;
    emit sigKeyToEdit(pKey);
}

void UISoftKeyboardWidget::setCurrentLayout(UISoftKeyboardLayout *pLayout)
{
    if (m_pCurrentKeyboardLayout == pLayout)
        return;
    m_pCurrentKeyboardLayout = pLayout;
    if (!m_pCurrentKeyboardLayout)
    {
        emit sigCurrentLayoutChange();
        return;
    }
    emit sigCurrentLayoutChange();

    const QMap<int, KeyCaptions> &keyCapMap = m_pCurrentKeyboardLayout->keyCapMap();

    UISoftKeyboardPhysicalLayout *pPhysicalLayout = findPhysicalLayout(m_pCurrentKeyboardLayout->physicalLayoutUuid());
    if (!pPhysicalLayout)
        return;

    /* Update the key captions: */
    QVector<UISoftKeyboardRow> &rows = pPhysicalLayout->rows();
    for (int i = 0; i < rows.size(); ++i)
    {
        QVector<UISoftKeyboardKey> &keys = rows[i].keys();
        for (int j = 0; j < keys.size(); ++j)
        {
            UISoftKeyboardKey &key = keys[j];
            if (!keyCapMap.contains(key.position()))
            {
                key.resetCaptions();
                continue;
            }
            const KeyCaptions &captions = keyCapMap.value(key.position());
            key.setBaseCaption(captions.m_strBase);
            key.setShiftCaption(captions.m_strShift);
            key.setAltGrCaption(captions.m_strAltGr);
            key.setShiftAltGrCaption(captions.m_strShiftAltGr);
        }
    }
    update();
}

UISoftKeyboardLayout *UISoftKeyboardWidget::findLayoutByName(const QString &strName)
{
    for (int i = 0; i < m_layouts.size(); ++i)
    {
        if (m_layouts[i].name() == strName)
            return &(m_layouts[i]);
    }
    return 0;
}

void UISoftKeyboardWidget::lookAtDefaultLayoutFolder(QStringList &fileList)
{
    QString strFolder = QString("%1%2%3").arg(vboxGlobal().homeFolder()).arg(QDir::separator()).arg(strSubDirectorName);
    QDir dir(strFolder);
    if (!dir.exists())
        return;
    QStringList filters;
    filters << "*.xml";
    dir.setNameFilters(filters);
    QFileInfoList fileInfoList = dir.entryInfoList();
    foreach (const QFileInfo &fileInfo, fileInfoList)
        fileList << fileInfo.absoluteFilePath();
}

QStringList UISoftKeyboardWidget::layoutNameList() const
{
    QStringList layoutNames;
    foreach (const UISoftKeyboardLayout &layout, m_layouts)
        layoutNames << layout.name();
    return layoutNames;
}

const QVector<UISoftKeyboardPhysicalLayout> &UISoftKeyboardWidget::physicalLayouts() const
{
    return m_physicalLayouts;
}

/*********************************************************************************************************************************
*   UIPhysicalLayoutReader implementation.                                                                                  *
*********************************************************************************************************************************/

bool UIPhysicalLayoutReader::parseXMLFile(const QString &strFileName, UISoftKeyboardPhysicalLayout &physicalLayout)
{
    QFile xmlFile(strFileName);
    if (!xmlFile.exists())
        return false;

    if (!xmlFile.open(QIODevice::ReadOnly))
        return false;

    m_xmlReader.setDevice(&xmlFile);

    if (!m_xmlReader.readNextStartElement() || m_xmlReader.name() != "physicallayout")
        return false;
    physicalLayout.m_strFileName = strFileName;

    QXmlStreamAttributes attributes = m_xmlReader.attributes();
    int iDefaultWidth = attributes.value("defaultWidth").toInt();
    int iDefaultHeight = attributes.value("defaultHeight").toInt();
    QVector<UISoftKeyboardRow> &rows = physicalLayout.rows();
    int iRowCount = 0;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "row")
        {
            parseRow(iDefaultWidth, iDefaultHeight, rows);
            ++iRowCount;
        }
        else if (m_xmlReader.name() == "space")
            parseRowSpace(rows);
        else if (m_xmlReader.name() == "name")
            physicalLayout.setName(m_xmlReader.readElementText());
        else if (m_xmlReader.name() == "id")
            physicalLayout.m_uId = m_xmlReader.readElementText();
        else
            m_xmlReader.skipCurrentElement();
    }

    return true;
}

void UIPhysicalLayoutReader::parseRow(int iDefaultWidth, int iDefaultHeight, QVector<UISoftKeyboardRow> &rows)
{
    rows.append(UISoftKeyboardRow());
    UISoftKeyboardRow &row = rows.back();

    row.setDefaultWidth(iDefaultWidth);
    row.setDefaultHeight(iDefaultHeight);
    row.setSpaceHeightAfter(0);

    /* Override the layout attributes if the row also has them: */
    QXmlStreamAttributes attributes = m_xmlReader.attributes();
    if (attributes.hasAttribute("defaultWidth"))
        row.setDefaultWidth(attributes.value("defaultWidth").toInt());
    if (attributes.hasAttribute("defaultHeight"))
        row.setDefaultHeight(attributes.value("defaultHeight").toInt());
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "key")
            parseKey(row);
        else if (m_xmlReader.name() == "space")
            parseKeySpace(row);
        else
            m_xmlReader.skipCurrentElement();
    }
}

void UIPhysicalLayoutReader::parseRowSpace(QVector<UISoftKeyboardRow> &rows)
{
    int iSpace = 0;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "height")
            iSpace = m_xmlReader.readElementText().toInt();
        else
            m_xmlReader.skipCurrentElement();
    }
    if (!rows.empty())
        rows.back().setSpaceHeightAfter(iSpace);
}

void UIPhysicalLayoutReader::parseKey(UISoftKeyboardRow &row)
{
    row.keys().append(UISoftKeyboardKey());
    UISoftKeyboardKey &key = row.keys().back();
    key.setWidth(row.defaultWidth());
    key.setHeight(row.defaultHeight());
    QString strKeyCap;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "width")
            key.setWidth(m_xmlReader.readElementText().toInt());
        else if (m_xmlReader.name() == "height")
            key.setHeight(m_xmlReader.readElementText().toInt());
        else if (m_xmlReader.name() == "scancode")
        {
            QString strCode = m_xmlReader.readElementText();
            bool fOk = false;
            key.setScanCode(strCode.toInt(&fOk, 16));
        }
        else if (m_xmlReader.name() == "scancodeprefix")
        {
            QString strCode = m_xmlReader.readElementText();
            bool fOk = false;
            key.setScanCodePrefix(strCode.toInt(&fOk, 16));
        }
        else if (m_xmlReader.name() == "cutout")
            parseCutout(key);
        else if (m_xmlReader.name() == "position")
            key.setPosition(m_xmlReader.readElementText().toInt());
        else if (m_xmlReader.name() == "type")
        {
            QString strType = m_xmlReader.readElementText();
            if (strType == "modifier")
                key.setType(UIKeyType_Modifier);
            else if (strType == "toggleable")
                key.setType(UIKeyType_Toggleable);
        }
        else if (m_xmlReader.name() == "osmenukey")
        {
            if (m_xmlReader.readElementText() == "true")
                key.setIsOSMenuKey(true);
        }
        else
            m_xmlReader.skipCurrentElement();
    }
}

void UIPhysicalLayoutReader::parseKeySpace(UISoftKeyboardRow &row)
{
    int iWidth = row.defaultWidth();
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "width")
            iWidth = m_xmlReader.readElementText().toInt();
        else
            m_xmlReader.skipCurrentElement();
    }
    if (row.keys().size() <= 0)
        return;
    row.keys().back().setSpaceWidthAfter(iWidth);
}

void UIPhysicalLayoutReader::parseCutout(UISoftKeyboardKey &key)
{
    int iWidth = 0;
    int iHeight = 0;
    int iCorner = 0;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "width")
            iWidth = m_xmlReader.readElementText().toInt();
        else if (m_xmlReader.name() == "height")
            iHeight = m_xmlReader.readElementText().toInt();
        else if (m_xmlReader.name() == "corner")
        {
            QString strCorner = m_xmlReader.readElementText();
            if (strCorner == "topLeft")
                    iCorner = 0;
            else if(strCorner == "topRight")
                    iCorner = 1;
            else if(strCorner == "bottomRight")
                    iCorner = 2;
            else if(strCorner == "bottomLeft")
                    iCorner = 3;
        }
        else
            m_xmlReader.skipCurrentElement();
    }
    key.setCutout(iCorner, iWidth, iHeight);
}

QVector<QPoint> UIPhysicalLayoutReader::computeKeyVertices(const UISoftKeyboardKey &key)
{
    QVector<QPoint> vertices;

    if (key.cutoutCorner() == -1 || key.width() <= key.cutoutWidth() || key.height() <= key.cutoutHeight())
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(0, key.height()));
        return vertices;
    }
    if (key.cutoutCorner() == 0)
    {
        vertices.append(QPoint(key.cutoutWidth(), 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(0, key.height()));
        vertices.append(QPoint(0, key.cutoutHeight()));
        vertices.append(QPoint(key.cutoutWidth(), key.cutoutHeight()));
    }
    else if (key.cutoutCorner() == 1)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width() - key.cutoutWidth(), 0));
        vertices.append(QPoint(key.width() - key.cutoutWidth(), key.cutoutHeight()));
        vertices.append(QPoint(key.width(), key.cutoutHeight()));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(0, key.height()));
    }
    else if (key.cutoutCorner() == 2)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.cutoutHeight()));
        vertices.append(QPoint(key.width() - key.cutoutWidth(), key.cutoutHeight()));
        vertices.append(QPoint(key.width() - key.cutoutWidth(), key.height()));
        vertices.append(QPoint(0, key.height()));
    }
    else if (key.cutoutCorner() == 3)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(key.cutoutWidth(), key.height()));
        vertices.append(QPoint(key.cutoutWidth(), key.height() - key.cutoutHeight()));
        vertices.append(QPoint(0, key.height() - key.cutoutHeight()));
    }
    return vertices;
}

/*********************************************************************************************************************************
*   UIKeyboardLayoutReader implementation.                                                                                  *
*********************************************************************************************************************************/

bool UIKeyboardLayoutReader::parseFile(const QString &strFileName)
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
        if (m_xmlReader.name() == "key")
            parseKey();
        else if (m_xmlReader.name() == "name")
            m_strName = m_xmlReader.readElementText();
        else if (m_xmlReader.name() == "physicallayoutid")
            m_physicalLayoutUid = QUuid(m_xmlReader.readElementText());
        else
            m_xmlReader.skipCurrentElement();
    }
    return true;
}

const QUuid &UIKeyboardLayoutReader::physicalLayoutUUID() const
{
    return m_physicalLayoutUid;
}

const QString &UIKeyboardLayoutReader::name() const
{
    return m_strName;
}

const QMap<int, KeyCaptions> &UIKeyboardLayoutReader::keyCapMap() const
{
    return m_keyCapMap;
}

void  UIKeyboardLayoutReader::parseKey()
{
    KeyCaptions keyCaptions;
    int iKeyPosition = 0;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "basecaption")
            keyCaptions.m_strBase = m_xmlReader.readElementText();
        else if (m_xmlReader.name() == "shiftcaption")
            keyCaptions.m_strShift = m_xmlReader.readElementText();
        else if (m_xmlReader.name() == "altgrcaption")
            keyCaptions.m_strAltGr = m_xmlReader.readElementText();
        else if (m_xmlReader.name() == "shiftaltgrcaption")
            keyCaptions.m_strShiftAltGr = m_xmlReader.readElementText();
        else if (m_xmlReader.name() == "position")
            iKeyPosition = m_xmlReader.readElementText().toInt();
        else
            m_xmlReader.skipCurrentElement();
    }
    m_keyCapMap.insert(iKeyPosition, keyCaptions);
}


/*********************************************************************************************************************************
*   UISoftKeyboardStatusBarWidget  implementation.                                                                               *
*********************************************************************************************************************************/

UISoftKeyboardStatusBarWidget::UISoftKeyboardStatusBarWidget(QWidget *pParent /* = 0*/ )
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pLayoutListButton(0)
    , m_pSettingsButton(0)
    , m_pMessageLabel(0)
{
    prepareObjects();
}

void UISoftKeyboardStatusBarWidget::retranslateUi()
{
    if (m_pLayoutListButton)
        m_pLayoutListButton->setToolTip(tr("Layout List"));
    if (m_pSettingsButton)
        m_pSettingsButton->setToolTip(tr("Settings"));
}

void UISoftKeyboardStatusBarWidget::prepareObjects()
{
    QHBoxLayout *pLayout = new QHBoxLayout;
    if (!pLayout)
        return;
    pLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(pLayout);

    m_pLayoutListButton = new QToolButton;
    if (m_pLayoutListButton)
    {
        m_pLayoutListButton->setIcon(UIIconPool::iconSet(":/file_manager_properties_24px.png"));
        m_pLayoutListButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        m_pLayoutListButton->resize(QSize(iIconMetric, iIconMetric));
        m_pLayoutListButton->setStyleSheet("QToolButton { border: 0px none black; margin: 0px 0px 0px 0px; } QToolButton::menu-indicator {image: none;}");
        connect(m_pLayoutListButton, &QToolButton::clicked, this, &UISoftKeyboardStatusBarWidget::sigShowHideSidePanel);
        pLayout->addWidget(m_pLayoutListButton);
    }

    m_pSettingsButton = new QToolButton;
    if (m_pSettingsButton)
    {
        m_pSettingsButton->setIcon(UIIconPool::iconSet(":/vm_settings_16px.png"));
        m_pSettingsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        m_pSettingsButton->resize(QSize(iIconMetric, iIconMetric));
        m_pSettingsButton->setStyleSheet("QToolButton { border: 0px none black; margin: 0px 0px 0px 0px; } QToolButton::menu-indicator {image: none;}");
        connect(m_pSettingsButton, &QToolButton::clicked, this, &UISoftKeyboardStatusBarWidget::sigShowSettingWidget);
        pLayout->addWidget(m_pSettingsButton);
    }

    m_pMessageLabel = new QLabel;
    pLayout->addWidget(m_pMessageLabel);
    retranslateUi();
}

void UISoftKeyboardStatusBarWidget::updateMessage(const QString &strMessage)
{
    if (!m_pMessageLabel)
        return;
    m_pMessageLabel->setText(strMessage);
}


/*********************************************************************************************************************************
*   UISoftKeyboardSettingsWidget implementation.                                                                                 *
*********************************************************************************************************************************/

UISoftKeyboardSettingsWidget::UISoftKeyboardSettingsWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pShowNumPadCheckBox(0)
    , m_pShowOsMenuButtonsCheckBox(0)
{
    prepareObjects();
}

void UISoftKeyboardSettingsWidget::setShowOSMenuKeys(bool fShow)
{
    if (m_pShowOsMenuButtonsCheckBox)
        m_pShowOsMenuButtonsCheckBox->setChecked(fShow);
}

void UISoftKeyboardSettingsWidget::setShowNumPad(bool fShow)
{
    if (m_pShowNumPadCheckBox)
        m_pShowNumPadCheckBox->setChecked(fShow);
}

void UISoftKeyboardSettingsWidget::retranslateUi()
{
    if (m_pShowNumPadCheckBox)
        m_pShowNumPadCheckBox->setText(UISoftKeyboard::tr("Show NumPad"));
    if (m_pShowOsMenuButtonsCheckBox)
        m_pShowOsMenuButtonsCheckBox->setText(UISoftKeyboard::tr("Show OS/Menu Keys"));

}

void UISoftKeyboardSettingsWidget::prepareObjects()
{
    QGridLayout *pSettingsLayout = new QGridLayout;
    if (!pSettingsLayout)
        return;
    m_pShowNumPadCheckBox = new QCheckBox;
    m_pShowOsMenuButtonsCheckBox = new QCheckBox;

    pSettingsLayout->addWidget(m_pShowNumPadCheckBox, 0, 0);
    pSettingsLayout->addWidget(m_pShowOsMenuButtonsCheckBox, 1, 0);

    connect(m_pShowNumPadCheckBox, &QCheckBox::toggled, this, &UISoftKeyboardSettingsWidget::sigShowNumPad);
    connect(m_pShowOsMenuButtonsCheckBox, &QCheckBox::toggled, this, &UISoftKeyboardSettingsWidget::sigShowOSMenuKeys);

    QSpacerItem *pSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (pSpacer)
        pSettingsLayout->addItem(pSpacer, 2, 0);

    setLayout(pSettingsLayout);
    retranslateUi();
}


/*********************************************************************************************************************************
*   UISoftKeyboard implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboard::UISoftKeyboard(QWidget *pParent,
                               UISession *pSession, QString strMachineName /* = QString()*/)
    :QIWithRetranslateUI<QMainWindow>(pParent)
    , m_pSession(pSession)
    , m_pMainLayout(0)
    , m_pKeyboardWidget(0)
    , m_strMachineName(strMachineName)
    , m_pSplitter(0)
    , m_pSidePanelWidget(0)
    , m_pLayoutEditor(0)
    , m_pLayoutSelector(0)
    , m_pSettingsWidget(0)
    , m_pStatusBarWidget(0)
{
    setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(tr("Soft Keyboard")));
    setAttribute(Qt::WA_DeleteOnClose);
    prepareObjects();
    prepareConnections();

    if (m_pKeyboardWidget)
    {
        m_pKeyboardWidget->loadLayouts();
        updateLayoutSelector();
        if (m_pLayoutEditor)
            m_pLayoutEditor->setPhysicalLayoutList(m_pKeyboardWidget->physicalLayouts());
    }

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

void UISoftKeyboard::sltKeyboardLedsChange()
{
    // bool fNumLockLed = m_pSession->isNumLock();
    // bool fCapsLockLed = m_pSession->isCapsLock();
    // bool fScrollLockLed = m_pSession->isScrollLock();
}

void UISoftKeyboard::sltPutKeyboardSequence(QVector<LONG> sequence)
{
    keyboard().PutScancodes(sequence);
}

void UISoftKeyboard::sltStatusBarContextMenuRequest(const QPoint &point)
{
    if (m_pKeyboardWidget)
        m_pKeyboardWidget->showContextMenu(statusBar()->mapToGlobal(point));
}

void UISoftKeyboard::sltLayoutSelectionChanged(const QString &strLayoutName)
{
    if (!m_pKeyboardWidget)
        return;
    m_pKeyboardWidget->setCurrentLayout(strLayoutName);
    if (m_pLayoutSelector && m_pKeyboardWidget->currentLayout())
        m_pLayoutSelector->setCurrentLayoutIsEditable(m_pKeyboardWidget->currentLayout()->editable());
}

void UISoftKeyboard::sltCurentLayoutChanged()
{
    if (!m_pKeyboardWidget)
        return;
    UISoftKeyboardLayout *pCurrentLayout = m_pKeyboardWidget->currentLayout();

    /* Update the status bar string: */
    QString strLayoutName = pCurrentLayout ? pCurrentLayout->name() : QString();
    updateStatusBarMessage(strLayoutName);
    updateLayoutSelector();
}

void UISoftKeyboard::sltShowLayoutSelector()
{
    if (m_pSidePanelWidget && m_pLayoutSelector)
        m_pSidePanelWidget->setCurrentWidget(m_pLayoutSelector);
    if (m_pKeyboardWidget)
        m_pKeyboardWidget->toggleEditMode(false);
    if (m_pLayoutEditor)
        m_pLayoutEditor->setKey(0);
}

void UISoftKeyboard::sltShowLayoutEditor()
{
    if (m_pSidePanelWidget && m_pLayoutEditor)
    {
        m_pLayoutEditor->setLayoutToEdit(m_pKeyboardWidget->currentLayout());
        m_pSidePanelWidget->setCurrentWidget(m_pLayoutEditor);
    }
    if (m_pKeyboardWidget)
        m_pKeyboardWidget->toggleEditMode(true);
}

void UISoftKeyboard::sltKeyToEditChanged(UISoftKeyboardKey* pKey)
{
    if (m_pLayoutEditor)
        m_pLayoutEditor->setKey(pKey);
}

void UISoftKeyboard::sltLayoutEdited()
{
    if (!m_pKeyboardWidget)
        return;
    m_pKeyboardWidget->update();
    updateLayoutSelector();
    UISoftKeyboardLayout *pCurrentLayout = m_pKeyboardWidget->currentLayout();

    /* Update the status bar string: */
    QString strLayoutName = pCurrentLayout ? pCurrentLayout->name() : QString();
    updateStatusBarMessage(strLayoutName);
}

void UISoftKeyboard::sltKeyCaptionsEdited(UISoftKeyboardKey* pKey)
{
    if (m_pKeyboardWidget)
    {
        m_pKeyboardWidget->updateKeyCaptionsInLayout(pKey);
        m_pKeyboardWidget->update();
    }
}

void UISoftKeyboard::sltShowHideSidePanel()
{
    if (!m_pSidePanelWidget)
        return;
    m_pSidePanelWidget->setVisible(!m_pSidePanelWidget->isVisible());

    if (m_pSidePanelWidget->isVisible() && m_pSettingsWidget->isVisible())
        m_pSettingsWidget->setVisible(false);
}

void UISoftKeyboard::sltShowHideSettingsWidget()
{
    if (!m_pSettingsWidget)
        return;
    m_pSettingsWidget->setVisible(!m_pSettingsWidget->isVisible());
    if (m_pSidePanelWidget->isVisible() && m_pSettingsWidget->isVisible())
        m_pSidePanelWidget->setVisible(false);
}


void UISoftKeyboard::sltCopyLayout()
{
    if (!m_pKeyboardWidget)
        return;
    m_pKeyboardWidget->copyCurentLayout();
    updateLayoutSelector();
}

void UISoftKeyboard::sltSaveLayout()
{
    if (m_pKeyboardWidget)
        m_pKeyboardWidget->saveCurentLayoutToFile();
}

void UISoftKeyboard::sltDeleteLayout()
{
    if (m_pKeyboardWidget)
        m_pKeyboardWidget->deleteCurrentLayout();
}

void UISoftKeyboard::sltStatusBarMessage(const QString &strMessage)
{
    statusBar()->showMessage(strMessage, iMessageTimeout);
}

void UISoftKeyboard::sltShowHideOSMenuKeys(bool fShow)
{
    if (m_pKeyboardWidget)
        m_pKeyboardWidget->setShowOSMenuKeys(fShow);
}

void UISoftKeyboard::sltShowHideNumPad(bool fShow)
{
    if (m_pKeyboardWidget)
        m_pKeyboardWidget->setShowNumPad(fShow);
}

void UISoftKeyboard::prepareObjects()
{
    m_pSplitter = new QSplitter;
    if (!m_pSplitter)
        return;
    setCentralWidget(m_pSplitter);
    m_pSidePanelWidget = new QStackedWidget;
    if (!m_pSidePanelWidget)
        return;

    m_pSidePanelWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    m_pSidePanelWidget->hide();

    m_pLayoutSelector = new UILayoutSelector;
    if (m_pLayoutSelector)
        m_pSidePanelWidget->addWidget(m_pLayoutSelector);

    m_pLayoutEditor = new UILayoutEditor;
    if (m_pLayoutEditor)
        m_pSidePanelWidget->addWidget(m_pLayoutEditor);

    m_pSettingsWidget = new UISoftKeyboardSettingsWidget;
    if (m_pSettingsWidget)
    {
        m_pSettingsWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        m_pSettingsWidget->hide();
    }
    m_pKeyboardWidget = new UISoftKeyboardWidget;
    if (!m_pKeyboardWidget)
        return;
    m_pKeyboardWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_pKeyboardWidget->updateGeometry();
    m_pSplitter->addWidget(m_pKeyboardWidget);
    m_pSplitter->addWidget(m_pSidePanelWidget);
    m_pSplitter->addWidget(m_pSettingsWidget);

    m_pSplitter->setCollapsible(0, false);
    m_pSplitter->setCollapsible(1, false);
    m_pSplitter->setCollapsible(2, false);

    statusBar()->setStyleSheet( "QStatusBar::item { border: 0px}" );
    m_pStatusBarWidget = new UISoftKeyboardStatusBarWidget;
    statusBar()->addPermanentWidget(m_pStatusBarWidget);

    retranslateUi();
}

void UISoftKeyboard::prepareConnections()
{
    connect(m_pSession, &UISession::sigKeyboardLedsChange, this, &UISoftKeyboard::sltKeyboardLedsChange);
    connect(m_pKeyboardWidget, &UISoftKeyboardWidget::sigPutKeyboardSequence, this, &UISoftKeyboard::sltPutKeyboardSequence);
    connect(m_pKeyboardWidget, &UISoftKeyboardWidget::sigCurrentLayoutChange, this, &UISoftKeyboard::sltCurentLayoutChanged);
    connect(m_pKeyboardWidget, &UISoftKeyboardWidget::sigKeyToEdit, this, &UISoftKeyboard::sltKeyToEditChanged);
    connect(m_pKeyboardWidget, &UISoftKeyboardWidget::sigStatusBarMessage, this, &UISoftKeyboard::sltStatusBarMessage);

    connect(m_pLayoutSelector, &UILayoutSelector::sigLayoutSelectionChanged, this, &UISoftKeyboard::sltLayoutSelectionChanged);
    connect(m_pLayoutSelector, &UILayoutSelector::sigShowLayoutEditor, this, &UISoftKeyboard::sltShowLayoutEditor);
    connect(m_pLayoutSelector, &UILayoutSelector::sigCloseLayoutList, this, &UISoftKeyboard::sltShowHideSidePanel);
    connect(m_pLayoutSelector, &UILayoutSelector::sigSaveLayout, this, &UISoftKeyboard::sltSaveLayout);
    connect(m_pLayoutSelector, &UILayoutSelector::sigDeleteLayout, this, &UISoftKeyboard::sltDeleteLayout);
    connect(m_pLayoutSelector, &UILayoutSelector::sigCopyLayout, this, &UISoftKeyboard::sltCopyLayout);
    connect(m_pLayoutEditor, &UILayoutEditor::sigGoBackButton, this, &UISoftKeyboard::sltShowLayoutSelector);
    connect(m_pLayoutEditor, &UILayoutEditor::sigLayoutEdited, this, &UISoftKeyboard::sltLayoutEdited);
    connect(m_pLayoutEditor, &UILayoutEditor::sigKeyCaptionsEdited, this, &UISoftKeyboard::sltKeyCaptionsEdited);

    connect(m_pStatusBarWidget, &UISoftKeyboardStatusBarWidget::sigShowHideSidePanel, this, &UISoftKeyboard::sltShowHideSidePanel);
    connect(m_pStatusBarWidget, &UISoftKeyboardStatusBarWidget::sigShowSettingWidget, this, &UISoftKeyboard::sltShowHideSettingsWidget);

    connect(m_pSettingsWidget, &UISoftKeyboardSettingsWidget::sigShowOSMenuKeys, this, &UISoftKeyboard::sltShowHideOSMenuKeys);
    connect(m_pSettingsWidget, &UISoftKeyboardSettingsWidget::sigShowNumPad, this, &UISoftKeyboard::sltShowHideNumPad);
}

void UISoftKeyboard::saveSettings()
{
    /* Save window geometry to extradata: */
    const QRect saveGeometry = geometry();
#ifdef VBOX_WS_MAC
    /* darwinIsWindowMaximized expects a non-const QWidget*. thus const_cast: */
    QWidget *pw = const_cast<QWidget*>(qobject_cast<const QWidget*>(this));
    gEDataManager->setSoftKeyboardDialogGeometry(saveGeometry, ::darwinIsWindowMaximized(pw));
#else /* !VBOX_WS_MAC */
    gEDataManager->setSoftKeyboardDialogGeometry(saveGeometry, isMaximized());
#endif /* !VBOX_WS_MAC */
    LogRel2(("GUI: Soft Keyboard: Geometry saved as: Origin=%dx%d, Size=%dx%d\n",
             saveGeometry.x(), saveGeometry.y(), saveGeometry.width(), saveGeometry.height()));
}

void UISoftKeyboard::loadSettings()
{
    float fKeyboardAspectRatio = 1.0f;
    if (m_pKeyboardWidget)
        fKeyboardAspectRatio = m_pKeyboardWidget->layoutAspectRatio();

    const QRect desktopRect = gpDesktop->availableGeometry(this);
    int iDefaultWidth = desktopRect.width() / 2;
    int iDefaultHeight = iDefaultWidth * fKeyboardAspectRatio;//desktopRect.height() * 3 / 4;
    QRect defaultGeometry(0, 0, iDefaultWidth, iDefaultHeight);

    QWidget *pParentWidget = qobject_cast<QWidget*>(parent());
    if (pParentWidget)
        defaultGeometry.moveCenter(pParentWidget->geometry().center());

    /* Load geometry from extradata: */
    QRect geometry = gEDataManager->softKeyboardDialogGeometry(this, defaultGeometry);

    /* Restore geometry: */
    LogRel2(("GUI: Soft Keyboard: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geometry.x(), geometry.y(), geometry.width(), geometry.height()));
    setDialogGeometry(geometry);

    if (m_pKeyboardWidget && m_pSettingsWidget)
    {
        m_pSettingsWidget->setShowOSMenuKeys(m_pKeyboardWidget->showOSMenuKeys());
        m_pSettingsWidget->setShowNumPad(m_pKeyboardWidget->showNumPad());
    }
}

void UISoftKeyboard::updateStatusBarMessage(const QString &strName)
{
    if (!m_pStatusBarWidget)
        return;
    QString strMessage;
    if (!strName.isEmpty())
    {
        strMessage += QString("%1: %2").arg(tr("Layout")).arg(strName);
        m_pStatusBarWidget->updateMessage(strMessage);
    }
    else
        m_pStatusBarWidget->updateMessage(QString());
}

void UISoftKeyboard::updateLayoutSelector()
{
    if (!m_pKeyboardWidget || !m_pLayoutSelector)
        return;
    m_pLayoutSelector->setLayoutList(m_pKeyboardWidget->layoutNameList());
    m_pLayoutSelector->setCurrentLayout(m_pKeyboardWidget->currentLayout() ? m_pKeyboardWidget->currentLayout()->name() : QString());
}

CKeyboard& UISoftKeyboard::keyboard() const
{
    return m_pSession->keyboard();
}

void UISoftKeyboard::setDialogGeometry(const QRect &geometry)
{
#ifdef VBOX_WS_MAC
    /* Use the old approach for OSX: */
    move(geometry.topLeft());
    resize(geometry.size());
#else /* VBOX_WS_MAC */
    /* Use the new approach for Windows/X11: */
    VBoxGlobal::setTopLevelGeometry(this, geometry);
#endif /* !VBOX_WS_MAC */

    /* Maximize (if necessary): */
    if (gEDataManager->softKeyboardDialogShouldBeMaximized())
        showMaximized();
}

#include "UISoftKeyboard.moc"
