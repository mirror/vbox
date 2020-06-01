/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserAbstractModel class implementation.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QThread>

/* GUI includes: */
#include "UICommon.h"
#include "UIChooser.h"
#include "UIChooserAbstractModel.h"
#include "UIChooserNode.h"
#include "UIChooserNodeGroup.h"
#include "UIChooserNodeGlobal.h"
#include "UIChooserNodeMachine.h"
#include "UICloudNetworkingStuff.h"
#include "UIExtraDataManager.h"
#include "UIMessageCenter.h"
#include "UITaskCloudListMachines.h"
#include "UIThreadPool.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualMachineItemCloud.h"

/* COM includes: */
#include "CCloudMachine.h"
#include "CCloudProfile.h"
#include "CCloudProvider.h"
#include "CMachine.h"

/* Type defs: */
typedef QSet<QString> UIStringSet;


/** QThread subclass allowing to save group settings asynchronously. */
class UIThreadGroupSettingsSave : public QThread
{
    Q_OBJECT;

signals:

    /** Notifies about machine with certain @a uMachineId to be reloaded. */
    void sigReload(const QUuid &uMachineId);

    /** Notifies about task is complete. */
    void sigComplete();

public:

    /** Returns group settings saving thread instance. */
    static UIThreadGroupSettingsSave *instance();
    /** Prepares group settings saving thread instance. */
    static void prepare();
    /** Cleanups group settings saving thread instance. */
    static void cleanup();

    /** Configures @a group settings saving thread with corresponding @a pListener.
      * @param  oldLists  Brings the old settings list to be compared.
      * @param  newLists  Brings the new settings list to be saved. */
    void configure(QObject *pParent,
                   const QMap<QString, QStringList> &oldLists,
                   const QMap<QString, QStringList> &newLists);

protected:

    /** Constructs group settings saving thread. */
    UIThreadGroupSettingsSave();
    /** Destructs group settings saving thread. */
    virtual ~UIThreadGroupSettingsSave() /* override */;

    /** Contains a thread task to be executed. */
    virtual void run() /* override */;

    /** Holds the singleton instance. */
    static UIThreadGroupSettingsSave *s_pInstance;

    /** Holds the map of group settings to be compared. */
    QMap<QString, QStringList> m_oldLists;
    /** Holds the map of group settings to be saved. */
    QMap<QString, QStringList> m_newLists;
};


/** QThread subclass allowing to save group definitions asynchronously. */
class UIThreadGroupDefinitionsSave : public QThread
{
    Q_OBJECT;

signals:

    /** Notifies about task is complete. */
    void sigComplete();

public:

    /** Returns group definitions saving thread instance. */
    static UIThreadGroupDefinitionsSave *instance();
    /** Prepares group definitions saving thread instance. */
    static void prepare();
    /** Cleanups group definitions saving thread instance. */
    static void cleanup();

    /** Configures group definitions saving thread with corresponding @a pListener.
      * @param  lists  Brings definitions lists to be saved. */
    void configure(QObject *pListener,
                   const QMap<QString, QStringList> &lists);

protected:

    /** Constructs group definitions saving thread. */
    UIThreadGroupDefinitionsSave();
    /** Destructs group definitions saving thread. */
    virtual ~UIThreadGroupDefinitionsSave() /* override */;

    /** Contains a thread task to be executed. */
    virtual void run() /* override */;

    /** Holds the singleton instance. */
    static UIThreadGroupDefinitionsSave *s_pInstance;

    /** Holds the map of group definitions to be saved. */
    QMap<QString, QStringList>  m_lists;
};


/*********************************************************************************************************************************
*   Class UIThreadGroupSettingsSave implementation.                                                                              *
*********************************************************************************************************************************/

/* static */
UIThreadGroupSettingsSave *UIThreadGroupSettingsSave::s_pInstance = 0;

/* static */
UIThreadGroupSettingsSave *UIThreadGroupSettingsSave::instance()
{
    return s_pInstance;
}

/* static */
void UIThreadGroupSettingsSave::prepare()
{
    /* Make sure instance is not prepared: */
    if (s_pInstance)
        return;

    /* Crate instance: */
    new UIThreadGroupSettingsSave;
}

/* static */
void UIThreadGroupSettingsSave::cleanup()
{
    /* Make sure instance is prepared: */
    if (!s_pInstance)
        return;

    /* Delete instance: */
    delete s_pInstance;
}

void UIThreadGroupSettingsSave::configure(QObject *pParent,
                                          const QMap<QString, QStringList> &oldLists,
                                          const QMap<QString, QStringList> &newLists)
{
    m_oldLists = oldLists;
    m_newLists = newLists;
    UIChooserAbstractModel *pChooserAbstractModel = qobject_cast<UIChooserAbstractModel*>(pParent);
    AssertPtrReturnVoid(pChooserAbstractModel);
    {
        connect(this, &UIThreadGroupSettingsSave::sigComplete,
                pChooserAbstractModel, &UIChooserAbstractModel::sltGroupSettingsSaveComplete);
    }
}

UIThreadGroupSettingsSave::UIThreadGroupSettingsSave()
{
    /* Assign instance: */
    s_pInstance = this;
}

UIThreadGroupSettingsSave::~UIThreadGroupSettingsSave()
{
    /* Make sure thread work is complete: */
    wait();

    /* Erase instance: */
    s_pInstance = 0;
}

void UIThreadGroupSettingsSave::run()
{
    /* COM prepare: */
    COMBase::InitializeCOM(false);

    /* For every particular machine ID: */
    foreach (const QString &strId, m_newLists.keys())
    {
        /* Get new group list/set: */
        const QStringList &newGroupList = m_newLists.value(strId);
        const UIStringSet &newGroupSet = UIStringSet::fromList(newGroupList);
        /* Get old group list/set: */
        const QStringList &oldGroupList = m_oldLists.value(strId);
        const UIStringSet &oldGroupSet = UIStringSet::fromList(oldGroupList);
        /* Make sure group set changed: */
        if (newGroupSet == oldGroupSet)
            continue;

        /* The next steps are subsequent.
         * Every of them is mandatory in order to continue
         * with common cleanup in case of failure.
         * We have to simulate a try-catch block. */
        CSession comSession;
        CMachine comMachine;
        do
        {
            /* 1. Open session: */
            comSession = uiCommon().openSession(QUuid(strId));
            if (comSession.isNull())
                break;

            /* 2. Get session machine: */
            comMachine = comSession.GetMachine();
            if (comMachine.isNull())
                break;

            /* 3. Set new groups: */
            comMachine.SetGroups(newGroupList.toVector());
            if (!comMachine.isOk())
            {
                msgCenter().cannotSetGroups(comMachine);
                break;
            }

            /* 4. Save settings: */
            comMachine.SaveSettings();
            if (!comMachine.isOk())
            {
                msgCenter().cannotSaveMachineSettings(comMachine);
                break;
            }
        } while (0);

        /* Cleanup if necessary: */
        if (comMachine.isNull() || !comMachine.isOk())
            emit sigReload(QUuid(strId));
        if (!comSession.isNull())
            comSession.UnlockMachine();
    }

    /* Notify listeners about completeness: */
    emit sigComplete();

    /* COM cleanup: */
    COMBase::CleanupCOM();
}


/*********************************************************************************************************************************
*   Class UIThreadGroupDefinitionsSave implementation.                                                                           *
*********************************************************************************************************************************/

/* static */
UIThreadGroupDefinitionsSave *UIThreadGroupDefinitionsSave::s_pInstance = 0;

/* static */
UIThreadGroupDefinitionsSave *UIThreadGroupDefinitionsSave::instance()
{
    return s_pInstance;
}

/* static */
void UIThreadGroupDefinitionsSave::prepare()
{
    /* Make sure instance is not prepared: */
    if (s_pInstance)
        return;

    /* Crate instance: */
    new UIThreadGroupDefinitionsSave;
}

/* static */
void UIThreadGroupDefinitionsSave::cleanup()
{
    /* Make sure instance is prepared: */
    if (!s_pInstance)
        return;

    /* Delete instance: */
    delete s_pInstance;
}

void UIThreadGroupDefinitionsSave::configure(QObject *pParent,
                                             const QMap<QString, QStringList> &groups)
{
    m_lists = groups;
    UIChooserAbstractModel *pChooserAbstractModel = qobject_cast<UIChooserAbstractModel*>(pParent);
    AssertPtrReturnVoid(pChooserAbstractModel);
    {
        connect(this, &UIThreadGroupDefinitionsSave::sigComplete,
                pChooserAbstractModel, &UIChooserAbstractModel::sltGroupDefinitionsSaveComplete);
    }
}

UIThreadGroupDefinitionsSave::UIThreadGroupDefinitionsSave()
{
    /* Assign instance: */
    s_pInstance = this;
}

UIThreadGroupDefinitionsSave::~UIThreadGroupDefinitionsSave()
{
    /* Make sure thread work is complete: */
    wait();

    /* Erase instance: */
    s_pInstance = 0;
}

void UIThreadGroupDefinitionsSave::run()
{
    /* COM prepare: */
    COMBase::InitializeCOM(false);

    /* Clear all the extra-data records related to group definitions: */
    gEDataManager->clearSelectorWindowGroupsDefinitions();
    /* For every particular group definition: */
    foreach (const QString &strId, m_lists.keys())
        gEDataManager->setSelectorWindowGroupsDefinitions(strId, m_lists.value(strId));

    /* Notify listeners about completeness: */
    emit sigComplete();

    /* COM cleanup: */
    COMBase::CleanupCOM();
}


/*********************************************************************************************************************************
*   Class UIChooserAbstractModel implementation.                                                                                 *
*********************************************************************************************************************************/

UIChooserAbstractModel:: UIChooserAbstractModel(UIChooser *pParent)
    : QObject(pParent)
    , m_pParent(pParent)
    , m_pInvisibleRootNode(0)
{
    prepare();
}

void UIChooserAbstractModel::init()
{
    /* Create invisible root group node: */
    m_pInvisibleRootNode = new UIChooserNodeGroup(0 /* parent */,
                                                  0 /* position */,
                                                  true /* opened */,
                                                  QString() /* name */,
                                                  UIChooserNodeGroupType_Local);
    if (invisibleRoot())
    {
        /* Link root to this model: */
        invisibleRoot()->setModel(this);

        /* Create global node: */
        new UIChooserNodeGlobal(invisibleRoot() /* parent */,
                                0 /* position */,
                                shouldGlobalNodeBeFavorite(invisibleRoot()),
                                QString() /* tip */);

        /* Acquire VBox: */
        const CVirtualBox comVBox = uiCommon().virtualBox();

        /* Add local machines: */
        LogRelFlow(("UIChooserAbstractModel: Loading local VMs...\n"));
        /* Acquire existing machines: */
        const QVector<CMachine> machines = comVBox.GetMachines();
        /* Show error message if necessary: */
        if (!comVBox.isOk())
            msgCenter().cannotAcquireVirtualBoxParameter(comVBox);
        else
        {
            /* Iterate through existing machines: */
            foreach (const CMachine &comMachine, machines)
            {
                /* Skip if we have nothing to populate (wtf happened?): */
                if (comMachine.isNull())
                    continue;

                /* Get machine ID: */
                const QUuid uMachineID = comMachine.GetId();
                /* Show error message if necessary: */
                if (!comMachine.isOk())
                {
                    msgCenter().cannotAcquireMachineParameter(comMachine);
                    continue;
                }

                /* Skip if we have nothing to show (wtf happened?): */
                if (uMachineID.isNull())
                    continue;

                /* Skip if machine is restricted from being shown: */
                if (!gEDataManager->showMachineInVirtualBoxManagerChooser(uMachineID))
                    continue;

                /* Add machine into tree: */
                addLocalMachineIntoTheTree(comMachine);
            }
        }
        LogRelFlow(("UIChooserAbstractModel: Local VMs loaded.\n"));

#ifdef VBOX_GUI_WITH_CLOUD_VMS
        /* Add cloud providers/profiles: */
        LogRelFlow(("UIChooserAbstractModel: Loading cloud providers/profiles...\n"));
        /* Iterate through existing providers: */
        foreach (CCloudProvider comCloudProvider, listCloudProviders())
        {
            /* Skip if we have nothing to populate: */
            if (comCloudProvider.isNull())
                continue;

            /* Acquire provider short name: */
            QString strProviderShortName;
            if (!cloudProviderShortName(comCloudProvider, strProviderShortName))
                continue;

            /* Acquire list of profiles: */
            const QVector<CCloudProfile> profiles = listCloudProfiles(comCloudProvider);
            if (profiles.isEmpty())
                continue;

            /* Add provider group node: */
            UIChooserNodeGroup *pProviderNode =
                new UIChooserNodeGroup(invisibleRoot() /* parent */,
                                       getDesiredNodePosition(invisibleRoot(),
                                                              UIChooserNodeDataPrefixType_Provider,
                                                              strProviderShortName),
                                       shouldGroupNodeBeOpened(invisibleRoot(),
                                                               UIChooserNodeDataPrefixType_Provider,
                                                               strProviderShortName),
                                       strProviderShortName,
                                       UIChooserNodeGroupType_Provider);

            /* Iterate through provider's profiles: */
            foreach (CCloudProfile comCloudProfile, profiles)
            {
                /* Skip if we have nothing to populate: */
                if (comCloudProfile.isNull())
                    continue;

                /* Acquire profile name: */
                QString strProfileName;
                if (!cloudProfileName(comCloudProfile, strProfileName))
                    continue;

                /* Add profile sub-group node: */
                UIChooserNodeGroup *pProfileNode =
                    new UIChooserNodeGroup(pProviderNode /* parent */,
                                           getDesiredNodePosition(pProviderNode,
                                                                  UIChooserNodeDataPrefixType_Profile,
                                                                  strProfileName),
                                           shouldGroupNodeBeOpened(pProviderNode,
                                                                   UIChooserNodeDataPrefixType_Profile,
                                                                   strProfileName),
                                           strProfileName,
                                           UIChooserNodeGroupType_Profile);

                /* Add fake cloud VM item: */
                new UIChooserNodeMachine(pProfileNode /* parent */,
                                         0 /* position */,
                                         UIFakeCloudVirtualMachineItemState_Loading);

                /* Create list cloud machines task: */
                UITaskCloudListMachines *pTask = new UITaskCloudListMachines(strProviderShortName,
                                                                             strProfileName);
                if (pTask)
                    uiCommon().threadPoolCloud()->enqueueTask(pTask);
            }
        }
        LogRelFlow(("UIChooserAbstractModel: Cloud providers/profiles loaded.\n"));
#endif /* VBOX_GUI_WITH_CLOUD_VMS */
    }
}

void UIChooserAbstractModel::deinit()
{
    // WORKAROUND:
    // Currently we are not saving group descriptors
    // (which reflecting group toggle-state) on-the-fly,
    // so for now we are additionally save group definitions
    // when exiting application:
    saveGroupDefinitions();

    /* Make sure all saving steps complete: */
    makeSureGroupSettingsSaveIsFinished();
    makeSureGroupDefinitionsSaveIsFinished();

    /* Delete tree: */
    delete m_pInvisibleRootNode;
    m_pInvisibleRootNode = 0;
}

void UIChooserAbstractModel::wipeOutEmptyGroups()
{
    wipeOutEmptyGroupsStartingFrom(invisibleRoot());
}

QStringList UIChooserAbstractModel::possibleGroupNodeNamesForMachineNodeToMove(const QUuid &uId)
{
    /* Search for all the machine nodes with passed ID: */
    QList<UIChooserNode*> machineNodes;
    invisibleRoot()->searchForNodes(uId.toString(),
                                    UIChooserItemSearchFlag_Machine | UIChooserItemSearchFlag_ExactId,
                                    machineNodes);

    /* Return group nodes starting from root one: */
    return gatherPossibleGroupNodeNames(invisibleRoot(), machineNodes);
}

QStringList UIChooserAbstractModel::possibleGroupNodeNamesForGroupNodeToMove(const QString &strFullName)
{
    /* Search for all the group nodes with passed full-name: */
    QList<UIChooserNode*> groupNodes;
    invisibleRoot()->searchForNodes(strFullName,
                                    UIChooserItemSearchFlag_LocalGroup | UIChooserItemSearchFlag_ExactId,
                                    groupNodes);

    /* Return group nodes starting from root one: */
    return gatherPossibleGroupNodeNames(invisibleRoot(), groupNodes);
}

/* static */
QString UIChooserAbstractModel::uniqueGroupName(UIChooserNode *pRoot)
{
    /* Enumerate all the group names: */
    QStringList groupNames;
    foreach (UIChooserNode *pNode, pRoot->nodes(UIChooserNodeType_Group))
        groupNames << pNode->name();

    /* Prepare reg-exp: */
    const QString strMinimumName = tr("New group");
    const QString strShortTemplate = strMinimumName;
    const QString strFullTemplate = strShortTemplate + QString(" (\\d+)");
    const QRegExp shortRegExp(strShortTemplate);
    const QRegExp fullRegExp(strFullTemplate);

    /* Search for the maximum index: */
    int iMinimumPossibleNumber = 0;
    foreach (const QString &strName, groupNames)
    {
        if (shortRegExp.exactMatch(strName))
            iMinimumPossibleNumber = qMax(iMinimumPossibleNumber, 2);
        else if (fullRegExp.exactMatch(strName))
            iMinimumPossibleNumber = qMax(iMinimumPossibleNumber, fullRegExp.cap(1).toInt() + 1);
    }

    /* Prepare/return result: */
    QString strResult = strMinimumName;
    if (iMinimumPossibleNumber)
        strResult += " " + QString::number(iMinimumPossibleNumber);
    return strResult;
}

void UIChooserAbstractModel::performSearch(const QString &strSearchTerm, int iSearchFlags)
{
    /* Make sure invisible root exists: */
    AssertPtrReturnVoid(invisibleRoot());

    /* Currently we perform the search only for machines, when this to be changed make
     * sure the disabled flags of the other item types are also managed correctly. */

    /* Reset the search first to erase the disabled flag,
     * this also returns a full list of all machine nodes: */
    const QList<UIChooserNode*> nodes = resetSearch();

    /* Stop here if no search conditions specified: */
    if (strSearchTerm.isEmpty())
        return;

    /* Search for all the nodes matching required condition: */
    invisibleRoot()->searchForNodes(strSearchTerm, iSearchFlags, m_searchResults);

    /* Assign/reset the disabled flag for required nodes: */
    foreach (UIChooserNode *pNode, nodes)
    {
        AssertPtrReturnVoid(pNode);
        pNode->setDisabled(!m_searchResults.contains(pNode));
    }
}

QList<UIChooserNode*> UIChooserAbstractModel::resetSearch()
{
    /* Prepare resulting nodes: */
    QList<UIChooserNode*> nodes;

    /* Make sure invisible root exists: */
    AssertPtrReturn(invisibleRoot(), nodes);

    /* Calling UIChooserNode::searchForNodes with an empty search term
     * returns a list all nodes (of the whole tree) of the required type: */
    invisibleRoot()->searchForNodes(QString(), UIChooserItemSearchFlag_Machine, nodes);

    /* Reset the disabled flag of the nodes first: */
    foreach (UIChooserNode *pNode, nodes)
    {
        AssertPtrReturn(pNode, nodes);
        pNode->setDisabled(false);
    }

    /* Reset the search result related data: */
    m_searchResults.clear();

    /* Return nodes: */
    return nodes;
}

QList<UIChooserNode*> UIChooserAbstractModel::searchResult() const
{
    return m_searchResults;
}

void UIChooserAbstractModel::saveGroups()
{
    emit sigStartGroupSaving();
}

bool UIChooserAbstractModel::isGroupSavingInProgress() const
{
    return    UIThreadGroupSettingsSave::instance()
           || UIThreadGroupDefinitionsSave::instance();
}

/* static */
QString UIChooserAbstractModel::toOldStyleUuid(const QUuid &uId)
{
    return uId.toString().remove(QRegExp("[{}]"));
}

/* static */
QString UIChooserAbstractModel::prefixToString(UIChooserNodeDataPrefixType enmType)
{
    switch (enmType)
    {
        /* Global nodes: */
        case UIChooserNodeDataPrefixType_Global:   return "n";
        /* Machine nodes: */
        case UIChooserNodeDataPrefixType_Machine:  return "m";
        /* Group nodes: */
        case UIChooserNodeDataPrefixType_Local:    return "g";
        case UIChooserNodeDataPrefixType_Provider: return "p";
        case UIChooserNodeDataPrefixType_Profile:  return "a";
    }
    return QString();
}

/* static */
QString UIChooserAbstractModel::optionToString(UIChooserNodeDataOptionType enmType)
{
    switch (enmType)
    {
        /* Global nodes: */
        case UIChooserNodeDataOptionType_GlobalFavorite: return "f";
        /* Group nodes: */
        case UIChooserNodeDataOptionType_GroupOpened:    return "o";
    }
    return QString();
}

/* static */
QString UIChooserAbstractModel::valueToString(UIChooserNodeDataValueType enmType)
{
    switch (enmType)
    {
        /* Global nodes: */
        case UIChooserNodeDataValueType_GlobalDefault: return "GLOBAL";
    }
    return QString();
}

void UIChooserAbstractModel::sltHandleCloudMachineStateChange()
{
    UIVirtualMachineItem *pCache = qobject_cast<UIVirtualMachineItem*>(sender());
    AssertPtrReturnVoid(pCache);
    emit sigCloudMachineStateChange(pCache->id());
}

void UIChooserAbstractModel::sltGroupSettingsSaveComplete()
{
    makeSureGroupSettingsSaveIsFinished();
    emit sigGroupSavingStateChanged();
}

void UIChooserAbstractModel::sltGroupDefinitionsSaveComplete()
{
    makeSureGroupDefinitionsSaveIsFinished();
    emit sigGroupSavingStateChanged();
}

void UIChooserAbstractModel::sltLocalMachineStateChanged(const QUuid &uMachineId, const KMachineState)
{
    /* Update machine-nodes with passed id: */
    invisibleRoot()->updateAllNodes(uMachineId);
}

void UIChooserAbstractModel::sltLocalMachineDataChanged(const QUuid &uMachineId)
{
    /* Update machine-nodes with passed id: */
    invisibleRoot()->updateAllNodes(uMachineId);
}

void UIChooserAbstractModel::sltLocalMachineRegistered(const QUuid &uMachineId, const bool fRegistered)
{
    /* Existing VM unregistered? */
    if (!fRegistered)
    {
        /* Remove machine-items with passed id: */
        invisibleRoot()->removeAllNodes(uMachineId);
        /* Wipe out empty groups: */
        wipeOutEmptyGroups();
    }
    /* New VM registered? */
    else
    {
        /* Should we show this VM? */
        if (gEDataManager->showMachineInVirtualBoxManagerChooser(uMachineId))
        {
            /* Add new machine-item: */
            const CMachine comMachine = uiCommon().virtualBox().FindMachine(uMachineId.toString());
            addLocalMachineIntoTheTree(comMachine, true /* make it visible */);
        }
    }
}

void UIChooserAbstractModel::sltCloudMachineRegistered(const QString &strProviderShortName, const QString &strProfileName,
                                                       const QUuid &uMachineId, const bool fRegistered)
{
    /* Search for profile node: */
    const QString strProfileNodeName = QString("/%1/%2").arg(strProviderShortName, strProfileName);
    QList<UIChooserNode*> profileNodes;
    invisibleRoot()->searchForNodes(strProfileNodeName, UIChooserItemSearchFlag_CloudProfile | UIChooserItemSearchFlag_ExactId, profileNodes);
    AssertReturnVoid(!profileNodes.isEmpty());
    UIChooserNode *pProfileNode = profileNodes.first();
    AssertPtrReturnVoid(pProfileNode);

    /* Existing VM unregistered? */
    if (!fRegistered)
    {
        /* Remove machine-items with passed id: */
        pProfileNode->removeAllNodes(uMachineId);

        /* If there are no items left: */
        if (pProfileNode->nodes(UIChooserNodeType_Machine).isEmpty())
        {
            /* Add fake cloud VM item: */
            new UIChooserNodeMachine(pProfileNode /* parent */,
                                     0 /* position */,
                                     UIFakeCloudVirtualMachineItemState_Done);
        }
    }
    /* New VM registered? */
    else
    {
        /* Add new machine-item: */
        const CCloudMachine comMachine = cloudMachineById(strProviderShortName, strProfileName, uMachineId);
        addCloudMachineIntoTheTree(strProfileNodeName, comMachine, true /* make it visible */);

        /* Search for possible fake node: */
        QList<UIChooserNode*> fakeNodes;
        pProfileNode->searchForNodes(QUuid().toString(), UIChooserItemSearchFlag_Machine | UIChooserItemSearchFlag_ExactId, fakeNodes);
        /* Delete fake node if present: */
        delete fakeNodes.value(0);
    }
}

void UIChooserAbstractModel::sltSessionStateChanged(const QUuid &uMachineId, const KSessionState)
{
    /* Update machine-nodes with passed id: */
    invisibleRoot()->updateAllNodes(uMachineId);
}

void UIChooserAbstractModel::sltSnapshotChanged(const QUuid &uMachineId, const QUuid &)
{
    /* Update machine-nodes with passed id: */
    invisibleRoot()->updateAllNodes(uMachineId);
}

void UIChooserAbstractModel::sltReloadMachine(const QUuid &uMachineId)
{
    /* Remove machine-items with passed id: */
    invisibleRoot()->removeAllNodes(uMachineId);
    /* Wipe out empty groups: */
    wipeOutEmptyGroups();

    /* Should we show this VM? */
    if (gEDataManager->showMachineInVirtualBoxManagerChooser(uMachineId))
    {
        /* Add new machine-item: */
        const CMachine comMachine = uiCommon().virtualBox().FindMachine(uMachineId.toString());
        addLocalMachineIntoTheTree(comMachine, true /* make it visible */);
    }
}

void UIChooserAbstractModel::sltStartGroupSaving()
{
    saveGroupSettings();
    saveGroupDefinitions();
}

void UIChooserAbstractModel::sltHandleCloudListMachinesTaskComplete(UITask *pTask)
{
    /* Skip unrelated tasks: */
    if (!pTask || pTask->type() != UITask::Type_CloudListMachines)
        return;

    /* Cast task to corresponding sub-class: */
    UITaskCloudListMachines *pAcquiringTask = static_cast<UITaskCloudListMachines*>(pTask);

    /* Search for profile node: */
    const QString strProfileNodeName = QString("/%1/%2").arg(pAcquiringTask->providerShortName(), pAcquiringTask->profileName());
    QList<UIChooserNode*> profileNodes;
    invisibleRoot()->searchForNodes(strProfileNodeName, UIChooserItemSearchFlag_CloudProfile | UIChooserItemSearchFlag_ExactId, profileNodes);
    AssertReturnVoid(!profileNodes.isEmpty());
    UIChooserNode *pProfileNode = profileNodes.first();
    AssertPtrReturnVoid(pProfileNode);

    /* Search for fake node: */
    QList<UIChooserNode*> fakeNodes;
    pProfileNode->searchForNodes(QUuid().toString(), UIChooserItemSearchFlag_Machine | UIChooserItemSearchFlag_ExactId, fakeNodes);
    UIChooserNode *pFakeNode = fakeNodes.value(0);
    AssertPtrReturnVoid(pFakeNode);

    /* And if we have at least one cloud machine: */
    const QVector<CCloudMachine> machines = pAcquiringTask->result();
    if (!machines.isEmpty())
    {
        /* Remove fake node: */
        delete pFakeNode;

        /* Add real cloud VM nodes: */
        foreach (const CCloudMachine &comCloudMachine, machines)
            createCloudMachineNode(pProfileNode, comCloudMachine);
    }
    else
    {
        /* Otherwise toggle and update "Empty" node: */
        UIChooserNodeMachine *pFakeMachineNode = pFakeNode->toMachineNode();
        AssertReturnVoid(pFakeMachineNode && pFakeMachineNode->cacheType() == UIVirtualMachineItemType_CloudFake);
        UIVirtualMachineItemCloud *pFakeCloudMachineItem = pFakeMachineNode->cache()->toCloud();
        AssertPtrReturnVoid(pFakeCloudMachineItem);
        pFakeCloudMachineItem->setFakeCloudItemState(UIFakeCloudVirtualMachineItemState_Done);
        pFakeCloudMachineItem->setFakeCloudItemErrorMessage(pAcquiringTask->errorInfo());
    }
}

void UIChooserAbstractModel::prepare()
{
    prepareConnections();
}

void UIChooserAbstractModel::prepareConnections()
{
#ifdef VBOX_GUI_WITH_CLOUD_VMS
    /* Cloud thread-pool connections: */
    connect(uiCommon().threadPoolCloud(), &UIThreadPool::sigTaskComplete,
            this, &UIChooserAbstractModel::sltHandleCloudListMachinesTaskComplete);
    /* Setup temporary connections,
     * this is to be replaced by corresponding Main API event later. */
    connect(&uiCommon(), &UICommon::sigCloudMachineRegistered,
            this, &UIChooserAbstractModel::sltCloudMachineRegistered);
#endif

    /* Setup global connections: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIChooserAbstractModel::sltLocalMachineStateChanged);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineDataChange,
            this, &UIChooserAbstractModel::sltLocalMachineDataChanged);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineRegistered,
            this, &UIChooserAbstractModel::sltLocalMachineRegistered);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSessionStateChange,
            this, &UIChooserAbstractModel::sltSessionStateChanged);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotTake,
            this, &UIChooserAbstractModel::sltSnapshotChanged);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotDelete,
            this, &UIChooserAbstractModel::sltSnapshotChanged);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotChange,
            this, &UIChooserAbstractModel::sltSnapshotChanged);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotRestore,
            this, &UIChooserAbstractModel::sltSnapshotChanged);

    /* Setup group saving connections: */
    connect(this, &UIChooserAbstractModel::sigStartGroupSaving,
            this, &UIChooserAbstractModel::sltStartGroupSaving,
            Qt::QueuedConnection);
}

void UIChooserAbstractModel::addLocalMachineIntoTheTree(const CMachine &comMachine,
                                                        bool fMakeItVisible /* = false */)
{
    /* Make sure passed VM is not NULL: */
    if (comMachine.isNull())
        LogRelFlow(("UIChooserModel: ERROR: Passed local VM is NULL!\n"));
    AssertReturnVoid(!comMachine.isNull());

    /* Which VM we are loading: */
    const QUuid uId = comMachine.GetId();
    LogRelFlow(("UIChooserModel: Loading local VM with ID={%s}...\n",
                toOldStyleUuid(uId).toUtf8().constData()));

    /* Is that machine accessible? */
    if (comMachine.GetAccessible())
    {
        /* Acquire VM name: */
        const QString strName = comMachine.GetName();
        LogRelFlow(("UIChooserModel:  Local VM {%s} is accessible.\n", strName.toUtf8().constData()));
        /* Which groups passed machine attached to? */
        const QVector<QString> groups = comMachine.GetGroups();
        const QStringList groupList = groups.toList();
        const QString strGroups = groupList.join(", ");
        LogRelFlow(("UIChooserModel:  Local VM {%s} has groups: {%s}.\n",
                    strName.toUtf8().constData(), strGroups.toUtf8().constData()));
        foreach (QString strGroup, groups)
        {
            /* Remove last '/' if any: */
            if (strGroup.right(1) == "/")
                strGroup.truncate(strGroup.size() - 1);
            /* Create machine-item with found group-item as parent: */
            LogRelFlow(("UIChooserModel:   Creating node for local VM {%s} in group {%s}.\n",
                        strName.toUtf8().constData(), strGroup.toUtf8().constData()));
            createLocalMachineNode(getLocalGroupNode(strGroup, invisibleRoot(), fMakeItVisible), comMachine);
        }
        /* Update group settings: */
        m_groups[toOldStyleUuid(uId)] = groupList;
    }
    /* Inaccessible machine: */
    else
    {
        /* VM is accessible: */
        LogRelFlow(("UIChooserModel:  Local VM {%s} is inaccessible.\n",
                    toOldStyleUuid(uId).toUtf8().constData()));
        /* Create machine-item with main-root group-item as parent: */
        createLocalMachineNode(invisibleRoot(), comMachine);
    }
}

void UIChooserAbstractModel::addCloudMachineIntoTheTree(const QString &strGroup,
                                                        const CCloudMachine &comMachine,
                                                        bool fMakeItVisible /* = false */)
{
    /* Make sure passed VM is not NULL: */
    if (comMachine.isNull())
        LogRelFlow(("UIChooserModel: ERROR: Passed cloud VM is NULL!\n"));
    AssertReturnVoid(!comMachine.isNull());

    /* Which VM we are loading: */
    const QUuid uId = comMachine.GetId();
    LogRelFlow(("UIChooserModel: Loading cloud VM with ID={%s}...\n",
                toOldStyleUuid(uId).toUtf8().constData()));

    /* Acquire VM name: */
    QString strName = comMachine.GetName();
    if (strName.isEmpty())
        strName = uId.toString();
    LogRelFlow(("UIChooserModel:  Creating node for cloud VM {%s} in group {%s}.\n",
                strName.toUtf8().constData(), strGroup.toUtf8().constData()));
    /* Create machine-item with found group-item as parent: */
    createCloudMachineNode(getCloudGroupNode(strGroup, invisibleRoot(), fMakeItVisible), comMachine);
    /* Update group settings: */
    const QStringList groupList(strGroup);
    m_groups[toOldStyleUuid(uId)] = groupList;
}

UIChooserNode *UIChooserAbstractModel::getLocalGroupNode(const QString &strName, UIChooserNode *pParentNode, bool fAllGroupsOpened)
{
    /* Check passed stuff: */
    if (pParentNode->name() == strName)
        return pParentNode;

    /* Prepare variables: */
    const QString strFirstSubName = strName.section('/', 0, 0);
    const QString strFirstSuffix = strName.section('/', 1, -1);
    const QString strSecondSubName = strFirstSuffix.section('/', 0, 0);
    const QString strSecondSuffix = strFirstSuffix.section('/', 1, -1);

    /* Passed group name equal to first sub-name: */
    if (pParentNode->name() == strFirstSubName)
    {
        /* Make sure first-suffix is NOT empty: */
        AssertMsg(!strFirstSuffix.isEmpty(), ("Invalid group name!"));
        /* Trying to get group node among our children: */
        foreach (UIChooserNode *pNode, pParentNode->nodes(UIChooserNodeType_Group))
        {
            AssertPtrReturn(pNode, 0);
            UIChooserNodeGroup *pGroupNode = pNode->toGroupNode();
            AssertPtrReturn(pGroupNode, 0);
            if (   pGroupNode->groupType() == UIChooserNodeGroupType_Local
                && pNode->name() == strSecondSubName)
            {
                UIChooserNode *pFoundNode = getLocalGroupNode(strFirstSuffix, pNode, fAllGroupsOpened);
                if (UIChooserNodeGroup *pFoundGroupNode = pFoundNode->toGroupNode())
                    if (fAllGroupsOpened && pFoundGroupNode->isClosed())
                        pFoundGroupNode->open();
                return pFoundNode;
            }
        }
    }

    /* Found nothing? Creating: */
    UIChooserNodeGroup *pNewGroupNode =
        new UIChooserNodeGroup(pParentNode,
                               getDesiredNodePosition(pParentNode,
                                                      UIChooserNodeDataPrefixType_Local,
                                                      strSecondSubName),
                               fAllGroupsOpened || shouldGroupNodeBeOpened(pParentNode,
                                                                           UIChooserNodeDataPrefixType_Local,
                                                                           strSecondSubName),
                               strSecondSubName,
                               UIChooserNodeGroupType_Local);
    return strSecondSuffix.isEmpty() ? pNewGroupNode : getLocalGroupNode(strFirstSuffix, pNewGroupNode, fAllGroupsOpened);
}

UIChooserNode *UIChooserAbstractModel::getCloudGroupNode(const QString &strName, UIChooserNode *pParentNode, bool fAllGroupsOpened)
{
    /* Check passed stuff: */
    if (pParentNode->name() == strName)
        return pParentNode;

    /* Prepare variables: */
    const QString strFirstSubName = strName.section('/', 0, 0);
    const QString strFirstSuffix = strName.section('/', 1, -1);
    const QString strSecondSubName = strFirstSuffix.section('/', 0, 0);

    /* Passed group name equal to first sub-name: */
    if (pParentNode->name() == strFirstSubName)
    {
        /* Make sure first-suffix is NOT empty: */
        AssertMsg(!strFirstSuffix.isEmpty(), ("Invalid group name!"));
        /* Trying to get group node among our children: */
        foreach (UIChooserNode *pNode, pParentNode->nodes(UIChooserNodeType_Group))
        {
            AssertPtrReturn(pNode, 0);
            UIChooserNodeGroup *pGroupNode = pNode->toGroupNode();
            AssertPtrReturn(pGroupNode, 0);
            if (   (   pGroupNode->groupType() == UIChooserNodeGroupType_Provider
                    || pGroupNode->groupType() == UIChooserNodeGroupType_Profile)
                && pNode->name() == strSecondSubName)
            {
                UIChooserNode *pFoundNode = getCloudGroupNode(strFirstSuffix, pNode, fAllGroupsOpened);
                if (UIChooserNodeGroup *pFoundGroupNode = pFoundNode->toGroupNode())
                    if (fAllGroupsOpened && pFoundGroupNode->isClosed())
                        pFoundGroupNode->open();
                return pFoundNode;
            }
        }
    }

    /* Found nothing? Returning parent: */
    AssertFailedReturn(pParentNode);
}

bool UIChooserAbstractModel::shouldGroupNodeBeOpened(UIChooserNode *pParentNode,
                                                     UIChooserNodeDataPrefixType enmDataType,
                                                     const QString &strName) const
{
    /* Read group definitions: */
    const QStringList definitions = gEDataManager->selectorWindowGroupsDefinitions(pParentNode->fullName());
    /* Return 'false' if no definitions found: */
    if (definitions.isEmpty())
        return false;

    /* Prepare required group definition reg-exp: */
    const QString strNodePrefix = prefixToString(enmDataType);
    const QString strNodeOptionOpened = optionToString(UIChooserNodeDataOptionType_GroupOpened);
    const QString strDefinitionTemplate = QString("%1(\\S)*=%2").arg(strNodePrefix, strName);
    const QRegExp definitionRegExp(strDefinitionTemplate);
    /* For each the group definition: */
    foreach (const QString &strDefinition, definitions)
    {
        /* Check if this is required definition: */
        if (definitionRegExp.indexIn(strDefinition) == 0)
        {
            /* Get group descriptor: */
            const QString strDescriptor(definitionRegExp.cap(1));
            if (strDescriptor.contains(strNodeOptionOpened))
                return true;
        }
    }

    /* Return 'false' by default: */
    return false;
}

bool UIChooserAbstractModel::shouldGlobalNodeBeFavorite(UIChooserNode *pParentNode) const
{
    /* Read group definitions: */
    const QStringList definitions = gEDataManager->selectorWindowGroupsDefinitions(pParentNode->fullName());
    /* Return 'false' if no definitions found: */
    if (definitions.isEmpty())
        return false;

    /* Prepare required group definition reg-exp: */
    const QString strNodePrefix = prefixToString(UIChooserNodeDataPrefixType_Global);
    const QString strNodeOptionFavorite = optionToString(UIChooserNodeDataOptionType_GlobalFavorite);
    const QString strNodeValueDefault = valueToString(UIChooserNodeDataValueType_GlobalDefault);
    const QString strDefinitionTemplate = QString("%1(\\S)*=%2").arg(strNodePrefix, strNodeValueDefault);
    const QRegExp definitionRegExp(strDefinitionTemplate);
    /* For each the group definition: */
    foreach (const QString &strDefinition, definitions)
    {
        /* Check if this is required definition: */
        if (definitionRegExp.indexIn(strDefinition) == 0)
        {
            /* Get group descriptor: */
            const QString strDescriptor(definitionRegExp.cap(1));
            if (strDescriptor.contains(strNodeOptionFavorite))
                return true;
        }
    }

    /* Return 'false' by default: */
    return false;
}

void UIChooserAbstractModel::wipeOutEmptyGroupsStartingFrom(UIChooserNode *pParent)
{
    /* Cleanup all the group children recursively first: */
    foreach (UIChooserNode *pNode, pParent->nodes(UIChooserNodeType_Group))
        wipeOutEmptyGroupsStartingFrom(pNode);
    /* If parent isn't root and has no nodes: */
    if (!pParent->isRoot() && !pParent->hasNodes())
    {
        /* Delete parent node and item: */
        delete pParent;
    }
}

int UIChooserAbstractModel::getDesiredNodePosition(UIChooserNode *pParentNode,
                                                   UIChooserNodeDataPrefixType enmDataType,
                                                   const QString &strName)
{
    /* End of list (by default)? */
    int iNewNodeDesiredPosition = -1;
    /* Which position should be new node placed by definitions: */
    const int iNewNodeDefinitionPosition = getDefinedNodePosition(pParentNode, enmDataType, strName);

    /* If some position defined: */
    if (iNewNodeDefinitionPosition != -1)
    {
        /* Start of list if some definition present: */
        iNewNodeDesiredPosition = 0;
        /* We have to check all the existing node positions: */
        UIChooserNodeType enmType = UIChooserNodeType_Any;
        switch (enmDataType)
        {
            case UIChooserNodeDataPrefixType_Global:   enmType = UIChooserNodeType_Global; break;
            case UIChooserNodeDataPrefixType_Machine:  enmType = UIChooserNodeType_Machine; break;
            case UIChooserNodeDataPrefixType_Local:
            case UIChooserNodeDataPrefixType_Provider:
            case UIChooserNodeDataPrefixType_Profile:  enmType = UIChooserNodeType_Group; break;
        }
        const QList<UIChooserNode*> nodes = pParentNode->nodes(enmType);
        for (int i = nodes.size() - 1; i >= 0; --i)
        {
            /* Get current node: */
            UIChooserNode *pNode = nodes.at(i);
            AssertPtrReturn(pNode, iNewNodeDesiredPosition);
            /* Which position should be current node placed by definitions? */
            UIChooserNodeDataPrefixType enmNodeDataType = UIChooserNodeDataPrefixType_Global;
            QString strDefinitionName;
            switch (pNode->type())
            {
                case UIChooserNodeType_Machine:
                {
                    enmNodeDataType = UIChooserNodeDataPrefixType_Machine;
                    strDefinitionName = toOldStyleUuid(pNode->toMachineNode()->id());
                    break;
                }
                case UIChooserNodeType_Group:
                {
                    /* Cast to group node: */
                    UIChooserNodeGroup *pGroupNode = pNode->toGroupNode();
                    AssertPtrReturn(pGroupNode, iNewNodeDesiredPosition);
                    switch (pGroupNode->groupType())
                    {
                        case UIChooserNodeGroupType_Local:    enmNodeDataType = UIChooserNodeDataPrefixType_Local; break;
                        case UIChooserNodeGroupType_Provider: enmNodeDataType = UIChooserNodeDataPrefixType_Provider; break;
                        case UIChooserNodeGroupType_Profile:  enmNodeDataType = UIChooserNodeDataPrefixType_Profile; break;
                        default: break;
                    }
                    strDefinitionName = pNode->name();
                    break;
                }
                default:
                    break;
            }
            /* If some position defined: */
            const int iNodeDefinitionPosition = getDefinedNodePosition(pParentNode, enmNodeDataType, strDefinitionName);
            if (iNodeDefinitionPosition != -1)
            {
                AssertReturn(iNodeDefinitionPosition != iNewNodeDefinitionPosition, iNewNodeDesiredPosition);
                if (iNodeDefinitionPosition < iNewNodeDefinitionPosition)
                {
                    iNewNodeDesiredPosition = i + 1;
                    break;
                }
            }
        }
    }

    /* Return desired node position: */
    return iNewNodeDesiredPosition;
}

int UIChooserAbstractModel::getDefinedNodePosition(UIChooserNode *pParentNode, UIChooserNodeDataPrefixType enmDataType, const QString &strName)
{
    /* Read group definitions: */
    const QStringList definitions = gEDataManager->selectorWindowGroupsDefinitions(pParentNode->fullName());
    /* Return 'false' if no definitions found: */
    if (definitions.isEmpty())
        return -1;

    /* Prepare definition reg-exp: */
    QString strDefinitionTemplateShort;
    QString strDefinitionTemplateFull;
    const QString strNodePrefixLocal = prefixToString(UIChooserNodeDataPrefixType_Local);
    const QString strNodePrefixProvider = prefixToString(UIChooserNodeDataPrefixType_Provider);
    const QString strNodePrefixProfile = prefixToString(UIChooserNodeDataPrefixType_Profile);
    const QString strNodePrefixMachine = prefixToString(UIChooserNodeDataPrefixType_Machine);
    switch (enmDataType)
    {
        case UIChooserNodeDataPrefixType_Local:
        {
            strDefinitionTemplateShort = QString("^[%1%2%3](\\S)*=").arg(strNodePrefixLocal, strNodePrefixProvider, strNodePrefixProfile);
            strDefinitionTemplateFull = QString("^%1(\\S)*=%2$").arg(strNodePrefixLocal, strName);
            break;
        }
        case UIChooserNodeDataPrefixType_Provider:
        {
            strDefinitionTemplateShort = QString("^[%1%2%3](\\S)*=").arg(strNodePrefixLocal, strNodePrefixProvider, strNodePrefixProfile);
            strDefinitionTemplateFull = QString("^%1(\\S)*=%2$").arg(strNodePrefixProvider, strName);
            break;
        }
        case UIChooserNodeDataPrefixType_Profile:
        {
            strDefinitionTemplateShort = QString("^[%1%2%3](\\S)*=").arg(strNodePrefixLocal, strNodePrefixProvider, strNodePrefixProfile);
            strDefinitionTemplateFull = QString("^%1(\\S)*=%2$").arg(strNodePrefixProfile, strName);
            break;
        }
        case UIChooserNodeDataPrefixType_Machine:
        {
            strDefinitionTemplateShort = QString("^%1=").arg(strNodePrefixMachine);
            strDefinitionTemplateFull = QString("^%1=%2$").arg(strNodePrefixMachine, strName);
            break;
        }
        default:
            return -1;
    }
    QRegExp definitionRegExpShort(strDefinitionTemplateShort);
    QRegExp definitionRegExpFull(strDefinitionTemplateFull);

    /* For each the definition: */
    int iDefinitionIndex = -1;
    foreach (const QString &strDefinition, definitions)
    {
        /* Check if this definition is of required type: */
        if (definitionRegExpShort.indexIn(strDefinition) == 0)
        {
            ++iDefinitionIndex;
            /* Check if this definition is exactly what we need: */
            if (definitionRegExpFull.indexIn(strDefinition) == 0)
                return iDefinitionIndex;
        }
    }

    /* Return result: */
    return -1;
}

void UIChooserAbstractModel::createLocalMachineNode(UIChooserNode *pParentNode, const CMachine &comMachine)
{
    new UIChooserNodeMachine(pParentNode,
                             getDesiredNodePosition(pParentNode,
                                                    UIChooserNodeDataPrefixType_Machine,
                                                    toOldStyleUuid(comMachine.GetId())),
                             comMachine);
}

void UIChooserAbstractModel::createCloudMachineNode(UIChooserNode *pParentNode, const CCloudMachine &comMachine)
{
    UIChooserNodeMachine *pNode = new UIChooserNodeMachine(pParentNode,
                                                           getDesiredNodePosition(pParentNode,
                                                                                  UIChooserNodeDataPrefixType_Machine,
                                                                                  toOldStyleUuid(comMachine.GetId())),
                                                           comMachine);
    /* Request for async node update if necessary: */
    if (!comMachine.GetAccessible())
    {
        AssertReturnVoid(pNode && pNode->cacheType() == UIVirtualMachineItemType_CloudReal);
        pNode->cache()->toCloud()->updateInfoAsync(false /* delayed? */);
    }
}

QStringList UIChooserAbstractModel::gatherPossibleGroupNodeNames(UIChooserNode *pCurrentNode, QList<UIChooserNode*> exceptions) const
{
    /* Prepare result: */
    QStringList result;

    /* Walk through all the children and make sure there are no exceptions: */
    bool fAddCurrent = true;
    foreach (UIChooserNode *pChild, pCurrentNode->nodes(UIChooserNodeType_Any))
    {
        AssertPtrReturn(pChild, result);
        if (exceptions.contains(pChild))
            fAddCurrent = false;
        else
        {
            if (pChild->type() == UIChooserNodeType_Group)
            {
                UIChooserNodeGroup *pChildGroup = pChild->toGroupNode();
                AssertPtrReturn(pChildGroup, result);
                if (pChildGroup->groupType() == UIChooserNodeGroupType_Local)
                    result << gatherPossibleGroupNodeNames(pChild, exceptions);
            }
        }
    }

    /* Add current item if not overridden: */
    if (fAddCurrent)
        result.prepend(pCurrentNode->fullName());

    /* Return result: */
    return result;
}

void UIChooserAbstractModel::saveGroupSettings()
{
    /* Make sure there is no group settings saving activity: */
    if (UIThreadGroupSettingsSave::instance())
        return;

    /* Prepare full group map: */
    QMap<QString, QStringList> groups;
    gatherGroupSettings(groups, invisibleRoot());

    /* Save information in other thread: */
    UIThreadGroupSettingsSave::prepare();
    emit sigGroupSavingStateChanged();
    connect(UIThreadGroupSettingsSave::instance(), &UIThreadGroupSettingsSave::sigReload,
            this, &UIChooserAbstractModel::sltReloadMachine);
    UIThreadGroupSettingsSave::instance()->configure(this, m_groups, groups);
    UIThreadGroupSettingsSave::instance()->start();
    m_groups = groups;
}

void UIChooserAbstractModel::saveGroupDefinitions()
{
    /* Make sure there is no group definitions save activity: */
    if (UIThreadGroupDefinitionsSave::instance())
        return;

    /* Prepare full group map: */
    QMap<QString, QStringList> groups;
    gatherGroupDefinitions(groups, invisibleRoot());

    /* Save information in other thread: */
    UIThreadGroupDefinitionsSave::prepare();
    emit sigGroupSavingStateChanged();
    UIThreadGroupDefinitionsSave::instance()->configure(this, groups);
    UIThreadGroupDefinitionsSave::instance()->start();
}

void UIChooserAbstractModel::gatherGroupSettings(QMap<QString, QStringList> &settings,
                                                 UIChooserNode *pParentGroup)
{
    /* Iterate over all the machine-nodes: */
    foreach (UIChooserNode *pNode, pParentGroup->nodes(UIChooserNodeType_Machine))
    {
        /* Make sure it's really machine node: */
        AssertPtrReturnVoid(pNode);
        UIChooserNodeMachine *pMachineNode = pNode->toMachineNode();
        AssertPtrReturnVoid(pMachineNode);
        /* Make sure it's local machine node exactly and it's accessible: */
        if (   pMachineNode->cacheType() == UIVirtualMachineItemType_Local
            && pMachineNode->accessible())
            settings[toOldStyleUuid(pMachineNode->id())] << pParentGroup->fullName();
    }
    /* Iterate over all the group-nodes: */
    foreach (UIChooserNode *pNode, pParentGroup->nodes(UIChooserNodeType_Group))
        gatherGroupSettings(settings, pNode);
}

void UIChooserAbstractModel::gatherGroupDefinitions(QMap<QString, QStringList> &definitions,
                                                    UIChooserNode *pParentGroup)
{
    /* Prepare extra-data key for current group: */
    const QString strExtraDataKey = pParentGroup->fullName();
    /* Iterate over all the global-nodes: */
    foreach (UIChooserNode *pNode, pParentGroup->nodes(UIChooserNodeType_Global))
    {
        /* Append node definition: */
        AssertPtrReturnVoid(pNode);
        definitions[strExtraDataKey] << pNode->definition(true /* full */);
    }
    /* Iterate over all the group-nodes: */
    foreach (UIChooserNode *pNode, pParentGroup->nodes(UIChooserNodeType_Group))
    {
        /* Append node definition: */
        AssertPtrReturnVoid(pNode);
        definitions[strExtraDataKey] << pNode->definition(true /* full */);
        /* Go recursively through children: */
        gatherGroupDefinitions(definitions, pNode);
    }
    /* Iterate over all the machine-nodes: */
    foreach (UIChooserNode *pNode, pParentGroup->nodes(UIChooserNodeType_Machine))
    {
        /* Make sure it's really machine node: */
        AssertPtrReturnVoid(pNode);
        UIChooserNodeMachine *pMachineNode = pNode->toMachineNode();
        AssertPtrReturnVoid(pMachineNode);
        /* Append node definition, make sure it's local or real cloud machine node only: */
        if (   pMachineNode->cacheType() == UIVirtualMachineItemType_Local
            || pMachineNode->cacheType() == UIVirtualMachineItemType_CloudReal)
            definitions[strExtraDataKey] << pNode->definition(true /* full */);
    }
}

void UIChooserAbstractModel::makeSureGroupSettingsSaveIsFinished()
{
    /* Cleanup if necessary: */
    if (UIThreadGroupSettingsSave::instance())
        UIThreadGroupSettingsSave::cleanup();
}

void UIChooserAbstractModel::makeSureGroupDefinitionsSaveIsFinished()
{
    /* Cleanup if necessary: */
    if (UIThreadGroupDefinitionsSave::instance())
        UIThreadGroupDefinitionsSave::cleanup();
}


#include "UIChooserAbstractModel.moc"
