/* $Id$ */
/** @file
 * InitVariable.h
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @file

  Implment all four UEFI runtime variable services and
  install variable architeture protocol.

Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "Variable.h"


EFI_EVENT   mVirtualAddressChangeEvent = NULL;

#ifdef VBOX
# include <Library/PrintLib.h>
# include <Library/TimerLib.h>
# include "VBoxPkg.h"
# include "DevEFI.h"
# include "iprt/asm.h"


static inline UINT32 VBoxReadNVRAM(UINT8 *pu8Buffer, UINT32 cbBuffer)
{
    UINT32 idxBuffer = 0;
    for (idxBuffer = 0; idxBuffer < cbBuffer; ++idxBuffer)
        pu8Buffer[idxBuffer] = ASMInU8(EFI_VARIABLE_OP);
    return idxBuffer;
}

static inline void VBoxWriteNVRAMU32Param(UINT32 u32CodeParam, UINT32 u32Param)
{
    ASMOutU32(EFI_VARIABLE_OP, u32CodeParam);
    ASMOutU32(EFI_VARIABLE_PARAM, u32Param);
}

static inline UINT32 VBoxWriteNVRAMByteArrayParam(const UINT8 *pu8Param, UINT32 cbParam)
{
    UINT32 idxParam = 0;
    for (idxParam = 0; idxParam < cbParam; ++idxParam)
        ASMOutU8(EFI_VARIABLE_PARAM, pu8Param[idxParam]);
    return idxParam;
}

static inline UINT32 VBoxWriteNVRAMStringParam(const CHAR16 *ps16VariableName)
{
    CHAR8 szVarName[512];
    UINT32 cbVarName = StrLen(ps16VariableName);
    LogFlowFuncEnter();
    ASSERT (cbVarName < 512);
    if (cbVarName > 512)
    {
        LogFlowFuncMarkVar(cbVarName, "%d");
        LogFlowFuncLeave();
        return 0;
    }
    UnicodeStrToAsciiStr(ps16VariableName, szVarName);

    VBoxWriteNVRAMU32Param(EFI_VM_VARIABLE_OP_NAME_LENGTH, cbVarName);

    ASMOutU32(EFI_VARIABLE_OP, EFI_VM_VARIABLE_OP_NAME);
    cbVarName = VBoxWriteNVRAMByteArrayParam((UINT8 *)szVarName, cbVarName);

    LogFlowFuncMarkVar(cbVarName, "%d");
    LogFlowFuncLeave();
    return cbVarName;
}

static inline UINT32 VBoxWriteNVRAMGuidParam(const EFI_GUID *pGuid)
{
    ASMOutU32(EFI_VARIABLE_OP, EFI_VM_VARIABLE_OP_GUID);
    return VBoxWriteNVRAMByteArrayParam((UINT8 *)pGuid, sizeof(EFI_GUID));
}

static inline UINT32 VBoxWriteNVRAMDoOp(UINT32 u32Operation)
{
    UINT32 u32Rc;
    LogFlowFuncEnter();
    LogFlowFuncMarkVar(u32Operation, "%x");
    VBoxWriteNVRAMU32Param(EFI_VM_VARIABLE_OP_START, u32Operation);

    while((u32Rc = ASMInU32(EFI_VARIABLE_OP)) == EFI_VARIABLE_OP_STATUS_BSY)
    {
#if 0
        MicroSecondDelay (400);
#endif
        /* @todo: sleep here */
    }
    LogFlowFuncMarkVar(u32Rc, "%x");
    LogFlowFuncLeave();
    return u32Rc;
}
#endif

/**

  This code finds variable in storage blocks (Volatile or Non-Volatile).

  @param VariableName               Name of Variable to be found.
  @param VendorGuid                 Variable vendor GUID.
  @param Attributes                 Attribute value of the variable found.
  @param DataSize                   Size of Data found. If size is less than the
                                    data, this value contains the required size.
  @param Data                       Data pointer.

  @return EFI_INVALID_PARAMETER     Invalid parameter
  @return EFI_SUCCESS               Find the specified variable
  @return EFI_NOT_FOUND             Not found
  @return EFI_BUFFER_TO_SMALL       DataSize is too small for the result

**/
EFI_STATUS
EFIAPI
RuntimeServiceGetVariable (
  IN CHAR16        *VariableName,
  IN EFI_GUID      *VendorGuid,
  OUT UINT32       *Attributes OPTIONAL,
  IN OUT UINTN     *DataSize,
  OUT VOID         *Data
  )
{
#ifndef VBOX
  return EmuGetVariable (
          VariableName,
          VendorGuid,
          Attributes OPTIONAL,
          DataSize,
          Data,
          &mVariableModuleGlobal->VariableGlobal[Physical]
          );
#else
    UINT32 VarLen;
    UINT32 u32Rc = 0;

    LogFlowFuncEnter();
    /* set uuid */
    ASMOutU32(EFI_VARIABLE_OP, EFI_VM_VARIABLE_OP_GUID);
    VBoxWriteNVRAMGuidParam(VendorGuid);

    /* set name */
    VBoxWriteNVRAMStringParam(VariableName);

    /* start operation */
    u32Rc = VBoxWriteNVRAMDoOp(EFI_VARIABLE_OP_QUERY);

    ASSERT (u32Rc != EFI_VARIABLE_OP_STATUS_ERROR);
    switch(u32Rc)
    {
        case EFI_VARIABLE_OP_STATUS_ERROR: /* for release build */
        case EFI_VARIABLE_OP_STATUS_NOT_FOUND:
            LogFlowFuncLeaveRC(EFI_NOT_FOUND);
            return EFI_NOT_FOUND;
        case EFI_VARIABLE_OP_STATUS_OK:
        {
            ASMOutU32(EFI_VARIABLE_OP, EFI_VM_VARIABLE_OP_VALUE_LENGTH);
            VarLen = ASMInU32(EFI_VARIABLE_OP);
            LogFlowFuncMarkVar(*DataSize, "%d");
            LogFlowFuncMarkVar(VarLen, "%d");
            if (   VarLen > *DataSize
                || !Data)
            {
                *DataSize = VarLen;
                /* @todo: should we end op ? */
                LogFlowFuncLeave();
                return EFI_BUFFER_TOO_SMALL;
            }
            ASMOutU32(EFI_VARIABLE_OP, EFI_VM_VARIABLE_OP_VALUE);
            *DataSize = VBoxReadNVRAM((UINT8 *)Data, VarLen);
            if (Attributes)
            {
                ASMOutU32(EFI_VARIABLE_OP, EFI_VM_VARIABLE_OP_ATTRIBUTE);
                *Attributes = ASMInU32(EFI_VARIABLE_OP);
                LogFlowFuncMarkVar(Attributes, "%x");
            }
            LogFlowFuncLeaveRC((EFI_SUCCESS));
            return EFI_SUCCESS;
        }
    }
#endif
    return EFI_SUCCESS;
}

/**

  This code Finds the Next available variable.

  @param VariableNameSize           Size of the variable name
  @param VariableName               Pointer to variable name
  @param VendorGuid                 Variable Vendor Guid

  @return EFI_INVALID_PARAMETER     Invalid parameter
  @return EFI_SUCCESS               Find the specified variable
  @return EFI_NOT_FOUND             Not found
  @return EFI_BUFFER_TO_SMALL       DataSize is too small for the result

**/
EFI_STATUS
EFIAPI
RuntimeServiceGetNextVariableName (
  IN OUT UINTN     *VariableNameSize,
  IN OUT CHAR16    *VariableName,
  IN OUT EFI_GUID  *VendorGuid
  )
{
#ifndef VBOX
  return EmuGetNextVariableName (
          VariableNameSize,
          VariableName,
          VendorGuid,
          &mVariableModuleGlobal->VariableGlobal[Physical]
          );
#else
    uint32_t u32Rc = 0;
    EFI_STATUS rc = EFI_NOT_FOUND;
    CHAR8   szVariableName[512];
    int cbVarName = 0;

    LogFlowFuncEnter();

    SetMem(szVariableName, 512, 0);
    u32Rc = VBoxWriteNVRAMDoOp(EFI_VARIABLE_OP_QUERY_NEXT);
    switch (u32Rc)
    {
        case EFI_VARIABLE_OP_STATUS_OK:
        {
            ASMOutU32(EFI_VARIABLE_OP, EFI_VM_VARIABLE_OP_NAME_LENGTH);
            cbVarName = ASMInU32(EFI_VARIABLE_OP);

            ASMOutU32(EFI_VARIABLE_OP, EFI_VM_VARIABLE_OP_GUID);
            VBoxReadNVRAM((UINT8 *)VendorGuid, sizeof(EFI_GUID));

            ASMOutU32(EFI_VARIABLE_OP, EFI_VM_VARIABLE_OP_NAME);
            VBoxReadNVRAM((UINT8 *)szVariableName, cbVarName);

            if (cbVarName < *VariableNameSize)
            {
                UnicodeSPrintAsciiFormat(VariableName, cbVarName, "%a", szVariableName);
                LogFlowFuncMarkVar(*VariableNameSize, "%d");
                LogFlowFuncMarkVar(VariableName, "%s");
                LogFlowFuncMarkVar(VendorGuid, "%g");
                *VariableNameSize = cbVarName;
                LogFlowFuncMarkVar(*VariableNameSize, "%d");
                rc = EFI_SUCCESS;
            }
            else
                rc = EFI_BUFFER_TOO_SMALL;
        }
        case EFI_VARIABLE_OP_STATUS_ERROR:
        case EFI_VARIABLE_OP_STATUS_NOT_FOUND:
        case EFI_VARIABLE_OP_STATUS_NOT_WP:
            rc = EFI_NOT_FOUND;
    }
    LogFlowFuncLeaveRC(rc);
    return rc;
#endif
}

/**

  This code sets variable in storage blocks (Volatile or Non-Volatile).

  @param VariableName                     Name of Variable to be found
  @param VendorGuid                       Variable vendor GUID
  @param Attributes                       Attribute value of the variable found
  @param DataSize                         Size of Data found. If size is less than the
                                          data, this value contains the required size.
  @param Data                             Data pointer

  @return EFI_INVALID_PARAMETER           Invalid parameter
  @return EFI_SUCCESS                     Set successfully
  @return EFI_OUT_OF_RESOURCES            Resource not enough to set variable
  @return EFI_NOT_FOUND                   Not found
  @return EFI_WRITE_PROTECTED             Variable is read-only

**/
EFI_STATUS
EFIAPI
RuntimeServiceSetVariable (
  IN CHAR16        *VariableName,
  IN EFI_GUID      *VendorGuid,
  IN UINT32        Attributes,
  IN UINTN         DataSize,
  IN VOID          *Data
  )
{
#ifndef VBOX
  return EmuSetVariable (
          VariableName,
          VendorGuid,
          Attributes,
          DataSize,
          Data,
          &mVariableModuleGlobal->VariableGlobal[Physical],
          &mVariableModuleGlobal->VolatileLastVariableOffset,
          &mVariableModuleGlobal->NonVolatileLastVariableOffset
          );
#else
    UINT32 u32Rc;
    LogFlowFuncEnter();
    LogFlowFuncMarkVar(VendorGuid, "%g");
    LogFlowFuncMarkVar(VariableName, "%s");
    LogFlowFuncMarkVar(DataSize, "%d");
    /* set guid */
    VBoxWriteNVRAMGuidParam(VendorGuid);
    /* set name */
    VBoxWriteNVRAMStringParam(VariableName);
    /* set attribute */
    VBoxWriteNVRAMU32Param(EFI_VM_VARIABLE_OP_ATTRIBUTE, Attributes);
    /* set value length */
    VBoxWriteNVRAMU32Param(EFI_VM_VARIABLE_OP_VALUE_LENGTH, DataSize);
    /* fill value bytes */
    ASMOutU32(EFI_VARIABLE_OP, EFI_VM_VARIABLE_OP_VALUE);
    VBoxWriteNVRAMByteArrayParam(Data, DataSize);
    /* start fetch operation */
    u32Rc = VBoxWriteNVRAMDoOp(EFI_VARIABLE_OP_ADD);
    /* process errors */
    LogFlowFuncLeave();
    switch(u32Rc)
    {
        case EFI_VARIABLE_OP_STATUS_OK:
            return EFI_SUCCESS;
        case EFI_VARIABLE_OP_STATUS_NOT_WP:
        default:
            return EFI_WRITE_PROTECTED;
    }
#endif
}

/**

  This code returns information about the EFI variables.

  @param Attributes                     Attributes bitmask to specify the type of variables
                                        on which to return information.
  @param MaximumVariableStorageSize     Pointer to the maximum size of the storage space available
                                        for the EFI variables associated with the attributes specified.
  @param RemainingVariableStorageSize   Pointer to the remaining size of the storage space available
                                        for EFI variables associated with the attributes specified.
  @param MaximumVariableSize            Pointer to the maximum size of an individual EFI variables
                                        associated with the attributes specified.

  @return EFI_INVALID_PARAMETER         An invalid combination of attribute bits was supplied.
  @return EFI_SUCCESS                   Query successfully.
  @return EFI_UNSUPPORTED               The attribute is not supported on this platform.

**/
EFI_STATUS
EFIAPI
RuntimeServiceQueryVariableInfo (
  IN  UINT32                 Attributes,
  OUT UINT64                 *MaximumVariableStorageSize,
  OUT UINT64                 *RemainingVariableStorageSize,
  OUT UINT64                 *MaximumVariableSize
  )
{
#ifndef VBOX
  return EmuQueryVariableInfo (
          Attributes,
          MaximumVariableStorageSize,
          RemainingVariableStorageSize,
          MaximumVariableSize,
          &mVariableModuleGlobal->VariableGlobal[Physical]
          );
#else
    *MaximumVariableStorageSize = 64 * 1024 * 1024;
    *MaximumVariableSize = 1024;
    *RemainingVariableStorageSize = 32 * 1024 * 1024;
    return EFI_SUCCESS;
#endif
}

/**
  Notification function of EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE.

  This is a notification function registered on EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE event.
  It convers pointer to new virtual address.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context.

**/
VOID
EFIAPI
VariableClassAddressChangeEvent (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
#ifndef VBOX
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal->PlatformLangCodes);
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal->LangCodes);
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal->PlatformLang);
  EfiConvertPointer (
    0x0,
    (VOID **) &mVariableModuleGlobal->VariableGlobal[Physical].NonVolatileVariableBase
    );
  EfiConvertPointer (
    0x0,
    (VOID **) &mVariableModuleGlobal->VariableGlobal[Physical].VolatileVariableBase
    );
  EfiConvertPointer (0x0, (VOID **) &mVariableModuleGlobal);
#endif
}

/**
  EmuVariable Driver main entry point. The Variable driver places the 4 EFI
  runtime services in the EFI System Table and installs arch protocols
  for variable read and write services being available. It also registers
  notification function for EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE event.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       Variable service successfully initialized.

**/
EFI_STATUS
EFIAPI
VariableServiceInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_HANDLE  NewHandle;
  EFI_STATUS  Status;

  Status = VariableCommonInitialize (ImageHandle, SystemTable);
  ASSERT_EFI_ERROR (Status);

  SystemTable->RuntimeServices->GetVariable         = RuntimeServiceGetVariable;
  SystemTable->RuntimeServices->GetNextVariableName = RuntimeServiceGetNextVariableName;
  SystemTable->RuntimeServices->SetVariable         = RuntimeServiceSetVariable;
  SystemTable->RuntimeServices->QueryVariableInfo   = RuntimeServiceQueryVariableInfo;

  //
  // Now install the Variable Runtime Architectural Protocol on a new handle
  //
  NewHandle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &NewHandle,
                  &gEfiVariableArchProtocolGuid,
                  NULL,
                  &gEfiVariableWriteArchProtocolGuid,
                  NULL,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  VariableClassAddressChangeEvent,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mVirtualAddressChangeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  /* Self Test */
    {
        EFI_GUID TestUUID = {0xe660597e, 0xb94d, 0x4209, {0x9c, 0x80, 0x18, 0x05, 0xb5, 0xd1, 0x9b, 0x69}};
        const char *pszVariable0 = "This is test!!!";
        const CHAR16 *pszVariable1 = L"This is test!!!";
        char szTestVariable[512];
#if 0
        rc = runtime->SetVariable(&TestUUID,
            NULL ,
            (EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS| EFI_VARIABLE_RUNTIME_ACCESS),
            0,
            NULL );
        ASSERT(rc == EFI_INVALID_PARAMETER);
#endif
        UINTN size = sizeof(szTestVariable),
        rc = RuntimeServiceSetVariable(
            L"Test0" ,
            &TestUUID,
            (EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS| EFI_VARIABLE_RUNTIME_ACCESS),
            AsciiStrSize(pszVariable0),
            (void *)pszVariable0);
        ASSERT_EFI_ERROR(rc);
        SetMem(szTestVariable, 512, 0);
        rc = RuntimeServiceGetVariable(
            L"Test0" ,
            &TestUUID,
            NULL,
            &size,
            (void *)szTestVariable);
        LogFlowFuncMarkVar(szTestVariable, "%a");

        ASSERT(CompareMem(szTestVariable, pszVariable0, size) == 0);

        rc = RuntimeServiceSetVariable(
            L"Test1" ,
            &TestUUID,
            (EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS| EFI_VARIABLE_RUNTIME_ACCESS),
            StrSize(pszVariable1),
            (void *)pszVariable1);
        ASSERT_EFI_ERROR(rc);
        SetMem(szTestVariable, 512, 0);
        size = StrSize(pszVariable1);
        rc = RuntimeServiceGetVariable(
            L"Test1" ,
            &TestUUID,
            NULL,
            &size,
            (void *)szTestVariable);
        LogFlowFuncMarkVar((CHAR16 *)szTestVariable, "%s");
        ASSERT(CompareMem(szTestVariable, pszVariable1, size) == 0);
    }

  return EFI_SUCCESS;
}
