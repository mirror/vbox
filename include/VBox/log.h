/** @file
 * VirtualBox - Logging.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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

#ifndef ___VBox_log_h
#define ___VBox_log_h

/*
 * Set the default loggroup.
 */
#ifndef LOG_GROUP
# define LOG_GROUP LOG_GROUP_DEFAULT
#endif

#include <iprt/log.h>


/** @defgroup grp_rt_vbox_log    VirtualBox Logging
 * @ingroup grp_rt_vbox
 * @{
 */

/** PC port for debug output */
#define RTLOG_DEBUG_PORT 0x504

/**
 * VirtualBox Logging Groups.
 * (Remember to update LOGGROUP_NAMES!)
 *
 * @remark It should be pretty obvious, but just to have
 *         mentioned it, the values are sorted alphabetically (using the
 *         english alphabet) except for _DEFAULT which is always first.
 *
 *         If anyone might be wondering what the alphabet looks like:
 *              A B C D E F G H I J K L M N O P Q R S T U V W X Y Z _
 */
typedef enum LOGGROUP
{
    /** The default VBox group. */
    LOG_GROUP_DEFAULT = RTLOGGROUP_FIRST_USER,
    /** Auto-logon group. */
    LOG_GROUP_AUTOLOGON,
    /** CFGM group. */
    LOG_GROUP_CFGM,
    /** CPUM group. */
    LOG_GROUP_CPUM,
    /** CSAM group. */
    LOG_GROUP_CSAM,
    /** Debug Console group. */
    LOG_GROUP_DBGC,
    /** DBGF group. */
    LOG_GROUP_DBGF,
    /** DBGF info group. */
    LOG_GROUP_DBGF_INFO,
    /** The debugger gui. */
    LOG_GROUP_DBGG,
    /** Generic Device group. */
    LOG_GROUP_DEV,
    /** ACPI Device group. */
    LOG_GROUP_DEV_ACPI,
    /** AHCI Device group. */
    LOG_GROUP_DEV_AHCI,
    /** APIC Device group. */
    LOG_GROUP_DEV_APIC,
    /** Audio Device group. */
    LOG_GROUP_DEV_AUDIO,
    /** BusLogic SCSI host adapter group. */
    LOG_GROUP_DEV_BUSLOGIC,
    /** DMA Controller group. */
    LOG_GROUP_DEV_DMA,
    /** Gigabit Ethernet Device group. */
    LOG_GROUP_DEV_E1000,
    /** Extensible Firmware Interface Device group. */
    LOG_GROUP_DEV_EFI,
    /** Floppy Controller Device group. */
    LOG_GROUP_DEV_FDC,
    /** High Precision Event Timer Device group. */
    LOG_GROUP_DEV_HPET,
    /** IDE Device group. */
    LOG_GROUP_DEV_IDE,
    /** The internal networking IP stack Device group. */
    LOG_GROUP_DEV_INIP,
    /** KeyBoard Controller Device group. */
    LOG_GROUP_DEV_KBD,
    /** Low Pin Count Device group. */
    LOG_GROUP_DEV_LPC,
    /** LsiLogic SCSI controller Device group. */
    LOG_GROUP_DEV_LSILOGICSCSI,
    /** NE2000 Device group. */
    LOG_GROUP_DEV_NE2000,
    /** Parallel Device group */
    LOG_GROUP_DEV_PARALLEL,
    /** PC Device group. */
    LOG_GROUP_DEV_PC,
    /** PC Architecture Device group. */
    LOG_GROUP_DEV_PC_ARCH,
    /** PC BIOS Device group. */
    LOG_GROUP_DEV_PC_BIOS,
    /** PCI Device group. */
    LOG_GROUP_DEV_PCI,
    /** PCI Raw Device group. */
    LOG_GROUP_DEV_PCI_RAW,
    /** PCNet Device group. */
    LOG_GROUP_DEV_PCNET,
    /** PIC Device group. */
    LOG_GROUP_DEV_PIC,
    /** PIT Device group. */
    LOG_GROUP_DEV_PIT,
    /** RTC Device group. */
    LOG_GROUP_DEV_RTC,
    /** Serial Device group */
    LOG_GROUP_DEV_SERIAL,
    /** System Management Controller Device group. */
    LOG_GROUP_DEV_SMC,
    /** USB Device group. */
    LOG_GROUP_DEV_USB,
    /** VGA Device group. */
    LOG_GROUP_DEV_VGA,
    /** Virtio PCI Device group. */
    LOG_GROUP_DEV_VIRTIO,
    /** Virtio Network Device group. */
    LOG_GROUP_DEV_VIRTIO_NET,
    /** VMM Device group. */
    LOG_GROUP_DEV_VMM,
    /** VMM Device group for backdoor logging. */
    LOG_GROUP_DEV_VMM_BACKDOOR,
    /** VMM Device group for logging guest backdoor logging to stderr. */
    LOG_GROUP_DEV_VMM_STDERR,
    /** Disassembler group. */
    LOG_GROUP_DIS,
    /** Generic driver group. */
    LOG_GROUP_DRV,
    /** ACPI driver group */
    LOG_GROUP_DRV_ACPI,
    /** Block driver group. */
    LOG_GROUP_DRV_BLOCK,
    /** Char driver group. */
    LOG_GROUP_DRV_CHAR,
    /** Disk integrity driver group. */
    LOG_GROUP_DRV_DISK_INTEGRITY,
    /** Video Display driver group. */
    LOG_GROUP_DRV_DISPLAY,
    /** Floppy media driver group. */
    LOG_GROUP_DRV_FLOPPY,
    /** Host Base block driver group. */
    LOG_GROUP_DRV_HOST_BASE,
    /** Host DVD block driver group. */
    LOG_GROUP_DRV_HOST_DVD,
    /** Host floppy block driver group. */
    LOG_GROUP_DRV_HOST_FLOPPY,
    /** Host Parallel Driver group */
    LOG_GROUP_DRV_HOST_PARALLEL,
    /** Host Serial Driver Group */
    LOG_GROUP_DRV_HOST_SERIAL,
    /** The internal networking transport driver group. */
    LOG_GROUP_DRV_INTNET,
    /** ISO (CD/DVD) media driver group. */
    LOG_GROUP_DRV_ISO,
    /** Keyboard Queue driver group. */
    LOG_GROUP_DRV_KBD_QUEUE,
    /** lwIP IP stack driver group. */
    LOG_GROUP_DRV_LWIP,
    /** Video Miniport driver group. */
    LOG_GROUP_DRV_MINIPORT,
    /** Mouse driver group. */
    LOG_GROUP_DRV_MOUSE,
    /** Mouse Queue driver group. */
    LOG_GROUP_DRV_MOUSE_QUEUE,
    /** Named Pipe stream driver group. */
    LOG_GROUP_DRV_NAMEDPIPE,
    /** NAT network transport driver group */
    LOG_GROUP_DRV_NAT,
    /** Raw image driver group */
    LOG_GROUP_DRV_RAW_IMAGE,
    /** SCSI driver group. */
    LOG_GROUP_DRV_SCSI,
    /** Host SCSI driver group. */
    LOG_GROUP_DRV_SCSIHOST,
    /** Async transport driver group */
    LOG_GROUP_DRV_TRANSPORT_ASYNC,
    /** TUN network transport driver group */
    LOG_GROUP_DRV_TUN,
    /** UDP tunnet network transport driver group. */
    LOG_GROUP_DRV_UDPTUNNEL,
    /** USB Proxy driver group. */
    LOG_GROUP_DRV_USBPROXY,
    /** VBoxHDD media driver group. */
    LOG_GROUP_DRV_VBOXHDD,
    /** VBox HDD container media driver group. */
    LOG_GROUP_DRV_VD,
    /** Virtual Switch transport driver group */
    LOG_GROUP_DRV_VSWITCH,
    /** VUSB driver group */
    LOG_GROUP_DRV_VUSB,
    /** EM group. */
    LOG_GROUP_EM,
    /** FTM group. */
    LOG_GROUP_FTM,
    /** GMM group. */
    LOG_GROUP_GMM,
    /** Guest control. */
    LOG_GROUP_GUEST_CONTROL,
    /** GUI group. */
    LOG_GROUP_GUI,
    /** GVMM group. */
    LOG_GROUP_GVMM,
    /** HGCM group */
    LOG_GROUP_HGCM,
    /** HGSMI group */
    LOG_GROUP_HGSMI,
    /** HM group. */
    LOG_GROUP_HM,
    /** IEM group. */
    LOG_GROUP_IEM,
    /** IOM group. */
    LOG_GROUP_IOM,
    /** XPCOM IPC group. */
    LOG_GROUP_IPC,
    /** Main group. */
    LOG_GROUP_MAIN,
    /** Main group, IAdditionsFacility. */
    LOG_GROUP_MAIN_ADDITIONSFACILITY,
    /** Main group, IAdditionsStateChangedEvent. */
    LOG_GROUP_MAIN_ADDITIONSSTATECHANGEDEVENT,
    /** Main group, IAppliance. */
    LOG_GROUP_MAIN_APPLIANCE,
    /** Main group, IAudioAdapter. */
    LOG_GROUP_MAIN_AUDIOADAPTER,
    /** Main group, IBandwidthControl. */
    LOG_GROUP_MAIN_BANDWIDTHCONTROL,
    /** Main group, IBandwidthGroup. */
    LOG_GROUP_MAIN_BANDWIDTHGROUP,
    /** Main group, IBandwidthGroupChangedEvent. */
    LOG_GROUP_MAIN_BANDWIDTHGROUPCHANGEDEVENT,
    /** Main group, IBIOSSettings. */
    LOG_GROUP_MAIN_BIOSSETTINGS,
    /** Main group, ICanShowWindowEvent. */
    LOG_GROUP_MAIN_CANSHOWWINDOWEVENT,
    /** Main group, IClipboardModeChangedEvent. */
    LOG_GROUP_MAIN_CLIPBOARDMODECHANGEDEVENT,
    /** Main group, IConsole. */
    LOG_GROUP_MAIN_CONSOLE,
    /** Main group, ICPUChangedEvent. */
    LOG_GROUP_MAIN_CPUCHANGEDEVENT,
    /** Main group, ICPUExecutionCapChangedEvent. */
    LOG_GROUP_MAIN_CPUEXECUTIONCAPCHANGEDEVENT,
    /** Main group, IDHCPServer. */
    LOG_GROUP_MAIN_DHCPSERVER,
    /** Main group, IDirectory. */
    LOG_GROUP_MAIN_DIRECTORY,
    /** Main group, IDisplay. */
    LOG_GROUP_MAIN_DISPLAY,
    /** Main group, IDragAndDropModeChangedEvent. */
    LOG_GROUP_MAIN_DRAGANDDROPMODECHANGEDEVENT,
    /** Main group, IEvent. */
    LOG_GROUP_MAIN_EVENT,
    /** Main group, IEventListener. */
    LOG_GROUP_MAIN_EVENTLISTENER,
    /** Main group, IEventSource. */
    LOG_GROUP_MAIN_EVENTSOURCE,
    /** Main group, IEventSourceChangedEvent. */
    LOG_GROUP_MAIN_EVENTSOURCECHANGEDEVENT,
    /** Main group, IExtPack. */
    LOG_GROUP_MAIN_EXTPACK,
    /** Main group, IExtPackBase. */
    LOG_GROUP_MAIN_EXTPACKBASE,
    /** Main group, IExtPackFile. */
    LOG_GROUP_MAIN_EXTPACKFILE,
    /** Main group, IExtPackManager. */
    LOG_GROUP_MAIN_EXTPACKMANAGER,
    /** Main group, IExtPackPlugIn. */
    LOG_GROUP_MAIN_EXTPACKPLUGIN,
    /** Main group, IExtraDataCanChangeEvent. */
    LOG_GROUP_MAIN_EXTRADATACANCHANGEEVENT,
    /** Main group, IExtraDataChangedEvent. */
    LOG_GROUP_MAIN_EXTRADATACHANGEDEVENT,
    /** Main group, IFile. */
    LOG_GROUP_MAIN_FILE,
    /** Main group, IFramebuffer. */
    LOG_GROUP_MAIN_FRAMEBUFFER,
    /** Main group, IFramebufferOverlay. */
    LOG_GROUP_MAIN_FRAMEBUFFEROVERLAY,
    /** Main group, IFsObjInfo. */
    LOG_GROUP_MAIN_FSOBJINFO,
    /** Main group, IGuest. */
    LOG_GROUP_MAIN_GUEST,
    /** Main group, IGuestDirectory. */
    LOG_GROUP_MAIN_GUESTDIRECTORY,
    /** Main group, IGuestErrorInfo. */
    LOG_GROUP_MAIN_GUESTERRORINFO,
    /** Main group, IGuestFile. */
    LOG_GROUP_MAIN_GUESTFILE,
    /** Main group, IGuestFileEvent. */
    LOG_GROUP_MAIN_GUESTFILEEVENT,
    /** Main group, IGuestFileIOEvent. */
    LOG_GROUP_MAIN_GUESTFILEIOEVENT,
    /** Main group, IGuestFileOffsetChangedEvent. */
    LOG_GROUP_MAIN_GUESTFILEOFFSETCHANGEDEVENT,
    /** Main group, IGuestFileReadEvent. */
    LOG_GROUP_MAIN_GUESTFILEREADEVENT,
    /** Main group, IGuestFileRegisteredEvent. */
    LOG_GROUP_MAIN_GUESTFILEREGISTEREDEVENT,
    /** Main group, IGuestFileStateChangedEvent. */
    LOG_GROUP_MAIN_GUESTFILESTATECHANGEDEVENT,
    /** Main group, IGuestFileWriteEvent. */
    LOG_GROUP_MAIN_GUESTFILEWRITEEVENT,
    /** Main group, IGuestFsObjInfo. */
    LOG_GROUP_MAIN_GUESTFSOBJINFO,
    /** Main group, IGuestKeyboardEvent. */
    LOG_GROUP_MAIN_GUESTKEYBOARDEVENT,
    /** Main group, IGuestMonitorChangedEvent. */
    LOG_GROUP_MAIN_GUESTMONITORCHANGEDEVENT,
    /** Main group, IGuestMouseEvent. */
    LOG_GROUP_MAIN_GUESTMOUSEEVENT,
    /** Main group, IGuestOSType. */
    LOG_GROUP_MAIN_GUESTOSTYPE,
    /** Main group, IGuestProcess. */
    LOG_GROUP_MAIN_GUESTPROCESS,
    /** Main group, IGuestProcessEvent. */
    LOG_GROUP_MAIN_GUESTPROCESSEVENT,
    /** Main group, IGuestProcessInputNotifyEvent. */
    LOG_GROUP_MAIN_GUESTPROCESSINPUTNOTIFYEVENT,
    /** Main group, IGuestProcessIOEvent. */
    LOG_GROUP_MAIN_GUESTPROCESSIOEVENT,
    /** Main group, IGuestProcessOutputEvent. */
    LOG_GROUP_MAIN_GUESTPROCESSOUTPUTEVENT,
    /** Main group, IGuestProcessRegisteredEvent. */
    LOG_GROUP_MAIN_GUESTPROCESSREGISTEREDEVENT,
    /** Main group, IGuestProcessStateChangedEvent. */
    LOG_GROUP_MAIN_GUESTPROCESSSTATECHANGEDEVENT,
    /** Main group, IGuestPropertyChangedEvent. */
    LOG_GROUP_MAIN_GUESTPROPERTYCHANGEDEVENT,
    /** Main group, IGuestSession. */
    LOG_GROUP_MAIN_GUESTSESSION,
    /** Main group, IGuestSessionEvent. */
    LOG_GROUP_MAIN_GUESTSESSIONEVENT,
    /** Main group, IGuestSessionRegisteredEvent. */
    LOG_GROUP_MAIN_GUESTSESSIONREGISTEREDEVENT,
    /** Main group, IGuestSessionStateChangedEvent. */
    LOG_GROUP_MAIN_GUESTSESSIONSTATECHANGEDEVENT,
    /** Main group, IHost. */
    LOG_GROUP_MAIN_HOST,
    /** Main group, IHostNetworkInterface. */
    LOG_GROUP_MAIN_HOSTNETWORKINTERFACE,
    /** Main group, IHostPCIDevicePlugEvent. */
    LOG_GROUP_MAIN_HOSTPCIDEVICEPLUGEVENT,
    /** Main group, IHostUSBDevice. */
    LOG_GROUP_MAIN_HOSTUSBDEVICE,
    /** Main group, IHostUSBDeviceFilter. */
    LOG_GROUP_MAIN_HOSTUSBDEVICEFILTER,
    /** Main group, IInternalMachineControl. */
    LOG_GROUP_MAIN_INTERNALMACHINECONTROL,
    /** Main group, IInternalSessionControl. */
    LOG_GROUP_MAIN_INTERNALSESSIONCONTROL,
    /** Main group, IKeyboard. */
    LOG_GROUP_MAIN_KEYBOARD,
    /** Main group, IKeyboardLedsChangedEvent. */
    LOG_GROUP_MAIN_KEYBOARDLEDSCHANGEDEVENT,
    /** Main group, IMachine. */
    LOG_GROUP_MAIN_MACHINE,
    /** Main group, IMachineDataChangedEvent. */
    LOG_GROUP_MAIN_MACHINEDATACHANGEDEVENT,
    /** Main group, IMachineDebugger. */
    LOG_GROUP_MAIN_MACHINEDEBUGGER,
    /** Main group, IMachineEvent. */
    LOG_GROUP_MAIN_MACHINEEVENT,
    /** Main group, IMachineRegisteredEvent. */
    LOG_GROUP_MAIN_MACHINEREGISTEREDEVENT,
    /** Main group, IMachineStateChangedEvent. */
    LOG_GROUP_MAIN_MACHINESTATECHANGEDEVENT,
    /** Main group, IMedium. */
    LOG_GROUP_MAIN_MEDIUM,
    /** Main group, IMediumAttachment. */
    LOG_GROUP_MAIN_MEDIUMATTACHMENT,
    /** Main group, IMediumChangedEvent. */
    LOG_GROUP_MAIN_MEDIUMCHANGEDEVENT,
    /** Main group, IMediumFormat. */
    LOG_GROUP_MAIN_MEDIUMFORMAT,
    /** Main group, IMediumRegisteredEvent. */
    LOG_GROUP_MAIN_MEDIUMREGISTEREDEVENT,
    /** Main group, IMouse. */
    LOG_GROUP_MAIN_MOUSE,
    /** Main group, IMouseCapabilityChangedEvent. */
    LOG_GROUP_MAIN_MOUSECAPABILITYCHANGEDEVENT,
    /** Main group, IMousePointerShapeChangedEvent. */
    LOG_GROUP_MAIN_MOUSEPOINTERSHAPECHANGEDEVENT,
    /** Main group, INATEngine. */
    LOG_GROUP_MAIN_NATENGINE,
    /** Main group, INATNetwork. */
    LOG_GROUP_MAIN_NATNETWORK,
    /** Main group, INATNetworkAlterEvent. */
    LOG_GROUP_MAIN_NATNETWORKALTEREVENT,
    /** Main group, INATNetworkChangedEvent. */
    LOG_GROUP_MAIN_NATNETWORKCHANGEDEVENT,
    /** Main group, INATNetworkCreationDeletionEvent. */
    LOG_GROUP_MAIN_NATNETWORKCREATIONDELETIONEVENT,
    /** Main group, INATNetworkPortForwardEvent. */
    LOG_GROUP_MAIN_NATNETWORKPORTFORWARDEVENT,
    /** Main group, INATNetworkSettingEvent. */
    LOG_GROUP_MAIN_NATNETWORKSETTINGEVENT,
    /** Main group, INATNetworkStartStopEvent. */
    LOG_GROUP_MAIN_NATNETWORKSTARTSTOPEVENT,
    /** Main group, INATRedirectEvent. */
    LOG_GROUP_MAIN_NATREDIRECTEVENT,
    /** Main group, INetworkAdapter. */
    LOG_GROUP_MAIN_NETWORKADAPTER,
    /** Main group, INetworkAdapterChangedEvent. */
    LOG_GROUP_MAIN_NETWORKADAPTERCHANGEDEVENT,
    /** Main group, IParallelPort. */
    LOG_GROUP_MAIN_PARALLELPORT,
    /** Main group, IParallelPortChangedEvent. */
    LOG_GROUP_MAIN_PARALLELPORTCHANGEDEVENT,
    /** Main group, IPCIAddress. */
    LOG_GROUP_MAIN_PCIADDRESS,
    /** Main group, IPCIDeviceAttachment. */
    LOG_GROUP_MAIN_PCIDEVICEATTACHMENT,
    /** Main group, IPerformanceCollector. */
    LOG_GROUP_MAIN_PERFORMANCECOLLECTOR,
    /** Main group, IPerformanceMetric. */
    LOG_GROUP_MAIN_PERFORMANCEMETRIC,
    /** Main group, IProcess. */
    LOG_GROUP_MAIN_PROCESS,
    /** Main group, IProgress. */
    LOG_GROUP_MAIN_PROGRESS,
    /** Main group, IReusableEvent. */
    LOG_GROUP_MAIN_REUSABLEEVENT,
    /** Main group, IRuntimeErrorEvent. */
    LOG_GROUP_MAIN_RUNTIMEERROREVENT,
    /** Main group, ISerialPort. */
    LOG_GROUP_MAIN_SERIALPORT,
    /** Main group, ISerialPortChangedEvent. */
    LOG_GROUP_MAIN_SERIALPORTCHANGEDEVENT,
    /** Main group, ISession. */
    LOG_GROUP_MAIN_SESSION,
    /** Main group, ISessionStateChangedEvent. */
    LOG_GROUP_MAIN_SESSIONSTATECHANGEDEVENT,
    /** Main group, ISharedFolder. */
    LOG_GROUP_MAIN_SHAREDFOLDER,
    /** Main group, ISharedFolderChangedEvent. */
    LOG_GROUP_MAIN_SHAREDFOLDERCHANGEDEVENT,
    /** Main group, IShowWindowEvent. */
    LOG_GROUP_MAIN_SHOWWINDOWEVENT,
    /** Main group, ISnapshot. */
    LOG_GROUP_MAIN_SNAPSHOT,
    /** Main group, ISnapshotChangedEvent. */
    LOG_GROUP_MAIN_SNAPSHOTCHANGEDEVENT,
    /** Main group, ISnapshotDeletedEvent. */
    LOG_GROUP_MAIN_SNAPSHOTDELETEDEVENT,
    /** Main group, ISnapshotEvent. */
    LOG_GROUP_MAIN_SNAPSHOTEVENT,
    /** Main group, ISnapshotTakenEvent. */
    LOG_GROUP_MAIN_SNAPSHOTTAKENEVENT,
    /** Main group, IStateChangedEvent. */
    LOG_GROUP_MAIN_STATECHANGEDEVENT,
    /** Main group, IStorageController. */
    LOG_GROUP_MAIN_STORAGECONTROLLER,
    /** Main group, IStorageControllerChangedEvent. */
    LOG_GROUP_MAIN_STORAGECONTROLLERCHANGEDEVENT,
    /** Main group, IStorageDeviceChangedEvent. */
    LOG_GROUP_MAIN_STORAGEDEVICECHANGEDEVENT,
    /** Main group, ISystemProperties. */
    LOG_GROUP_MAIN_SYSTEMPROPERTIES,
    /** Main group, IUSBController. */
    LOG_GROUP_MAIN_USBCONTROLLER,
    /** Main group, IUSBControllerChangedEvent. */
    LOG_GROUP_MAIN_USBCONTROLLERCHANGEDEVENT,
    /** Main group, IUSBDevice. */
    LOG_GROUP_MAIN_USBDEVICE,
    /** Main group, IUSBDeviceFilter. */
    LOG_GROUP_MAIN_USBDEVICEFILTER,
    /** Main group, IUSBDeviceStateChangedEvent. */
    LOG_GROUP_MAIN_USBDEVICESTATECHANGEDEVENT,
    /** Main group, IVBoxSVCAvailabilityChangedEvent. */
    LOG_GROUP_MAIN_VBOXSVCAVAILABILITYCHANGEDEVENT,
    /** Main group, IVetoEvent. */
    LOG_GROUP_MAIN_VETOEVENT,
    /** Main group, IVFSExplorer. */
    LOG_GROUP_MAIN_VFSEXPLORER,
    /** Main group, IVideoCaptureChangedEvent. */
    LOG_GROUP_MAIN_VIDEOCAPTURECHANGEDEVENT,
    /** Main group, IVirtualBox. */
    LOG_GROUP_MAIN_VIRTUALBOX,
    /** Main group, IVirtualBoxClient. */
    LOG_GROUP_MAIN_VIRTUALBOXCLIENT,
    /** Main group, IVirtualSystemDescription. */
    LOG_GROUP_MAIN_VIRTUALSYSTEMDESCRIPTION,
    /** Main group, IVRDEServer. */
    LOG_GROUP_MAIN_VRDESERVER,
    /** Main group, IVRDEServerChangedEvent. */
    LOG_GROUP_MAIN_VRDESERVERCHANGEDEVENT,
    /** Main group, IVRDEServerInfo. */
    LOG_GROUP_MAIN_VRDESERVERINFO,
    /** Main group, IVRDEServerInfoChangedEvent. */
    LOG_GROUP_MAIN_VRDESERVERINFOCHANGEDEVENT,
    /** Misc. group intended for external use only. */
    LOG_GROUP_MISC,
    /** MM group. */
    LOG_GROUP_MM,
    /** MM group. */
    LOG_GROUP_MM_HEAP,
    /** MM group. */
    LOG_GROUP_MM_HYPER,
    /** MM Hypervisor Heap group. */
    LOG_GROUP_MM_HYPER_HEAP,
    /** MM Physical/Ram group. */
    LOG_GROUP_MM_PHYS,
    /** MM Page pool group. */
    LOG_GROUP_MM_POOL,
    /** The NAT service group */
    LOG_GROUP_NAT_SERVICE,
    /** The network adaptor driver group. */
    LOG_GROUP_NET_ADP_DRV,
    /** The network filter driver group. */
    LOG_GROUP_NET_FLT_DRV,
    /** The common network service group */
    LOG_GROUP_NET_SERVICE,
    /** Network traffic shaper driver group. */
    LOG_GROUP_NET_SHAPER,
    /** PATM group. */
    LOG_GROUP_PATM,
    /** PDM group. */
    LOG_GROUP_PDM,
    /** PDM Async completion group. */
    LOG_GROUP_PDM_ASYNC_COMPLETION,
    /** PDM Block cache group. */
    LOG_GROUP_PDM_BLK_CACHE,
    /** PDM Device group. */
    LOG_GROUP_PDM_DEVICE,
    /** PDM Driver group. */
    LOG_GROUP_PDM_DRIVER,
    /** PDM Loader group. */
    LOG_GROUP_PDM_LDR,
    /** PDM Loader group. */
    LOG_GROUP_PDM_QUEUE,
    /** PGM group. */
    LOG_GROUP_PGM,
    /** PGM dynamic mapping group. */
    LOG_GROUP_PGM_DYNMAP,
    /** PGM physical group. */
    LOG_GROUP_PGM_PHYS,
    /** PGM physical access group. */
    LOG_GROUP_PGM_PHYS_ACCESS,
    /** PGM shadow page pool group. */
    LOG_GROUP_PGM_POOL,
    /** PGM shared paging group. */
    LOG_GROUP_PGM_SHARED,
    /** REM group. */
    LOG_GROUP_REM,
    /** REM disassembly handler group. */
    LOG_GROUP_REM_DISAS,
    /** REM access handler group. */
    LOG_GROUP_REM_HANDLER,
    /** REM I/O port access group. */
    LOG_GROUP_REM_IOPORT,
    /** REM MMIO access group. */
    LOG_GROUP_REM_MMIO,
    /** REM Printf. */
    LOG_GROUP_REM_PRINTF,
    /** REM running group. */
    LOG_GROUP_REM_RUN,
    /** SELM group. */
    LOG_GROUP_SELM,
    /** Shared clipboard host service group. */
    LOG_GROUP_SHARED_CLIPBOARD,
    /** Chromium OpenGL host service group. */
    LOG_GROUP_SHARED_CROPENGL,
    /** Shared folders host service group. */
    LOG_GROUP_SHARED_FOLDERS,
    /** OpenGL host service group. */
    LOG_GROUP_SHARED_OPENGL,
    /** The internal networking service group. */
    LOG_GROUP_SRV_INTNET,
    /** SSM group. */
    LOG_GROUP_SSM,
    /** STAM group. */
    LOG_GROUP_STAM,
    /** SUP group. */
    LOG_GROUP_SUP,
    /** SUPport driver group. */
    LOG_GROUP_SUP_DRV,
    /** TM group. */
    LOG_GROUP_TM,
    /** TRPM group. */
    LOG_GROUP_TRPM,
    /** USB cardreader group. */
    LOG_GROUP_USB_CARDREADER,
    /** USB driver group. */
    LOG_GROUP_USB_DRV,
    /** USBFilter group. */
    LOG_GROUP_USB_FILTER,
    /** USB keyboard device group. */
    LOG_GROUP_USB_KBD,
    /** USB mouse/tablet device group. */
    LOG_GROUP_USB_MOUSE,
    /** MSD USB device group. */
    LOG_GROUP_USB_MSD,
    /** USB webcam. */
    LOG_GROUP_USB_WEBCAM,
    /** VBox Guest Additions Driver (VBoxGuest). */
    LOG_GROUP_VBGD,
    /** VBox Guest Additions Library. */
    LOG_GROUP_VBGL,
    /** Generic virtual disk layer. */
    LOG_GROUP_VD,
    /** DMG virtual disk backend. */
    LOG_GROUP_VD_DMG,
    /** iSCSI virtual disk backend. */
    LOG_GROUP_VD_ISCSI,
    /** Parallels HDD virtual disk backend. */
    LOG_GROUP_VD_PARALLELS,
    /** QCOW virtual disk backend. */
    LOG_GROUP_VD_QCOW,
    /** QED virtual disk backend. */
    LOG_GROUP_VD_QED,
    /** Raw virtual disk backend. */
    LOG_GROUP_VD_RAW,
    /** VDI virtual disk backend. */
    LOG_GROUP_VD_VDI,
    /** VHD virtual disk backend. */
    LOG_GROUP_VD_VHD,
    /** VMDK virtual disk backend. */
    LOG_GROUP_VD_VMDK,
    /** VM group. */
    LOG_GROUP_VM,
    /** VMM group. */
    LOG_GROUP_VMM,
    /** VRDE group */
    LOG_GROUP_VRDE,
    /** VRDP group */
    LOG_GROUP_VRDP,
    /** VSCSI group */
    LOG_GROUP_VSCSI,
    /** Webservice group. */
    LOG_GROUP_WEBSERVICE
    /* !!!ALPHABETICALLY!!! */
} VBOX_LOGGROUP;


/** @def VBOX_LOGGROUP_NAMES
 * VirtualBox Logging group names.
 *
 * Must correspond 100% to LOGGROUP!
 * Don't forget commas!
 *
 * @remark It should be pretty obvious, but just to have
 *         mentioned it, the values are sorted alphabetically (using the
 *         english alphabet) except for _DEFAULT which is always first.
 *
 *         If anyone might be wondering what the alphabet looks like:
 *              a b c d e f g h i j k l m n o p q r s t u v w x y z
 */
#define VBOX_LOGGROUP_NAMES \
{                   \
    RT_LOGGROUP_NAMES, \
    "DEFAULT",      \
    "AUTOLOGON",    \
    "CFGM",         \
    "CPUM",         \
    "CSAM",         \
    "DBGC",         \
    "DBGF",         \
    "DBGF_INFO",    \
    "DBGG",         \
    "DEV",          \
    "DEV_ACPI",     \
    "DEV_AHCI",     \
    "DEV_APIC",     \
    "DEV_AUDIO",    \
    "DEV_BUSLOGIC", \
    "DEV_DMA",      \
    "DEV_E1000",    \
    "DEV_EFI",      \
    "DEV_FDC",      \
    "DEV_HPET",     \
    "DEV_IDE",      \
    "DEV_INIP",     \
    "DEV_KBD",      \
    "DEV_LPC",      \
    "DEV_LSILOGICSCSI", \
    "DEV_NE2000",   \
    "DEV_PARALLEL", \
    "DEV_PC",       \
    "DEV_PC_ARCH",  \
    "DEV_PC_BIOS",  \
    "DEV_PCI",      \
    "DEV_PCI_RAW",  \
    "DEV_PCNET",    \
    "DEV_PIC",      \
    "DEV_PIT",      \
    "DEV_RTC",      \
    "DEV_SERIAL",   \
    "DEV_SMC",      \
    "DEV_USB",      \
    "DEV_VGA",      \
    "DEV_VIRTIO",   \
    "DEV_VIRTIO_NET", \
    "DEV_VMM",      \
    "DEV_VMM_BACKDOOR", \
    "DEV_VMM_STDERR", \
    "DIS",          \
    "DRV",          \
    "DRV_ACPI",     \
    "DRV_BLOCK",    \
    "DRV_CHAR",     \
    "DRV_DISK_INTEGRITY", \
    "DRV_DISPLAY",  \
    "DRV_FLOPPY",   \
    "DRV_HOST_BASE", \
    "DRV_HOST_DVD", \
    "DRV_HOST_FLOPPY", \
    "DRV_HOST_PARALLEL", \
    "DRV_HOST_SERIAL", \
    "DRV_INTNET",   \
    "DRV_ISO",      \
    "DRV_KBD_QUEUE", \
    "DRV_LWIP",     \
    "DRV_MINIPORT", \
    "DRV_MOUSE",    \
    "DRV_MOUSE_QUEUE", \
    "DRV_NAMEDPIPE", \
    "DRV_NAT",      \
    "DRV_RAW_IMAGE", \
    "DRV_SCSI",     \
    "DRV_SCSIHOST", \
    "DRV_TRANSPORT_ASYNC", \
    "DRV_TUN",      \
    "DRV_UDPTUNNEL", \
    "DRV_USBPROXY", \
    "DRV_VBOXHDD",  \
    "DRV_VD",       \
    "DRV_VSWITCH",  \
    "DRV_VUSB",     \
    "EM",           \
    "FTM",          \
    "GMM",          \
    "GUEST_CONTROL", \
    "GUI",          \
    "GVMM",         \
    "HGCM",         \
    "HGSMI",        \
    "HM",           \
    "IEM",          \
    "IOM",          \
    "IPC",          \
    "MAIN",         \
    "MAIN_ADDITIONSFACILITY", \
    "MAIN_ADDITIONSSTATECHANGEDEVENT", \
    "MAIN_APPLIANCE", \
    "MAIN_AUDIOADAPTER", \
    "MAIN_BANDWIDTHCONTROL", \
    "MAIN_BANDWIDTHGROUP", \
    "MAIN_BANDWIDTHGROUPCHANGEDEVENT", \
    "MAIN_BIOSSETTINGS", \
    "MAIN_CANSHOWWINDOWEVENT", \
    "MAIN_CLIPBOARDMODECHANGEDEVENT", \
    "MAIN_CONSOLE", \
    "MAIN_CPUCHANGEDEVENT", \
    "MAIN_CPUEXECUTIONCAPCHANGEDEVENT", \
    "MAIN_DHCPSERVER", \
    "MAIN_DIRECTORY", \
    "MAIN_DISPLAY", \
    "MAIN_DRAGANDDROPMODECHANGEDEVENT", \
    "MAIN_EVENT",   \
    "MAIN_EVENTLISTENER", \
    "MAIN_EVENTSOURCE", \
    "MAIN_EVENTSOURCECHANGEDEVENT", \
    "MAIN_EXTPACK", \
    "MAIN_EXTPACKBASE", \
    "MAIN_EXTPACKFILE", \
    "MAIN_EXTPACKMANAGER", \
    "MAIN_EXTPACKPLUGIN", \
    "MAIN_EXTRADATACANCHANGEEVENT", \
    "MAIN_EXTRADATACHANGEDEVENT", \
    "MAIN_FILE",    \
    "MAIN_FRAMEBUFFER", \
    "MAIN_FRAMEBUFFEROVERLAY", \
    "MAIN_FSOBJINFO", \
    "MAIN_GUEST",   \
    "MAIN_GUESTDIRECTORY", \
    "MAIN_GUESTERRORINFO", \
    "MAIN_GUESTFILE", \
    "MAIN_GUESTFILEEVENT", \
    "MAIN_GUESTFILEIOEVENT", \
    "MAIN_GUESTFILEOFFSETCHANGEDEVENT", \
    "MAIN_GUESTFILEREADEVENT", \
    "MAIN_GUESTFILEREGISTEREDEVENT", \
    "MAIN_GUESTFILESTATECHANGEDEVENT", \
    "MAIN_GUESTFILEWRITEEVENT", \
    "MAIN_GUESTFSOBJINFO", \
    "MAIN_GUESTKEYBOARDEVENT", \
    "MAIN_GUESTMONITORCHANGEDEVENT", \
    "MAIN_GUESTMOUSEEVENT", \
    "MAIN_GUESTOSTYPE", \
    "MAIN_GUESTPROCESS", \
    "MAIN_GUESTPROCESSEVENT", \
    "MAIN_GUESTPROCESSINPUTNOTIFYEVENT", \
    "MAIN_GUESTPROCESSIOEVENT", \
    "MAIN_GUESTPROCESSOUTPUTEVENT", \
    "MAIN_GUESTPROCESSREGISTEREDEVENT", \
    "MAIN_GUESTPROCESSSTATECHANGEDEVENT", \
    "MAIN_GUESTPROPERTYCHANGEDEVENT", \
    "MAIN_GUESTSESSION", \
    "MAIN_GUESTSESSIONEVENT", \
    "MAIN_GUESTSESSIONREGISTEREDEVENT", \
    "MAIN_GUESTSESSIONSTATECHANGEDEVENT", \
    "MAIN_HOST",    \
    "MAIN_HOSTNETWORKINTERFACE", \
    "MAIN_HOSTPCIDEVICEPLUGEVENT", \
    "MAIN_HOSTUSBDEVICE", \
    "MAIN_HOSTUSBDEVICEFILTER", \
    "MAIN_INTERNALMACHINECONTROL", \
    "MAIN_INTERNALSESSIONCONTROL", \
    "MAIN_KEYBOARD", \
    "MAIN_KEYBOARDLEDSCHANGEDEVENT", \
    "MAIN_MACHINE", \
    "MAIN_MACHINEDATACHANGEDEVENT", \
    "MAIN_MACHINEDEBUGGER", \
    "MAIN_MACHINEEVENT", \
    "MAIN_MACHINEREGISTEREDEVENT", \
    "MAIN_MACHINESTATECHANGEDEVENT", \
    "MAIN_MEDIUM",  \
    "MAIN_MEDIUMATTACHMENT", \
    "MAIN_MEDIUMCHANGEDEVENT", \
    "MAIN_MEDIUMFORMAT", \
    "MAIN_MEDIUMREGISTEREDEVENT", \
    "MAIN_MOUSE",   \
    "MAIN_MOUSECAPABILITYCHANGEDEVENT", \
    "MAIN_MOUSEPOINTERSHAPECHANGEDEVENT", \
    "MAIN_NATENGINE", \
    "MAIN_NATNETWORK", \
    "MAIN_NATNETWORKALTEREVENT", \
    "MAIN_NATNETWORKCHANGEDEVENT", \
    "MAIN_NATNETWORKCREATIONDELETIONEVENT", \
    "MAIN_NATNETWORKPORTFORWARDEVENT", \
    "MAIN_NATNETWORKSETTINGEVENT", \
    "MAIN_NATNETWORKSTARTSTOPEVENT", \
    "MAIN_NATREDIRECTEVENT", \
    "MAIN_NETWORKADAPTER", \
    "MAIN_NETWORKADAPTERCHANGEDEVENT", \
    "MAIN_PARALLELPORT", \
    "MAIN_PARALLELPORTCHANGEDEVENT", \
    "MAIN_PCIADDRESS", \
    "MAIN_PCIDEVICEATTACHMENT", \
    "MAIN_PERFORMANCECOLLECTOR", \
    "MAIN_PERFORMANCEMETRIC", \
    "MAIN_PROCESS", \
    "MAIN_PROGRESS", \
    "MAIN_REUSABLEEVENT", \
    "MAIN_RUNTIMEERROREVENT", \
    "MAIN_SERIALPORT", \
    "MAIN_SERIALPORTCHANGEDEVENT", \
    "MAIN_SESSION", \
    "MAIN_SESSIONSTATECHANGEDEVENT", \
    "MAIN_SHAREDFOLDER", \
    "MAIN_SHAREDFOLDERCHANGEDEVENT", \
    "MAIN_SHOWWINDOWEVENT", \
    "MAIN_SNAPSHOT", \
    "MAIN_SNAPSHOTCHANGEDEVENT", \
    "MAIN_SNAPSHOTDELETEDEVENT", \
    "MAIN_SNAPSHOTEVENT", \
    "MAIN_SNAPSHOTTAKENEVENT", \
    "MAIN_STATECHANGEDEVENT", \
    "MAIN_STORAGECONTROLLER", \
    "MAIN_STORAGECONTROLLERCHANGEDEVENT", \
    "MAIN_STORAGEDEVICECHANGEDEVENT", \
    "MAIN_SYSTEMPROPERTIES", \
    "MAIN_USBCONTROLLER", \
    "MAIN_USBCONTROLLERCHANGEDEVENT", \
    "MAIN_USBDEVICE", \
    "MAIN_USBDEVICEFILTER", \
    "MAIN_USBDEVICESTATECHANGEDEVENT", \
    "MAIN_VBOXSVCAVAILABILITYCHANGEDEVENT", \
    "MAIN_VETOEVENT", \
    "MAIN_VFSEXPLORER", \
    "MAIN_VIDEOCAPTURECHANGEDEVENT", \
    "MAIN_VIRTUALBOX", \
    "MAIN_VIRTUALBOXCLIENT", \
    "MAIN_VIRTUALSYSTEMDESCRIPTION", \
    "MAIN_VRDESERVER", \
    "MAIN_VRDESERVERCHANGEDEVENT", \
    "MAIN_VRDESERVERINFO", \
    "MAIN_VRDESERVERINFOCHANGEDEVENT", \
    "MISC",         \
    "MM",           \
    "MM_HEAP",      \
    "MM_HYPER",     \
    "MM_HYPER_HEAP",\
    "MM_PHYS",      \
    "MM_POOL",      \
    "NAT_SERVICE",  \
    "NET_ADP_DRV",  \
    "NET_FLT_DRV",  \
    "NET_SERVICE",  \
    "NET_SHAPER",   \
    "PATM",         \
    "PDM",          \
    "PDM_ASYNC_COMPLETION", \
    "PDM_BLK_CACHE", \
    "PDM_DEVICE",   \
    "PDM_DRIVER",   \
    "PDM_LDR",      \
    "PDM_QUEUE",    \
    "PGM",          \
    "PGM_DYNMAP",   \
    "PGM_PHYS",     \
    "PGM_PHYS_ACCESS",\
    "PGM_POOL",     \
    "PGM_SHARED",   \
    "REM",          \
    "REM_DISAS",    \
    "REM_HANDLER",  \
    "REM_IOPORT",   \
    "REM_MMIO",     \
    "REM_PRINTF",   \
    "REM_RUN",      \
    "SELM",         \
    "SHARED_CLIPBOARD",\
    "SHARED_CROPENGL",\
    "SHARED_FOLDERS",\
    "SHARED_OPENGL",\
    "SRV_INTNET",   \
    "SSM",          \
    "STAM",         \
    "SUP",          \
    "SUP_DRV",      \
    "TM",           \
    "TRPM",         \
    "USB_CARDREADER",\
    "USB_DRV",      \
    "USB_FILTER",   \
    "USB_KBD",      \
    "USB_MOUSE",    \
    "USB_MSD",      \
    "USB_WEBCAM",   \
    "VBGD",         \
    "VBGL",         \
    "VD",           \
    "VD_DMG",       \
    "VD_ISCSI",     \
    "VD_PARALLELS", \
    "VD_QCOW",      \
    "VD_QED",       \
    "VD_RAW",       \
    "VD_VDI",       \
    "VD_VHD",       \
    "VD_VMDK",      \
    "VM",           \
    "VMM",          \
    "VRDE",         \
    "VRDP",         \
    "VSCSI",        \
    "WEBSERVICE",   \
}

/** @} */
#endif
