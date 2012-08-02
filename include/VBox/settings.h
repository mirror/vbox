/** @file
 * Settings file data structures.
 *
 * These structures are created by the settings file loader and filled with values
 * copied from the raw XML data. This was all new with VirtualBox 3.1 and allows us
 * to finally make the XML reader version-independent and read VirtualBox XML files
 * from earlier and even newer (future) versions without requiring complicated,
 * tedious and error-prone XSLT conversions.
 *
 * It is this file that defines all structures that map VirtualBox global and
 * machine settings to XML files. These structures are used by the rest of Main,
 * even though this header file does not require anything else in Main.
 *
 * Note: Headers in Main code have been tweaked to only declare the structures
 * defined here so that this header need only be included from code files that
 * actually use these structures.
 */

/*
 * Copyright (C) 2007-2012 Oracle Corporation
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
 */

#ifndef ___VBox_settings_h
#define ___VBox_settings_h

#include <iprt/time.h>

#include "VBox/com/VirtualBox.h"

#include <VBox/com/Guid.h>
#include <VBox/com/string.h>

#include <list>
#include <map>

namespace xml
{
    class ElementNode;
}

namespace settings
{

class ConfigFileError;

////////////////////////////////////////////////////////////////////////////////
//
// Structures shared between Machine XML and VirtualBox.xml
//
////////////////////////////////////////////////////////////////////////////////

/**
 * USB device filter definition. This struct is used both in MainConfigFile
 * (for global USB filters) and MachineConfigFile (for machine filters).
 *
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct USBDeviceFilter
{
    USBDeviceFilter()
        : fActive(false),
          action(USBDeviceFilterAction_Null),
          ulMaskedInterfaces(0)
    {}

    bool operator==(const USBDeviceFilter&u) const;

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

typedef std::map<com::Utf8Str, com::Utf8Str> StringsMap;
typedef std::list<com::Utf8Str> StringsList;

// ExtraDataItem (used by both VirtualBox.xml and machines XML)
struct USBDeviceFilter;
typedef std::list<USBDeviceFilter> USBDeviceFiltersList;

struct Medium;
typedef std::list<Medium> MediaList;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct Medium
{
    Medium()
        : fAutoReset(false),
          hdType(MediumType_Normal)
    {}

    com::Guid       uuid;
    com::Utf8Str    strLocation;
    com::Utf8Str    strDescription;

    // the following are for hard disks only:
    com::Utf8Str    strFormat;
    bool            fAutoReset;         // optional, only for diffs, default is false
    StringsMap      properties;
    MediumType_T    hdType;

    MediaList       llChildren;         // only used with hard disks

    bool operator==(const Medium &m) const;
};

/**
 * A media registry. Starting with VirtualBox 3.3, this can appear in both the
 * VirtualBox.xml file as well as machine XML files with settings version 1.11
 * or higher, so these lists are now in ConfigFileBase.
 *
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct MediaRegistry
{
    MediaList               llHardDisks,
                            llDvdImages,
                            llFloppyImages;

    bool operator==(const MediaRegistry &m) const;
};

/**
 * Common base class for both MainConfigFile and MachineConfigFile
 * which contains some common logic for both.
 */
class ConfigFileBase
{
public:
    bool fileExists();

    void copyBaseFrom(const ConfigFileBase &b);

protected:
    ConfigFileBase(const com::Utf8Str *pstrFilename);
    /* Note: this copy constructor doesn't create a full copy of other, cause
     * the file based stuff (xml doc) could not be copied. */
    ConfigFileBase(const ConfigFileBase &other);

    ~ConfigFileBase();

    void parseUUID(com::Guid &guid,
                   const com::Utf8Str &strUUID) const;
    void parseTimestamp(RTTIMESPEC &timestamp,
                        const com::Utf8Str &str) const;

    com::Utf8Str makeString(const RTTIMESPEC &tm);

    void readExtraData(const xml::ElementNode &elmExtraData,
                       StringsMap &map);
    void readUSBDeviceFilters(const xml::ElementNode &elmDeviceFilters,
                              USBDeviceFiltersList &ll);
    typedef enum {Error, HardDisk, DVDImage, FloppyImage} MediaType;
    void readMedium(MediaType t, const xml::ElementNode &elmMedium, MediaList &llMedia);
    void readMediaRegistry(const xml::ElementNode &elmMediaRegistry, MediaRegistry &mr);

    void setVersionAttribute(xml::ElementNode &elm);
    void createStubDocument();

    void buildExtraData(xml::ElementNode &elmParent, const StringsMap &me);
    void buildUSBDeviceFilters(xml::ElementNode &elmParent,
                               const USBDeviceFiltersList &ll,
                               bool fHostMode);
    void buildMedium(xml::ElementNode &elmMedium,
                     DeviceType_T devType,
                     const Medium &m,
                     uint32_t level);
    void buildMediaRegistry(xml::ElementNode &elmParent,
                            const MediaRegistry &mr);
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
    com::Utf8Str            strVRDEAuthLibrary;
    com::Utf8Str            strWebServiceAuthLibrary;
    com::Utf8Str            strDefaultVRDEExtPack;
    com::Utf8Str            strAutostartDatabasePath;
    uint32_t                ulLogHistoryCount;
};

struct MachineRegistryEntry
{
    com::Guid       uuid;
    com::Utf8Str    strSettingsFile;
};
typedef std::list<MachineRegistryEntry> MachinesRegistry;

struct DHCPServer
{
    DHCPServer()
        : fEnabled(false)
    {}

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

    void readMachineRegistry(const xml::ElementNode &elmMachineRegistry);
    void readDHCPServers(const xml::ElementNode &elmDHCPServers);

    void write(const com::Utf8Str strFilename);

    Host                    host;
    SystemProperties        systemProperties;
    MediaRegistry           mediaRegistry;
    MachinesRegistry        llMachines;
    DHCPServersList         llDhcpServers;
    StringsMap              mapExtraDataItems;
};

////////////////////////////////////////////////////////////////////////////////
//
// Machine XML structures
//
////////////////////////////////////////////////////////////////////////////////

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct VRDESettings
{
    VRDESettings()
        : fEnabled(true),
          authType(AuthType_Null),
          ulAuthTimeout(5000),
          fAllowMultiConnection(false),
          fReuseSingleConnection(false)
    {}

    bool operator==(const VRDESettings& v) const;

    bool            fEnabled;
    AuthType_T      authType;
    uint32_t        ulAuthTimeout;
    com::Utf8Str    strAuthLibrary;
    bool            fAllowMultiConnection,
                    fReuseSingleConnection;
    com::Utf8Str    strVrdeExtPack;
    StringsMap      mapProperties;
};

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct BIOSSettings
{
    BIOSSettings()
        : fACPIEnabled(true),
          fIOAPICEnabled(false),
          fLogoFadeIn(true),
          fLogoFadeOut(true),
          ulLogoDisplayTime(0),
          biosBootMenuMode(BIOSBootMenuMode_MessageAndMenu),
          fPXEDebugEnabled(false),
          llTimeOffset(0)
    {}

    bool operator==(const BIOSSettings &d) const;

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

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct USBController
{
    USBController()
        : fEnabled(false),
          fEnabledEHCI(false)
    {}

    bool operator==(const USBController &u) const;

    bool                    fEnabled;
    bool                    fEnabledEHCI;
    USBDeviceFiltersList    llDeviceFilters;
};

 struct NATRule
 {
     NATRule()
         : proto(NATProtocol_TCP),
           u16HostPort(0),
           u16GuestPort(0)
     {}

     bool operator==(const NATRule &r) const
     {
         return strName == r.strName
             && proto == r.proto
             && u16HostPort == r.u16HostPort
             && strHostIP == r.strHostIP
             && u16GuestPort == r.u16GuestPort
             && strGuestIP == r.strGuestIP;
     }

     com::Utf8Str            strName;
     NATProtocol_T           proto;
     uint16_t                u16HostPort;
     com::Utf8Str            strHostIP;
     uint16_t                u16GuestPort;
     com::Utf8Str            strGuestIP;
 };
 typedef std::list<NATRule> NATRuleList;

 struct NAT
 {
     NAT()
         : u32Mtu(0),
           u32SockRcv(0),
           u32SockSnd(0),
           u32TcpRcv(0),
           u32TcpSnd(0),
           fDNSPassDomain(true), /* historically this value is true */
           fDNSProxy(false),
           fDNSUseHostResolver(false),
           fAliasLog(false),
           fAliasProxyOnly(false),
           fAliasUseSamePorts(false)
     {}

     bool operator==(const NAT &n) const
     {
        return strNetwork           == n.strNetwork
             && strBindIP           == n.strBindIP
             && u32Mtu              == n.u32Mtu
             && u32SockRcv          == n.u32SockRcv
             && u32SockSnd          == n.u32SockSnd
             && u32TcpSnd           == n.u32TcpSnd
             && u32TcpRcv           == n.u32TcpRcv
             && strTFTPPrefix       == n.strTFTPPrefix
             && strTFTPBootFile     == n.strTFTPBootFile
             && strTFTPNextServer   == n.strTFTPNextServer
             && fDNSPassDomain      == n.fDNSPassDomain
             && fDNSProxy           == n.fDNSProxy
             && fDNSUseHostResolver == n.fDNSUseHostResolver
             && fAliasLog           == n.fAliasLog
             && fAliasProxyOnly     == n.fAliasProxyOnly
             && fAliasUseSamePorts  == n.fAliasUseSamePorts
             && llRules             == n.llRules;
     }

     com::Utf8Str            strNetwork;
     com::Utf8Str            strBindIP;
     uint32_t                u32Mtu;
     uint32_t                u32SockRcv;
     uint32_t                u32SockSnd;
     uint32_t                u32TcpRcv;
     uint32_t                u32TcpSnd;
     com::Utf8Str            strTFTPPrefix;
     com::Utf8Str            strTFTPBootFile;
     com::Utf8Str            strTFTPNextServer;
     bool                    fDNSPassDomain;
     bool                    fDNSProxy;
     bool                    fDNSUseHostResolver;
     bool                    fAliasLog;
     bool                    fAliasProxyOnly;
     bool                    fAliasUseSamePorts;
     NATRuleList             llRules;
 };
/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct NetworkAdapter
{
    NetworkAdapter()
        : ulSlot(0),
          type(NetworkAdapterType_Am79C970A),
          fEnabled(false),
          fCableConnected(false),
          ulLineSpeed(0),
          enmPromiscModePolicy(NetworkAdapterPromiscModePolicy_Deny),
          fTraceEnabled(false),
          mode(NetworkAttachmentType_Null),
          ulBootPriority(0)
    {}

    bool operator==(const NetworkAdapter &n) const;

    uint32_t                ulSlot;

    NetworkAdapterType_T                type;
    bool                                fEnabled;
    com::Utf8Str                        strMACAddress;
    bool                                fCableConnected;
    uint32_t                            ulLineSpeed;
    NetworkAdapterPromiscModePolicy_T   enmPromiscModePolicy;
    bool                                fTraceEnabled;
    com::Utf8Str                        strTraceFile;

    NetworkAttachmentType_T             mode;
    NAT                                 nat;
    com::Utf8Str                        strBridgedName;
    com::Utf8Str                        strHostOnlyName;
    com::Utf8Str                        strInternalNetworkName;
    com::Utf8Str                        strGenericDriver;
    StringsMap                          genericProperties;
    uint32_t                            ulBootPriority;
    com::Utf8Str                        strBandwidthGroup; // requires settings version 1.13 (VirtualBox 4.2)
};
typedef std::list<NetworkAdapter> NetworkAdaptersList;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct SerialPort
{
    SerialPort()
        : ulSlot(0),
          fEnabled(false),
          ulIOBase(0x3f8),
          ulIRQ(4),
          portMode(PortMode_Disconnected),
          fServer(false)
    {}

    bool operator==(const SerialPort &n) const;

    uint32_t        ulSlot;

    bool            fEnabled;
    uint32_t        ulIOBase;
    uint32_t        ulIRQ;
    PortMode_T      portMode;
    com::Utf8Str    strPath;
    bool            fServer;
};
typedef std::list<SerialPort> SerialPortsList;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct ParallelPort
{
    ParallelPort()
        : ulSlot(0),
          fEnabled(false),
          ulIOBase(0x378),
          ulIRQ(7)
    {}

    bool operator==(const ParallelPort &d) const;

    uint32_t        ulSlot;

    bool            fEnabled;
    uint32_t        ulIOBase;
    uint32_t        ulIRQ;
    com::Utf8Str    strPath;
};
typedef std::list<ParallelPort> ParallelPortsList;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct AudioAdapter
{
    AudioAdapter()
        : fEnabled(true),
          controllerType(AudioControllerType_AC97),
          driverType(AudioDriverType_Null)
    {}

    bool operator==(const AudioAdapter &a) const
    {
        return     (this == &a)
                || (    (fEnabled        == a.fEnabled)
                     && (controllerType  == a.controllerType)
                     && (driverType      == a.driverType)
                   );
    }

    bool                    fEnabled;
    AudioControllerType_T   controllerType;
    AudioDriverType_T       driverType;
};

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct SharedFolder
{
    SharedFolder()
        : fWritable(false)
        , fAutoMount(false)
    {}

    bool operator==(const SharedFolder &a) const;

    com::Utf8Str    strName,
                    strHostPath;
    bool            fWritable;
    bool            fAutoMount;
};
typedef std::list<SharedFolder> SharedFoldersList;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct GuestProperty
{
    GuestProperty()
        : timestamp(0)
    {};

    bool operator==(const GuestProperty &g) const;

    com::Utf8Str    strName,
                    strValue;
    uint64_t        timestamp;
    com::Utf8Str    strFlags;
};
typedef std::list<GuestProperty> GuestPropertiesList;

typedef std::map<uint32_t, DeviceType_T> BootOrderMap;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct CpuIdLeaf
{
    CpuIdLeaf()
        : ulId(UINT32_MAX),
          ulEax(0),
          ulEbx(0),
          ulEcx(0),
          ulEdx(0)
    {}

    bool operator==(const CpuIdLeaf &c) const
    {
        return (    (this == &c)
                 || (    (ulId      == c.ulId)
                      && (ulEax     == c.ulEax)
                      && (ulEbx     == c.ulEbx)
                      && (ulEcx     == c.ulEcx)
                      && (ulEdx     == c.ulEdx)
                    )
               );
    }

    uint32_t                ulId;
    uint32_t                ulEax;
    uint32_t                ulEbx;
    uint32_t                ulEcx;
    uint32_t                ulEdx;
};
typedef std::list<CpuIdLeaf> CpuIdLeafsList;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct Cpu
{
    Cpu()
        : ulId(UINT32_MAX)
    {}

    bool operator==(const Cpu &c) const
    {
        return (ulId == c.ulId);
    }

    uint32_t                ulId;
};
typedef std::list<Cpu> CpuList;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct BandwidthGroup
{
    BandwidthGroup()
        : cMaxBytesPerSec(0),
          enmType(BandwidthGroupType_Null)
    {}

    bool operator==(const BandwidthGroup &i) const
    {
        return (   (strName      == i.strName)
                && (cMaxBytesPerSec == i.cMaxBytesPerSec)
                && (enmType      == i.enmType));
    }

    com::Utf8Str         strName;
    uint64_t             cMaxBytesPerSec;
    BandwidthGroupType_T enmType;
};
typedef std::list<BandwidthGroup> BandwidthGroupList;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct IOSettings
{
    IOSettings();

    bool operator==(const IOSettings &i) const
    {
        return (   (fIOCacheEnabled   == i.fIOCacheEnabled)
                && (ulIOCacheSize     == i.ulIOCacheSize)
                && (llBandwidthGroups == i.llBandwidthGroups));
    }

    bool               fIOCacheEnabled;
    uint32_t           ulIOCacheSize;
    BandwidthGroupList llBandwidthGroups;
};

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct HostPCIDeviceAttachment
{
    HostPCIDeviceAttachment()
        : uHostAddress(0),
          uGuestAddress(0)
    {}

    bool operator==(const HostPCIDeviceAttachment &a) const
    {
        return (   (uHostAddress   == a.uHostAddress)
                && (uGuestAddress  == a.uGuestAddress)
                && (strDeviceName  == a.strDeviceName)
               );
    }

    com::Utf8Str    strDeviceName;
    uint32_t        uHostAddress;
    uint32_t        uGuestAddress;
};
typedef std::list<HostPCIDeviceAttachment> HostPCIDeviceAttachmentList;

/**
 * Representation of Machine hardware; this is used in the MachineConfigFile.hardwareMachine
 * field.
 *
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct Hardware
{
    Hardware();

    bool operator==(const Hardware&) const;

    com::Utf8Str        strVersion;             // hardware version, optional
    com::Guid           uuid;                   // hardware uuid, optional (null).

    bool                fHardwareVirt,
                        fHardwareVirtExclusive,
                        fNestedPaging,
                        fLargePages,
                        fVPID,
                        fHardwareVirtForce,
                        fSyntheticCpu,
                        fPAE;
    uint32_t            cCPUs;
    bool                fCpuHotPlug;            // requires settings version 1.10 (VirtualBox 3.2)
    CpuList             llCpus;                 // requires settings version 1.10 (VirtualBox 3.2)
    bool                fHPETEnabled;           // requires settings version 1.10 (VirtualBox 3.2)
    uint32_t            ulCpuExecutionCap;      // requires settings version 1.11 (VirtualBox 3.3)

    CpuIdLeafsList      llCpuIdLeafs;

    uint32_t            ulMemorySizeMB;

    BootOrderMap        mapBootOrder;           // item 0 has highest priority

    uint32_t            ulVRAMSizeMB;
    uint32_t            cMonitors;
    bool                fAccelerate3D,
                        fAccelerate2DVideo;     // requires settings version 1.8 (VirtualBox 3.1)
    FirmwareType_T      firmwareType;           // requires settings version 1.9 (VirtualBox 3.1)

    PointingHIDType_T   pointingHIDType;        // requires settings version 1.10 (VirtualBox 3.2)
    KeyboardHIDType_T   keyboardHIDType;        // requires settings version 1.10 (VirtualBox 3.2)

    ChipsetType_T       chipsetType;            // requires settings version 1.11 (VirtualBox 4.0)

    bool                fEmulatedUSBCardReader; // 1.12 (VirtualBox 4.1)

    VRDESettings        vrdeSettings;

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
    DragAndDropMode_T   dragAndDropMode;

    uint32_t            ulMemoryBalloonSize;
    bool                fPageFusionEnabled;

    GuestPropertiesList llGuestProperties;
    com::Utf8Str        strNotificationPatterns;

    IOSettings          ioSettings;             // requires settings version 1.10 (VirtualBox 3.2)
    HostPCIDeviceAttachmentList pciAttachments; // requires settings version 1.12 (VirtualBox 4.1)
};

/**
 * A device attached to a storage controller. This can either be a
 * hard disk or a DVD drive or a floppy drive and also specifies
 * which medium is "in" the drive; as a result, this is a combination
 * of the Main IMedium and IMediumAttachment interfaces.
 *
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct AttachedDevice
{
    AttachedDevice()
        : deviceType(DeviceType_Null),
          fPassThrough(false),
          fTempEject(false),
          fNonRotational(false),
          lPort(0),
          lDevice(0)
    {}

    bool operator==(const AttachedDevice &a) const;

    DeviceType_T        deviceType;         // only HardDisk, DVD or Floppy are allowed

    // DVDs can be in pass-through mode:
    bool                fPassThrough;

    // Whether guest-triggered eject of DVDs will keep the medium in the
    // VM config or not:
    bool                fTempEject;

    // Whether the medium is non-rotational:
    bool                fNonRotational;

    // Whether the medium supports discarding unused blocks:
    bool                fDiscard;

    int32_t             lPort;
    int32_t             lDevice;

    // if an image file is attached to the device (ISO, RAW, or hard disk image such as VDI),
    // this is its UUID; it depends on deviceType which media registry this then needs to
    // be looked up in. If no image file (only permitted for DVDs and floppies), then the UUID is NULL
    com::Guid           uuid;

    // for DVDs and floppies, the attachment can also be a host device:
    com::Utf8Str        strHostDriveSrc;        // if != NULL, value of <HostDrive>/@src

    // Bandwidth group the device is attached to.
    com::Utf8Str        strBwGroup;
};
typedef std::list<AttachedDevice> AttachedDevicesList;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct StorageController
{
    StorageController()
        : storageBus(StorageBus_IDE),
          controllerType(StorageControllerType_PIIX3),
          ulPortCount(2),
          ulInstance(0),
          fUseHostIOCache(true),
          fBootable(true),
          lIDE0MasterEmulationPort(0),
          lIDE0SlaveEmulationPort(0),
          lIDE1MasterEmulationPort(0),
          lIDE1SlaveEmulationPort(0)
    {}

    bool operator==(const StorageController &s) const;

    com::Utf8Str            strName;
    StorageBus_T            storageBus;             // _SATA, _SCSI, _IDE, _SAS
    StorageControllerType_T controllerType;
    uint32_t                ulPortCount;
    uint32_t                ulInstance;
    bool                    fUseHostIOCache;
    bool                    fBootable;

    // only for when controllerType == StorageControllerType_IntelAhci:
    int32_t                 lIDE0MasterEmulationPort,
                            lIDE0SlaveEmulationPort,
                            lIDE1MasterEmulationPort,
                            lIDE1SlaveEmulationPort;

    AttachedDevicesList     llAttachedDevices;
};
typedef std::list<StorageController> StorageControllersList;

/**
 * We wrap the storage controllers list into an extra struct so we can
 * use an undefined struct without needing std::list<> in all the headers.
 *
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct Storage
{
    bool operator==(const Storage &s) const;

    StorageControllersList  llStorageControllers;
};

/**
 * Settings that has to do with debugging.
 */
struct Debugging
{
    Debugging()
        : fTracingEnabled(false),
          fAllowTracingToAccessVM(false),
          strTracingConfig()
    { }

    bool operator==(const Debugging &rOther) const
    {
        return fTracingEnabled          == rOther.fTracingEnabled
            && fAllowTracingToAccessVM  == rOther.fAllowTracingToAccessVM
            && strTracingConfig         == rOther.strTracingConfig;
    }

    bool areDefaultSettings() const
    {
        return !fTracingEnabled
            && !fAllowTracingToAccessVM
            && strTracingConfig.isEmpty();
    }

    bool                    fTracingEnabled;
    bool                    fAllowTracingToAccessVM;
    com::Utf8Str            strTracingConfig;
};

/**
 * Settings that has to do with autostart.
 */
struct Autostart
{
    Autostart()
        : fAutostartEnabled(false),
          uAutostartDelay(0),
          enmAutostopType(AutostopType_Disabled)
    { }

    bool operator==(const Autostart &rOther) const
    {
        return fAutostartEnabled == rOther.fAutostartEnabled
            && uAutostartDelay   == rOther.uAutostartDelay
            && enmAutostopType   == rOther.enmAutostopType;
    }

    bool areDefaultSettings() const
    {
        return !fAutostartEnabled
            && !uAutostartDelay
            && enmAutostopType == AutostopType_Disabled;
    }

    bool                    fAutostartEnabled;
    uint32_t                uAutostartDelay;
    AutostopType_T          enmAutostopType;
};

struct Snapshot;
typedef std::list<Snapshot> SnapshotsList;

/**
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by MachineConfigFile::operator==(), or otherwise
 * your settings might never get saved.
 */
struct Snapshot
{
    bool operator==(const Snapshot &s) const;

    com::Guid       uuid;
    com::Utf8Str    strName,
                    strDescription;             // optional
    RTTIMESPEC      timestamp;

    com::Utf8Str    strStateFile;               // for online snapshots only

    Hardware        hardware;
    Storage         storage;

    Debugging       debugging;
    Autostart       autostart;

    SnapshotsList   llChildSnapshots;
};

struct MachineUserData
{
    MachineUserData()
        : fNameSync(true),
          fTeleporterEnabled(false),
          uTeleporterPort(0),
          enmFaultToleranceState(FaultToleranceState_Inactive),
          uFaultTolerancePort(0),
          uFaultToleranceInterval(0),
          fRTCUseUTC(false)
    {
        llGroups.push_back("/");
    }

    bool operator==(const MachineUserData &c) const
    {
        return    (strName                    == c.strName)
               && (fNameSync                  == c.fNameSync)
               && (strDescription             == c.strDescription)
               && (llGroups                   == c.llGroups)
               && (strOsType                  == c.strOsType)
               && (strSnapshotFolder          == c.strSnapshotFolder)
               && (fTeleporterEnabled         == c.fTeleporterEnabled)
               && (uTeleporterPort            == c.uTeleporterPort)
               && (strTeleporterAddress       == c.strTeleporterAddress)
               && (strTeleporterPassword      == c.strTeleporterPassword)
               && (enmFaultToleranceState     == c.enmFaultToleranceState)
               && (uFaultTolerancePort        == c.uFaultTolerancePort)
               && (uFaultToleranceInterval    == c.uFaultToleranceInterval)
               && (strFaultToleranceAddress   == c.strFaultToleranceAddress)
               && (strFaultTolerancePassword  == c.strFaultTolerancePassword)
               && (fRTCUseUTC                 == c.fRTCUseUTC);
    }

    com::Utf8Str            strName;
    bool                    fNameSync;
    com::Utf8Str            strDescription;
    StringsList             llGroups;
    com::Utf8Str            strOsType;
    com::Utf8Str            strSnapshotFolder;
    bool                    fTeleporterEnabled;
    uint32_t                uTeleporterPort;
    com::Utf8Str            strTeleporterAddress;
    com::Utf8Str            strTeleporterPassword;
    FaultToleranceState_T   enmFaultToleranceState;
    uint32_t                uFaultTolerancePort;
    com::Utf8Str            strFaultToleranceAddress;
    com::Utf8Str            strFaultTolerancePassword;
    uint32_t                uFaultToleranceInterval;
    bool                    fRTCUseUTC;
};

/**
 * MachineConfigFile represents an XML machine configuration. All the machine settings
 * that go out to the XML (or are read from it) are in here.
 *
 * NOTE: If you add any fields in here, you must update a) the constructor and b)
 * the operator== which is used by Machine::saveSettings(), or otherwise your settings
 * might never get saved.
 */
class MachineConfigFile : public ConfigFileBase
{
public:
    com::Guid               uuid;

    MachineUserData         machineUserData;

    com::Utf8Str            strStateFile;
    bool                    fCurrentStateModified;      // optional, default is true
    RTTIMESPEC              timeLastStateChange;        // optional, defaults to now
    bool                    fAborted;                   // optional, default is false

    com::Guid               uuidCurrentSnapshot;

    Hardware                hardwareMachine;
    Storage                 storageMachine;
    MediaRegistry           mediaRegistry;
    Debugging               debugging;
    Autostart               autostart;

    StringsMap              mapExtraDataItems;

    SnapshotsList           llFirstSnapshot;            // first snapshot or empty list if there's none

    MachineConfigFile(const com::Utf8Str *pstrFilename);

    bool operator==(const MachineConfigFile &m) const;

    bool canHaveOwnMediaRegistry() const;

    void importMachineXML(const xml::ElementNode &elmMachine);

    void write(const com::Utf8Str &strFilename);

    enum
    {
        BuildMachineXML_IncludeSnapshots = 0x01,
        BuildMachineXML_WriteVboxVersionAttribute = 0x02,
        BuildMachineXML_SkipRemovableMedia = 0x04,
        BuildMachineXML_MediaRegistry = 0x08,
        BuildMachineXML_SuppressSavedState = 0x10
    };
    void buildMachineXML(xml::ElementNode &elmMachine,
                         uint32_t fl,
                         std::list<xml::ElementNode*> *pllElementsWithUuidAttributes);

    static bool isAudioDriverAllowedOnThisHost(AudioDriverType_T drv);
    static AudioDriverType_T getHostDefaultAudioDriver();

private:
    void readNetworkAdapters(const xml::ElementNode &elmHardware, NetworkAdaptersList &ll);
    void readAttachedNetworkMode(const xml::ElementNode &pelmMode, bool fEnabled, NetworkAdapter &nic);
    void readCpuIdTree(const xml::ElementNode &elmCpuid, CpuIdLeafsList &ll);
    void readCpuTree(const xml::ElementNode &elmCpu, CpuList &ll);
    void readSerialPorts(const xml::ElementNode &elmUART, SerialPortsList &ll);
    void readParallelPorts(const xml::ElementNode &elmLPT, ParallelPortsList &ll);
    void readAudioAdapter(const xml::ElementNode &elmAudioAdapter, AudioAdapter &aa);
    void readGuestProperties(const xml::ElementNode &elmGuestProperties, Hardware &hw);
    void readStorageControllerAttributes(const xml::ElementNode &elmStorageController, StorageController &sctl);
    void readHardware(const xml::ElementNode &elmHardware, Hardware &hw, Storage &strg);
    void readHardDiskAttachments_pre1_7(const xml::ElementNode &elmHardDiskAttachments, Storage &strg);
    void readStorageControllers(const xml::ElementNode &elmStorageControllers, Storage &strg);
    void readDVDAndFloppies_pre1_9(const xml::ElementNode &elmHardware, Storage &strg);
    void readTeleporter(const xml::ElementNode *pElmTeleporter, MachineUserData *pUserData);
    void readDebugging(const xml::ElementNode *pElmDbg, Debugging *pDbg);
    void readAutostart(const xml::ElementNode *pElmAutostart, Autostart *pAutostart);
    void readGroups(const xml::ElementNode *elmGroups, StringsList *pllGroups);
    void readSnapshot(const xml::ElementNode &elmSnapshot, Snapshot &snap);
    void convertOldOSType_pre1_5(com::Utf8Str &str);
    void readMachine(const xml::ElementNode &elmMachine);

    void buildHardwareXML(xml::ElementNode &elmParent, const Hardware &hw, const Storage &strg);
    void buildNetworkXML(NetworkAttachmentType_T mode, xml::ElementNode &elmParent, bool fEnabled, const NetworkAdapter &nic);
    void buildStorageControllersXML(xml::ElementNode &elmParent,
                                    const Storage &st,
                                    bool fSkipRemovableMedia,
                                    std::list<xml::ElementNode*> *pllElementsWithUuidAttributes);
    void buildDebuggingXML(xml::ElementNode *pElmParent, const Debugging *pDbg);
    void buildAutostartXML(xml::ElementNode *pElmParent, const Autostart *pAutostart);
    void buildGroupsXML(xml::ElementNode *pElmParent, const StringsList *pllGroups);
    void buildSnapshotXML(xml::ElementNode &elmParent, const Snapshot &snap);

    void bumpSettingsVersionIfNeeded();
};

} // namespace settings


#endif /* ___VBox_settings_h */
