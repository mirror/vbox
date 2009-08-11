/** @file
 * Settings file data structures.
 *
 * These structures are created by the settings file loader and filled with values
 * copied from the raw XML data. This allows us to finally make the XML reader
 * version-independent and read VirtualBox XML files from earlier versions without
 * requiring complicated, tedious and error-prone XSLT conversions.
 *
 * Note: Headers in Main code have been tweaked to only declare the structures
 * defined here so that this header need only be included from code files that
 * actually use these structures.
 */

/*
 * Copyright (C) 2007-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_settings_h
#define ___VBox_settings_h

#include <iprt/time.h>

#include "VBox/com/VirtualBox.h"

#include <VBox/com/Guid.h>
#include <VBox/com/string.h>

#include <list>
#include <map>
#include <vector>

#ifdef IN_VBOXXML_R3
# define VBOXXML_CLASS DECLEXPORT_CLASS
#else
# define VBOXXML_CLASS DECLIMPORT_CLASS
#endif

namespace xml
{
    class ElementNode;
}

namespace settings
{

class ConfigFileError;

////////////////////////////////////////////////////////////////////////////////
//
// Helper classes
//
////////////////////////////////////////////////////////////////////////////////

// ExtraDataItem (used by both VirtualBox.xml and machines XML)
typedef std::map<com::Utf8Str, com::Utf8Str> ExtraDataItemsMap;
class USBDeviceFilter;
typedef std::list<USBDeviceFilter> USBDeviceFiltersList;

/**
 * Common base class for both MainConfigFile and MachineConfigFile
 * which contains some common logic for both.
 */
class VBOXXML_CLASS ConfigFileBase
{
public:
    bool fileExists();

protected:
    ConfigFileBase(const com::Utf8Str *pstrFilename);
    ~ConfigFileBase();

    void parseUUID(com::Guid &guid,
                   const com::Utf8Str &strUUID) const;
    void parseTimestamp(RTTIMESPEC &timestamp,
                        const com::Utf8Str &str) const;

    com::Utf8Str makeString(const RTTIMESPEC &tm);
    com::Utf8Str makeString(const com::Guid &guid);

    void readExtraData(const xml::ElementNode &elmExtraData,
                       ExtraDataItemsMap &map);
    void readUSBDeviceFilters(const xml::ElementNode &elmDeviceFilters,
                              USBDeviceFiltersList &ll);

    void createStubDocument();

    void writeExtraData(xml::ElementNode &elmParent, const ExtraDataItemsMap &me);
    void writeUSBDeviceFilters(xml::ElementNode &elmParent,
                               const USBDeviceFiltersList &ll,
                               bool fHostMode);

    void clearDocument();

    struct Data;
    Data *m;

    friend class ConfigFileError;
};

////////////////////////////////////////////////////////////////////////////////
//
// VirtualBox.xml structures
//
////////////////////////////////////////////////////////////////////////////////

struct VBOXXML_CLASS USBDeviceFilter
{
    USBDeviceFilter()
        : fActive(false),
          action(USBDeviceFilterAction_Null),
          ulMaskedInterfaces(0)
    {}

    com::Utf8Str            strName;
    bool                    fActive;
    com::Utf8Str            strVendorId,
                            strProductId,
                            strRevision,
                            strManufacturer,
                            strProduct,
                            strSerialNumber,
                            strPort;
    USBDeviceFilterAction_T action;                 // only used with host USB filters
    com::Utf8Str            strRemote;              // irrelevant for host USB objects
    uint32_t                ulMaskedInterfaces;     // irrelevant for host USB objects
};

struct VBOXXML_CLASS Host
{
    USBDeviceFiltersList    llUSBDeviceFilters;
};

struct VBOXXML_CLASS SystemProperties
{
    SystemProperties()
        : ulLogHistoryCount(3)
    {}

    com::Utf8Str            strDefaultMachineFolder;
    com::Utf8Str            strDefaultHardDiskFolder;
    com::Utf8Str            strDefaultHardDiskFormat;
    com::Utf8Str            strRemoteDisplayAuthLibrary;
    com::Utf8Str            strWebServiceAuthLibrary;
    uint32_t                ulLogHistoryCount;
};

typedef std::map<com::Utf8Str, com::Utf8Str> PropertiesMap;

struct Medium;
typedef std::list<Medium> MediaList;

struct VBOXXML_CLASS Medium
{
    com::Guid       uuid;
    com::Utf8Str    strLocation;
    com::Utf8Str    strDescription;

    // the following are for hard disks only:
    com::Utf8Str    strFormat;
    bool            fAutoReset;         // optional, only for diffs, default is false
    PropertiesMap   properties;
    HardDiskType_T  hdType;

    MediaList       llChildren;         // only used with hard disks
};

struct VBOXXML_CLASS MachineRegistryEntry
{
    com::Guid       uuid;
    com::Utf8Str    strSettingsFile;
};
typedef std::list<MachineRegistryEntry> MachinesRegistry;

struct VBOXXML_CLASS DHCPServer
{
    com::Utf8Str    strNetworkName,
                    strIPAddress,
                    strIPNetworkMask,
                    strIPLower,
                    strIPUpper;
    bool            fEnabled;
};
typedef std::list<DHCPServer> DHCPServersList;

class VBOXXML_CLASS MainConfigFile : public ConfigFileBase
{
public:
    MainConfigFile(const com::Utf8Str *pstrFilename);

    typedef enum {Error, HardDisk, DVDImage, FloppyImage} MediaType;
    void readMedium(MediaType t, const xml::ElementNode &elmMedium, MediaList &llMedia);
    void readMediaRegistry(const xml::ElementNode &elmMediaRegistry);
    void readMachineRegistry(const xml::ElementNode &elmMachineRegistry);
    void readDHCPServers(const xml::ElementNode &elmDHCPServers);

    void writeHardDisk(xml::ElementNode &elmMedium,
                       const Medium &m,
                       uint32_t level);
    void write();

    Host                    host;
    SystemProperties        systemProperties;
    MediaList               llHardDisks,
                            llDvdImages,
                            llFloppyImages;
    MachinesRegistry        llMachines;
    DHCPServersList         llDhcpServers;
    ExtraDataItemsMap       mapExtraDataItems;
};

////////////////////////////////////////////////////////////////////////////////
//
// Machine XML structures
//
////////////////////////////////////////////////////////////////////////////////

struct VBOXXML_CLASS VRDPSettings
{
    VRDPSettings()
        : fEnabled(true),
          ulPort(0),
          authType(VRDPAuthType_Null),
          ulAuthTimeout(5000),
          fAllowMultiConnection(false),
          fReuseSingleConnection(false)
    {}

    bool            fEnabled;
    uint32_t        ulPort;
    com::Utf8Str    strNetAddress;
    VRDPAuthType_T  authType;
    uint32_t        ulAuthTimeout;
    bool            fAllowMultiConnection,
                    fReuseSingleConnection;
};

struct VBOXXML_CLASS BIOSSettings
{
    BIOSSettings()
        : fACPIEnabled(true),
          fIOAPICEnabled(false),
          fLogoFadeIn(true),
          fLogoFadeOut(false),
          ulLogoDisplayTime(0),
          biosBootMenuMode(BIOSBootMenuMode_MessageAndMenu),
          fPXEDebugEnabled(false),
          llTimeOffset(0)
    {}

    bool            fACPIEnabled,
                    fIOAPICEnabled,
                    fLogoFadeIn,
                    fLogoFadeOut;
    uint32_t        ulLogoDisplayTime;
    com::Utf8Str    strLogoImagePath;
    BIOSBootMenuMode_T  biosBootMenuMode;
    bool            fPXEDebugEnabled;
    LONG64          llTimeOffset;
};

struct VBOXXML_CLASS DVDDrive
{
    DVDDrive()
        : fPassThrough(false)
    {}

    bool            fPassThrough;
    com::Guid       uuid;               // if != NULL, UUID of mounted ISO image
    com::Utf8Str    strHostDriveSrc;    // if != NULL, value of <HostDrive>/@src
};

struct VBOXXML_CLASS FloppyDrive
{
    FloppyDrive()
        : fEnabled(true)
    {}

    bool            fEnabled;           // optional, defaults to true
    com::Guid       uuid;               // if != NULL, UUID of mounted ISO image
    com::Utf8Str    strHostDriveSrc;    // if != NULL, value of <HostDrive>/@src
};

struct VBOXXML_CLASS USBController
{
    USBController()
        : fEnabled(false),
          fEnabledEHCI(false)
    {}

    bool                    fEnabled;
    bool                    fEnabledEHCI;
    USBDeviceFiltersList    llDeviceFilters;
};

struct VBOXXML_CLASS NetworkAdapter
{
    NetworkAdapter()
        : ulSlot(0),
          type(NetworkAdapterType_Am79C970A),
          fEnabled(false),
          fCableConnected(false),
          ulLineSpeed(0),
          fTraceEnabled(false),
          mode(NetworkAttachmentType_Null)
    {}

    uint32_t                ulSlot;

    NetworkAdapterType_T    type;
    bool                    fEnabled;
    com::Utf8Str            strMACAddress;
    bool                    fCableConnected;
    uint32_t                ulLineSpeed;
    bool                    fTraceEnabled;
    com::Utf8Str            strTraceFile;

    NetworkAttachmentType_T mode;
    com::Utf8Str            strName;            // with NAT: nat network or empty;
                                                // with bridged: host interface or empty;
                                                // otherwise: network name (required)
};
typedef std::list<NetworkAdapter> NetworkAdaptersList;

struct VBOXXML_CLASS SerialPort
{
    SerialPort()
        : fEnabled(false),
          ulIOBase(0),
          ulIRQ(0),
          portMode(PortMode_Disconnected),
          fServer(false)
    {}

    uint32_t        ulSlot;

    bool            fEnabled;
    uint32_t        ulIOBase;
    uint32_t        ulIRQ;
    PortMode_T      portMode;
    com::Utf8Str    strPath;
    bool            fServer;
};
typedef std::list<SerialPort> SerialPortsList;

struct VBOXXML_CLASS ParallelPort
{
    ParallelPort()
        : fEnabled(false),
          ulIOBase(0),
          ulIRQ(0)
    {}

    uint32_t        ulSlot;

    bool            fEnabled;
    uint32_t        ulIOBase;
    uint32_t        ulIRQ;
    com::Utf8Str    strPath;
};
typedef std::list<ParallelPort> ParallelPortsList;

struct VBOXXML_CLASS AudioAdapter
{
    AudioAdapter()
        : fEnabled(true),
          controllerType(AudioControllerType_AC97),
          driverType(AudioDriverType_Null)
    {}

    bool                    fEnabled;
    AudioControllerType_T   controllerType;
    AudioDriverType_T       driverType;
};

struct VBOXXML_CLASS SharedFolder
{
    SharedFolder()
        : fWritable(false)
    {}

    com::Utf8Str    strName,
                    strHostPath;
    bool            fWritable;
};
typedef std::list<SharedFolder> SharedFoldersList;

struct VBOXXML_CLASS GuestProperty
{
    GuestProperty()
        : timestamp(0)
    {};

    com::Utf8Str    strName,
                    strValue;
    uint64_t        timestamp;
    com::Utf8Str    strFlags;
};
typedef std::list<GuestProperty> GuestPropertiesList;

typedef std::map<uint32_t, DeviceType_T> BootOrderMap;

struct VBOXXML_CLASS Hardware
{
    Hardware()
        : strVersion("2"),
          fHardwareVirt(true),
          fNestedPaging(false),
          fVPID(false),
          fPAE(false),
          cCPUs(1),
          ulMemorySizeMB((uint32_t)-1),
          ulVRAMSizeMB(8),
          cMonitors(1),
          fAccelerate3D(false),
          fAccelerate2DVideo(false),
          clipboardMode(ClipboardMode_Bidirectional),
          ulMemoryBalloonSize(0),
          ulStatisticsUpdateInterval(0)
    {
/*        mBootOrder [0] = DeviceType_Floppy;
        mBootOrder [1] = DeviceType_DVD;
        mBootOrder [2] = DeviceType_HardDisk;
        for (size_t i = 3; i < RT_ELEMENTS (mBootOrder); i++)
            mBootOrder [i] = DeviceType_Null;*/
    }

    com::Utf8Str        strVersion;             // hardware version, optional

    bool                fHardwareVirt,
                        fNestedPaging,
                        fVPID,
                        fPAE;
    uint32_t            cCPUs;

    uint32_t            ulMemorySizeMB;

    BootOrderMap        mapBootOrder;           // item 0 has highest priority

    uint32_t            ulVRAMSizeMB;
    uint32_t            cMonitors;
    bool                fAccelerate3D,
                        fAccelerate2DVideo;     // requires settings version 1.8 (VirtualBox 3.1)

    VRDPSettings        vrdpSettings;

    BIOSSettings        biosSettings;
    DVDDrive            dvdDrive;
    FloppyDrive         floppyDrive;
    USBController       usbController;
    NetworkAdaptersList llNetworkAdapters;
    SerialPortsList     llSerialPorts;
    ParallelPortsList   llParallelPorts;
    AudioAdapter        audioAdapter;

    // technically these two have no business in the hardware section, but for some
    // clever reason <Hardware> is where they are in the XML....
    SharedFoldersList   llSharedFolders;
    ClipboardMode_T     clipboardMode;

    uint32_t            ulMemoryBalloonSize;
    uint32_t            ulStatisticsUpdateInterval;

    GuestPropertiesList llGuestProperties;
    com::Utf8Str        strNotificationPatterns;
};

struct VBOXXML_CLASS AttachedDevice
{
    AttachedDevice()
        : type(HardDisk),
          lPort(0),
          lDevice(0)
    {}

    enum { HardDisk }   type;           // @todo: implement DVD attachments here
    LONG                lPort;
    LONG                lDevice;
    com::Guid           uuid;
};
typedef std::list<AttachedDevice> AttachedDevicesList;

struct VBOXXML_CLASS StorageController
{
    StorageController()
        : storageBus(StorageBus_IDE),
          controllerType(StorageControllerType_PIIX3),
          ulPortCount(2),
          lIDE0MasterEmulationPort(0),
          lIDE0SlaveEmulationPort(0),
          lIDE1MasterEmulationPort(0),
          lIDE1SlaveEmulationPort(0)
    {}

    com::Utf8Str        strName;
    StorageBus_T        storageBus;             // _SATA, _SCSI, _IDE
    StorageControllerType_T controllerType;
    uint32_t            ulPortCount;

    // only for when controllerType == StorageControllerType_IntelAhci:
    LONG                lIDE0MasterEmulationPort,
                        lIDE0SlaveEmulationPort,
                        lIDE1MasterEmulationPort,
                        lIDE1SlaveEmulationPort;

    AttachedDevicesList llAttachedDevices;
};
typedef std::list<StorageController> StorageControllersList;

// wrap the list into an extra struct so we can use the struct without
// having to define the typedef above in headers
struct VBOXXML_CLASS Storage
{
    StorageControllersList  llStorageControllers;
};

struct Snapshot;
typedef std::list<Snapshot> SnapshotsList;

struct VBOXXML_CLASS Snapshot
{
    com::Guid       uuid;
    com::Utf8Str    strName,
                    strDescription;             // optional
    RTTIMESPEC      timestamp;

    com::Utf8Str    strStateFile;               // for online snapshots only

    Hardware        hardware;
    Storage         storage;

    SnapshotsList   llChildSnapshots;
};


class VBOXXML_CLASS MachineConfigFile : public ConfigFileBase
{
public:
    MachineConfigFile(const com::Utf8Str *pstrFilename);

    void readNetworkAdapters(const xml::ElementNode &elmHardware, NetworkAdaptersList &ll);
    void readSerialPorts(const xml::ElementNode &elmUART, SerialPortsList &ll);
    void readParallelPorts(const xml::ElementNode &elmLPT, ParallelPortsList &ll);
    void readGuestProperties(const xml::ElementNode &elmGuestProperties, Hardware &hw);
    void readHardware(const xml::ElementNode &elmHardware, Hardware &hw);
    void readStorageControllers(const xml::ElementNode &elmStorageControllers, Storage &strg);
    void readSnapshot(const xml::ElementNode &elmSnapshot, Snapshot &snap);
    void readMachine(const xml::ElementNode &elmMachine);

    void writeHardware(xml::ElementNode &elmParent, const Hardware &hw);
    void writeStorageControllers(xml::ElementNode &elmParent, const Storage &st);
    void writeSnapshot(xml::ElementNode &elmParent, const Snapshot &snap);
    void write(const com::Utf8Str &strFilename);

    com::Guid               uuid;
    com::Utf8Str            strName;
    bool                    fNameSync;
    com::Utf8Str            strDescription;
    com::Utf8Str            strOsType;
    com::Utf8Str            strStateFile;
    com::Guid               uuidCurrentSnapshot;
    com::Utf8Str            strSnapshotFolder;

    bool                    fCurrentStateModified;      // optional, default is true
    RTTIMESPEC              timeLastStateChange;        // optional, defaults to now
    bool                    fAborted;                   // optional, default is false

    Hardware                hardwareMachine;
    Storage                 storageMachine;

    ExtraDataItemsMap       mapExtraDataItems;

    SnapshotsList           llFirstSnapshot;            // first snapshot or empty list if there's none
};

} // namespace settings


#endif /* ___VBox_settings_h */
