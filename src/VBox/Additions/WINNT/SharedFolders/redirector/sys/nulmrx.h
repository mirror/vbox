/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 nulmrx.h

 Abstract:

 This header exports all symbols and definitions shared between
 user-mode clients of nulmrx and the driver itself.

 Notes:

 This module has been built and tested only in UNICODE environment

 --*/

#ifndef _MRX_VBOX_H_
#define _MRX_VBOX_H_

/** ULONG tags used by the FSD. */
#define TAG_PP                          'PfsV'
#define TAG_FCB                         'FfsV'
#define TAG_CCB                         'CfsV'
#define TAG_IRPCTX                      'IfsV'

// Device name for this driver
#define MRX_VBOX_DEVICE_NAME_A "VBoxMiniRdr"
#define MRX_VBOX_DEVICE_NAME_U L"VBoxMiniRdr"

// Provider name for this driver
#define MRX_VBOX_PROVIDER_NAME_A "VirtualBox Shared Folders"
#define MRX_VBOX_PROVIDER_NAME_U L"VirtualBox Shared Folders"

// File system name
#define MRX_VBOX_FILESYS_NAME_A "VBoxSharedFolderFS"
#define MRX_VBOX_FILESYS_NAME_U L"VBoxSharedFolderFS"

// The following constant defines the length of the above name.
#define MRX_VBOX_DEVICE_NAME_A_LENGTH (15)

// The following constant defines the path in the ob namespace
#define DD_MRX_VBOX_FS_DEVICE_NAME_U L"\\Device\\VBoxMiniRdr"

/* File system volume name prefix */
#define VBOX_VOLNAME_PREFIX     L"VBOX_"
#define VBOX_VOLNAME_PREFIX_SIZE        (sizeof(VBOX_VOLNAME_PREFIX) - sizeof(VBOX_VOLNAME_PREFIX[0]))

#ifndef MRX_VBOX_DEVICE_NAME
#define MRX_VBOX_DEVICE_NAME

//
//  The Devicename string required to access the nullmini device
//  from User-Mode. Clients should use DD_MRX_VBOX_USERMODE_DEV_NAME_U.
//
//  WARNING The next two strings must be kept in sync. Change one and you must
//  change the other. These strings have been chosen such that they are
//  unlikely to coincide with names of other drivers.
//
#define DD_MRX_VBOX_USERMODE_SHADOW_DEV_NAME_U     L"\\??\\VBoxMiniRdrDN"
#define DD_MRX_VBOX_USERMODE_DEV_NAME_U            L"\\\\.\\VBoxMiniRdrDN"

//
//  Prefix needed for disk filesystems
//
#define DD_MRX_VBOX_MINIRDR_PREFIX                 L"\\;E:"

#endif // MRX_VBOX_DEVICE_NAME
//
// BEGIN WARNING WARNING WARNING WARNING
//  The following are from the ddk include files and cannot be changed

#define FILE_DEVICE_NETWORK_FILE_SYSTEM 0x00000014 // from ddk\inc\ntddk.h
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0

// END WARNING WARNING WARNING WARNING

#define IOCTL_MRX_VBOX_BASE FILE_DEVICE_NETWORK_FILE_SYSTEM

#define _MRX_VBOX_CONTROL_CODE(request, method, access) \
                CTL_CODE(IOCTL_MRX_VBOX_BASE, request, method, access)

//
//  IOCTL codes supported by NullMini Device.
//

#define IOCTL_CODE_ADDCONN          100
#define IOCTL_CODE_GETCONN          101
#define IOCTL_CODE_DELCONN          102
#define IOCTL_CODE_GETLIST          103
#define IOCTL_CODE_GETGLOBALLIST    104
#define IOCTL_CODE_GETGLOBALCONN    105
#define IOCTL_CODE_START            106
#define IOCTL_CODE_STOP             107

//
//  Following is the IOCTL definition and associated structs.
//  for IOCTL_CODE_SAMPLE1
//
#define IOCTL_MRX_VBOX_ADDCONN          _MRX_VBOX_CONTROL_CODE(IOCTL_CODE_ADDCONN, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MRX_VBOX_GETCONN          _MRX_VBOX_CONTROL_CODE(IOCTL_CODE_GETCONN, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MRX_VBOX_DELCONN          _MRX_VBOX_CONTROL_CODE(IOCTL_CODE_DELCONN, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MRX_VBOX_GETLIST          _MRX_VBOX_CONTROL_CODE(IOCTL_CODE_GETLIST, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MRX_VBOX_GETGLOBALLIST    _MRX_VBOX_CONTROL_CODE(IOCTL_CODE_GETGLOBALLIST, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MRX_VBOX_GETGLOBALCONN    _MRX_VBOX_CONTROL_CODE(IOCTL_CODE_GETGLOBALCONN, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MRX_VBOX_START            _MRX_VBOX_CONTROL_CODE(IOCTL_CODE_START, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MRX_VBOX_STOP             _MRX_VBOX_CONTROL_CODE(IOCTL_CODE_STOP, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif // _MRX_VBOX_H_

