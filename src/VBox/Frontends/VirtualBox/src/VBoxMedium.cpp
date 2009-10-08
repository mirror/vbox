/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMedium class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/* Global includes */
#include <QDir>

/* Local includes */
#include "VBoxMedium.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

/**
 * Queries the medium state. Call this and then read the state field instad
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

    mState = mMedium.GetState();

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
        mName = VBoxGlobal::tr ("Host Drive %1 (%2)", "medium").arg (mMedium.GetDescription(), QDir::toNativeSeparators (mMedium.GetLocation()));

    mLocation = mMedium.isNull() || mIsHostDrive ? QString ("--") :
                QDir::toNativeSeparators (mMedium.GetLocation());

    if (mType == VBoxDefs::MediumType_HardDisk)
    {
        mHardDiskFormat = mMedium.GetFormat();
        mHardDiskType = vboxGlobal().mediumTypeString (mMedium);
        mIsReadOnly = mMedium.GetReadOnly();

        /* Adjust the parent if necessary (note that mParent must always point
         * to an item from VBoxGlobal::currentMediaList()) */
        CMedium parent = mMedium.GetParent();
        Assert (!parent.isNull() || mParent == NULL);

        if (!parent.isNull() && (mParent == NULL || mParent->mMedium != parent))
        {
            /* Search for the parent (must be there) */
            const VBoxMediaList &list = vboxGlobal().currentMediaList();
            for (VBoxMediaList::const_iterator it = list.begin(); it != list.end(); ++ it)
            {
                if ((*it).mType != VBoxDefs::MediumType_HardDisk)
                    break;

                if ((*it).mMedium == parent)
                {
                    mParent = unconst (&*it);
                    break;
                }
            }

            Assert (mParent != NULL && mParent->mMedium == parent);
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
            mLogicalSize = vboxGlobal().formatSize (mMedium.GetLogicalSize() * _1M);
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
            QString usage;

            CVirtualBox vbox = vboxGlobal().virtualBox();

            for (QVector <QString>::ConstIterator it = machineIds.begin(); it != machineIds.end(); ++ it)
            {
                CMachine machine = vbox.GetMachine (*it);

                QString name = machine.GetName();
                QString snapshots;

                QVector <QString> snapIds = mMedium.GetSnapshotIds (*it);
                for (QVector <QString>::ConstIterator jt = snapIds.begin(); jt != snapIds.end(); ++ jt)
                {
                    if (*jt == *it)
                    {
                        /* the medium is attached to the machine in the current
                         * state, we don't distinguish this for now by always
                         * giving the VM name in front of snapshot names. */

                        mCurStateMachineIds.push_back (*jt);
                        continue;
                    }

                    CSnapshot snapshot = machine.GetSnapshot (*jt);
                    if (!snapshots.isNull())
                        snapshots += ", ";
                    snapshots += snapshot.GetName();
                }

                if (!usage.isNull())
                    usage += ", ";

                usage += name;

                if (!snapshots.isNull())
                {
                    usage += QString (" (%2)").arg (snapshots);
                    mIsUsedInSnapshots = true;
                }
                else
                    mIsUsedInSnapshots = false;
            }

            Assert (!usage.isEmpty());
            mUsage = usage;
        }
    }

    /* Compose the tooltip */
    if (!mMedium.isNull())
    {
        mToolTip = QString ("<nobr><b>%1</b></nobr>").arg (mIsHostDrive ? mName : mLocation);

        if (mType == VBoxDefs::MediumType_HardDisk)
        {
            mToolTip += VBoxGlobal::tr (
                "<br><nobr>Type&nbsp;(Format):&nbsp;&nbsp;%2&nbsp;(%3)</nobr>", "hard disk")
                .arg (mHardDiskType).arg (mHardDiskFormat);
        }

        mToolTip += VBoxGlobal::tr (
            "<br><nobr>Attached&nbsp;to:&nbsp;&nbsp;%1</nobr>", "medium")
            .arg (mUsage.isNull() ? VBoxGlobal::tr ("<i>Not&nbsp;Attached</i>", "medium") : mUsage);

        switch (mState)
        {
            case KMediumState_NotCreated:
            {
                mToolTip += VBoxGlobal::tr ("<br><i>Checking accessibility...</i>", "medium");
                break;
            }
            case KMediumState_Inaccessible:
            {
                if (mResult.isOk())
                {
                    /* not accessibile */
                    mToolTip += QString ("<hr>%1").
                        arg (VBoxGlobal::highlight (mLastAccessError,
                                                    true /* aToolTip */));
                }
                else
                {
                    /* accessibility check (eg GetState()) itself failed */
                    mToolTip = VBoxGlobal::tr (
                        "<hr>Failed to check media accessibility.<br>%1.", "medium").
                        arg (VBoxProblemReporter::formatErrorInfo (mResult));
                }
                break;
            }
            default:
                break;
        }
    }

    /* reset mNoDiffs */
    mNoDiffs.isSet = false;
}

/**
 * Returns a root medium of this medium. For non-hard disk media, this is always
 * this medium itself.
 */
VBoxMedium &VBoxMedium::root() const
{
    VBoxMedium *root = unconst (this);
    while (root->mParent != NULL)
        root = root->mParent;

    return *root;
}

/**
 * Returns a tooltip for this medium.
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
QString VBoxMedium::toolTip (bool aNoDiffs /*= false*/, bool aCheckRO /*= false*/, bool aNullAllowed /*= false*/) const
{
    QString tip;

    if (mMedium.isNull())
    {
        tip = aNullAllowed ? VBoxGlobal::tr ("<nobr><b>Not&nbsp;Set</b></nobr><br>"
                                             "Required virtual image or host-drive could be mounted at runtime.") :
                             VBoxGlobal::tr ("<nobr><b>Not&nbsp;Available</b></nobr><br>"
                                             "Use the Virtual Media Manager to add image of the corresponding type.");
    }
    else
    {
        unconst (this)->checkNoDiffs (aNoDiffs);

        tip = aNoDiffs ? mNoDiffs.toolTip : mToolTip;

        if (aCheckRO && mIsReadOnly)
            tip += VBoxGlobal::tr ("<hr><img src=%1/>&nbsp;Attaching this hard disk will "
                                   "be performed indirectly using a newly created "
                                   "differencing hard disk.", "medium").arg (":/new_16px.png");
    }

    return tip;
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
QPixmap VBoxMedium::icon (bool aNoDiffs /*= false*/,
                          bool aCheckRO /*= false*/) const
{
    QPixmap icon;

    if (state (aNoDiffs) == KMediumState_Inaccessible)
        icon = result (aNoDiffs).isOk() ? vboxGlobal().warningIcon() : vboxGlobal().errorIcon();

    if (aCheckRO && mIsReadOnly)
        icon = VBoxGlobal::joinPixmaps (icon, QPixmap (":/new_16px.png"));

    return icon;
}

/**
 * Returns the details of this medium as a single-line string
 *
 * For hard disks, the details include the location, type and the logical size
 * of the hard disk. Note that if @a aNoDiffs is @c true, these properties are
 * queried on the root hard disk of the given hard disk because the primary
 * purpose of the returned string is to be human readabile (so that seeing a
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
QString VBoxMedium::details (bool aNoDiffs /*= false*/,
                             bool aPredictDiff /*= false*/,
                             bool aUseHTML /*= false */) const
{
    // @todo *** the below check is rough; if mMedium becomes uninitialized, any
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

    QString details, str;

    VBoxMedium *root = unconst (this);
    KMediumState state = mState;

    if (mType == VBoxDefs::MediumType_HardDisk)
    {
        if (aNoDiffs)
        {
            root = &this->root();

            bool isDiff = (!aPredictDiff && mParent != NULL) || (aPredictDiff && mIsReadOnly);

            details = isDiff && aUseHTML ?
                QString ("<i>%1</i>, ").arg (root->mHardDiskType) :
                QString ("%1, ").arg (root->mHardDiskType);

            /* overall (worst) state */
            state = this->state (true /* aNoDiffs */);

            /* we cannot get the logical size if the root is not checked yet */
            if (root->mState == KMediumState_NotCreated)
                state = KMediumState_NotCreated;
        }
        else
        {
            details = QString ("%1, ").arg (root->mHardDiskType);
        }
    }

    /// @todo prepend the details with the warning/error
    //  icon when not accessible

    switch (state)
    {
        case KMediumState_NotCreated:
            str = VBoxGlobal::tr ("Checking...", "medium");
            details += aUseHTML ? QString ("<i>%1</i>").arg (str) : str;
            break;
        case KMediumState_Inaccessible:
            str = VBoxGlobal::tr ("Inaccessible", "medium");
            details += aUseHTML ? QString ("<b>%1</b>").arg (str) : str;
            break;
        default:
            details += mType == VBoxDefs::MediumType_HardDisk ?
                       root->mLogicalSize : root->mSize;
            break;
    }

    details = aUseHTML ?
        QString ("%1 (<nobr>%2</nobr>)").
            arg (VBoxGlobal::locationForHTML (root->mName), details) :
        QString ("%1 (%2)").
            arg (VBoxGlobal::locationForHTML (root->mName), details);

    return details;
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

    /* fill mNoDiffs */

    mNoDiffs.toolTip = QString::null;

    /* detect the overall (worst) state of the given hard disk chain */
    mNoDiffs.state = mState;
    for (VBoxMedium *cur = mParent; cur != NULL; cur = cur->mParent)
    {
        if (cur->mState == KMediumState_Inaccessible)
        {
            mNoDiffs.state = cur->mState;

            if (mNoDiffs.toolTip.isNull())
                mNoDiffs.toolTip = VBoxGlobal::tr (
                    "<hr>Some of the media in this hard disk chain are "
                    "inaccessible. Please use the Virtual Media Manager "
                    "in <b>Show Differencing Hard Disks</b> mode to inspect "
                    "these media.");

            if (!cur->mResult.isOk())
            {
                mNoDiffs.result = cur->mResult;
                break;
            }

            /* comtinue looking for another !cur->mResult.isOk() */
        }
    }

    if (mParent != NULL && !mIsReadOnly)
    {
        mNoDiffs.toolTip = VBoxGlobal::tr (
            "%1"
            "<hr>This base hard disk is indirectly attached using the "
            "following differencing hard disk:<br>"
            "%2%3").
            arg (root().toolTip(), mToolTip, mNoDiffs.toolTip);
    }

    if (mNoDiffs.toolTip.isNull())
        mNoDiffs.toolTip = mToolTip;

    mNoDiffs.isSet = true;
}

