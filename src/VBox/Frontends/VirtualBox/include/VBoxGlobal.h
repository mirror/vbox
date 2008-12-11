/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxGlobal class declaration
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef __VBoxGlobal_h__
#define __VBoxGlobal_h__

#include "COMDefs.h"

#include "VBoxGlobalSettings.h"

#include <qapplication.h>
#include <qpixmap.h>
#include <qiconset.h>
#include <qcolor.h>
#include <quuid.h>
#include <qthread.h>
#include <qpopupmenu.h>
#include <qtooltip.h>

#include <qptrvector.h>
#include <qvaluevector.h>
#include <qvaluelist.h>
#include <qdict.h>
#include <qintdict.h>

class QAction;
class QLabel;
class QToolButton;

// Auxiliary types
////////////////////////////////////////////////////////////////////////////////

/**
 * Media descriptor for the GUI.
 *
 * Maintains the results of the last state (accessibility) check and precomposes
 * string parameters such as location, size which can be used in various GUI
 * controls.
 *
 * Many getter methods take the boolean @a aNoDiffs argument. Unless explicitly
 * stated otherwise, this argument, when set to @c true, will cause the
 * corresponding property of this object's root medium to be returned instead of
 * its own one. This is useful when hard disk media is represented in the
 * user-friendly "don't show diffs" mode. For non-hard disk media, the value of
 * this argument is irrelevant because the root object for such medium is
 * the medium itself.
 *
 * Note that this class "abuses" the KMediaState_NotCreated state value to
 * indicate that the accessibility check of the given medium (see
 * #blockAndQueryState()) has not been done yet and therefore some parameters
 * such as #size() are meaningless because they can be read only from the
 * accessible medium. The real KMediaState_NotCreated state is not necessary
 * because this class is only used with created (existing) media.
 */
class VBoxMedium
{
public:

    /**
     * Creates a null medium descriptor which is not associated with any medium.
     * The state field is set to KMediaState_NotCreated.
     */
    VBoxMedium()
        : mType (VBoxDefs::MediaType_Invalid)
        , mState (KMediaState_NotCreated)
        , mIsReadOnly (false), mIsUsedInSnapshots (false)
        , mParent (NULL) {}

    /**
     * Creates a media descriptor associated with the given medium.
     *
     * The state field remain KMediaState_NotCreated until #blockAndQueryState()
     * is called. All precomposed strings are filled up by implicitly calling
     * #refresh(), see the #refresh() details for more info.
     *
     * One of the hardDisk, dvdImage, or floppyImage members is assigned from
     * aMedium according to aType. @a aParent must be always NULL for non-hard
     * disk media.
     */
    VBoxMedium (const CMedium &aMedium, VBoxDefs::MediaType aType,
                VBoxMedium *aParent = NULL)
        : mMedium (aMedium), mType (aType)
        , mState (KMediaState_NotCreated)
        , mIsReadOnly (false), mIsUsedInSnapshots (false)
        , mParent (aParent) { init(); }

    /**
     * Similar to the other non-null constructor but sets the media state to
     * @a aState. Suitable when the media state is known such as right after
     * creation.
     */
    VBoxMedium (const CMedium &aMedium, VBoxDefs::MediaType aType,
                KMediaState aState)
        : mMedium (aMedium), mType (aType)
        , mState (aState)
        , mIsReadOnly (false), mIsUsedInSnapshots (false)
        , mParent (NULL) { init(); }

    void blockAndQueryState();
    void refresh();

    const CMedium &medium() const { return mMedium; };

    VBoxDefs::MediaType type() const { return mType; }

    /**
     * Media state. In "don't show diffs" mode, this is the worst state (in
     * terms of inaccessibility) detected on the given hard disk chain.
     *
     * @param aNoDiffs  @c true to enable user-friendly "don't show diffs" mode.
     */
    KMediaState state (bool aNoDiffs = false) const
    {
        unconst (this)->checkNoDiffs (aNoDiffs);
        return aNoDiffs ? mNoDiffs.state : mState;
    }

    QString lastAccessError() const { return mLastAccessError; }

    /**
     * Result of the last blockAndQueryState() call. Will indicate an error and
     * contain a proper error info if the last state check fails. In "don't show
     * diffs" mode, this is the worst result (in terms of inaccessibility)
     * detected on the given hard disk chain.
     *
     * @param aNoDiffs  @c true to enable user-friendly "don't show diffs" mode.
     */
    const COMResult &result (bool aNoDiffs = false) const
    {
        unconst (this)->checkNoDiffs (aNoDiffs);
        return aNoDiffs ? mNoDiffs.result : mResult;
    }

    const CHardDisk2 &hardDisk() const { return mHardDisk; }
    const CDVDImage2 &dvdImage() const { return mDVDImage; }
    const CFloppyImage2 &floppyImage() const { return mFloppyImage; }

    QUuid id() const { return mId; }

    QString location (bool aNoDiffs = false) const
        { return aNoDiffs ? root().mLocation : mLocation; }
    QString name (bool aNoDiffs = false) const
        { return aNoDiffs ? root().mName : mName; }

    QString size (bool aNoDiffs = false) const
        { return aNoDiffs ? root().mSize : mSize; }

    QString hardDiskFormat (bool aNoDiffs = false) const
        { return aNoDiffs ? root().mHardDiskFormat : mHardDiskFormat; }
    QString hardDiskType (bool aNoDiffs = false) const
        { return aNoDiffs ? root().mHardDiskType : mHardDiskType; }
    QString logicalSize (bool aNoDiffs = false) const
        { return aNoDiffs ? root().mLogicalSize : mLogicalSize; }

    QString usage (bool aNoDiffs = false) const
    { return aNoDiffs ? root().mUsage : mUsage; }

    /**
     * Returns @c true if this medium is read-only (either because it is
     * Immutable or because it has child hard disks). Read-only media can only
     * be attached indirectly.
     */
    bool isReadOnly() const { return mIsReadOnly; }

    /**
     * Returns @c true if this medium is attached to any VM (in the current
     * state or in a snapshot) in which case #usage() will contain a string with
     * comma-sparated VM names (with snapshot names, if any, in parenthesis).
     */
    bool isUsed() const { return !mUsage.isNull(); }

    /**
     * Returns @c true if this medium is attached to any VM in any snapshot.
     * which case #usage() will contain a string with comma-sparated VM names.
     */
    bool isUsedInSnapshots() const { return mIsUsedInSnapshots; }

    /**
     * Returns @c true if this medium is attached to the given machine in the
     * current state.
     */
    bool isAttachedInCurStateTo (const QUuid &aMachineId) const
        { return mCurStateMachineIds.findIndex (aMachineId) >= 0; }

    /**
     * Returns a vector of IDs of all machines this medium is attached
     * to in their current state (i.e. excluding snapshots).
     */
    const QValueList <QUuid> &curStateMachineIds() const
        { return mCurStateMachineIds; }

    /**
     * Returns a parent medium. For non-hard disk media, this is always NULL.
     */
    VBoxMedium *parent() const { return mParent; }

    VBoxMedium &root() const;

    QString toolTip(bool aNoDiffs = false, bool aCheckRO = false) const;
    QPixmap icon (bool aNoDiffs = false, bool aCheckRO = false) const;

    /** Shortcut to <tt>#toolTip (aNoDiffs, true)</tt>. */
    QString toolTipCheckRO (bool aNoDiffs = false) const
        { return toolTip (aNoDiffs, true); }

    /** Shortcut to <tt>#icon (aNoDiffs, true)</tt>. */
    QPixmap iconCheckRO (bool aNoDiffs = false) const
        { return icon (aNoDiffs, true); }

    QString details (bool aNoDiffs = false, bool aPredictDiff = false,
                     bool aUseHTML = false) const;

    /** Shortcut to <tt>#details (aNoDiffs, aPredictDiff, true)</tt>. */
    QString detailsHTML (bool aNoDiffs = false, bool aPredictDiff = false) const
        { return details (aNoDiffs, aPredictDiff, true); }

    /** Returns @c true if this media descriptor is a null object. */
    bool isNull() const { return mMedium.isNull(); }

private:

    void init();

    void checkNoDiffs (bool aNoDiffs);

    CMedium mMedium;

    VBoxDefs::MediaType mType;

    KMediaState mState;
    QString mLastAccessError;
    COMResult mResult;

    CHardDisk2 mHardDisk;
    CDVDImage2 mDVDImage;
    CFloppyImage2 mFloppyImage;

    QUuid mId;
    QString mLocation;
    QString mName;
    QString mSize;

    QString mHardDiskFormat;
    QString mHardDiskType;
    QString mLogicalSize;

    QString mUsage;
    QString mToolTip;

    bool mIsReadOnly        : 1;
    bool mIsUsedInSnapshots : 1;

    QValueList <QUuid> mCurStateMachineIds;

    VBoxMedium *mParent;

    /**
     * Used to override some attributes in the user-friendly "don't show diffs"
     * mode.
     */
    struct NoDiffs
    {
        NoDiffs() : isSet (false), state (KMediaState_NotCreated) {}

        bool isSet : 1;

        KMediaState state;
        COMResult result;
        QString toolTip;
    }
    mNoDiffs;
};

typedef QValueList <VBoxMedium> VBoxMediaList;

// VirtualBox callback events
////////////////////////////////////////////////////////////////////////////////

class VBoxMachineStateChangeEvent : public QEvent
{
public:
    VBoxMachineStateChangeEvent (const QUuid &aId, KMachineState aState)
        : QEvent ((QEvent::Type) VBoxDefs::MachineStateChangeEventType)
        , id (aId), state (aState)
        {}

    const QUuid id;
    const KMachineState state;
};

class VBoxMachineDataChangeEvent : public QEvent
{
public:
    VBoxMachineDataChangeEvent (const QUuid &aId)
        : QEvent ((QEvent::Type) VBoxDefs::MachineDataChangeEventType)
        , id (aId)
        {}

    const QUuid id;
};

class VBoxMachineRegisteredEvent : public QEvent
{
public:
    VBoxMachineRegisteredEvent (const QUuid &aId, bool aRegistered)
        : QEvent ((QEvent::Type) VBoxDefs::MachineRegisteredEventType)
        , id (aId), registered (aRegistered)
        {}

    const QUuid id;
    const bool registered;
};

class VBoxSessionStateChangeEvent : public QEvent
{
public:
    VBoxSessionStateChangeEvent (const QUuid &aId, KSessionState aState)
        : QEvent ((QEvent::Type) VBoxDefs::SessionStateChangeEventType)
        , id (aId), state (aState)
        {}

    const QUuid id;
    const KSessionState state;
};

class VBoxSnapshotEvent : public QEvent
{
public:

    enum What { Taken, Discarded, Changed };

    VBoxSnapshotEvent (const QUuid &aMachineId, const QUuid &aSnapshotId,
                       What aWhat)
        : QEvent ((QEvent::Type) VBoxDefs::SnapshotEventType)
        , what (aWhat)
        , machineId (aMachineId), snapshotId (aSnapshotId)
        {}

    const What what;

    const QUuid machineId;
    const QUuid snapshotId;
};

class VBoxCanShowRegDlgEvent : public QEvent
{
public:
    VBoxCanShowRegDlgEvent (bool aCanShow)
        : QEvent ((QEvent::Type) VBoxDefs::CanShowRegDlgEventType)
        , mCanShow (aCanShow)
        {}

    const bool mCanShow;
};

// VBoxGlobal
////////////////////////////////////////////////////////////////////////////////

class VBoxSelectorWnd;
class VBoxConsoleWnd;
class VBoxRegistrationDlg;

class VBoxGlobal : public QObject
{
    Q_OBJECT

public:

    typedef QMap <ulong, QString> QULongStringMap;
    typedef QMap <long, QString> QLongStringMap;

    static VBoxGlobal &instance();

    bool isValid() { return mValid; }

    QString versionString() { return verString; }

    CVirtualBox virtualBox() const { return mVBox; }

    const VBoxGlobalSettings &settings() const { return gset; }
    bool setSettings (const VBoxGlobalSettings &gs);

    VBoxSelectorWnd &selectorWnd();
    VBoxConsoleWnd &consoleWnd();

    bool isVMConsoleProcess() const { return !vmUuid.isNull(); }
    QUuid managedVMUuid() const { return vmUuid; }

    VBoxDefs::RenderMode vmRenderMode() const { return vm_render_mode; }
    const char *vmRenderModeStr() const { return vm_render_mode_str; }

#ifdef VBOX_WITH_DEBUGGER_GUI
    bool isDebuggerEnabled() const { return dbg_enabled; }
    bool isDebuggerVisibleAtStartup() const { return dbg_visible_at_startup; }
#endif

    /* VBox enum to/from string/icon/color convertors */

    QStringList vmGuestOSTypeDescriptions() const;
    CGuestOSType vmGuestOSType (int aIndex) const;
    int vmGuestOSTypeIndex (const QString &aId) const;
    QPixmap vmGuestOSTypeIcon (const QString &aId) const;
    QString vmGuestOSTypeDescription (const QString &aId) const;

    QPixmap toIcon (KMachineState s) const
    {
        QPixmap *pm = mStateIcons [s];
        AssertMsg (pm, ("Icon for VM state %d must be defined", s));
        return pm ? *pm : QPixmap();
    }

    const QColor &toColor (KMachineState s) const
    {
        static const QColor none;
        AssertMsg (vm_state_color.find (s), ("No color for %d", s));
        return vm_state_color.find (s) ? *vm_state_color [s] : none;
    }

    QString toString (KMachineState s) const
    {
        AssertMsg (!machineStates [s].isNull(), ("No text for %d", s));
        return machineStates [s];
    }

    QString toString (KSessionState s) const
    {
        AssertMsg (!sessionStates [s].isNull(), ("No text for %d", s));
        return sessionStates [s];
    }

    /**
     * Returns a string representation of the given KStorageBus enum value.
     * Complementary to #toStorageBusType (const QString &) const.
     */
    QString toString (KStorageBus aBus) const
    {
        AssertMsg (!storageBuses [aBus].isNull(), ("No text for %d", aBus));
        return storageBuses [aBus];
    }

    /**
     * Returns a KStorageBus enum value corresponding to the given string
     * representation. Complementary to #toString (KStorageBus) const.
     */
    KStorageBus toStorageBusType (const QString &aBus) const
    {
        QULongStringMap::const_iterator it =
            qFind (storageBuses.begin(), storageBuses.end(), aBus);
        AssertMsg (it != storageBuses.end(), ("No value for {%s}", aBus.latin1()));
        return KStorageBus (it.key());
    }

    QString toString (KStorageBus aBus, LONG aChannel) const;
    LONG toStorageChannel (KStorageBus aBus, const QString &aChannel) const;

    QString toString (KStorageBus aBus, LONG aChannel, LONG aDevice) const;
    LONG toStorageDevice (KStorageBus aBus, LONG aChannel, const QString &aDevice) const;

    QString toFullString (KStorageBus aBus, LONG aChannel, LONG aDevice) const;

    QString toString (KHardDiskType t) const
    {
        AssertMsg (!diskTypes [t].isNull(), ("No text for %d", t));
        return diskTypes [t];
    }

    /**
     * Similar to toString (KHardDiskType), but returns 'Differencing' for
     * normal hard disks that have a parent.
     */
    QString hardDiskTypeString (const CHardDisk2 &aHD) const
    {
        if (!aHD.GetParent().isNull())
        {
            Assert (aHD.GetType() == KHardDiskType_Normal);
            return diskTypes_Differencing;
        }
        return toString (aHD.GetType());
    }

    QString toString (KVRDPAuthType t) const
    {
        AssertMsg (!vrdpAuthTypes [t].isNull(), ("No text for %d", t));
        return vrdpAuthTypes [t];
    }

    QString toString (KPortMode t) const
    {
        AssertMsg (!portModeTypes [t].isNull(), ("No text for %d", t));
        return portModeTypes [t];
    }

    QString toString (KUSBDeviceFilterAction t) const
    {
        AssertMsg (!usbFilterActionTypes [t].isNull(), ("No text for %d", t));
        return usbFilterActionTypes [t];
    }

    QString toString (KClipboardMode t) const
    {
        AssertMsg (!clipboardTypes [t].isNull(), ("No text for %d", t));
        return clipboardTypes [t];
    }

    KClipboardMode toClipboardModeType (const QString &s) const
    {
        QULongStringMap::const_iterator it =
            qFind (clipboardTypes.begin(), clipboardTypes.end(), s);
        AssertMsg (it != clipboardTypes.end(), ("No value for {%s}", s.latin1()));
        return KClipboardMode (it.key());
    }

    QString toString (KIDEControllerType t) const
    {
        AssertMsg (!ideControllerTypes [t].isNull(), ("No text for %d", t));
        return ideControllerTypes [t];
    }

    KIDEControllerType toIDEControllerType (const QString &s) const
    {
        QULongStringMap::const_iterator it =
            qFind (ideControllerTypes.begin(), ideControllerTypes.end(), s);
        AssertMsg (it != ideControllerTypes.end(), ("No value for {%s}", s.latin1()));
        return KIDEControllerType (it.key());
    }

    KVRDPAuthType toVRDPAuthType (const QString &s) const
    {
        QULongStringMap::const_iterator it =
            qFind (vrdpAuthTypes.begin(), vrdpAuthTypes.end(), s);
        AssertMsg (it != vrdpAuthTypes.end(), ("No value for {%s}", s.latin1()));
        return KVRDPAuthType (it.key());
    }

    KPortMode toPortMode (const QString &s) const
    {
        QULongStringMap::const_iterator it =
            qFind (portModeTypes.begin(), portModeTypes.end(), s);
        AssertMsg (it != portModeTypes.end(), ("No value for {%s}", s.latin1()));
        return KPortMode (it.key());
    }

    KUSBDeviceFilterAction toUSBDevFilterAction (const QString &s) const
    {
        QULongStringMap::const_iterator it =
            qFind (usbFilterActionTypes.begin(), usbFilterActionTypes.end(), s);
        AssertMsg (it != usbFilterActionTypes.end(), ("No value for {%s}", s.latin1()));
        return KUSBDeviceFilterAction (it.key());
    }

    QString toString (KDeviceType t) const
    {
        AssertMsg (!deviceTypes [t].isNull(), ("No text for %d", t));
        return deviceTypes [t];
    }

    KDeviceType toDeviceType (const QString &s) const
    {
        QULongStringMap::const_iterator it =
            qFind (deviceTypes.begin(), deviceTypes.end(), s);
        AssertMsg (it != deviceTypes.end(), ("No value for {%s}", s.latin1()));
        return KDeviceType (it.key());
    }

    QStringList deviceTypeStrings() const;

    QString toString (KAudioDriverType t) const
    {
        AssertMsg (!audioDriverTypes [t].isNull(), ("No text for %d", t));
        return audioDriverTypes [t];
    }

    KAudioDriverType toAudioDriverType (const QString &s) const
    {
        QULongStringMap::const_iterator it =
            qFind (audioDriverTypes.begin(), audioDriverTypes.end(), s);
        AssertMsg (it != audioDriverTypes.end(), ("No value for {%s}", s.latin1()));
        return KAudioDriverType (it.key());
    }

    QString toString (KAudioControllerType t) const
    {
        AssertMsg (!audioControllerTypes [t].isNull(), ("No text for %d", t));
        return audioControllerTypes [t];
    }

    KAudioControllerType toAudioControllerType (const QString &s) const
    {
        QULongStringMap::const_iterator it =
            qFind (audioControllerTypes.begin(), audioControllerTypes.end(), s);
        AssertMsg (it != audioControllerTypes.end(), ("No value for {%s}", s.latin1()));
        return KAudioControllerType (it.key());
    }

    QString toString (KNetworkAdapterType t) const
    {
        AssertMsg (!networkAdapterTypes [t].isNull(), ("No text for %d", t));
        return networkAdapterTypes [t];
    }

    KNetworkAdapterType toNetworkAdapterType (const QString &s) const
    {
        QULongStringMap::const_iterator it =
            qFind (networkAdapterTypes.begin(), networkAdapterTypes.end(), s);
        AssertMsg (it != networkAdapterTypes.end(), ("No value for {%s}", s.latin1()));
        return KNetworkAdapterType (it.key());
    }

    QString toString (KNetworkAttachmentType t) const
    {
        AssertMsg (!networkAttachmentTypes [t].isNull(), ("No text for %d", t));
        return networkAttachmentTypes [t];
    }

    KNetworkAttachmentType toNetworkAttachmentType (const QString &s) const
    {
        QULongStringMap::const_iterator it =
            qFind (networkAttachmentTypes.begin(), networkAttachmentTypes.end(), s);
        AssertMsg (it != networkAttachmentTypes.end(), ("No value for {%s}", s.latin1()));
        return KNetworkAttachmentType (it.key());
    }

    QString toString (KUSBDeviceState aState) const
    {
        AssertMsg (!USBDeviceStates [aState].isNull(), ("No text for %d", aState));
        return USBDeviceStates [aState];
    }

    QStringList COMPortNames() const;
    QString toCOMPortName (ulong aIRQ, ulong aIOBase) const;
    bool toCOMPortNumbers (const QString &aName, ulong &aIRQ, ulong &aIOBase) const;

    QStringList LPTPortNames() const;
    QString toLPTPortName (ulong aIRQ, ulong aIOBase) const;
    bool toLPTPortNumbers (const QString &aName, ulong &aIRQ, ulong &aIOBase) const;

    QPixmap snapshotIcon (bool online) const
    {
        return online ? mOnlineSnapshotIcon : mOfflineSnapshotIcon;
    }

    QPixmap warningIcon() const { return mWarningIcon; }
    QPixmap errorIcon() const { return mErrorIcon; }

    /* details generators */

    QString details (const CHardDisk2 &aHD, bool aPredictDiff);

    QString details (const CUSBDevice &aDevice) const;
    QString toolTip (const CUSBDevice &aDevice) const;

    QString detailsReport (const CMachine &aMachine, bool aIsNewVM,
                           bool aWithLinks);

    /* VirtualBox helpers */

#ifdef Q_WS_X11
    bool showVirtualBoxLicense();
#endif

    bool checkForAutoConvertedSettings();

    CSession openSession (const QUuid &aId, bool aExisting = false);

    /** Shortcut to openSession (aId, true). */
    CSession openExistingSession (const QUuid &aId) { return openSession (aId, true); }

    bool startMachine (const QUuid &id);

    void startEnumeratingMedia();

    /**
     * Returns a list of all currently registered media. This list is used to
     * globally track the accessiblity state of all media on a dedicated thread.
     *
     * Note that the media list is initially empty (i.e. before the enumeration
     * process is started for the first time using #startEnumeratingMedia()).
     * See #startEnumeratingMedia() for more information about how meida are
     * sorted in the returned list.
     */
    const VBoxMediaList &currentMediaList() const { return mMediaList; }

    /** Returns true if the media enumeration is in progress. */
    bool isMediaEnumerationStarted() const { return mMediaEnumThread != NULL; }

    void addMedium (const VBoxMedium &);
    void updateMedium (const VBoxMedium &);
    void removeMedium (VBoxDefs::MediaType, const QUuid &);

    bool findMedium (const CMedium &, VBoxMedium &) const;

    /** Compact version of #findMediumTo(). Asserts if not found. */
    VBoxMedium getMedium (const CMedium &aObj) const
    {
        VBoxMedium medium;
        if (!findMedium (aObj, medium))
            AssertFailed();
        return medium;
    }

    /* various helpers */

    QString languageName() const;
    QString languageCountry() const;
    QString languageNameEnglish() const;
    QString languageCountryEnglish() const;
    QString languageTranslators() const;

    void languageChange();

    /** @internal made public for internal purposes */
    void cleanup();

    /* public static stuff */

    static bool isDOSType (const QString &aOSTypeId);

    static void adoptLabelPixmap (QLabel *);

    static QString languageId();
    static void loadLanguage (const QString &aLangId = QString::null);

    static QIconSet iconSet (const char *aNormal,
                             const char *aDisabled = 0,
                             const char *aActive = 0);
    static QIconSet iconSetEx (const char *aNormal, const char *aSmallNormal,
                               const char *aDisabled = 0, const char *aSmallDisabled = 0,
                               const char *aActive = 0, const char *aSmallActive = 0);

    static void setTextLabel (QToolButton *aToolButton, const QString &aTextLabel);

    static QRect normalizeGeometry (const QRect &aRect, const QRect &aBoundRect,
                                    bool aCanResize = true);

    static void centerWidget (QWidget *aWidget, QWidget *aRelative,
                              bool aCanResize = true);

    static QChar decimalSep();
    static QString sizeRegexp();

    static Q_UINT64 parseSize (const QString &);
    static QString formatSize (Q_UINT64, int aMode = 0);

    static QString locationForHTML (const QString &aFileName);

    static QString highlight (const QString &aStr, bool aToolTip = false);

    static QString systemLanguageId();

    static QString getExistingDirectory (const QString &aDir, QWidget *aParent,
                                         const char *aName = 0,
                                         const QString &aCaption = QString::null,
                                         bool aDirOnly = TRUE,
                                         bool resolveSymlinks = TRUE);

    static QString getOpenFileName (const QString &, const QString &, QWidget*,
                                    const char*, const QString &,
                                    QString *defaultFilter = 0,
                                    bool resolveSymLinks = true);

    static QString getFirstExistingDir (const QString &);

    static bool activateWindow (WId aWId, bool aSwitchDesktop = true);

    static QString removeAccelMark (const QString &aText);

    static QPixmap joinPixmaps (const QPixmap &aPM1, const QPixmap &aPM2);

    static QWidget *findWidget (QWidget *aParent, const char *aName,
                                const char *aClassName = NULL,
                                bool aRecursive = false);

signals:

    /**
     * Emitted at the beginning of the enumeration process started by
     * #startEnumeratingMedia().
     */
    void mediaEnumStarted();

    /**
     * Emitted when a new medium item from the list has updated its
     * accessibility state.
     */
    void mediumEnumerated (const VBoxMedium &aMedum, int aIndex);

    /**
     * Emitted at the end of the enumeration process started by
     * #startEnumeratingMedia(). The @a aList argument is passed for
     * convenience, it is exactly the same as returned by #currentMediaList().
     */
    void mediaEnumFinished (const VBoxMediaList &aList);

    /** Emitted when a new media is added using #addMedia(). */
    void mediumAdded (const VBoxMedium &);

    /** Emitted when the media is updated using #updateMedia(). */
    void mediumUpdated (const VBoxMedium &);

    /** Emitted when the media is removed using #removeMedia(). */
    void mediumRemoved (VBoxDefs::MediaType, const QUuid &);

    /* signals emitted when the VirtualBox callback is called by the server
     * (not that currently these signals are emitted only when the application
     * is the in the VM selector mode) */

    void machineStateChanged (const VBoxMachineStateChangeEvent &e);
    void machineDataChanged (const VBoxMachineDataChangeEvent &e);
    void machineRegistered (const VBoxMachineRegisteredEvent &e);
    void sessionStateChanged (const VBoxSessionStateChangeEvent &e);
    void snapshotChanged (const VBoxSnapshotEvent &e);

    void canShowRegDlg (bool aCanShow);

public slots:

    bool openURL (const QString &aURL);

    void showRegistrationDialog (bool aForce = true);

protected:

    bool event (QEvent *e);
    bool eventFilter (QObject *, QEvent *);

private:

    VBoxGlobal();
    ~VBoxGlobal() {}

    void init();

    bool mValid;

    CVirtualBox mVBox;

    VBoxGlobalSettings gset;

    VBoxSelectorWnd *mSelectorWnd;
    VBoxConsoleWnd *mConsoleWnd;

#ifdef VBOX_WITH_REGISTRATION
    VBoxRegistrationDlg *mRegDlg;
#endif

    QUuid vmUuid;

    QThread *mMediaEnumThread;
    VBoxMediaList mMediaList;

    VBoxDefs::RenderMode vm_render_mode;
    const char * vm_render_mode_str;

#ifdef VBOX_WITH_DEBUGGER_GUI
    bool dbg_enabled;
    bool dbg_visible_at_startup;
#endif

#if defined (Q_WS_WIN32)
    DWORD dwHTMLHelpCookie;
#endif

    CVirtualBoxCallback callback;

    QString verString;

    QValueVector <CGuestOSType> vm_os_types;
    QDict <QPixmap> vm_os_type_icons;
    QIntDict <QColor> vm_state_color;

    QIntDict <QPixmap> mStateIcons;
    QPixmap mOfflineSnapshotIcon, mOnlineSnapshotIcon;

    QULongStringMap machineStates;
    QULongStringMap sessionStates;
    QULongStringMap deviceTypes;

    QULongStringMap storageBuses;
    QLongStringMap storageBusDevices;
    QLongStringMap storageBusChannels;

    QULongStringMap diskTypes;
    QString diskTypes_Differencing;

    QULongStringMap vrdpAuthTypes;
    QULongStringMap portModeTypes;
    QULongStringMap usbFilterActionTypes;
    QULongStringMap audioDriverTypes;
    QULongStringMap audioControllerTypes;
    QULongStringMap networkAdapterTypes;
    QULongStringMap networkAttachmentTypes;
    QULongStringMap clipboardTypes;
    QULongStringMap ideControllerTypes;
    QULongStringMap USBDeviceStates;

    QString mUserDefinedPortName;

    QPixmap mWarningIcon, mErrorIcon;

    mutable bool detailReportTemplatesReady;

    friend VBoxGlobal &vboxGlobal();
    friend class VBoxCallback;
};

inline VBoxGlobal &vboxGlobal() { return VBoxGlobal::instance(); }

// Helper classes
////////////////////////////////////////////////////////////////////////////////

/**
 *  Generic asyncronous event.
 *
 *  This abstract class is intended to provide a conveinent way to execute
 *  code on the main GUI thread asynchronously to the calling party. This is
 *  done by putting necessary actions to the #handle() function in a subclass
 *  and then posting an instance of the subclass using #post(). The instance
 *  must be allocated on the heap using the <tt>new</tt> operation and will be
 *  automatically deleted after processing. Note that if you don't call #post()
 *  on the created instance, you have to delete it yourself.
 */
class VBoxAsyncEvent : public QEvent
{
public:

    VBoxAsyncEvent() : QEvent ((QEvent::Type) VBoxDefs::AsyncEventType) {}

    /**
     *  Worker function. Gets executed on the GUI thread when the posted event
     *  is processed by the main event loop.
     */
    virtual void handle() = 0;

    /**
     *  Posts this event to the main event loop.
     *  The caller loses ownership of this object after this method returns
     *  and must not delete the object.
     */
    void post()
    {
        QApplication::postEvent (&vboxGlobal(), this);
    }
};

/**
 *  USB Popup Menu class.
 *  This class provides the list of USB devices attached to the host.
 */
class VBoxUSBMenu : public QPopupMenu
{
    Q_OBJECT

public:

    enum { USBDevicesMenuNoDevicesId = 1 };

    VBoxUSBMenu (QWidget *);

    const CUSBDevice& getUSB (int);

    void setConsole (const CConsole &);

private slots:

    void processAboutToShow();

    void processHighlighted (int);

private:

    QMap <int, CUSBDevice> mUSBDevicesMap;
    CConsole mConsole;
};


/**
 *  Enable/Disable Menu class.
 *  This class provides enable/disable menu items.
 */
class VBoxSwitchMenu : public QPopupMenu
{
    Q_OBJECT

public:

    VBoxSwitchMenu (QWidget *, QAction *, bool aInverted = false);

    void setToolTip (const QString &);

private slots:

    void processAboutToShow();

    void processActivated (int);

private:

    QAction *mAction;
    QString  mTip;
    bool     mInverted;
};

#endif /* __VBoxGlobal_h__ */
