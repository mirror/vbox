/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_MEDIUMIMPL
#define ____H_MEDIUMIMPL

#include <VBox/vd.h>

#include "VirtualBoxBase.h"
#include "MediumLock.h"

class Progress;
class MediumFormat;

namespace settings
{
    struct Medium;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * Medium component class for all media types.
 */
class ATL_NO_VTABLE Medium :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IMedium)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Medium, IMedium)

    DECLARE_NOT_AGGREGATABLE(Medium)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Medium)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IMedium)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(Medium)

    HRESULT FinalConstruct();
    void FinalRelease();

    enum HDDOpenMode  { OpenReadWrite, OpenReadOnly };
                // have to use a special enum for the overloaded init() below;
                // can't use AccessMode_T from XIDL because that's mapped to an int
                // and would be ambiguous

    // public initializer/uninitializer for internal purposes only

    // initializer to create empty medium (VirtualBox::CreateHardDisk())
    HRESULT init(VirtualBox *aVirtualBox,
                 const Utf8Str &aFormat,
                 const Utf8Str &aLocation,
                 const Guid &uuidMachineRegistry,
                 GuidList *pllRegistriesThatNeedSaving);

    // initializer for opening existing media
    // (VirtualBox::OpenMedium(); Machine::AttachDevice())
    HRESULT init(VirtualBox *aVirtualBox,
                 const Utf8Str &aLocation,
                 HDDOpenMode enOpenMode,
                 bool fForceNewUuid,
                 DeviceType_T aDeviceType);

    // initializer used when loading settings
    HRESULT init(VirtualBox *aVirtualBox,
                 Medium *aParent,
                 DeviceType_T aDeviceType,
                 const Guid &uuidMachineRegistry,
                 const settings::Medium &data,
                 const Utf8Str &strMachineFolder);

    // initializer for host floppy/DVD
    HRESULT init(VirtualBox *aVirtualBox,
                 DeviceType_T aDeviceType,
                 const Utf8Str &aLocation,
                 const Utf8Str &aDescription = Utf8Str::Empty);

    void uninit();

    void deparent();
    void setParent(const ComObjPtr<Medium> &pParent);

    // IMedium properties
    STDMETHOD(COMGETTER(Id))(BSTR *aId);
    STDMETHOD(COMGETTER(Description))(BSTR *aDescription);
    STDMETHOD(COMSETTER(Description))(IN_BSTR aDescription);
    STDMETHOD(COMGETTER(State))(MediumState_T *aState);
    STDMETHOD(COMGETTER(Variant))(ULONG *aVariant);
    STDMETHOD(COMGETTER(Location))(BSTR *aLocation);
    STDMETHOD(COMSETTER(Location))(IN_BSTR aLocation);
    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(DeviceType))(DeviceType_T *aDeviceType);
    STDMETHOD(COMGETTER(HostDrive))(BOOL *aHostDrive);
    STDMETHOD(COMGETTER(Size))(LONG64 *aSize);
    STDMETHOD(COMGETTER(Format))(BSTR *aFormat);
    STDMETHOD(COMGETTER(MediumFormat))(IMediumFormat **aMediumFormat);
    STDMETHOD(COMGETTER(Type))(MediumType_T *aType);
    STDMETHOD(COMSETTER(Type))(MediumType_T aType);
    STDMETHOD(COMGETTER(AllowedTypes))(ComSafeArrayOut(MediumType_T, aAllowedTypes));
    STDMETHOD(COMGETTER(Parent))(IMedium **aParent);
    STDMETHOD(COMGETTER(Children))(ComSafeArrayOut(IMedium *, aChildren));
    STDMETHOD(COMGETTER(Base))(IMedium **aBase);
    STDMETHOD(COMGETTER(ReadOnly))(BOOL *aReadOnly);
    STDMETHOD(COMGETTER(LogicalSize))(LONG64 *aLogicalSize);
    STDMETHOD(COMGETTER(AutoReset))(BOOL *aAutoReset);
    STDMETHOD(COMSETTER(AutoReset))(BOOL aAutoReset);
    STDMETHOD(COMGETTER(LastAccessError))(BSTR *aLastAccessError);
    STDMETHOD(COMGETTER(MachineIds))(ComSafeArrayOut(BSTR, aMachineIds));

    // IMedium methods
    STDMETHOD(SetIDs)(BOOL aSetImageId, IN_BSTR aImageId,
                      BOOL aSetParentId, IN_BSTR aParentId);
    STDMETHOD(RefreshState)(MediumState_T *aState);
    STDMETHOD(GetSnapshotIds)(IN_BSTR aMachineId,
                              ComSafeArrayOut(BSTR, aSnapshotIds));
    STDMETHOD(LockRead)(MediumState_T *aState);
    STDMETHOD(UnlockRead)(MediumState_T *aState);
    STDMETHOD(LockWrite)(MediumState_T *aState);
    STDMETHOD(UnlockWrite)(MediumState_T *aState);
    STDMETHOD(Close)();
    STDMETHOD(GetProperty)(IN_BSTR aName, BSTR *aValue);
    STDMETHOD(SetProperty)(IN_BSTR aName, IN_BSTR aValue);
    STDMETHOD(GetProperties)(IN_BSTR aNames,
                             ComSafeArrayOut(BSTR, aReturnNames),
                             ComSafeArrayOut(BSTR, aReturnValues));
    STDMETHOD(SetProperties)(ComSafeArrayIn(IN_BSTR, aNames),
                             ComSafeArrayIn(IN_BSTR, aValues));
    STDMETHOD(CreateBaseStorage)(LONG64 aLogicalSize,
                                 ULONG aVariant,
                                 IProgress **aProgress);
    STDMETHOD(DeleteStorage)(IProgress **aProgress);
    STDMETHOD(CreateDiffStorage)(IMedium *aTarget,
                                 ULONG aVariant,
                                 IProgress **aProgress);
    STDMETHOD(MergeTo)(IMedium *aTarget, IProgress **aProgress);
    STDMETHOD(CloneTo)(IMedium *aTarget, ULONG aVariant,
                        IMedium *aParent, IProgress **aProgress);
    STDMETHOD(Compact)(IProgress **aProgress);
    STDMETHOD(Resize)(LONG64 aLogicalSize, IProgress **aProgress);
    STDMETHOD(Reset)(IProgress **aProgress);

    // unsafe methods for internal purposes only (ensure there is
    // a caller and a read lock before calling them!)
    const ComObjPtr<Medium>& getParent() const;
    const MediaList& getChildren() const;

    const Guid& getId() const;
    MediumState_T getState() const;
    MediumVariant_T getVariant() const;
    bool isHostDrive() const;
    const Utf8Str& getLocationFull() const;
    const Utf8Str& getFormat() const;
    const ComObjPtr<MediumFormat> & getMediumFormat() const;
    bool isMediumFormatFile() const;
    uint64_t getSize() const;
    DeviceType_T getDeviceType() const;
    MediumType_T getType() const;
    Utf8Str getName();

    bool addRegistry(const Guid& id, bool fRecurse);
private:
    void addRegistryImpl(const Guid& id, bool fRecurse);
public:
    bool removeRegistry(const Guid& id, bool fRecurse);
    bool isInRegistry(const Guid& id);
    bool getFirstRegistryMachineId(Guid &uuid) const;
    HRESULT addToRegistryIDList(GuidList &llRegistryIDs);

    HRESULT addBackReference(const Guid &aMachineId,
                             const Guid &aSnapshotId = Guid::Empty);
    HRESULT removeBackReference(const Guid &aMachineId,
                                const Guid &aSnapshotId = Guid::Empty);

    const Guid* getFirstMachineBackrefId() const;
    const Guid* getAnyMachineBackref() const;
    const Guid* getFirstMachineBackrefSnapshotId() const;
    size_t getMachineBackRefCount() const;

#ifdef DEBUG
    void dumpBackRefs();
#endif

    HRESULT updatePath(const Utf8Str &strOldPath, const Utf8Str &strNewPath);

    ComObjPtr<Medium> getBase(uint32_t *aLevel = NULL);

    bool isReadOnly();
    void updateId(const Guid &id);

    HRESULT saveSettings(settings::Medium &data,
                         const Utf8Str &strHardDiskFolder);

    HRESULT createMediumLockList(bool fFailIfInaccessible,
                                 bool fMediumLockWrite,
                                 Medium *pToBeParent,
                                 MediumLockList &mediumLockList);

    HRESULT createDiffStorage(ComObjPtr<Medium> &aTarget,
                              MediumVariant_T aVariant,
                              MediumLockList *pMediumLockList,
                              ComObjPtr<Progress> *aProgress,
                              bool aWait,
                              GuidList *pllRegistriesThatNeedSaving);
    Utf8Str getPreferredDiffFormat();

    HRESULT close(GuidList *pllRegistriesThatNeedSaving, AutoCaller &autoCaller);
    HRESULT deleteStorage(ComObjPtr<Progress> *aProgress, bool aWait, GuidList *pllRegistriesThatNeedSaving);
    HRESULT markForDeletion();
    HRESULT unmarkForDeletion();
    HRESULT markLockedForDeletion();
    HRESULT unmarkLockedForDeletion();

    HRESULT prepareMergeTo(const ComObjPtr<Medium> &pTarget,
                           const Guid *aMachineId,
                           const Guid *aSnapshotId,
                           bool fLockMedia,
                           bool &fMergeForward,
                           ComObjPtr<Medium> &pParentForTarget,
                           MediaList &aChildrenToReparent,
                           MediumLockList * &aMediumLockList);
    HRESULT mergeTo(const ComObjPtr<Medium> &pTarget,
                    bool fMergeForward,
                    const ComObjPtr<Medium> &pParentForTarget,
                    const MediaList &aChildrenToReparent,
                    MediumLockList *aMediumLockList,
                    ComObjPtr<Progress> *aProgress,
                    bool aWait,
                    GuidList *pllRegistriesThatNeedSaving);
    void cancelMergeTo(const MediaList &aChildrenToReparent,
                       MediumLockList *aMediumLockList);

    HRESULT fixParentUuidOfChildren(const MediaList &childrenToReparent);

    HRESULT exportFile(const char *aFilename,
                       const ComObjPtr<MediumFormat> &aFormat,
                       MediumVariant_T aVariant,
                       void *aVDImageIOCallbacks, void *aVDImageIOUser,
                       const ComObjPtr<Progress> &aProgress);
    HRESULT importFile(const char *aFilename,
                       const ComObjPtr<MediumFormat> &aFormat,
                       MediumVariant_T aVariant,
                       void *aVDImageIOCallbacks, void *aVDImageIOUser,
                       const ComObjPtr<Medium> &aParent,
                       const ComObjPtr<Progress> &aProgress);

private:

    HRESULT queryInfo(bool fSetImageId, bool fSetParentId);

    HRESULT canClose();
    HRESULT unregisterWithVirtualBox(GuidList *pllRegistriesThatNeedSaving);

    HRESULT setStateError();

    HRESULT setLocation(const Utf8Str &aLocation, const Utf8Str &aFormat = Utf8Str::Empty);
    HRESULT setFormat(const Utf8Str &aFormat);

    VDTYPE convertDeviceType();
    DeviceType_T convertToDeviceType(VDTYPE enmType);

    Utf8Str vdError(int aVRC);

    static DECLCALLBACK(void) vdErrorCall(void *pvUser, int rc, RT_SRC_POS_DECL,
                                          const char *pszFormat, va_list va);

    static DECLCALLBACK(bool) vdConfigAreKeysValid(void *pvUser,
                                                   const char *pszzValid);
    static DECLCALLBACK(int) vdConfigQuerySize(void *pvUser, const char *pszName,
                                               size_t *pcbValue);
    static DECLCALLBACK(int) vdConfigQuery(void *pvUser, const char *pszName,
                                           char *pszValue, size_t cchValue);

    static DECLCALLBACK(int) vdTcpSocketCreate(uint32_t fFlags, PVDSOCKET pSock);
    static DECLCALLBACK(int) vdTcpSocketDestroy(VDSOCKET Sock);
    static DECLCALLBACK(int) vdTcpClientConnect(VDSOCKET Sock, const char *pszAddress, uint32_t uPort);
    static DECLCALLBACK(int) vdTcpClientClose(VDSOCKET Sock);
    static DECLCALLBACK(bool) vdTcpIsClientConnected(VDSOCKET Sock);
    static DECLCALLBACK(int) vdTcpSelectOne(VDSOCKET Sock, RTMSINTERVAL cMillies);
    static DECLCALLBACK(int) vdTcpRead(VDSOCKET Sock, void *pvBuffer, size_t cbBuffer, size_t *pcbRead);
    static DECLCALLBACK(int) vdTcpWrite(VDSOCKET Sock, const void *pvBuffer, size_t cbBuffer);
    static DECLCALLBACK(int) vdTcpSgWrite(VDSOCKET Sock, PCRTSGBUF pSgBuf);
    static DECLCALLBACK(int) vdTcpFlush(VDSOCKET Sock);
    static DECLCALLBACK(int) vdTcpSetSendCoalescing(VDSOCKET Sock, bool fEnable);
    static DECLCALLBACK(int) vdTcpGetLocalAddress(VDSOCKET Sock, PRTNETADDR pAddr);
    static DECLCALLBACK(int) vdTcpGetPeerAddress(VDSOCKET Sock, PRTNETADDR pAddr);

    class Task;
    class CreateBaseTask;
    class CreateDiffTask;
    class CloneTask;
    class CompactTask;
    class ResizeTask;
    class ResetTask;
    class DeleteTask;
    class MergeTask;
    class ExportTask;
    class ImportTask;
    friend class Task;
    friend class CreateBaseTask;
    friend class CreateDiffTask;
    friend class CloneTask;
    friend class CompactTask;
    friend class ResizeTask;
    friend class ResetTask;
    friend class DeleteTask;
    friend class MergeTask;
    friend class ExportTask;
    friend class ImportTask;

    HRESULT startThread(Medium::Task *pTask);
    HRESULT runNow(Medium::Task *pTask, GuidList *pllRegistriesThatNeedSaving);

    HRESULT taskCreateBaseHandler(Medium::CreateBaseTask &task);
    HRESULT taskCreateDiffHandler(Medium::CreateDiffTask &task);
    HRESULT taskMergeHandler(Medium::MergeTask &task);
    HRESULT taskCloneHandler(Medium::CloneTask &task);
    HRESULT taskDeleteHandler(Medium::DeleteTask &task);
    HRESULT taskResetHandler(Medium::ResetTask &task);
    HRESULT taskCompactHandler(Medium::CompactTask &task);
    HRESULT taskResizeHandler(Medium::ResizeTask &task);
    HRESULT taskExportHandler(Medium::ExportTask &task);
    HRESULT taskImportHandler(Medium::ImportTask &task);

    struct Data;            // opaque data struct, defined in MediumImpl.cpp
    Data *m;
};

#endif /* ____H_MEDIUMIMPL */

