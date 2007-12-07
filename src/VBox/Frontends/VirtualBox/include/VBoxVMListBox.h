/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxVMListBox, VBoxVMListBoxItem class declarations
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxVMListBox_h__
#define __VBoxVMListBox_h__

#include "COMDefs.h"

#include "VBoxSelectorWnd.h"
#include "VBoxGlobal.h"

#include <qlistbox.h>
#include <qfont.h>
#include <qdatetime.h>

struct QUuid;
class QColorGroup;

class VBoxVMListBoxTip;
class VBoxVMListBoxItem;

/**
 *
 *  The VBoxVMListBox class is a visual representation of the list of
 *  existing VMs in the VBox GUI.
 *
 *  Every item in the list box is an instance of the VBoxVMListBoxItem
 *  class.
 */
class VBoxVMListBox : public QListBox
{
    Q_OBJECT

public:

    VBoxVMListBox (QWidget *aParent = 0, const char *aName = NULL,
                   WFlags aFlags = 0);

    virtual ~VBoxVMListBox();

    QFont nameFont() const { return mNameFont; }

    QFont shotFont() const { return mShotFont; }

    QFont stateFont (CEnums::SessionState aS) const
    {
        return aS == CEnums::SessionClosed ? font() : mStateBusyFont;
    }

    int margin() const { return mMargin; }

    void refresh();
    void refresh (const QUuid &aID);

    VBoxVMListBoxItem *item (const QUuid &aID);

    const QColorGroup &activeColorGroup() const;

protected:

    virtual void focusInEvent (QFocusEvent *aE);
    virtual void focusOutEvent (QFocusEvent *aE);

private:

    CVirtualBox mVBox;
    QFont mNameFont;
    QFont mShotFont;
    QFont mStateBusyFont;
    int mMargin;

    VBoxVMListBoxTip *mToolTip;
    bool mGaveFocusToPopup;
};

/**
 *
 *  The VBoxVMListBoxItem class is a visual representation of the virtual
 *  machine in the VBoxVMListBox widget.
 *
 *  It holds a CMachine instance (passed to the constructor) to
 *  get an access to various VM data.
 */
class VBoxVMListBoxItem : public QListBoxItem
{
public:

    VBoxVMListBoxItem (VBoxVMListBox *aLB, const CMachine &aM);
    virtual ~VBoxVMListBoxItem();

    QString text() const { return mName; }

    VBoxVMListBox *vmListBox() const
    {
        return static_cast <VBoxVMListBox *> (listBox());
    }

    CMachine machine() const { return mMachine; }
    void setMachine (const CMachine &aM);

    void recache();

    QString toolTipText() const;

    int height (const QListBox *) const;
    int width (const QListBox *) const;

    QUuid id() const { return mId; }
    bool accessible() const { return mAccessible; }
    const CVirtualBoxErrorInfo &accessError() const { return mAccessError; }
    QString name() const { return mName; }
    CEnums::MachineState state() const { return mState; }
    CEnums::SessionState sessionState() const { return mSessionState; }
    ULONG snapshotCount() const { return mSnapshotCount; }

    /// @todo see comments in #switchTo() in VBoxVMListBox.cpp
#if 0
    bool canSwitchTo() const { return mWinId != (WId) ~0; }
#endif
    bool canSwitchTo() const;
    bool switchTo();

protected:

    void paint (QPainter *aP);

private:

    CMachine mMachine;

    /* cached machine data (to minimize server requests) */

    QUuid mId;
    QString mSettingsFile;

    bool mAccessible;
    CVirtualBoxErrorInfo mAccessError;

    QString mName;
    QString mSnapshotName;
    CEnums::MachineState mState;
    QDateTime mLastStateChange;
    CEnums::SessionState mSessionState;
    QString mOSTypeId;
    ULONG mSnapshotCount;

    ULONG mPid;
    /// @todo see comments in #switchTo() in VBoxVMListBox.cpp
#if 0
    WId mWinId;
#endif
};

#endif // __VBoxVMListItem_h__
