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
struct USBDeviceFilter;
typedef std::list<USBDeviceFilter> USBDeviceFiltersList;

/**
 * Common base class for both MainConfigFile and MachineConfigFile
 * which contains some common logic for both.
 */
class ConfigFileBase
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

struct USBDeviceFilter
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

struct Host
{
    USBDeviceFiltersList    llUSBDeviceFilters;
};

struct SystemProperties
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

struct Medium
{
    com::Guid       uuid;
    com::Utf8Str    strLocation;
    com::Utf8Str    strDescription;

    // the following are for hard disks only:
    com::Utf8Str    strFormat;
    bool            fAutoReset;         // optional, only for diffs, default is false
    PropertiesMap   properties;
    MediumType_T    hdType;

    MediaList       llChildren;         // only used with hard disks
};

struct MachineRegistryEntry
{
    com::Guid       uuid;
    com::Utf8Str    strSettingsFile;
};
typedef std::list<MachineRegistryEntry> MachinesRegistry;

struct DHCPServer
{
    com::Utf8Str    strNetworkName,
                    strIPAddress,
                    strIPNetworkMask,
                    strIPLower,
                    strIPUpper;
    bool            fEnabled;
};
typedef std::list<DHCPServer> DHCPServersList;

class MainConfigFile : public ConfigFileBase
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
    void write(const com::Utf8Str strFilename);

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

struct VRDPSettings
{
    VRDPSettings()
        : fEnabled(true),
          authType(VRDPAuthType_Null),
          ulAuthTimeout(5000),
          fAllowMultiConnection(false),
          fReuseSingleConnection(false)
    {}

    bool            fEnabled;
    com::Utf8Str    strPort;
    com::Utf8Str    strNetAddress;
    VRDPAuthType_T  authType;
    uint32_t        ulAuthTimeout;
    bool            fAllowMultiConnection,
                    fReuseSingleConnection;
};

struct BIOSSettings
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
    int64_t         llTimeOffset;
};

struct USBController
{
    USBController()
        : fEnabled(false),
          fEnabledEHCI(false)
    {}

    bool                    fEnabled;
    bool                    fEnabledEHCI;
    USBDeviceFiltersList    llDeviceFilters;
};

struct NetworkAdapter
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

struct SerialPort
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

struct ParallelPort
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

struct AudioAdapter
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

struct SharedFolder
{
    SharedFolder()
        : fWritable(false)
    {}

    com::Utf8Str    strName,
                    strHostPath;
    bool            fWritable;
};
typedef std::list<SharedFolder> SharedFoldersList;

struct GuestProperty
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

struct Hardware
{
    Hardware();

    com::Utf8Str        strVersion;             // hardware version, optional

    bool                fHardwareVirt,
                        fHardwareVirtExclusive,
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
    FirmwareType_T      firmwareType;           // requires settings version 1.9 (VirtualBox 3.1)

    VRDPSettings        vrdpSettings;

    BIOSSettings        biosSettings;
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

/**
 * A device attached to a storage controller. This can either be a
 * hard disk or a DVD drive or a floppy drive and also specifies
 * which medium is "in" the drive; as a result, this is a combination
 * of the Main IMedium and IMediumAttachment interfaces.
 */
struct AttachedDevice
{
    AttachedDevice()
        : deviceType(DeviceType_Null),
          fPassThrough(false),
          lPort(0),
          lDevice(0)
    {}

    DeviceType_T        deviceType;         // only HardDisk, DVD or Floppy are allowed

    // DVDs can be in pass-through mode:
    bool                fPassThrough;

    int32_t             lPort;
    int32_t             lDevice;

    // if an image file is attached to the device (ISO, RAW, or hard disk image such as VDI),
    // this is its UUID; it depends on deviceType which media registry this then needs to
    // be looked up in. If no image file (only permitted for DVDs and floppies), then the UUID is NULL
    com::Guid           uuid;

    // for DVDs and floppies, the attachment can also be a host device:
    com::Utf8Str        strHostDriveSrc;        // if != NULL, value of <HostDrive>/@src
};
typedef std::list<AttachedDevice> AttachedDevicesList;

struct StorageController
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
    int32_t             lIDE0MasterEmulationPort,
                        lIDE0SlaveEmulationPort,
                        lIDE1MasterEmulationPort,
                        lIDE1SlaveEmulationPort;

    AttachedDevicesList llAttachedDevices;
};
typedef std::list<StorageController> StorageControllersList;

// wrap the list into an extra struct so we can use the struct without
// having to define the typedef above in headers
struct Storage
{
    StorageControllersList  llStorageControllers;
};

struct Snapshot;
typedef std::list<Snapshot> SnapshotsList;

struct Snapshot
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


class MachineConfigFile : public ConfigFileBase
{
public:
    MachineConfigFile(const com::Utf8Str *pstrFilename);

    void readNetworkAdapters(const xml::ElementNode &elmHardware, NetworkAdaptersList &ll);
    void readSerialPorts(const xml::ElementNode &elmUART, SerialPortsList &ll);
    void readParallelPorts(const xml::ElementNode &elmLPT, ParallelPortsList &ll);
    void readGuestProperties(const xml::ElementNode &elmGuestProperties, Hardware &hw);
    void readStorageControllerAttributes(const xml::ElementNode &elmStorageController, StorageController &sctl);
    void readHardware(const xml::ElementNode &elmHardware, Hardware &hw, Storage &strg);
    void readHardDiskAttachments_pre1_7(const xml::ElementNode &elmHardDiskAttachments, Storage &strg);
    void readStorageControllers(const xml::ElementNode &elmStorageControllers, Storage &strg);
    void readDVDAndFloppies_pre1_9(const xml::ElementNode &elmHardware, Storage &strg);
    void readSnapshot(const xml::ElementNode &elmSnapshot, Snapshot &snap);
    void convertOldOSType_pre1_5(com::Utf8Str &str);
    void readMachine(const xml::ElementNode &elmMachine);

    void writeHardware(xml::ElementNode &elmParent, const Hardware &hw, const Storage &strg);
    void writeStorageControllers(xml::ElementNode &elmParent, const Storage &st);
    void writeSnapshot(xml::ElementNode &elmParent, const Snapshot &snap);
    void bumpSettingsVersionIfNeeded();
    void write(const com::Utf8Str &strFilename);

    com::Guid               uuid;
    com::Utf8Str            strName;
    bool                    fNameSync;
    com::Utf8Str            strDescription;
    com::Utf8Str            strOsType;
    com::Utf8Str            strStateFile;
    com::Guid               uuidCurrentSnapshot;
    com::Utf8Str            strSnapshotFolder;
    bool                    fLiveMigrationTarget;
    uint32_t                uLiveMigrationPort;
    com::Utf8Str            strLiveMigrationPassword;

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
