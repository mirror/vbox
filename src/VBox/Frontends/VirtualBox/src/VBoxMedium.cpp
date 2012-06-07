/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMedium class implementation
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#include <QDir>

/* GUI includes */
#include "VBoxMedium.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "COMEnumsWrapper.h"

/* COM includes: */
#include "CSnapshot.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

QString VBoxMedium::mTable = QString ("<table>%1</table>");
QString VBoxMedium::mRow = QString ("<tr><td>%1</td></tr>");

VBoxMedium& VBoxMedium::operator= (const VBoxMedium &aOther)
{
    mMedium = aOther.medium();
    mType = aOther.type();
    mState = aOther.state();
    mLastAccessError = aOther.lastAccessError();
    mResult = aOther.result();

    mId = aOther.id();
    mName = aOther.name();
    mLocation = aOther.location();

    mSize = aOther.size();
    mLogicalSize = aOther.logicalSize();

    mHardDiskFormat = aOther.hardDiskFormat();
    mHardDiskType = aOther.hardDiskType();

    mStorageDetails = aOther.storageDetails();

    mUsage = aOther.usage();
    mToolTip = aOther.tip();

    mIsReadOnly = aOther.isReadOnly();
    mIsUsedInSnapshots = aOther.isUsedInSnapshots();
    mIsHostDrive = aOther.isHostDrive();

    mCurStateMachineIds = aOther.curStateMachineIds();

    mParent = aOther.parent();

    mNoDiffs = aOther.cache();

    return *this;
}

/**
 * Queries the medium state. Call this and then read the state field instead
 * of calling GetState() on medium directly as it will properly handle the
 * situation when GetState() itself fails by setting state to Inaccessible
 * and memorizing the error info describing why GetState() failed.
 *
 * As the last step, this method calls #refresh() to refresh all precomposed
 * strings.
 *
 * @note This method blocks for the duration of the state check. Since this
 *       check may take quite a while (e.g. for a medium located on a
 *       network share), the calling thread must not be the UI thread. You
 *       have been warned.
 */
void VBoxMedium::blockAndQueryState()
{
    if (mMedium.isNull()) return;

    mState = mMedium.RefreshState();

    /* Save the result to distinguish between inaccessible and e.g. uninitialized objects */
    mResult = COMResult (mMedium);

    if (!mResult.isOk())
    {
        mState = KMediumState_Inaccessible;
        mLastAccessError = QString::null;
    }
    else
        mLastAccessError = mMedium.GetLastAccessError();

    refresh();
}

/**
 * Refreshes the precomposed strings containing such media parameters as
 * location, size by querying the respective data from the associated
 * media object.
 *
 * Note that some string such as #size() are meaningless if the media state is
 * KMediumState_NotCreated (i.e. the medium has not yet been checked for
 * accessibility).
 */
void VBoxMedium::refresh()
{
    /* Detect basic parameters */
    mId = mMedium.isNull() ? QUuid().toString().remove ('{').remove ('}') : mMedium.GetId();

    mIsHostDrive = mMedium.isNull() ? false : mMedium.GetHostDrive();

    if (mMedium.isNull())
        mName = VBoxGlobal::tr ("Empty", "medium");
    else if (!mIsHostDrive)
        mName = mMedium.GetName();
    else if (mMedium.GetDescription().isEmpty())
        mName = VBoxGlobal::tr ("Host Drive '%1'", "medium").arg (QDir::toNativeSeparators (mMedium.GetLocation()));
    else
        mName = VBoxGlobal::tr ("Host Drive %1 (%2)", "medium").arg (mMedium.GetDescription(), mMedium.GetName());

    mLocation = mMedium.isNull() || mIsHostDrive ? QString ("--") :
                QDir::toNativeSeparators (mMedium.GetLocation());

    if (mType == VBoxDefs::MediumType_HardDisk)
    {
        mHardDiskFormat = mMedium.GetFormat();
        mHardDiskType = vboxGlobal().mediumTypeString (mMedium);
        mStorageDetails = gCOMenum->toString((KMediumVariant)mMedium.GetVariant());
        mIsReadOnly = mMedium.GetReadOnly();

        /* Adjust the parent if its possible */
        CMedium parentMedium = mMedium.GetParent();
        Assert (!parentMedium.isNull() || mParent == NULL);

        if (!parentMedium.isNull() && (mParent == NULL || mParent->mMedium != parentMedium))
        {
            /* Search for the parent (might be there) */
            const VBoxMediaList &list = vboxGlobal().currentMediaList();
            for (VBoxMediaList::const_iterator it = list.begin(); it != list.end(); ++ it)
            {
                if ((*it).mType != VBoxDefs::MediumType_HardDisk)
                    break;

                if ((*it).mMedium == parentMedium)
                {
                    mParent = unconst (&*it);
                    break;
                }
            }
        }
    }
    else
    {
        mHardDiskFormat = QString::null;
        mHardDiskType = QString::null;
        mIsReadOnly = false;
    }

    /* Detect sizes */
    if (mState != KMediumState_Inaccessible && mState != KMediumState_NotCreated && !mIsHostDrive)
    {
        mSize = vboxGlobal().formatSize (mMedium.GetSize());
        if (mType == VBoxDefs::MediumType_HardDisk)
            mLogicalSize = vboxGlobal().formatSize(mMedium.GetLogicalSize());
        else
            mLogicalSize = mSize;
    }
    else
    {
        mSize = mLogicalSize = QString ("--");
    }

    /* Detect usage */
    mUsage = QString::null;
    if (!mMedium.isNull())
    {
        mCurStateMachineIds.clear();
        QVector <QString> machineIds = mMedium.GetMachineIds();
        if (machineIds.size() > 0)
        {
            QString sUsage;

            CVirtualBox vbox = vboxGlobal().virtualBox();

            for (QVector <QString>::ConstIterator it = machineIds.begin(); it != machineIds.end(); ++ it)
            {
                CMachine machine = vbox.FindMachine(*it);

                QString sName = machine.GetName();
                QString sSnapshots;

                QVector <QString> snapIds = mMedium.GetSnapshotIds (*it);
                for (QVector <QString>::ConstIterator jt = snapIds.begin(); jt != snapIds.end(); ++ jt)
                {
                    if (*jt == *it)
                    {
                        /* The medium is attached to the machine in the current
                         * state, we don't distinguish this for now by always
                         * giving the VM name in front of snapshot names. */
                        mCurStateMachineIds.push_back (*jt);
                        continue;
                    }

                    CSnapshot snapshot = machine.FindSnapshot(*jt);
                    if (!snapshot.isNull())           // can be NULL while takeSnaphot is in progress
                    {
                        if (!sSnapshots.isNull())
                            sSnapshots += ", ";
                        sSnapshots += snapshot.GetName();
                    }
                }

                if (!sUsage.isNull())
                    sUsage += ", ";

                sUsage += sName;

                if (!sSnapshots.isNull())
                {
                    sUsage += QString (" (%2)").arg (sSnapshots);
                    mIsUsedInSnapshots = true;
                }
                else
                    mIsUsedInSnapshots = false;
            }

            Assert (!sUsage.isEmpty());
            mUsage = sUsage;
        }
    }

    /* Compose the tooltip */
    if (!mMedium.isNull())
    {
        mToolTip = mRow.arg (QString ("<p style=white-space:pre><b>%1</b></p>").arg (mIsHostDrive ? mName : mLocation));

        if (mType == VBoxDefs::MediumType_HardDisk)
        {
            mToolTip += mRow.arg (VBoxGlobal::tr ("<p style=white-space:pre>Type (Format):  %1 (%2)</p>", "medium")
                                                  .arg (mHardDiskType).arg (mHardDiskFormat));
        }

        mToolTip += mRow.arg (VBoxGlobal::tr ("<p>Attached to:  %1</p>", "image")
                                              .arg (mUsage.isNull() ? VBoxGlobal::tr ("<i>Not Attached</i>", "image") : mUsage));

        switch (mState)
        {
            case KMediumState_NotCreated:
            {
                mToolTip += mRow.arg (VBoxGlobal::tr ("<i>Checking accessibility...</i>", "medium"));
                break;
            }
            case KMediumState_Inaccessible:
            {
                if (mResult.isOk())
                {
                    /* Not Accessible */
                    mToolTip += mRow.arg ("<hr>") + mRow.arg (VBoxGlobal::highlight (mLastAccessError, true /* aToolTip */));
                }
                else
                {
                    /* Accessibility check (eg GetState()) itself failed */
                    mToolTip += mRow.arg ("<hr>") + mRow.arg (VBoxGlobal::tr ("Failed to check media accessibility.", "medium")) +
                                mRow.arg (UIMessageCenter::formatErrorInfo (mResult) + ".");
                }
                break;
            }
            default:
                break;
        }
    }

    /* Reset mNoDiffs */
    mNoDiffs.isSet = false;
}

/**
 * Returns a root medium of this medium. For non-hard disk media, this is always
 * this medium itself.
 */
VBoxMedium &VBoxMedium::root() const
{
    VBoxMedium *pRoot = unconst (this);
    while (pRoot->mParent != NULL)
        pRoot = pRoot->mParent;

    return *pRoot;
}

/**
 * Returns generated tooltip for this medium.
 *
 * In "don't show diffs" mode (where the attributes of the base hard disk are
 * shown instead of the attributes of the differencing hard disk), extra
 * information will be added to the tooltip to give the user a hint that the
 * medium is actually a differencing hard disk.
 *
 * @param aNoDiffs  @c true to enable user-friendly "don't show diffs" mode.
 * @param aCheckRO  @c true to perform the #readOnly() check and add a notice
 *                  accordingly.
 */
QString VBoxMedium::toolTip (bool aNoDiffs /* = false */, bool aCheckRO /* = false */, bool aNullAllowed /* = false */) const
{
    QString sTip;

    if (mMedium.isNull())
    {
        sTip = aNullAllowed ? mRow.arg (VBoxGlobal::tr ("<b>No medium selected</b>", "medium")) +
                              mRow.arg (VBoxGlobal::tr ("You can also change this while the machine is running.")) :
                              mRow.arg (VBoxGlobal::tr ("<b>No media available</b>", "medium")) +
                              mRow.arg (VBoxGlobal::tr ("You can create media images using the virtual media manager."));
    }
    else
    {
        unconst (this)->checkNoDiffs (aNoDiffs);

        sTip = aNoDiffs ? mNoDiffs.toolTip : mToolTip;

        if (aCheckRO && mIsReadOnly)
            sTip += mRow.arg ("<hr>") +
                    mRow.arg (VBoxGlobal::tr ("Attaching this hard disk will be performed indirectly using "
                                              "a newly created differencing hard disk.", "medium"));
    }

    return mTable.arg (sTip);
}

/**
 * Returns an icon corresponding to the media state. Distinguishes between
 * the Inaccessible state and the situation when querying the state itself
 * failed.
 *
 * In "don't show diffs" mode (where the attributes of the base hard disk are
 * shown instead of the attributes of the differencing hard disk), the most
 * worst media state on the given hard disk chain will be used to select the
 * media icon.
 *
 * @param aNoDiffs  @c true to enable user-friendly "don't show diffs" mode.
 * @param aCheckRO  @c true to perform the #readOnly() check and change the icon
 *                  accordingly.
 */
QPixmap VBoxMedium::icon (bool aNoDiffs /* = false */, bool aCheckRO /* = false */) const
{
    QPixmap pixmap;

    if (state (aNoDiffs) == KMediumState_Inaccessible)
        pixmap = result (aNoDiffs).isOk() ? vboxGlobal().warningIcon() : vboxGlobal().errorIcon();

    if (aCheckRO && mIsReadOnly)
        pixmap = VBoxGlobal::joinPixmaps (pixmap, QPixmap (":/new_16px.png"));

    return pixmap;
}

/**
 * Returns the details of this medium as a single-line string
 *
 * For hard disks, the details include the location, type and the logical size
 * of the hard disk. Note that if @a aNoDiffs is @c true, these properties are
 * queried on the root hard disk of the given hard disk because the primary
 * purpose of the returned string is to be human readable (so that seeing a
 * complex diff hard disk name is usually not desirable).
 *
 * For other media types, the location and the actual size are returned.
 * Arguments @a aPredictDiff and @a aNoRoot are ignored in this case.
 *
 * @param aNoDiffs      @c true to enable user-friendly "don't show diffs" mode.
 * @param aPredictDiff  @c true to mark the hard disk as differencing if
 *                      attaching it would create a differencing hard disk (not
 *                      used when @a aNoRoot is true).
 * @param aUseHTML      @c true to allow for emphasizing using bold and italics.
 *
 * @note Use #detailsHTML() instead of passing @c true for @a aUseHTML.
 *
 * @note The media object may become uninitialized by a third party while this
 *       method is reading its properties. In this case, the method will return
 *       an empty string.
 */
QString VBoxMedium::details (bool aNoDiffs /* = false */,
                             bool aPredictDiff /* = false */,
                             bool aUseHTML /* = false */) const
{
    // @todo the below check is rough; if mMedium becomes uninitialized, any
    // of getters called afterwards will also fail. The same relates to the
    // root hard disk object (that will be the hard disk itself in case of
    // non-differencing disks). However, this check was added to fix a
    // particular use case: when the hard disk is a differencing hard disk and
    // it happens to be discarded (and uninitialized) after this method is
    // called but before we read all its properties (yes, it's possible!), the
    // root object will be null and calling methods on it will assert in the
    // debug builds. This check seems to be enough as a quick solution (fresh
    // hard disk attachments will be re-read by a machine state change signal
    // after the discard operation is finished, so the user will eventually see
    // correct data), but in order to solve the problem properly we need to use
    // exceptions everywhere (or check the result after every method call). See
    // also Defect #2149.

    if (mMedium.isNull() || mIsHostDrive)
        return mName;

    if (!mMedium.isOk())
        return QString::null;

    QString sDetails, sStr;

    VBoxMedium *pRoot = unconst (this);
    KMediumState eState = mState;

    if (mType == VBoxDefs::MediumType_HardDisk)
    {
        if (aNoDiffs)
        {
            pRoot = &this->root();

            bool isDiff = (!aPredictDiff && mParent != NULL) || (aPredictDiff && mIsReadOnly);

            sDetails = isDiff && aUseHTML ?
                QString ("<i>%1</i>, ").arg (pRoot->mHardDiskType) :
                QString ("%1, ").arg (pRoot->mHardDiskType);

            eState = this->state (true /* aNoDiffs */);

            if (pRoot->mState == KMediumState_NotCreated)
                eState = KMediumState_NotCreated;
        }
        else
        {
            sDetails = QString ("%1, ").arg (pRoot->mHardDiskType);
        }
    }

    // @todo prepend the details with the warning/error icon when not accessible

    switch (eState)
    {
        case KMediumState_NotCreated:
            sStr = VBoxGlobal::tr ("Checking...", "medium");
            sDetails += aUseHTML ? QString ("<i>%1</i>").arg (sStr) : sStr;
            break;
        case KMediumState_Inaccessible:
            sStr = VBoxGlobal::tr ("Inaccessible", "medium");
            sDetails += aUseHTML ? QString ("<b>%1</b>").arg (sStr) : sStr;
            break;
        default:
            sDetails += mType == VBoxDefs::MediumType_HardDisk ? pRoot->mLogicalSize : pRoot->mSize;
            break;
    }

    sDetails = aUseHTML ?
        QString ("%1 (<nobr>%2</nobr>)").arg (VBoxGlobal::locationForHTML (pRoot->mName), sDetails) :
        QString ("%1 (%2)").arg (VBoxGlobal::locationForHTML (pRoot->mName), sDetails);

    return sDetails;
}

/**
 * Checks if mNoDiffs is filled in and does it if not.
 *
 * @param aNoDiffs  @if false, this method immediately returns.
 */
void VBoxMedium::checkNoDiffs (bool aNoDiffs)
{
    if (!aNoDiffs || mNoDiffs.isSet)
        return;

    mNoDiffs.toolTip = QString::null;

    mNoDiffs.state = mState;
    for (VBoxMedium *cur = mParent; cur != NULL; cur = cur->mParent)
    {
        if (cur->mState == KMediumState_Inaccessible)
        {
            mNoDiffs.state = cur->mState;

            if (mNoDiffs.toolTip.isNull())
                mNoDiffs.toolTip = mRow.arg (VBoxGlobal::tr ("Some of the media in this hard disk chain "
                                                             "are inaccessible. Please use the Virtual Media "
                                                             "Manager in <b>Show Differencing Hard Disks</b> "
                                                             "mode to inspect these media.", "medium"));

            if (!cur->mResult.isOk())
            {
                mNoDiffs.result = cur->mResult;
                break;
            }
        }
    }

    if (mParent != NULL && !mIsReadOnly)
    {
        mNoDiffs.toolTip = root().tip() +
                           mRow.arg ("<hr>") +
                           mRow.arg (VBoxGlobal::tr ("This base hard disk is indirectly attached using "
                                                     "the following differencing hard disk:", "medium")) +
                           mToolTip + mNoDiffs.toolTip;
    }

    if (mNoDiffs.toolTip.isNull())
        mNoDiffs.toolTip = mToolTip;

    mNoDiffs.isSet = true;
}

