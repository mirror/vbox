/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxVMListItem class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "VBoxVMListBox.h"

#include "VBoxProblemReporter.h"

#include <qpainter.h>
#include <qfontmetrics.h>
#include <qdrawutil.h>
#include <qtooltip.h>
#include <qmetaobject.h>
#include <qstyle.h>

#include <qfileinfo.h>


////////////////////////////////////////////////////////////////////////////////
// VBoxVMListBoxItem class
////////////////////////////////////////////////////////////////////////////////

class VBoxVMListBoxTip : public QToolTip
{
public:

    VBoxVMListBoxTip (VBoxVMListBox *aLB, QToolTipGroup *aTG = 0)
        : QToolTip (aLB, aTG)
    {}

    virtual ~VBoxVMListBoxTip()
    {
        remove (parentWidget());
    }

    void maybeTip (const QPoint &aPnt);
};

void VBoxVMListBoxTip::maybeTip (const QPoint &aPnt)
{
    const VBoxVMListBox *lb = static_cast <VBoxVMListBox *> (parentWidget());

    VBoxVMListBoxItem *vmi = static_cast <VBoxVMListBoxItem *> (lb->itemAt (aPnt));
    if (!vmi)
        return;

    if (parentWidget()->topLevelWidget()->inherits ("QMainWindow"))
    {
        /*
         *  Ensure the main window doesn't show the text from the previous
         *  tooltip in the status bar.
         */
        QToolTipGroup *toolTipGroup =
            (::qt_cast <QMainWindow *> (parentWidget()->topLevelWidget()))->
                toolTipGroup();
        if (toolTipGroup)
        {
            int index = toolTipGroup->metaObject()->findSignal("removeTip()", false);
            toolTipGroup->qt_emit (index, 0);
        }
    }

    tip (lb->itemRect (vmi), vmi->toolTipText());
}

////////////////////////////////////////////////////////////////////////////////
// VBoxVMListBox class
////////////////////////////////////////////////////////////////////////////////

/**
 *  Creates a box with the list of all virtual machines in the
 *  VirtualBox installation.
 */
VBoxVMListBox::
VBoxVMListBox (QWidget *aParent /* = 0 */, const char *aName /* = NULL */,
               WFlags aFlags /* = 0 */)
    : QListBox (aParent, aName, aFlags)
{
    mVBox = vboxGlobal().virtualBox();

    mNameFont = QFont (font().family(), font().pointSize() + 1, QFont::Bold);
    mShotFont = QFont (font().family(), font().pointSize() + 1);
    mStateBusyFont = font();
    mStateBusyFont.setItalic (true);

    mMargin = QMAX (fontMetrics().width (' ') * 2, 8);

    mToolTip = new VBoxVMListBoxTip (this);

    mGaveFocusToPopup = false;

    refresh();
}

VBoxVMListBox::~VBoxVMListBox()
{
    delete mToolTip;
}

// public members
////////////////////////////////////////////////////////////////////////////////

/**
 *  Refreshes the box contents by rereading the list of VM's using
 *  the IVirtualBox instance.
 */
void VBoxVMListBox::refresh()
{
    clear();

    CMachineEnumerator en = mVBox.GetMachines().Enumerate();
    while (en.HasMore())
    {
        CMachine m = en.GetNext();
        new VBoxVMListBoxItem (this, m);
    }

    sort();
    setCurrentItem (0);
}

/**
 *  Refreshes the item corresponding to the given UUID.
 */
void VBoxVMListBox::refresh (const QUuid &aID)
{
    for (uint i = 0; i < count(); i++)
    {
        VBoxVMListBoxItem *vmi = (VBoxVMListBoxItem *) QListBox::item (i);
        if (vmi->id() == aID)
        {
            vmi->recache();
            /* we call triggerUpdate() instead of updateItem() to cause the
             * layout to be redone for the case when the item width changes */
            triggerUpdate (true);
            break;
        }
    }
}

/**
 *  Returns the list item with the given UUID.
 */
VBoxVMListBoxItem *VBoxVMListBox::item (const QUuid &aID)
{
    for (uint i = 0; i < count(); i++)
    {
        VBoxVMListBoxItem *vmi = (VBoxVMListBoxItem *) QListBox::item (i);
        if (vmi->id() == aID)
            return vmi;
    }
    return 0;
}

/**
 *  Returns the color group QListBox uses to prepare a place for painting
 *  a list box item (for example, to draw the highlighted rectangle of the
 *  currently selected item, etc.). Useful in item's QListBoxItem::paint()
 *  reimplementations to select correct colors that will contrast with the
 *  item rectangle.
 */
const QColorGroup &VBoxVMListBox::activeColorGroup() const
{
    /* Too bad QListBox doesn't allow to determine what color group it uses
     * to draw the item's rectangle before calling its paint() method. Not
     * knowing that, we cannot choose correct colors that will contrast with
     * that rectangle. So, we have to use the logic of QListBox itself.
     * Here is a modified extract from qlistbox.cpp: */

    bool drawActiveSelection =
        !style().styleHint (QStyle::SH_ItemView_ChangeHighlightOnFocus, this) ||
        hasFocus() ||
        mGaveFocusToPopup;

    return (drawActiveSelection ? colorGroup() : palette().inactive());
}

// protected members
////////////////////////////////////////////////////////////////////////////////

void VBoxVMListBox::focusInEvent (QFocusEvent *aE)
{
    mGaveFocusToPopup = false;
    QListBox::focusInEvent (aE);
}

void VBoxVMListBox::focusOutEvent (QFocusEvent *aE)
{
    /* A modified extract from qlistbox.cpp (see #activeColorGroup()): */
    mGaveFocusToPopup =
        QFocusEvent::reason() == QFocusEvent::Popup ||
        (qApp->focusWidget() && qApp->focusWidget()->inherits ("QMenuBar"));
    QListBox::focusOutEvent (aE);
}

// private members
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// VBoxVMListBoxItem class
////////////////////////////////////////////////////////////////////////////////

VBoxVMListBoxItem::VBoxVMListBoxItem (VBoxVMListBox *aLB, const CMachine &aM)
    : QListBoxItem (aLB), mMachine (aM)
{
    recache();
}

VBoxVMListBoxItem::~VBoxVMListBoxItem()
{
}

// public members
////////////////////////////////////////////////////////////////////////////////

void VBoxVMListBoxItem::setMachine (const CMachine &aM)
{
    Assert (!aM.isNull());

    mMachine = aM;
    recache();
    vmListBox()->triggerUpdate (true);
}

void VBoxVMListBoxItem::recache()
{
    bool needsResort = true;

    mId = mMachine.GetId();
    mSettingsFile = mMachine.GetSettingsFilePath();

    mAccessible = mMachine.GetAccessible();
    if (mAccessible)
    {
        QString name = mMachine.GetName();
        CSnapshot snp = mMachine.GetCurrentSnapshot();
        mSnapshotName = snp.isNull() ? QString::null : snp.GetName();
        needsResort = name != mName;
        mName = name;

        mState = mMachine.GetState();
        mLastStateChange.setTime_t (mMachine.GetLastStateChange() / 1000);
        mSessionState = mMachine.GetSessionState();
        mOSType = mMachine.GetOSType().GetId();
    }
    else
    {
        mAccessError = mMachine.GetAccessError();

        /* this should be in sync with
         * VBoxProblemReporter::confirmMachineDeletion() */
        QFileInfo fi (mSettingsFile);
        QString name = fi.extension().lower() == "xml" ?
                       fi.baseName (true) : fi.fileName();
        needsResort = name != mName;
        mName = name;
        mState = CEnums::InvalidMachineState;
        mSessionState = CEnums::InvalidSessionState;
        mLastStateChange = QDateTime::currentDateTime();
        mOSType = QString::null;
    }

    if (needsResort)
        vmListBox()->sort();
}

QString VBoxVMListBoxItem::toolTipText() const
{
    QString dateTime = (mLastStateChange.date() == QDate::currentDate()) ?
                        mLastStateChange.time().toString (Qt::LocalDate) :
                        mLastStateChange.toString (Qt::LocalDate);

    QString toolTip;

    if (mAccessible)
    {
        toolTip = QString ("<b>%1</b>").arg (mName);
        if (!mSnapshotName.isNull())
            toolTip += QString (" (%1)").arg (mSnapshotName);
        toolTip = QString (VBoxVMListBox::tr (
            "<nobr>%1<br></nobr>"
            "<nobr>%2 since %3</nobr><br>"
            "<nobr>Session %4</nobr>",
            "VM tooltip (name, last state change, session state)"))
            .arg (toolTip)
            .arg (vboxGlobal().toString (mState))
            .arg (dateTime)
            .arg (vboxGlobal().toString (mSessionState));
    }
    else
    {
        toolTip = QString (VBoxVMListBox::tr (
            "<nobr><b>%1</b><br></nobr>"
            "<nobr>Inaccessible since %2</nobr>",
            "Inaccessible VM tooltip (name, last state change)"))
            .arg (mSettingsFile)
            .arg (dateTime);
    }

    return toolTip;
}

int VBoxVMListBoxItem::width (const QListBox *) const
{
    /* see the picture below for dimensions */

    const VBoxVMListBox *lb = vmListBox();

    QFontMetrics fmName = QFontMetrics (lb->nameFont());
    QFontMetrics fmShot = QFontMetrics (lb->shotFont());
    QFontMetrics fmState = QFontMetrics (lb->stateFont (mSessionState));
    const int marg = lb->margin();

    QPixmap pmOSType;
    QPixmap pmState;
    QString strState;

    if (mAccessible)
    {
        pmOSType = vboxGlobal().vmGuestOSTypeIcon (mOSType);
        pmState = vboxGlobal().toIcon (mState);
        strState = vboxGlobal().toString (mState);
    }
    else
    {
        /// @todo (r=dmik) temporary
        pmOSType = QPixmap::fromMimeSource ("os_other.png");
        pmState = QPixmap::fromMimeSource ("state_aborted_16px.png");
        strState = QListBox::tr ("Inaccessible");
    }

    int nameWidth = fmName.width (mName);
    if (!mSnapshotName.isNull())
        nameWidth += fmShot.width (QString (" (%1)").arg (mSnapshotName));
    int stateWidth = pmState.width() + fmState.width (' ') +
                     fmState.width (strState);

    return marg + pmOSType.width() + marg * 3 / 2 +
           QMAX (nameWidth, stateWidth) + marg;
}

int VBoxVMListBoxItem::height (const QListBox *) const
{
    /* see the picture below for dimensions */

    const VBoxVMListBox *lb = vmListBox();

    QFontMetrics fmName = QFontMetrics (lb->nameFont());
    QFontMetrics fmState = QFontMetrics (lb->stateFont (mSessionState));
    const int marg = lb->margin();

    QPixmap pmOSType;
    QPixmap pmState;

    if (mAccessible)
    {
        pmOSType = vboxGlobal().vmGuestOSTypeIcon (mOSType);
        pmState = vboxGlobal().toIcon (mState);
    }
    else
    {
        /// @todo (r=dmik) temporary
        pmOSType = QPixmap::fromMimeSource ("os_other.png");
        pmState = QPixmap::fromMimeSource ("state_aborted_16px.png");
    }

    int strHeight = fmName.lineSpacing() +
                    QMAX (pmState.height(), fmState.height());

    return marg + QMAX (pmOSType.height(), strHeight) + marg;
}

// protected members
////////////////////////////////////////////////////////////////////////////////

/*
 +----------------------------------------------+
 |       marg                                   |
 |   +----------+   m                           |
 | m |          | m a  name_string___________ m |
 | a |  OSType  | a r                         a |
 | r |  icon    | r g  +--+                   r |
 | g |          | g /  |si|  state_string     g |
 |   +----------+   2  +--+                     |
 |       marg                                   |
 +----------------------------------------------+

 si = state icon

*/

void VBoxVMListBoxItem::paint (QPainter *aP)
{
    const VBoxVMListBox *lb = vmListBox();

    QFontMetrics fmName = QFontMetrics (lb->nameFont());
    QFontMetrics fmShot = QFontMetrics (lb->shotFont());
    QFontMetrics fmState = QFontMetrics (lb->stateFont (mSessionState));
    const int marg = lb->margin();

    QPixmap pmOSType;
    QPixmap pmState;
    QString strState;

    if (mAccessible)
    {
        pmOSType = vboxGlobal().vmGuestOSTypeIcon (mOSType);
        pmState = vboxGlobal().toIcon (mState);
        strState = vboxGlobal().toString (mState);
    }
    else
    {
        /// @todo (r=dmik) temporary
        pmOSType = QPixmap::fromMimeSource ("os_other.png");
        pmState = QPixmap::fromMimeSource ("state_aborted_16px.png");
        strState = QListBox::tr("Inaccessible");
    }

    const QColorGroup &cg = lb->activeColorGroup();

    int osTypeY = marg;
    int strHeight = fmName.lineSpacing() +
                    QMAX (pmState.height(), fmState.height());
    if (strHeight > pmOSType.height())
        osTypeY += (strHeight - pmOSType.height()) / 2;

    /* draw the OS type icon with border, vertically centered */
    aP->drawPixmap (marg, osTypeY, pmOSType);
    qDrawShadePanel (aP, marg, osTypeY, pmOSType.width(), pmOSType.height(),
                     cg, false, 1);

    aP->setPen (isSelected() ? cg.highlightedText() : cg.text());

    int textX = marg + pmOSType.width() + marg * 3 / 2;

    /* draw the VM name */
    aP->setFont (lb->nameFont());
    aP->drawText (textX, marg + fmName.ascent(), mName);

    if (!mSnapshotName.isNull())
    {
        int nameWidth = fmName.width (mName);
        aP->setFont (lb->shotFont());
        QString shotName = QString (" (%1)").arg (mSnapshotName);
        aP->drawText (textX + nameWidth, marg + fmName.ascent(), shotName);
    }

    int stateY = marg + fmName.lineSpacing();
    int stateH = QMAX (pmState.height(), fmState.height());

    /* draw the VM state icon */
    aP->drawPixmap (textX, stateY + (stateH - pmState.height()) / 2,
                    pmState);

    /* draw the VM state text */
    aP->setFont (lb->stateFont (mSessionState));
    aP->drawText (textX + pmState.width() + fmState.width(' '),
                  stateY + (stateH - fmState.height()) / 2 + fmState.ascent(),
                  strState);
}
