/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
/*
 * Based in part on Microsoft DDK sample code for Sample Notify Object
 *+---------------------------------------------------------------------------
 *
 *  Microsoft Windows
 *  Copyright (C) Microsoft Corporation, 1992-2001.
 *
 *  Author:     Alok Sinha
 *
 *----------------------------------------------------------------------------
 */
#ifndef ___VboxNetFltNotify_h___
#define ___VboxNetFltNotify_h___

#include <windows.h>
#include <atlbase.h>
extern CComModule _Module;  // required by atlcom.h
#include <atlcom.h>
#include <VBoxNetFltNotifyn.h>
//#include <Netcfgx.h>

#include "VBoxNetFltNotifyRc.h"

#define VBOXNETFLTNOTIFY_ONFAIL_BINDDEFAULT false

/*
 * VboxNetFlt Notify Object used to control bindings
 */
class VBoxNetFltNotify :

               /*
                * Must inherit from CComObjectRoot(Ex) for reference count
                * management and default threading model.
                */
               public CComObjectRoot,

               /*
                * Define the default class factory and aggregation model.
                */
               public CComCoClass<VBoxNetFltNotify, &CLSID_VBoxNetFltNotify>,

               /*
                * Notify Object's interfaces.
                */
               public INetCfgComponentControl,
               public INetCfgComponentNotifyBinding
{

   /*
    * Public members.
    */
   public:

      /*
       * Constructor
       */
       VBoxNetFltNotify(VOID);

      /*
       * Destructors.
       */
      ~VBoxNetFltNotify(VOID);

      /*
       * Notify Object's interfaces.
       */
      BEGIN_COM_MAP(VBoxNetFltNotify)
         COM_INTERFACE_ENTRY(INetCfgComponentControl)
//         COM_INTERFACE_ENTRY(INetCfgComponentSetup)
//         COM_INTERFACE_ENTRY(INetCfgComponentPropertyUi)
         COM_INTERFACE_ENTRY(INetCfgComponentNotifyBinding)
//         COM_INTERFACE_ENTRY(INetCfgComponentNotifyGlobal)
      END_COM_MAP()

      /*
       * Uncomment the the line below if you don't want your object to
       * support aggregation. The default is to support it
       *
       * DECLARE_NOT_AGGREGATABLE(CMuxNotify)
       */

      DECLARE_REGISTRY_RESOURCEID(IDR_REG_VBOXNETFLT_NOTIFY)

      /*
       * INetCfgComponentControl
       */
      STDMETHOD (Initialize) (
                   IN INetCfgComponent  *pIComp,
                   IN INetCfg           *pINetCfg,
                   IN BOOL              fInstalling);

      STDMETHOD (CancelChanges) ();

      STDMETHOD (ApplyRegistryChanges) ();

      STDMETHOD (ApplyPnpChanges) (
                   IN INetCfgPnpReconfigCallback* pICallback);

      /*
       * INetCfgNotifyBinding
       */
      STDMETHOD (QueryBindingPath) (
                   IN DWORD dwChangeFlag,
                   IN INetCfgBindingPath* pncbp);

      STDMETHOD (NotifyBindingPath) (
                   IN DWORD dwChangeFlag,
                   IN INetCfgBindingPath* pncbp);

  /*
   * Private members.
   */
  private:

     /*
      * Private member variables.
      */
     INetCfgComponent  *m_pncc;  /* Our Protocol's Net Config component */
     INetCfg           *m_pnc;
};

#endif
