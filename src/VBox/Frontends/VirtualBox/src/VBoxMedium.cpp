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

#include "VBoxMedium.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

/* Qt includes */
#include <QDir>

void VBoxMedium::init()
{
    AssertReturnVoid (!mMedium.isNull());

    switch (mType)
    {
        case VBoxDefs::MediaType_HardDisk:
        {
            mHardDisk = mMedium;
            AssertReturnVoid (!mHardDisk.isNull());
            break;
        }
        case VBoxDefs::MediaType_DVD:
        {
            mDVDImage = mMedium;
            AssertReturnVoid (!mDVDImage.isNull());
            Assert (mParent == NULL);
            break;
        }
        case VBoxDefs::MediaType_Floppy:
        {
            mFloppyImage = mMedium;
            AssertReturnVoid (!mFloppyImage.isNull());
            Assert (mParent == NULL);
            break;
        }
        default:
            AssertFailed();
    }

    refresh();
}

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
    mState = mMedium.GetState();

    /* save the result to distinguish between inaccessible and e.g.
     * uninitialized objects */
    mResult = COMResult (mMedium);

    if (!mResult.isOk())
    {
        mState = KMediaState_Inaccessible;
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
 * KMediaState_NotCreated (i.e. the medium has not yet been checked for
 * accessibility).
 */
void VBoxMedium::refresh()
{
    mId = mMedium.GetId();
    mLocation = mMedium.GetLocation();
    mName = mMedium.GetName();

    if (mType == VBoxDefs::MediaType_HardDisk)
    {
        /// @todo NEWMEDIA use CSystemProperties::GetHardDIskFormats to see if the
        /// given hard disk format is a file
        mLocation = QDir::toNativeSeparators (mLocation);
        mHardDiskFormat = mHardDisk.GetFormat();
        mHardDiskType = vboxGlobal().hardDiskTypeString (mHardDisk);

        mIsReadOnly = mHardDisk.GetReadOnly();

        /* adjust the parent if necessary (note that mParent must always point
         * to an item from VBoxGlobal::currentMediaList()) */

        CHardDisk parent = mHardDisk.GetParent();
        Assert (!parent.isNull() || mParent == NULL);

        if (!parent.isNull() &&
            (mParent == NULL || mParent->mHardDisk != parent))
        {
            /* search for the parent (must be there) */
            const VBoxMediaList &list = vboxGlobal().currentMediaList();
            for (VBoxMediaList::const_iterator it = list.begin();
                 it != list.end(); ++ it)
            {
                if ((*it).mType != VBoxDefs::MediaType_HardDisk)
                    break;

                if ((*it).mHardDisk == parent)
                {
                    /* we unconst here because by the const list we don't mean
                     * const items */
                    mParent = unconst (&*it);
                    break;
                }
            }

            Assert (mParent != NULL && mParent->mHardDisk == parent);
        }
    }
    else
    {
        mLocation = QDir::toNativeSeparators (mLocation);
        mHardDiskFormat = QString::null;
        mHardDiskType = QString::null;

        mIsReadOnly = false;
    }

    if (mState != KMediaState_Inaccessible &&
        mState != KMediaState_NotCreated)
    {
        mSize = vboxGlobal().formatSize (mMedium.GetSize());
        if (mType == VBoxDefs::MediaType_HardDisk)
            mLogicalSize = vboxGlobal()
                .formatSize (mHardDisk.GetLogicalSize() * _1M);
        else
            mLogicalSize = mSize;
    }
    else
    {
        mSize = mLogicalSize = QString ("--");
    }

    /* detect usage */

    mUsage = QString::null; /* important: null means not used! */

    mCurStateMachineIds.clear();

    QVector <QString> machineIds = mMedium.GetMachineIds();
    if (machineIds.size() > 0)
    {
        QString usage;

        CVirtualBox vbox = vboxGlobal().virtualBox();

        for (QVector <QString>::ConstIterator it = machineIds.begin();
             it != machineIds.end(); ++ it)
        {
            CMachine machine = vbox.GetMachine (*it);

            QString name = machine.GetName();
            QString snapshots;

            QVector <QString> snapIds = mMedium.GetSnapshotIds (*it);
            for (QVector <QString>::ConstIterator jt = snapIds.begin();
                 jt != snapIds.end(); ++ jt)
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

    /* compose the tooltip (makes sense to keep the format in sync with
     * VBoxMediaManagerDlg::languageChangeImp() and
     * VBoxMediaManagerDlg::processCurrentChanged()) */

    mToolTip = QString ("<nobr><b>%1</b></nobr>").arg (mLocation);

    if (mType == VBoxDefs::MediaType_HardDisk)
    {
        mToolTip += VBoxGlobal::tr (
            "<br><nobr>Type&nbsp;(Format):&nbsp;&nbsp;%2&nbsp;(%3)</nobr>",
            "hard disk")
            .arg (mHardDiskType)
            .arg (mHardDiskFormat);
    }

    mToolTip += VBoxGlobal::tr (
        "<br><nobr>Attached to:&nbsp;&nbsp;%1</nobr>", "medium")
        .arg (mUsage.isNull() ?
              VBoxGlobal::tr ("<i>Not&nbsp;Attached</i>", "medium") :
              mUsage);

    switch (mState)
    {
        case KMediaState_NotCreated:
        {
            mToolTip += VBoxGlobal::tr ("<br><i>Checking accessibility...</i>",
                                        "medium");
            break;
        }
        case KMediaState_Inaccessible:
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
                    "<hr>Failed to check media accessibility.<br>%1.",
                    "medium").
                    arg (VBoxProblemReporter::formatErrorInfo (mResult));
            }
            break;
        }
        default:
            break;
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
QString VBoxMedium::toolTip (bool aNoDiffs /*= false*/,
                             bool aCheckRO /*= false*/) const
{
    unconst (this)->checkNoDiffs (aNoDiffs);

    QString tip = aNoDiffs ? mNoDiffs.toolTip : mToolTip;

    if (aCheckRO && mIsReadOnly)
        tip += VBoxGlobal::tr (
            "<hr><img src=%1/>&nbsp;Attaching this hard disk will "
            "be performed indirectly using a newly created "
            "differencing hard disk.",
            "medium").
            arg (":/new_16px.png");

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

    if (state (aNoDiffs) == KMediaState_Inaccessible)
        icon = result (aNoDiffs).isOk() ?
            vboxGlobal().warningIcon() : vboxGlobal().errorIcon();

    if (aCheckRO && mIsReadOnly)
        icon = VBoxGlobal::
            joinPixmaps (icon, QPixmap (":/new_16px.png"));

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
    if (!mMedium.isOk())
        return QString::null;

    QString details, str;

    VBoxMedium *root = unconst (this);
    KMediaState state = mState;

    if (mType == VBoxDefs::MediaType_HardDisk)
    {
        if (aNoDiffs)
        {
            root = &this->root();

            bool isDiff =
                (!aPredictDiff && mParent != NULL) ||
                (aPredictDiff && mIsReadOnly);

            details = isDiff && aUseHTML ?
                QString ("<i>%1</i>, ").arg (root->mHardDiskType) :
                QString ("%1, ").arg (root->mHardDiskType);

            /* overall (worst) state */
            state = this->state (true /* aNoDiffs */);

            /* we cannot get the logical size if the root is not checked yet */
            if (root->mState == KMediaState_NotCreated)
                state = KMediaState_NotCreated;
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
        case KMediaState_NotCreated:
            str = VBoxGlobal::tr ("Checking...", "medium");
            details += aUseHTML ? QString ("<i>%1</i>").arg (str) : str;
            break;
        case KMediaState_Inaccessible:
            str = VBoxGlobal::tr ("Inaccessible", "medium");
            details += aUseHTML ? QString ("<b>%1</b>").arg (str) : str;
            break;
        default:
            details += mType == VBoxDefs::MediaType_HardDisk ?
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
    for (VBoxMedium *cur = mParent; cur != NULL;
         cur = cur->mParent)
    {
        if (cur->mState == KMediaState_Inaccessible)
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

