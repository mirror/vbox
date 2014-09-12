/* $Id$ */
/** @file
 * VBox Qt GUI - UIMedium class implementation.
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
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

/* GUI includes: */
#include "UIMedium.h"
#include "VBoxGlobal.h"
#include "UIConverter.h"
#include "UIMessageCenter.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"

/* COM includes: */
#include "CMachine.h"
#include "CSnapshot.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

QString UIMedium::m_sstrNullID = QUuid().toString().remove('{').remove('}');
QString UIMedium::m_sstrTable = QString("<table>%1</table>");
QString UIMedium::m_sstrRow = QString("<tr><td>%1</td></tr>");

UIMedium::UIMedium()
    : m_type(UIMediumType_Invalid)
    , m_state(KMediumState_NotCreated)
{
    refresh();
//    printf("UIMedium: New NULL medium created.\n");
}

UIMedium::UIMedium(const CMedium &medium, UIMediumType type)
    : m_medium(medium)
    , m_type(type)
    , m_state(KMediumState_NotCreated)
{
    refresh();
//    printf("UIMedium: New medium with ID={%s} created.\n", id().toAscii().constData());
}

UIMedium::UIMedium(const CMedium &medium, UIMediumType type, KMediumState state)
    : m_medium(medium)
    , m_type(type)
    , m_state(state)
{
    refresh();
//    printf("UIMedium: New medium with ID={%s} created (with known state).\n", id().toAscii().constData());
}

UIMedium::UIMedium(const UIMedium &other)
{
    *this = other;
}

UIMedium& UIMedium::operator=(const UIMedium &other)
{
    m_medium = other.medium();
    m_type = other.type();
    m_state = other.state();
    m_strLastAccessError = other.lastAccessError();
    m_result = other.result();

    m_strKey = other.key();
    m_strId = other.id();
    m_strName = other.name();
    m_strLocation = other.location();

    m_strSize = other.size();
    m_strLogicalSize = other.logicalSize();

    m_strHardDiskFormat = other.hardDiskFormat();
    m_strHardDiskType = other.hardDiskType();

    m_strStorageDetails = other.storageDetails();

    m_strUsage = other.usage();
    m_strToolTip = other.tip();

    m_fHidden = other.m_fHidden;
    m_fAttachedToHiddenMachinesOnly = other.m_fAttachedToHiddenMachinesOnly;
    m_fReadOnly = other.isReadOnly();
    m_fUsedInSnapshots = other.isUsedInSnapshots();
    m_fHostDrive = other.isHostDrive();

    m_machineIds = other.machineIds();
    m_curStateMachineIds = other.curStateMachineIds();

    m_strParentID = other.parentID();
    m_strRootID = other.rootID();

    m_noDiffs = other.cache();

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
void UIMedium::blockAndQueryState()
{
    if (m_medium.isNull())
        return;

    m_state = m_medium.RefreshState();

    /* Save the result to distinguish between
     * inaccessible and e.g. uninitialized objects: */
    m_result = COMResult(m_medium);
    if (!m_result.isOk())
    {
        m_state = KMediumState_Inaccessible;
        m_strLastAccessError = QString();
    }
    else
        m_strLastAccessError = m_medium.GetLastAccessError();

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
void UIMedium::refresh()
{
    /* Flags are 'false' by default: */
    m_fHidden = false;
    m_fAttachedToHiddenMachinesOnly = false;
    m_fReadOnly = false;
    m_fUsedInSnapshots = false;
    m_fHostDrive = false;

    /* Detect basic parameters... */

    m_strId = m_medium.isNull() ? nullID() : m_medium.GetId();

    if (m_strKey.isNull() && !m_strId.isNull())
        m_strKey = m_strId;

    m_fHostDrive = m_medium.isNull() ? false : m_medium.GetHostDrive();

    if (m_medium.isNull())
        m_strName = VBoxGlobal::tr("Empty", "medium");
    else if (!m_fHostDrive)
        m_strName = m_medium.GetName();
    else if (m_medium.GetDescription().isEmpty())
        m_strName = VBoxGlobal::tr("Host Drive '%1'", "medium").arg(QDir::toNativeSeparators(m_medium.GetLocation()));
    else
        m_strName = VBoxGlobal::tr("Host Drive %1 (%2)", "medium").arg(m_medium.GetDescription(), m_medium.GetName());

    m_strLocation = m_medium.isNull() || m_fHostDrive ? QString("--") :
                    QDir::toNativeSeparators(m_medium.GetLocation());

    QString tmp;
    if (!m_medium.isNull())
        tmp = m_medium.GetProperty("Special/GUI/Hints");
    if (!tmp.isEmpty())
    {
        QStringList tmpList(tmp.split(','));
        if (tmpList.contains("Hide", Qt::CaseInsensitive))
            m_fHidden = true;
    }

    /* Initialize parent/root IDs: */
    m_strParentID = nullID();
    m_strRootID = m_strId;
    if (m_type == UIMediumType_HardDisk)
    {
        m_strHardDiskFormat = m_medium.GetFormat();
        m_strHardDiskType = vboxGlobal().mediumTypeString(m_medium);

        QVector<KMediumVariant> mediumVariants = m_medium.GetVariant();
        qlonglong mediumVariant = 0;
        for (int i = 0; i < mediumVariants.size(); ++i)
            mediumVariant |= mediumVariants[i];

        m_strStorageDetails = gpConverter->toString((KMediumVariant)mediumVariant);
        m_fReadOnly = m_medium.GetReadOnly();

        /* Adjust parent/root IDs: */
        CMedium parentMedium = m_medium.GetParent();
        if (!parentMedium.isNull())
            m_strParentID = parentMedium.GetId();
        while (!parentMedium.isNull())
        {
            m_strRootID = parentMedium.GetId();
            parentMedium = parentMedium.GetParent();
        }
    }
    else
    {
        m_strHardDiskFormat = QString();
        m_strHardDiskType = QString();
        m_fReadOnly = false;
    }

    /* Detect sizes */
    if (m_state != KMediumState_Inaccessible && m_state != KMediumState_NotCreated && !m_fHostDrive)
    {
        m_strSize = vboxGlobal().formatSize(m_medium.GetSize());
        if (m_type == UIMediumType_HardDisk)
            m_strLogicalSize = vboxGlobal().formatSize(m_medium.GetLogicalSize());
        else
            m_strLogicalSize = m_strSize;
    }
    else
    {
        m_strSize = m_strLogicalSize = QString("--");
    }

    /* Detect usage */
    m_strUsage = QString();
    if (!m_medium.isNull())
    {
        m_curStateMachineIds.clear();
        m_machineIds = m_medium.GetMachineIds().toList();
        if (m_machineIds.size() > 0)
        {
            /* We assume this flag is 'true' if at least one machine present: */
            m_fAttachedToHiddenMachinesOnly = true;

            QString strUsage;

            CVirtualBox vbox = vboxGlobal().virtualBox();

            foreach (const QString &strMachineID, m_machineIds)
            {
                CMachine machine = vbox.FindMachine(strMachineID);

                /* UIMedium object can wrap newly created CMedium object which belongs to
                 * not yet registered machine, like while creating VM clone.
                 * We can skip such a machines in usage string.
                 * CVirtualBox::FindMachine() will return null machine for such case. */
                if (machine.isNull())
                {
                    /* We can't decide for that medium yet,
                     * assume this flag is 'false' for now: */
                    m_fAttachedToHiddenMachinesOnly = false;
                    continue;
                }

                /* Finally, we are checking if current machine overrides this flag: */
                if (m_fAttachedToHiddenMachinesOnly && gEDataManager->showMachineInSelectorChooser(strMachineID))
                    m_fAttachedToHiddenMachinesOnly = false;

                QString strName = machine.GetName();
                QString strSnapshots;

                foreach (const QString &strSnapshotID, m_medium.GetSnapshotIds(strMachineID))
                {
                    if (strSnapshotID == strMachineID)
                    {
                        /* The medium is attached to the machine in the current
                         * state, we don't distinguish this for now by always
                         * giving the VM name in front of snapshot names. */
                        m_curStateMachineIds.push_back(strSnapshotID);
                        continue;
                    }

                    CSnapshot snapshot = machine.FindSnapshot(strSnapshotID);
                    if (!snapshot.isNull()) // can be NULL while takeSnaphot is in progress
                    {
                        if (!strSnapshots.isNull())
                            strSnapshots += ", ";
                        strSnapshots += snapshot.GetName();
                    }
                }

                if (!strUsage.isNull())
                    strUsage += ", ";

                strUsage += strName;

                if (!strSnapshots.isNull())
                {
                    strUsage += QString(" (%2)").arg(strSnapshots);
                    m_fUsedInSnapshots = true;
                }
                else
                    m_fUsedInSnapshots = false;
            }

            if (!strUsage.isEmpty())
                m_strUsage = strUsage;
        }
    }

    /* Compose the tooltip */
    if (!m_medium.isNull())
    {
        m_strToolTip = m_sstrRow.arg(QString("<p style=white-space:pre><b>%1</b></p>").arg(m_fHostDrive ? m_strName : m_strLocation));

        if (m_type == UIMediumType_HardDisk)
        {
            m_strToolTip += m_sstrRow.arg(VBoxGlobal::tr("<p style=white-space:pre>Type (Format):  %1 (%2)</p>", "medium")
                                                         .arg(m_strHardDiskType).arg(m_strHardDiskFormat));
        }

        m_strToolTip += m_sstrRow.arg(VBoxGlobal::tr("<p>Attached to:  %1</p>", "image")
                                                     .arg(m_strUsage.isNull() ? VBoxGlobal::tr("<i>Not Attached</i>", "image") : m_strUsage));

        switch (m_state)
        {
            case KMediumState_NotCreated:
            {
                m_strToolTip += m_sstrRow.arg(VBoxGlobal::tr("<i>Checking accessibility...</i>", "medium"));
                break;
            }
            case KMediumState_Inaccessible:
            {
                if (m_result.isOk())
                {
                    /* Not Accessible */
                    m_strToolTip += m_sstrRow.arg("<hr>") + m_sstrRow.arg(VBoxGlobal::highlight(m_strLastAccessError, true /* aToolTip */));
                }
                else
                {
                    /* Accessibility check (eg GetState()) itself failed: */
                    m_strToolTip += m_sstrRow.arg("<hr>") + m_sstrRow.arg(VBoxGlobal::tr("Failed to check accessibility of disk image files.", "medium")) +
                                    m_sstrRow.arg(UIMessageCenter::formatErrorInfo(m_result) + ".");
                }
                break;
            }
            default:
                break;
        }
    }

    /* Reset m_noDiffs */
    m_noDiffs.isSet = false;
}

void UIMedium::updateParentID()
{
    m_strParentID = nullID();
    if (m_type == UIMediumType_HardDisk)
    {
        CMedium parentMedium = m_medium.GetParent();
        if (!parentMedium.isNull())
            m_strParentID = parentMedium.GetId();
    }
}

UIMedium UIMedium::parent() const
{
    /* Redirect call to VBoxGlobal: */
    return vboxGlobal().medium(m_strParentID);
}

UIMedium UIMedium::root() const
{
    /* Redirect call to VBoxGlobal: */
    return vboxGlobal().medium(m_strRootID);
}

/**
 * Returns generated tooltip for this medium.
 *
 * In "don't show diffs" mode (where the attributes of the base hard disk are
 * shown instead of the attributes of the differencing hard disk), extra
 * information will be added to the tooltip to give the user a hint that the
 * medium is actually a differencing hard disk.
 *
 * @param fNoDiffs  @c true to enable user-friendly "don't show diffs" mode.
 * @param fCheckRO  @c true to perform the #readOnly() check and add a notice
 *                  accordingly.
 */
QString UIMedium::toolTip (bool fNoDiffs /* = false */, bool fCheckRO /* = false */, bool fNullAllowed /* = false */) const
{
    QString strTip;

    if (m_medium.isNull())
    {
        strTip = fNullAllowed ? m_sstrRow.arg(VBoxGlobal::tr("<b>No disk image file selected</b>", "medium")) +
                                m_sstrRow.arg(VBoxGlobal::tr("You can also change this while the machine is running.")) :
                                m_sstrRow.arg(VBoxGlobal::tr("<b>No disk image files available</b>", "medium")) +
                                m_sstrRow.arg(VBoxGlobal::tr("You can create or add disk image files in the virtual machine settings."));
    }
    else
    {
        unconst(this)->checkNoDiffs(fNoDiffs);

        strTip = fNoDiffs ? m_noDiffs.toolTip : m_strToolTip;

        if (fCheckRO && m_fReadOnly)
            strTip += m_sstrRow.arg("<hr>") +
                      m_sstrRow.arg(VBoxGlobal::tr("Attaching this hard disk will be performed indirectly using "
                                                   "a newly created differencing hard disk.", "medium"));
    }

    return m_sstrTable.arg(strTip);
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
 * @param fNoDiffs  @c true to enable user-friendly "don't show diffs" mode.
 * @param fCheckRO  @c true to perform the #readOnly() check and change the icon
 *                  accordingly.
 */
QPixmap UIMedium::icon(bool fNoDiffs /* = false */, bool fCheckRO /* = false */) const
{
    QPixmap pixmap;

    if (state(fNoDiffs) == KMediumState_Inaccessible)
        pixmap = result(fNoDiffs).isOk() ? vboxGlobal().warningIcon() : vboxGlobal().errorIcon();

    if (fCheckRO && m_fReadOnly)
    {
        QIcon icon = UIIconPool::iconSet(":/hd_new_16px.png");
        pixmap = VBoxGlobal::joinPixmaps(pixmap, icon.pixmap(icon.availableSizes().first()));
    }

    return pixmap;
}

/**
 * Returns the details of this medium as a single-line string
 *
 * For hard disks, the details include the location, type and the logical size
 * of the hard disk. Note that if @a fNoDiffs is @c true, these properties are
 * queried on the root hard disk of the given hard disk because the primary
 * purpose of the returned string is to be human readable (so that seeing a
 * complex diff hard disk name is usually not desirable).
 *
 * For other media types, the location and the actual size are returned.
 * Arguments @a fPredictDiff and @a aNoRoot are ignored in this case.
 *
 * @param fNoDiffs      @c true to enable user-friendly "don't show diffs" mode.
 * @param fPredictDiff  @c true to mark the hard disk as differencing if
 *                      attaching it would create a differencing hard disk (not
 *                      used when @a aNoRoot is true).
 * @param fUseHTML      @c true to allow for emphasizing using bold and italics.
 *
 * @note Use #detailsHTML() instead of passing @c true for @a fUseHTML.
 *
 * @note The media object may become uninitialized by a third party while this
 *       method is reading its properties. In this case, the method will return
 *       an empty string.
 */
QString UIMedium::details(bool fNoDiffs /* = false */,
                          bool fPredictDiff /* = false */,
                          bool fUseHTML /* = false */) const
{
    // @todo the below check is rough; if m_medium becomes uninitialized, any
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
    // @bugref{2149}.

    if (m_medium.isNull() || m_fHostDrive)
        return m_strName;

    if (!m_medium.isOk())
        return QString();

    QString strDetails, strText;

    /* Note: root accessible only if medium enumerated: */
    UIMedium rootMedium = root();
    KMediumState eState = m_state;

    if (m_type == UIMediumType_HardDisk)
    {
        if (fNoDiffs)
        {
            bool isDiff = (!fPredictDiff && parentID() != nullID()) || (fPredictDiff && m_fReadOnly);

            strDetails = isDiff && fUseHTML ?
                QString("<i>%1</i>, ").arg(rootMedium.m_strHardDiskType) :
                QString("%1, ").arg(rootMedium.m_strHardDiskType);

            eState = this->state(true /* fNoDiffs */);

            if (rootMedium.m_state == KMediumState_NotCreated)
                eState = KMediumState_NotCreated;
        }
        else
        {
            strDetails = QString("%1, ").arg(rootMedium.m_strHardDiskType);
        }
    }

    // @todo prepend the details with the warning/error icon when not accessible

    switch (eState)
    {
        case KMediumState_NotCreated:
            strText = VBoxGlobal::tr("Checking...", "medium");
            strDetails += fUseHTML ? QString("<i>%1</i>").arg(strText) : strText;
            break;
        case KMediumState_Inaccessible:
            strText = VBoxGlobal::tr("Inaccessible", "medium");
            strDetails += fUseHTML ? QString("<b>%1</b>").arg(strText) : strText;
            break;
        default:
            strDetails += m_type == UIMediumType_HardDisk ? rootMedium.m_strLogicalSize : rootMedium.m_strSize;
            break;
    }

    strDetails = fUseHTML ?
        QString("%1 (<nobr>%2</nobr>)").arg(VBoxGlobal::locationForHTML(rootMedium.m_strName), strDetails) :
        QString("%1 (%2)").arg(VBoxGlobal::locationForHTML(rootMedium.m_strName), strDetails);

    return strDetails;
}

/* static */
QString UIMedium::nullID()
{
    return m_sstrNullID;
}

/* static */
bool UIMedium::isMediumAttachedToHiddenMachinesOnly(const UIMedium &medium)
{
    /* Iterate till the root: */
    UIMedium mediumIterator = medium;
    do
    {
        /* Ignore medium if its hidden
         * or attached to hidden machines only: */
        if (mediumIterator.isHidden())
            return true;
        /* Move iterator to parent: */
        mediumIterator = mediumIterator.parent();
    }
    while (!mediumIterator.isNull());
    /* False by default: */
    return false;
}

/**
 * Checks if m_noDiffs is filled in and does it if not.
 *
 * @param fNoDiffs  @if false, this method immediately returns.
 */
void UIMedium::checkNoDiffs(bool fNoDiffs)
{
    if (!fNoDiffs || m_noDiffs.isSet)
        return;

    m_noDiffs.toolTip = QString();

    m_noDiffs.state = m_state;
    for (UIMedium parentMedium = parent(); !parentMedium.isNull(); parentMedium = parentMedium.parent())
    {
        if (parentMedium.m_state == KMediumState_Inaccessible)
        {
            m_noDiffs.state = parentMedium.m_state;

            if (m_noDiffs.toolTip.isNull())
                m_noDiffs.toolTip = m_sstrRow.arg(VBoxGlobal::tr("Some of the files in this hard disk chain "
                                                                 "are inaccessible. Please use the Virtual Media "
                                                                 "Manager in <b>Show Differencing Hard Disks</b> "
                                                                 "mode to inspect these files.", "medium"));

            if (!parentMedium.m_result.isOk())
            {
                m_noDiffs.result = parentMedium.m_result;
                break;
            }
        }
    }

    if (parentID() != nullID() && !m_fReadOnly)
    {
        m_noDiffs.toolTip = root().tip() +
                            m_sstrRow.arg("<hr>") +
                            m_sstrRow.arg(VBoxGlobal::tr("This base hard disk is indirectly attached using "
                                                         "the following differencing hard disk:", "medium")) +
                            m_strToolTip + m_noDiffs.toolTip;
    }

    if (m_noDiffs.toolTip.isNull())
        m_noDiffs.toolTip = m_strToolTip;

    m_noDiffs.isSet = true;
}

