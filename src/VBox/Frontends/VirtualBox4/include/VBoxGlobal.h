/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxGlobal class declaration
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

#ifndef __VBoxGlobal_h__
#define __VBoxGlobal_h__

#include "COMDefs.h"

#include "VBoxGlobalSettings.h"

#include <qapplication.h>
#include <qpixmap.h>
#include <qicon.h>
#include <qcolor.h>
#include <quuid.h>
#include <qthread.h>
#include <q3popupmenu.h>
#include <qtooltip.h>

#include <q3ptrvector.h>
#include <q3valuevector.h>
#include <q3valuelist.h>
#include <q3dict.h>
#include <q3intdict.h>
//Added by qt3to4:
#include <QLabel>
#include <QEvent>

class QAction;
class QLabel;
class QToolButton;

// Auxiliary types
////////////////////////////////////////////////////////////////////////////////

/** Simple media descriptor type. */
struct VBoxMedia
{
    enum Status { Unknown, Ok, Error, Inaccessible };

    VBoxMedia() : type (VBoxDefs::InvalidType), status (Ok) {}

    VBoxMedia (const CUnknown &d, VBoxDefs::DiskType t, Status s)
        : disk (d), type (t), status (s) {}

    CUnknown disk;
    VBoxDefs::DiskType type;
    Status status;
};

typedef Q3ValueList <VBoxMedia> VBoxMediaList;

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
        AssertMsg (vm_state_color [s], ("No color for %d", s));
        return vm_state_color [s] ? *vm_state_color [s] : none;
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

    QString toString (KDiskControllerType t) const
    {
        AssertMsg (!diskControllerTypes [t].isNull(), ("No text for %d", t));
        return diskControllerTypes [t];
    }

    QString toString (KHardDiskType t) const
    {
        AssertMsg (!diskTypes [t].isNull(), ("No text for %d", t));
        return diskTypes [t];
    }

    QString toString (KHardDiskStorageType t) const
    {
        AssertMsg (!diskStorageTypes [t].isNull(), ("No text for %d", t));
        return diskStorageTypes [t];
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
        QStringVector::const_iterator it =
            qFind (clipboardTypes.begin(), clipboardTypes.end(), s);
        AssertMsg (it != clipboardTypes.end(), ("No value for {%s}", s.latin1()));
        return KClipboardMode (it - clipboardTypes.begin());
    }

    QString toString (KIDEControllerType t) const
    {
        AssertMsg (!ideControllerTypes [t].isNull(), ("No text for %d", t));
        return ideControllerTypes [t];
    }

    KIDEControllerType toIDEControllerType (const QString &s) const
    {
        QStringVector::const_iterator it =
            qFind (ideControllerTypes.begin(), ideControllerTypes.end(), s);
        AssertMsg (it != ideControllerTypes.end(), ("No value for {%s}", s.latin1()));
        return KIDEControllerType (it - ideControllerTypes.begin());
    }

    KVRDPAuthType toVRDPAuthType (const QString &s) const
    {
        QStringVector::const_iterator it =
            qFind (vrdpAuthTypes.begin(), vrdpAuthTypes.end(), s);
        AssertMsg (it != vrdpAuthTypes.end(), ("No value for {%s}", s.latin1()));
        return KVRDPAuthType (it - vrdpAuthTypes.begin());
    }

    KPortMode toPortMode (const QString &s) const
    {
        QStringVector::const_iterator it =
            qFind (portModeTypes.begin(), portModeTypes.end(), s);
        AssertMsg (it != portModeTypes.end(), ("No value for {%s}", s.latin1()));
        return KPortMode (it - portModeTypes.begin());
    }

    KUSBDeviceFilterAction toUSBDevFilterAction (const QString &s) const
    {
        QStringVector::const_iterator it =
            qFind (usbFilterActionTypes.begin(), usbFilterActionTypes.end(), s);
        AssertMsg (it != usbFilterActionTypes.end(), ("No value for {%s}", s.latin1()));
        return KUSBDeviceFilterAction (it - usbFilterActionTypes.begin());
    }

    /**
     *  Similar to toString (KHardDiskType), but returns 'Differencing'
     *  for normal hard disks that have a parent hard disk.
     */
    QString hardDiskTypeString (const CHardDisk &aHD) const
    {
        if (!aHD.GetParent().isNull())
        {
            Assert (aHD.GetType() == KHardDiskType_Normal);
            return tr ("Differencing", "hard disk");
        }
        return toString (aHD.GetType());
    }

    QString toString (KDiskControllerType t, LONG d) const;

    QString toString (KDeviceType t) const
    {
        AssertMsg (!deviceTypes [t].isNull(), ("No text for %d", t));
        return deviceTypes [t];
    }

    KDeviceType toDeviceType (const QString &s) const
    {
        QStringVector::const_iterator it =
            qFind (deviceTypes.begin(), deviceTypes.end(), s);
        AssertMsg (it != deviceTypes.end(), ("No value for {%s}", s.latin1()));
        return KDeviceType (it - deviceTypes.begin());
    }

    QStringList deviceTypeStrings() const;

    QString toString (KAudioDriverType t) const
    {
        AssertMsg (!audioDriverTypes [t].isNull(), ("No text for %d", t));
        return audioDriverTypes [t];
    }

    KAudioDriverType toAudioDriverType (const QString &s) const
    {
        QStringVector::const_iterator it =
            qFind (audioDriverTypes.begin(), audioDriverTypes.end(), s);
        AssertMsg (it != audioDriverTypes.end(), ("No value for {%s}", s.latin1()));
        return KAudioDriverType (it - audioDriverTypes.begin());
    }

    QString toString (KAudioControllerType t) const
    {
        AssertMsg (!audioControllerTypes [t].isNull(), ("No text for %d", t));
        return audioControllerTypes [t];
    }

    KAudioControllerType toAudioControllerType (const QString &s) const
    {
        QStringVector::const_iterator it =
            qFind (audioControllerTypes.begin(), audioControllerTypes.end(), s);
        AssertMsg (it != audioControllerTypes.end(), ("No value for {%s}", s.latin1()));
        return KAudioControllerType (it - audioControllerTypes.begin());
    }

    QString toString (KNetworkAdapterType t) const
    {
        AssertMsg (!networkAdapterTypes [t].isNull(), ("No text for %d", t));
        return networkAdapterTypes [t];
    }

    KNetworkAdapterType toNetworkAdapterType (const QString &s) const
    {
        QStringVector::const_iterator it =
            qFind (networkAdapterTypes.begin(), networkAdapterTypes.end(), s);
        AssertMsg (it != networkAdapterTypes.end(), ("No value for {%s}", s.latin1()));
        return KNetworkAdapterType (it - networkAdapterTypes.begin());
    }

    QString toString (KNetworkAttachmentType t) const
    {
        AssertMsg (!networkAttachmentTypes [t].isNull(), ("No text for %d", t));
        return networkAttachmentTypes [t];
    }

    KNetworkAttachmentType toNetworkAttachmentType (const QString &s) const
    {
        QStringVector::const_iterator it =
            qFind (networkAttachmentTypes.begin(), networkAttachmentTypes.end(), s);
        AssertMsg (it != networkAttachmentTypes.end(), ("No value for {%s}", s.latin1()));
        return KNetworkAttachmentType (it - networkAttachmentTypes.begin());
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

    /* details generators */

    QString details (const CHardDisk &aHD, bool aPredict = false,
                     bool aDoRefresh = true);

    QString details (const CUSBDevice &aDevice) const;
    QString toolTip (const CUSBDevice &aDevice) const;

    QString prepareFileNameForHTML (const QString &fn) const;

    QString detailsReport (const CMachine &m, bool isNewVM, bool withLinks,
                           bool aDoRefresh = true);

    /* VirtualBox helpers */

#ifdef Q_WS_X11
    bool showVirtualBoxLicense();
#endif

    CSession openSession (const QUuid &aId, bool aExisting = false);

    /** Shortcut to openSession (aId, true). */
    CSession openExistingSession (const QUuid &aId) { return openSession (aId, true); }

    bool startMachine (const QUuid &id);

    void startEnumeratingMedia();

    /**
     *  Returns a list of all currently registered media. This list is used
     *  to globally track the accessiblity state of all media on a dedicated
     *  thread. This the list is initially empty (before the first enumeration
     *  process is started using #startEnumeratingMedia()).
     */
    const VBoxMediaList &currentMediaList() const { return media_list; }

    /** Returns true if the media enumeration is in progress. */
    bool isMediaEnumerationStarted() const { return media_enum_thread != NULL; }

    void addMedia (const VBoxMedia &);
    void updateMedia (const VBoxMedia &);
    void removeMedia (VBoxDefs::DiskType, const QUuid &);

    bool findMedia (const CUnknown &, VBoxMedia &) const;

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

    static QIcon iconSet (const QString &aNormal,
                          const QString &aDisabled = "",
                          const QString &aActive = "");
    static QIcon iconSetEx (const QString &aNormal, const QString &aSmallNormal,
                            const QString &aDisabled = "", const QString &aSmallDisabled = "",
                            const QString &aActive = "", const QString &aSmallActive = "");

    static void setTextLabel (QToolButton *aToolButton, const QString &aTextLabel);

    static QRect normalizeGeometry (const QRect &aRect, const QRect &aBoundRect,
                                    bool aCanResize = true);

    static void centerWidget (QWidget *aWidget, QWidget *aRelative,
                              bool aCanResize = true);

    static QChar decimalSep();
    static QString sizeRegexp();

    static Q_UINT64 parseSize (const QString &);
    static QString formatSize (Q_UINT64, int aMode = 0);

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

    static QWidget *findWidget (QWidget *aParent, const char *aName,
                                const char *aClassName = NULL,
                                bool aRecursive = false);

signals:

    /**
     *  Emitted at the beginning of the enumeration process started
     *  by #startEnumeratingMedia().
     */
    void mediaEnumStarted();

    /**
     *  Emitted when a new media item from the list has updated
     *  its accessibility state.
     */
    void mediaEnumerated (const VBoxMedia &aMedia, int aIndex);

    /**
     *  Emitted at the end of the enumeration process started
     *  by #startEnumeratingMedia(). The @a aList argument is passed for
     *  convenience, it is exactly the same as returned by #currentMediaList().
     */
    void mediaEnumFinished (const VBoxMediaList &aList);

    /** Emitted when a new media is added using #addMedia(). */
    void mediaAdded (const VBoxMedia &);

    /** Emitted when the media is updated using #updateMedia(). */
    void mediaUpdated (const VBoxMedia &);

    /** Emitted when the media is removed using #removeMedia(). */
    void mediaRemoved (VBoxDefs::DiskType, const QUuid &);

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

    QThread *media_enum_thread;
    VBoxMediaList media_list;

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

    typedef Q3ValueVector <QString> QStringVector;

    QString verString;

    Q3ValueVector <CGuestOSType> vm_os_types;
    Q3Dict <QPixmap> vm_os_type_icons;
    Q3PtrVector <QColor> vm_state_color;

    Q3IntDict <QPixmap> mStateIcons;
    QPixmap mOfflineSnapshotIcon, mOnlineSnapshotIcon;

    QStringVector machineStates;
    QStringVector sessionStates;
    QStringVector deviceTypes;
    QStringVector diskControllerTypes;
    QStringVector diskTypes;
    QStringVector diskStorageTypes;
    QStringVector vrdpAuthTypes;
    QStringVector portModeTypes;
    QStringVector usbFilterActionTypes;
    QStringVector diskControllerDevices;
    QStringVector audioDriverTypes;
    QStringVector audioControllerTypes;
    QStringVector networkAdapterTypes;
    QStringVector networkAttachmentTypes;
    QStringVector clipboardTypes;
    QStringVector ideControllerTypes;
    QStringVector USBDeviceStates;

    QString mUserDefinedPortName;

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
class VBoxUSBMenu : public Q3PopupMenu
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
class VBoxSwitchMenu : public Q3PopupMenu
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
