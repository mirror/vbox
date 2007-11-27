/** @file
 * VirtualBox - Logging.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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
    /** DMA Controller group. */
    LOG_GROUP_DEV_DMA,
    /** Gigabit Ethernet Device group. */
    LOG_GROUP_DEV_E1000,
    /** Floppy Controller Device group. */
    LOG_GROUP_DEV_FDC,
    /** IDE Device group. */
    LOG_GROUP_DEV_IDE,
    /** The internal networking IP stack Device group. */
    LOG_GROUP_DEV_INIP,
    /** KeyBoard Controller Device group. */
    LOG_GROUP_DEV_KBD,
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
    /** USB Device group. */
    LOG_GROUP_DEV_USB,
    /** VGA Device group. */
    LOG_GROUP_DEV_VGA,
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
    /** Floppy media driver group. */
    LOG_GROUP_DRV_FLOPPY,
    /** Host Base block driver group. */
    LOG_GROUP_DRV_HOST_BASE,
    /** Host DVD block driver group. */
    LOG_GROUP_DRV_HOST_DVD,
    /** Host floppy block driver group. */
    LOG_GROUP_DRV_HOST_FLOPPY,
    /** Host hard disk (raw partition) media driver group. */
    LOG_GROUP_DRV_HOST_HDD,
    /** Host Parallel Driver group */
    LOG_GROUP_DRV_HOST_PARALLEL,
    /** Host Serial Driver Group */
    LOG_GROUP_DRV_HOST_SERIAL,
    /** The internal networking transport driver group. */
    LOG_GROUP_DRV_INTNET,
    /** iSCSI Initiator driver group. */
    LOG_GROUP_DRV_ISCSI,
    /** iSCSI TCP transport driver group. */
    LOG_GROUP_DRV_ISCSI_TRANSPORT_TCP,
    /** ISO (CD/DVD) media driver group. */
    LOG_GROUP_DRV_ISO,
    /** Keyboard Queue driver group. */
    LOG_GROUP_DRV_KBD_QUEUE,
    /** lwIP IP stack driver group. */
    LOG_GROUP_DRV_LWIP,
    /** Mouse Queue driver group. */
    LOG_GROUP_DRV_MOUSE_QUEUE,
    /** Named Pipe stream driver group. */
    LOG_GROUP_DRV_NAMEDPIPE,
    /** NAT network transport driver group */
    LOG_GROUP_DRV_NAT,
    /** Raw image driver group */
    LOG_GROUP_DRV_RAW_IMAGE,
    /** TUN network transport driver group */
    LOG_GROUP_DRV_TUN,
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
    /** GMM group. */
    LOG_GROUP_GMM,
    /** GUI group. */
    LOG_GROUP_GUI,
    /** GVMM group. */
    LOG_GROUP_GVMM,
    /** HGCM group */
    LOG_GROUP_HGCM,
    /** HWACCM group. */
    LOG_GROUP_HWACCM,
    /** IOM group. */
    LOG_GROUP_IOM,
    /** XPCOM IPC group. */
    LOG_GROUP_IPC,
    /** Main group. */
    LOG_GROUP_MAIN,
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
    /** PATM group. */
    LOG_GROUP_PATM,
    /** PDM group. */
    LOG_GROUP_PDM,
    /** PDM Async completion group. */
    LOG_GROUP_PDM_ASYNC_COMPLETION,
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
    /** PGMCACHE group. */
    LOG_GROUP_PGMCACHE,
    /** PGM physical group. */
    LOG_GROUP_PGM_PHYS,
    /** PGM physical access group. */
    LOG_GROUP_PGM_PHYS_ACCESS,
    /** PGM shadow page pool group. */
    LOG_GROUP_PGM_POOL,
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
    /** TM group. */
    LOG_GROUP_TM,
    /** TRPM group. */
    LOG_GROUP_TRPM,
    /** Generic virtual disk layer. */
    LOG_GROUP_VD,
    /** VMDK virtual disk backend. */
    LOG_GROUP_VD_VMDK,
    /** VM group. */
    LOG_GROUP_VM,
    /** VMM group. */
    LOG_GROUP_VMM,
    /** VRDP group */
    LOG_GROUP_VRDP
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
    "DEV_DMA",      \
    "DEV_E1000",    \
    "DEV_FDC",      \
    "DEV_IDE",      \
    "DEV_INIP",     \
    "DEV_KBD",      \
    "DEV_NE2000",   \
    "DEV_PARALLEL", \
    "DEV_PC",       \
    "DEV_PC_ARCH",  \
    "DEV_PC_BIOS",  \
    "DEV_PCI",      \
    "DEV_PCNET",    \
    "DEV_PIC",      \
    "DEV_PIT",      \
    "DEV_RTC",      \
    "DEV_SERIAL",   \
    "DEV_USB",      \
    "DEV_VGA",      \
    "DEV_VMM",      \
    "DEV_VMM_BACKDOOR", \
    "DEV_VMM_STDERR",\
    "DIS",          \
    "DRV",          \
    "DRV_ACPI",     \
    "DRV_BLOCK",    \
    "DRV_CHAR",     \
    "DRV_FLOPPY",   \
    "DRV_HOST_BASE", \
    "DRV_HOST_DVD", \
    "DRV_HOST_FLOPPY", \
    "DRV_HOST_HDD", \
    "DRV_HOST_PARALLEL", \
    "DRV_HOST_SERIAL", \
    "DRV_INTNET",   \
    "DRV_ISCSI",    \
    "DRV_ISCSI_TRANSPORT_TCP", \
    "DRV_ISO",      \
    "DRV_KBD_QUEUE", \
    "DRV_LWIP",     \
    "DRV_MOUSE_QUEUE", \
    "DRV_NAMEDPIPE", \
    "DRV_NAT",      \
    "DRV_RAW_IMAGE", \
    "DRV_TUN",      \
    "DRV_USBPROXY", \
    "DRV_VBOXHDD",  \
    "DRV_VD",       \
    "DRV_VSWITCH",  \
    "DRV_VUSB",     \
    "EM",           \
    "GMM",          \
    "GUI",          \
    "GVMM",         \
    "HGCM",         \
    "HWACCM",       \
    "IOM",          \
    "IPC",          \
    "MAIN",         \
    "MISC",         \
    "MM",           \
    "MM_HEAP",      \
    "MM_HYPER",     \
    "MM_HYPER_HEAP",\
    "MM_PHYS",      \
    "MM_POOL",      \
    "PATM",         \
    "PDM",          \
    "PDM_ASYNC_COMPLETION", \
    "PDM_DEVICE",   \
    "PDM_DRIVER",   \
    "PDM_LDR",      \
    "PDM_QUEUE",    \
    "PGM",          \
    "PGMCACHE",     \
    "PGM_PHYS",     \
    "PGM_PHYS_ACCESS",\
    "PGM_POOL",     \
    "REM",          \
    "REM_DISAS",    \
    "REM_HANDLER",  \
    "REM_IOPORT",   \
    "REM_MMIO",     \
    "REM_PRINTF",   \
    "REM_RUN",      \
    "SELM",         \
    "SHARED_FOLDERS",\
    "SHARED_OPENGL",\
    "SRV_INTNET",   \
    "SSM",          \
    "STAM",         \
    "SUP",          \
    "TM",           \
    "TRPM",         \
    "VD",           \
    "VD_VMDK",      \
    "VM",           \
    "VMM",          \
    "VRDP",         \
}

/** @} */
#endif

