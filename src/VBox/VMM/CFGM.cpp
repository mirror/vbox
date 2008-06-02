/* $Id$ */
/** @file
 * CFGM - Configuration Manager.
 *
 * This is the main file of the \ref pg_cfgm "CFGM (Configuration Manager)".
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

/** @page pg_cfgm       CFGM - The Configuration Manager
 *
 * The configuration manager will load and keep the configuration of a VM
 * handy (thru query interface) while the VM is running. The VM properties
 * are organized in a tree and individual nodes can be accessed by normal
 * path walking.
 *
 * Exactly how the CFGM obtains the configuration is specific to the build.
 * The default for a full build is to query it thru the IMachine interface and
 * applies it onto a default setup. It's necessary to have a default in the
 * bottom of this because the IMachine interface doesn't provide all the
 * required details.
 *
 * Devices are given their own subtree where they are protected from accessing
 * information of any parents. The exported PDM callback interfaces makes sure
 * of this.
 *
 * Validating of the data obtained, except for validation of the primitive type,
 * is all up to the user. The CFGM user is concidered in a better position to
 * know the validation rules of the individual properties.
 *
 *
 * @section sec_cfgm_primitives     Data Primitives
 *
 * CFGM supports the following data primitives:
 *      - Integers. Representation is signed 64-bit. Boolean, unsigned and
 *        small integers are all represented using this primitive.
 *      - Zero terminated character strings. As everywhere else
 *        strings are UTF-8.
 *      - Variable length byte strings. This can be used to get/put binary
 *        objects.
 *
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_CFGM
#include <VBox/cfgm.h>
#include <VBox/dbgf.h>
#include <VBox/mm.h>
#include "CFGMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/uuid.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int  cfgmR3CreateDefault(PVM pVM);
static void cfgmR3DumpPath(PCFGMNODE pNode, PCDBGFINFOHLP pHlp);
static void cfgmR3Dump(PCFGMNODE pRoot, unsigned iLevel, PCDBGFINFOHLP pHlp);
static DECLCALLBACK(void) cfgmR3Info(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static int  cfgmR3ResolveNode(PCFGMNODE pNode, const char *pszPath, PCFGMNODE *ppChild);
static int  cfgmR3ResolveLeaf(PCFGMNODE pNode, const char *pszName, PCFGMLEAF *ppLeaf);
static int  cfgmR3InsertLeaf(PCFGMNODE pNode, const char *pszName, PCFGMLEAF *ppLeaf);
static void cfgmR3RemoveLeaf(PCFGMNODE pNode, PCFGMLEAF pLeaf);
static void cfgmR3FreeValue(PCFGMLEAF pLeaf);



/**
 * Constructs the configuration for the VM.
 *
 * @returns VBox status code.
 * @param   pVM                 Pointer to VM which configuration has not yet been loaded.
 * @param   pfnCFGMConstructor  Pointer to callback function for constructing the VM configuration tree.
 *                              This is called in the EM.
 * @param   pvUser              The user argument passed to pfnCFGMConstructor.
 */
CFGMR3DECL(int) CFGMR3Init(PVM pVM, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUser)
{
    LogFlow(("CFGMR3Init: pfnCFGMConstructor=%p pvUser=%p\n", pfnCFGMConstructor, pvUser));

    /*
     * Init data members.
     */
    pVM->cfgm.s.offVM = RT_OFFSETOF(VM, cfgm);
    pVM->cfgm.s.pRoot = NULL;

    /*
     * Register DBGF into item.
     */
    int rc = DBGFR3InfoRegisterInternal(pVM, "cfgm", "Dumps a part of the CFGM tree. The argument indicates where to start.", cfgmR3Info);
    AssertRCReturn(rc,rc);

    /*
     * Create the configuration tree.
     */
    if (pfnCFGMConstructor)
    {
        /*
         * Root Node.
         */
        PCFGMNODE pRoot = (PCFGMNODE)MMR3HeapAllocZ(pVM, MM_TAG_CFGM, sizeof(*pRoot));
        if (!pRoot)
            return VERR_NO_MEMORY;
        pRoot->pVM           = pVM;
        pRoot->cchName       = 0;
        pVM->cfgm.s.pRoot   = pRoot;

        /*
         * Call the constructor.
         */
        rc = pfnCFGMConstructor(pVM, pvUser);
    }
    else
        rc = cfgmR3CreateDefault(pVM);
    if (VBOX_SUCCESS(rc))
    {
        Log(("CFGMR3Init: Successfully constructed the configuration\n"));
        CFGMR3Dump(CFGMR3GetRoot(pVM));

    }
    else
        NOT_DMIK(AssertMsgFailed(("Constructor failed with rc=%Vrc pfnCFGMConstructor=%p\n", rc, pfnCFGMConstructor)));

    return rc;
}


/**
 * Terminates the configuration manager.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 */
CFGMR3DECL(int) CFGMR3Term(PVM pVM)
{
    CFGMR3RemoveNode(pVM->cfgm.s.pRoot);
    return 0;
}


/**
 * Gets the root node for the VM.
 *
 * @returns Pointer to root node.
 * @param   pVM             VM handle.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetRoot(PVM pVM)
{
    return pVM->cfgm.s.pRoot;
}


/**
 * Gets the parent of a CFGM node.
 *
 * @returns Pointer to the parent node.
 * @returns NULL if pNode is Root or pNode is the start of a
 *          restricted subtree (use CFGMr3GetParentEx() for that).
 *
 * @param   pNode           The node which parent we query.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetParent(PCFGMNODE pNode)
{
    if (pNode && !pNode->fRestrictedRoot)
        return pNode->pParent;
    return NULL;
}


/**
 * Gets the parent of a CFGM node.
 *
 * @returns Pointer to the parent node.
 * @returns NULL if pNode is Root or pVM is not correct.
 *
 * @param   pVM             The VM handle, used as token that the caller is trusted.
 * @param   pNode           The node which parent we query.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetParentEx(PVM pVM, PCFGMNODE pNode)
{
    if (pNode && pNode->pVM == pVM)
        return pNode->pParent;
    return NULL;
}


/**
 * Query a child node.
 *
 * @returns Pointer to the specified node.
 * @returns NULL if node was not found or pNode is NULL.
 * @param   pNode           Node pszPath is relative to.
 * @param   pszPath         Path to the child node or pNode.
 *                          It's good style to end this with '/'.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetChild(PCFGMNODE pNode, const char *pszPath)
{
    PCFGMNODE pChild;
    int rc = cfgmR3ResolveNode(pNode, pszPath, &pChild);
    if (VBOX_SUCCESS(rc))
        return pChild;
    return NULL;
}


/**
 * Query a child node by a format string.
 *
 * @returns Pointer to the specified node.
 * @returns NULL if node was not found or pNode is NULL.
 * @param   pNode           Node pszPath is relative to.
 * @param   pszPathFormat   Path to the child node or pNode.
 *                          It's good style to end this with '/'.
 * @param   ...             Arguments to pszPathFormat.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetChildF(PCFGMNODE pNode, const char *pszPathFormat, ...)
{
    va_list Args;
    va_start(Args,  pszPathFormat);
    PCFGMNODE pRet = CFGMR3GetChildFV(pNode, pszPathFormat, Args);
    va_end(Args);
    return pRet;
}


/**
 * Query a child node by a format string.
 *
 * @returns Pointer to the specified node.
 * @returns NULL if node was not found or pNode is NULL.
 * @param   pNode           Node pszPath is relative to.
 * @param   pszPathFormat   Path to the child node or pNode.
 *                          It's good style to end this with '/'.
 * @param   Args            Arguments to pszPathFormat.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetChildFV(PCFGMNODE pNode, const char *pszPathFormat, va_list Args)
{
    char *pszPath;
    RTStrAPrintfV(&pszPath, pszPathFormat, Args);
    if (pszPath)
    {
        PCFGMNODE pChild;
        int rc = cfgmR3ResolveNode(pNode, pszPath, &pChild);
        if (VBOX_SUCCESS(rc))
            return pChild;
        RTStrFree(pszPath);
    }
    return NULL;
}


/**
 * Gets the first child node.
 * Use this to start an enumeration of child nodes.
 *
 * @returns Pointer to the first child.
 * @returns NULL if no children.
 * @param   pNode           Node to enumerate children for.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetFirstChild(PCFGMNODE pNode)
{
    return pNode ? pNode->pFirstChild : NULL;
}


/**
 * Gets the next sibling node.
 * Use this to continue an enumeration.
 *
 * @returns Pointer to the first child.
 * @returns NULL if no children.
 * @param   pCur            Node to returned by a call to CFGMR3GetFirstChild()
 *                          or successive calls to this function.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetNextChild(PCFGMNODE pCur)
{
    return pCur ? pCur->pNext : NULL;
}


/**
 * Gets the name of the current node.
 * (Needed for enumeration.)
 *
 * @returns VBox status code.
 * @param   pCur            Node to returned by a call to CFGMR3GetFirstChild()
 *                          or successive calls to CFGMR3GetNextChild().
 * @param   pszName         Where to store the node name.
 * @param   cchName         Size of the buffer pointed to by pszName (with terminator).
 */
CFGMR3DECL(int) CFGMR3GetName(PCFGMNODE pCur, char *pszName, size_t cchName)
{
    int rc;
    if (pCur)
    {
        if (cchName > pCur->cchName)
        {
            rc = VINF_SUCCESS;
            memcpy(pszName, pCur->szName, pCur->cchName + 1);
        }
        else
            rc = VERR_CFGM_NOT_ENOUGH_SPACE;
    }
    else
        rc = VERR_CFGM_NO_NODE;
    return rc;
}


/**
 * Gets the length of the current node's name.
 * (Needed for enumeration.)
 *
 * @returns Node name length in bytes including the terminating null char.
 * @returns 0 if pCur is NULL.
 * @param   pCur            Node to returned by a call to CFGMR3GetFirstChild()
 *                          or successive calls to CFGMR3GetNextChild().
 */
CFGMR3DECL(int) CFGMR3GetNameLen(PCFGMNODE pCur)
{
    return pCur ? pCur->cchName + 1 : 0;
}


/**
 * Validates that the child nodes are within a set of valid names.
 *
 * @returns true if all names are found in pszzAllowed.
 * @returns false if not.
 * @param   pNode           The node which children should be examined.
 * @param   pszzValid       List of valid names separated by '\\0' and ending with
 *                          a double '\\0'.
 */
CFGMR3DECL(bool) CFGMR3AreChildrenValid(PCFGMNODE pNode, const char *pszzValid)
{
    if (pNode)
    {
        for (PCFGMNODE pChild = pNode->pFirstChild; pChild; pChild = pChild->pNext)
        {
            /* search pszzValid for the name */
            const char *psz = pszzValid;
            while (*psz)
            {
                size_t cch = strlen(psz);
                if (    cch == pChild->cchName
                    &&  !memcmp(psz, pChild->szName, cch))
                    break;

                /* next */
                psz += cch + 1;
            }

            /* if at end of pszzValid we didn't find it => failure */
            if (!*psz)
            {
                AssertMsgFailed(("Couldn't find '%s' in the valid values\n", pChild->szName));
                return false;
            }
        }
    }

    /* all ok. */
    return true;
}


/**
 * Gets the first value of a node.
 * Use this to start an enumeration of values.
 *
 * @returns Pointer to the first value.
 * @param   pCur            The node (Key) which values to enumerate.
 */
CFGMR3DECL(PCFGMLEAF) CFGMR3GetFirstValue(PCFGMNODE pCur)
{
    return pCur ? pCur->pFirstLeaf : NULL;
}

/**
 * Gets the next value in enumeration.
 *
 * @returns Pointer to the next value.
 * @param   pCur            The current value as returned by this function or CFGMR3GetFirstValue().
 */
CFGMR3DECL(PCFGMLEAF) CFGMR3GetNextValue(PCFGMLEAF pCur)
{
    return pCur ? pCur->pNext : NULL;
}

/**
 * Get the value name.
 * (Needed for enumeration.)
 *
 * @returns VBox status code.
 * @param   pCur            Value returned by a call to CFGMR3GetFirstValue()
 *                          or successive calls to CFGMR3GetNextValue().
 * @param   pszName         Where to store the value name.
 * @param   cchName         Size of the buffer pointed to by pszName (with terminator).
 */
CFGMR3DECL(int) CFGMR3GetValueName(PCFGMLEAF pCur, char *pszName, size_t cchName)
{
    int rc;
    if (pCur)
    {
        if (cchName > pCur->cchName)
        {
            rc = VINF_SUCCESS;
            memcpy(pszName, pCur->szName, pCur->cchName + 1);
        }
        else
            rc = VERR_CFGM_NOT_ENOUGH_SPACE;
    }
    else
        rc = VERR_CFGM_NO_NODE;
    return rc;
}


/**
 * Gets the length of the current node's name.
 * (Needed for enumeration.)
 *
 * @returns Value name length in bytes including the terminating null char.
 * @returns 0 if pCur is NULL.
 * @param   pCur            Value returned by a call to CFGMR3GetFirstValue()
 *                          or successive calls to CFGMR3GetNextValue().
 */
CFGMR3DECL(int) CFGMR3GetValueNameLen(PCFGMLEAF pCur)
{
    return pCur ? pCur->cchName + 1 : 0;
}

/**
 * Gets the value type.
 * (For enumeration.)
 *
 * @returns VBox status code.
 * @param   pCur            Value returned by a call to CFGMR3GetFirstValue()
 *                          or successive calls to CFGMR3GetNextValue().
 */
CFGMR3DECL(CFGMVALUETYPE) CFGMR3GetValueType(PCFGMLEAF pCur)
{
    Assert(pCur);
    return pCur->enmType;
}


/**
 * Validates that the values are within a set of valid names.
 *
 * @returns true if all names are found in pszzAllowed.
 * @returns false if not.
 * @param   pNode           The node which values should be examined.
 * @param   pszzValid       List of valid names separated by '\\0' and ending with
 *                          a double '\\0'.
 */
CFGMR3DECL(bool) CFGMR3AreValuesValid(PCFGMNODE pNode, const char *pszzValid)
{
    if (pNode)
    {
        for (PCFGMLEAF pLeaf = pNode->pFirstLeaf; pLeaf; pLeaf = pLeaf->pNext)
        {
            /* search pszzValid for the name */
            const char *psz = pszzValid;
            while (*psz)
            {
                size_t cch = strlen(psz);
                if (    cch == pLeaf->cchName
                    &&  !memcmp(psz, pLeaf->szName, cch))
                    break;

                /* next */
                psz += cch + 1;
            }

            /* if at end of pszzValid we didn't find it => failure */
            if (!*psz)
            {
                AssertMsgFailed(("Couldn't find '%s' in the valid values\n", pLeaf->szName));
                return false;
            }
        }
    }

    /* all ok. */
    return true;
}



/**
 * Query value type.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   penmType        Where to store the type.
 */
CFGMR3DECL(int) CFGMR3QueryType(PCFGMNODE pNode, const char *pszName, PCFGMVALUETYPE penmType)
{
    PCFGMLEAF pLeaf;
    int rc = cfgmR3ResolveLeaf(pNode, pszName, &pLeaf);
    if (VBOX_SUCCESS(rc))
    {
        if (penmType)
            *penmType = pLeaf->enmType;
    }
    return rc;
}


/**
 * Query value size.
 * This works on all types of values.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pcb             Where to store the value size.
 */
CFGMR3DECL(int) CFGMR3QuerySize(PCFGMNODE pNode, const char *pszName, size_t *pcb)
{
    PCFGMLEAF pLeaf;
    int rc = cfgmR3ResolveLeaf(pNode, pszName, &pLeaf);
    if (VBOX_SUCCESS(rc))
    {
        switch (pLeaf->enmType)
        {
            case CFGMVALUETYPE_INTEGER:
                *pcb = sizeof(pLeaf->Value.Integer.u64);
                break;

            case CFGMVALUETYPE_STRING:
                *pcb = pLeaf->Value.String.cch;
                break;

            case CFGMVALUETYPE_BYTES:
                *pcb = pLeaf->Value.Bytes.cb;
                break;

            default:
                rc = VERR_INTERNAL_ERROR;
                AssertMsgFailed(("Invalid value type %d\n", pLeaf->enmType));
                break;
        }
    }
    return rc;
}


/**
 * Query integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu64            Where to store the integer value.
 */
CFGMR3DECL(int) CFGMR3QueryInteger(PCFGMNODE pNode, const char *pszName, uint64_t *pu64)
{
    PCFGMLEAF pLeaf;
    int rc = cfgmR3ResolveLeaf(pNode, pszName, &pLeaf);
    if (VBOX_SUCCESS(rc))
    {
        if (pLeaf->enmType == CFGMVALUETYPE_INTEGER)
            *pu64 = pLeaf->Value.Integer.u64;
        else
            rc = VERR_CFGM_NOT_INTEGER;
    }
    return rc;
}


/**
 * Query integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu64            Where to store the integer value.
 * @param   u64Def          The default value.
 */
CFGMR3DECL(int) CFGMR3QueryIntegerDef(PCFGMNODE pNode, const char *pszName, uint64_t *pu64, uint64_t u64Def)
{
    PCFGMLEAF pLeaf;
    int rc = cfgmR3ResolveLeaf(pNode, pszName, &pLeaf);
    if (VBOX_SUCCESS(rc))
    {
        if (pLeaf->enmType == CFGMVALUETYPE_INTEGER)
            *pu64 = pLeaf->Value.Integer.u64;
        else
            rc = VERR_CFGM_NOT_INTEGER;
    }
    else if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
    {
        *pu64 = u64Def;
        rc = VINF_SUCCESS;
    }
    return rc;
}


/**
 * Query zero terminated character value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of a zero terminate character value.
 * @param   pszString       Where to store the string.
 * @param   cchString       Size of the string buffer. (Includes terminator.)
 */
CFGMR3DECL(int) CFGMR3QueryString(PCFGMNODE pNode, const char *pszName, char *pszString, size_t cchString)
{
    PCFGMLEAF pLeaf;
    int rc = cfgmR3ResolveLeaf(pNode, pszName, &pLeaf);
    if (VBOX_SUCCESS(rc))
    {
        if (pLeaf->enmType == CFGMVALUETYPE_STRING)
        {
            if (cchString >= pLeaf->Value.String.cch)
            {
                memcpy(pszString, pLeaf->Value.String.psz, pLeaf->Value.String.cch);
                memset(pszString + pLeaf->Value.String.cch, 0, cchString - pLeaf->Value.String.cch);
            }
            else
                rc = VERR_CFGM_NOT_ENOUGH_SPACE;
        }
        else
            rc = VERR_CFGM_NOT_STRING;
    }
    return rc;
}


/**
 * Query zero terminated character value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of a zero terminate character value.
 * @param   pszString       Where to store the string.
 * @param   cchString       Size of the string buffer. (Includes terminator.)
 * @param   pszDef          The default value.
 */
CFGMR3DECL(int) CFGMR3QueryStringDef(PCFGMNODE pNode, const char *pszName, char *pszString, size_t cchString, const char *pszDef)
{
    PCFGMLEAF pLeaf;
    int rc = cfgmR3ResolveLeaf(pNode, pszName, &pLeaf);
    if (VBOX_SUCCESS(rc))
    {
        if (pLeaf->enmType == CFGMVALUETYPE_STRING)
        {
            if (cchString >= pLeaf->Value.String.cch)
            {
                memcpy(pszString, pLeaf->Value.String.psz, pLeaf->Value.String.cch);
                memset(pszString + pLeaf->Value.String.cch, 0, cchString - pLeaf->Value.String.cch);
            }
            else
                rc = VERR_CFGM_NOT_ENOUGH_SPACE;
        }
        else
            rc = VERR_CFGM_NOT_STRING;
    }
    else if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
    {
        size_t cchDef = strlen(pszDef);
        if (cchString > cchDef)
        {
            memcpy(pszString, pszDef, cchDef);
            memset(pszString + cchDef, 0, cchString - cchDef);
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_CFGM_NOT_ENOUGH_SPACE;
    }
    return rc;
}


/**
 * Query byte string value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of a byte string value.
 * @param   pvData          Where to store the binary data.
 * @param   cbData          Size of buffer pvData points too.
 */
CFGMR3DECL(int) CFGMR3QueryBytes(PCFGMNODE pNode, const char *pszName, void *pvData, size_t cbData)
{
    PCFGMLEAF pLeaf;
    int rc = cfgmR3ResolveLeaf(pNode, pszName, &pLeaf);
    if (VBOX_SUCCESS(rc))
    {
        if (pLeaf->enmType == CFGMVALUETYPE_BYTES)
        {
            if (cbData >= pLeaf->Value.Bytes.cb)
            {
                memcpy(pvData, pLeaf->Value.Bytes.pau8, pLeaf->Value.Bytes.cb);
                memset((char *)pvData + pLeaf->Value.Bytes.cb, 0, cbData - pLeaf->Value.Bytes.cb);
            }
            else
                rc = VERR_CFGM_NOT_ENOUGH_SPACE;
        }
        else
            rc = VERR_CFGM_NOT_BYTES;
    }
    return rc;
}


/**
 * Creates the default configuration.
 * This assumes an empty tree.
 *
 * @returns VBox status code.
 * @param   pVM     VM handle.
 */
static int cfgmR3CreateDefault(PVM pVM)
{
    int rc;
    int rcAll = VINF_SUCCESS;
#define UPDATERC() do { if (VBOX_FAILURE(rc) && VBOX_SUCCESS(rcAll)) rcAll = rc; } while (0)

    /*
     * Root level.
     */
    PCFGMNODE pRoot = (PCFGMNODE)MMR3HeapAllocZ(pVM, MM_TAG_CFGM, sizeof(*pRoot));
    if (!pRoot)
        return VERR_NO_MEMORY;
    pRoot->pVM           = pVM;
    pRoot->cchName       = 0;

    Assert(!pVM->cfgm.s.pRoot);
    pVM->cfgm.s.pRoot   = pRoot;

    /*
     * Create VM default values.
     */
    rc = CFGMR3InsertString(pRoot,  "Name",                 "Default VM");
    UPDATERC();
    rc = CFGMR3InsertInteger(pRoot, "RamSize",              128 * _1M);
    UPDATERC();
    rc = CFGMR3InsertInteger(pRoot, "TimerMillies",         10);
    UPDATERC();
    rc = CFGMR3InsertInteger(pRoot, "RawR3Enabled",         1);
    UPDATERC();
    /** @todo CFGM Defaults: RawR0, PATMEnabled and CASMEnabled needs attention later. */
    rc = CFGMR3InsertInteger(pRoot, "RawR0Enabled",         1);
    UPDATERC();
    rc = CFGMR3InsertInteger(pRoot, "PATMEnabled",          1);
    UPDATERC();
    rc = CFGMR3InsertInteger(pRoot, "CSAMEnabled",          1);
    UPDATERC();

    /*
     * PDM.
     */
    PCFGMNODE pPdm;
    rc = CFGMR3InsertNode(pRoot, "PDM", &pPdm);
    UPDATERC();
    PCFGMNODE pDevices = NULL;
    rc = CFGMR3InsertNode(pPdm, "Devices", &pDevices);
    UPDATERC();
    rc = CFGMR3InsertInteger(pDevices, "LoadBuiltin",       1); /* boolean */
    UPDATERC();
    PCFGMNODE pDrivers = NULL;
    rc = CFGMR3InsertNode(pPdm, "Drivers", &pDrivers);
    UPDATERC();
    rc = CFGMR3InsertInteger(pDrivers, "LoadBuiltin",       1); /* boolean */
    UPDATERC();


    /*
     * Devices
     */
    pDevices = NULL;
    rc = CFGMR3InsertNode(pRoot, "Devices", &pDevices);
    UPDATERC();
    /* device */
    PCFGMNODE pDev = NULL;
    PCFGMNODE pInst = NULL;
    PCFGMNODE pCfg = NULL;
#if 0
    PCFGMNODE pLunL0 = NULL;
    PCFGMNODE pLunL1 = NULL;
#endif

    /*
     * PC Arch.
     */
    rc = CFGMR3InsertNode(pDevices, "pcarch", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * PC Bios.
     */
    rc = CFGMR3InsertNode(pDevices, "pcbios", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();
    rc = CFGMR3InsertInteger(pCfg,  "RamSize",              128 * _1M);
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice0",          "IDE");
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice1",          "NONE");
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice2",          "NONE");
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "BootDevice3",          "NONE");
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "HardDiskDevice",       "piix3ide");
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "FloppyDevice",         "");
    UPDATERC();
    RTUUID Uuid;
    RTUuidClear(&Uuid);
    rc = CFGMR3InsertBytes(pCfg,    "UUID", &Uuid, sizeof(Uuid));
    UPDATERC();

    /*
     * PCI bus.
     */
    rc = CFGMR3InsertNode(pDevices, "pci", &pDev); /* piix3 */
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * PS/2 keyboard & mouse
     */
    rc = CFGMR3InsertNode(pDevices, "pckbd", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();
#if 0
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);
    UPDATERC();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "KeyboardQueue");
    UPDATERC();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);
    UPDATERC();
    rc = CFGMR3InsertInteger(pCfg,  "QueueSize",            64);
    UPDATERC();
    rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);
    UPDATERC();
    rc = CFGMR3InsertString(pLunL1, "Driver",               "MainKeyboard");
    UPDATERC();
    rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);
    UPDATERC();
#endif
#if 0
    rc = CFGMR3InsertNode(pInst,    "LUN#1", &pLunL0);
    UPDATERC();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MouseQueue");
    UPDATERC();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);
    UPDATERC();
    rc = CFGMR3InsertInteger(pCfg,  "QueueSize",            128);
    UPDATERC();
    rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);
    UPDATERC();
    rc = CFGMR3InsertString(pLunL1, "Driver",               "MainMouse");
    UPDATERC();
    rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);
    UPDATERC();
#endif

    /*
     * i8254 Programmable Interval Timer And Dummy Speaker
     */
    rc = CFGMR3InsertNode(pDevices, "i8254", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
#ifdef DEBUG
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
#endif
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * i8259 Programmable Interrupt Controller.
     */
    rc = CFGMR3InsertNode(pDevices, "i8259", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * RTC MC146818.
     */
    rc = CFGMR3InsertNode(pDevices, "mc146818", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();

    /*
     * VGA.
     */
    rc = CFGMR3InsertNode(pDevices, "vga", &pDev);
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();
    rc = CFGMR3InsertInteger(pCfg,  "VRamSize",             4 * _1M);
    UPDATERC();

    /* Bios logo. */
    rc = CFGMR3InsertInteger(pCfg,  "FadeIn",               1);
    UPDATERC();
    rc = CFGMR3InsertInteger(pCfg,  "FadeOut",              1);
    UPDATERC();
    rc = CFGMR3InsertInteger(pCfg,  "LogoTime",             0);
    UPDATERC();
    rc = CFGMR3InsertString(pCfg,   "LogoFile",             "");
    UPDATERC();

#if 0
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);
    UPDATERC();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainDisplay");
    UPDATERC();
#endif

    /*
     * IDE controller.
     */
    rc = CFGMR3InsertNode(pDevices, "piix3ide", &pDev); /* piix3 */
    UPDATERC();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);
    UPDATERC();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);         /* boolean */
    UPDATERC();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);
    UPDATERC();



    /*
     * ...
     */

#undef UPDATERC
    return rcAll;
}




/**
 * Resolves a path reference to a child node.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszPath         Path to the child node.
 * @param   ppChild         Where to store the pointer to the child node.
 */
static int cfgmR3ResolveNode(PCFGMNODE pNode, const char *pszPath, PCFGMNODE *ppChild)
{
    if (pNode)
    {
        PCFGMNODE pChild = NULL;
        for (;;)
        {
            /* skip leading slashes. */
            while (*pszPath == '/')
                pszPath++;

            /* End of path? */
            if (!*pszPath)
            {
                if (!pChild)
                    return VERR_CFGM_INVALID_CHILD_PATH;
                *ppChild = pChild;
                return VINF_SUCCESS;
            }

            /* find end of component. */
            const char *pszNext = strchr(pszPath, '/');
            if (!pszNext)
                pszNext = strchr(pszPath,  '\0');
            RTUINT cchName = pszNext - pszPath;

            /* search child list. */
            pChild = pNode->pFirstChild;
            for ( ; pChild; pChild = pChild->pNext)
                if (    pChild->cchName == cchName
                    &&  !memcmp(pszPath, pChild->szName, cchName) )
                    break;

            /* if not found, we're done. */
            if (!pChild)
                return VERR_CFGM_CHILD_NOT_FOUND;

            /* next iteration */
            pNode = pChild;
            pszPath = pszNext;
        }

        /* won't get here */
    }
    else
        return VERR_CFGM_NO_PARENT;
}


/**
 * Resolves a path reference to a child node.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of a byte string value.
 * @param   ppLeaf          Where to store the pointer to the leaf node.
 */
static int cfgmR3ResolveLeaf(PCFGMNODE pNode, const char *pszName, PCFGMLEAF *ppLeaf)
{
    int rc;
    if (pNode)
    {
        RTUINT      cchName = strlen(pszName);
        PCFGMLEAF   pLeaf = pNode->pFirstLeaf;
        while (pLeaf)
        {
            if (    cchName == pLeaf->cchName
                && !memcmp(pszName, pLeaf->szName, cchName) )
            {
                *ppLeaf = pLeaf;
                return VINF_SUCCESS;
            }

            /* next */
            pLeaf = pLeaf->pNext;
        }
        rc = VERR_CFGM_VALUE_NOT_FOUND;
    }
    else
        rc = VERR_CFGM_NO_PARENT;
    return rc;
}



/**
 * Creates a CFGM tree.
 *
 * This is intended for creating device/driver configs can be
 * passed around and later attached to the main tree in the
 * correct location.
 *
 * @returns Pointer to the root node.
 * @param   pVM         The VM handle.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3CreateTree(PVM pVM)
{
    PCFGMNODE pNew = (PCFGMNODE)MMR3HeapAlloc(pVM, MM_TAG_CFGM, sizeof(*pNew));
    if (pNew)
    {
        pNew->pPrev         = NULL;
        pNew->pNext         = NULL;
        pNew->pParent       = NULL;
        pNew->pFirstChild   = NULL;
        pNew->pFirstLeaf    = NULL;
        pNew->pVM           = pVM;
        pNew->fRestrictedRoot = false;
        pNew->cchName       = 0;
        pNew->szName[0]     = 0;
    }
    return pNew;
}


/**
 * Insert subtree.
 *
 * This function inserts (no duplication) a tree created by CFGMR3CreateTree()
 * into the main tree.
 *
 * The root node of the inserted subtree will need to be reallocated, which
 * effectually means that the passed in pSubTree handle becomes invalid
 * upon successful return. Use the value returned in ppChild instead
 * of pSubTree.
 *
 * @returns VBox status code.
 * @returns VERR_CFGM_NODE_EXISTS if the final child node name component exists.
 * @param   pNode       Parent node.
 * @param   pszName     Name or path of the new child node.
 * @param   pSubTree    The subtree to insert. Must be returned by CFGMR3CreateTree().
 * @param   ppChild     Where to store the address of the new child node. (optional)
 */
CFGMR3DECL(int) CFGMR3InsertSubTree(PCFGMNODE pNode, const char *pszName, PCFGMNODE pSubTree, PCFGMNODE *ppChild)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pSubTree, VERR_INVALID_POINTER);
    AssertReturn(!pSubTree->pParent, VERR_INVALID_PARAMETER);
    AssertReturn(pSubTree->pVM, VERR_INVALID_PARAMETER);
    AssertReturn(pSubTree->pParent != pSubTree->pVM->cfgm.s.pRoot, VERR_INVALID_PARAMETER);
    Assert(!pSubTree->pNext);
    Assert(!pSubTree->pPrev);

    /*
     * Use CFGMR3InsertNode to create a new node and then
     * re-attach the children and leafs of the subtree to it.
     */
    PCFGMNODE pNewChild;
    int rc = CFGMR3InsertNode(pNode, pszName, &pNewChild);
    if (RT_SUCCESS(rc))
    {
        Assert(!pNewChild->pFirstChild);
        pNewChild->pFirstChild = pSubTree->pFirstChild;
        Assert(!pNewChild->pFirstLeaf);
        pNewChild->pFirstLeaf = pSubTree->pFirstLeaf;
        if (ppChild)
            *ppChild = pNewChild;

        /* free the old subtree root */
        pSubTree->pVM = NULL;
        pSubTree->pFirstLeaf = NULL;
        pSubTree->pFirstChild = NULL;
        MMR3HeapFree(pSubTree);
    }
    return rc;
}


/**
 * Insert a node.
 *
 * @returns VBox status code.
 * @returns VERR_CFGM_NODE_EXISTS if the final child node name component exists.
 * @param   pNode       Parent node.
 * @param   pszName     Name or path of the new child node.
 * @param   ppChild     Where to store the address of the new child node. (optional)
 */
CFGMR3DECL(int) CFGMR3InsertNode(PCFGMNODE pNode, const char *pszName, PCFGMNODE *ppChild)
{
    int rc;
    if (pNode)
    {
        /*
         * If given a path we have to deal with it component by compontent.
         */
        while (*pszName == '/')
            pszName++;
        if (strchr(pszName, '/'))
        {
            char *pszDup = RTStrDup(pszName);
            if (pszDup)
            {
                char *psz = pszDup;
                for (;;)
                {
                    /* Terminate at '/' and find the next component. */
                    char *pszNext = strchr(psz, '/');
                    if (pszNext)
                    {
                        *pszNext++ = '\0';
                        while (*pszNext == '/')
                            pszNext++;
                        if (*pszNext == '\0')
                            pszNext = NULL;
                    }

                    /* does it exist? */
                    PCFGMNODE pChild = CFGMR3GetChild(pNode, psz);
                    if (!pChild)
                    {
                        /* no, insert it */
                        rc = CFGMR3InsertNode(pNode, psz, &pChild);
                        if (VBOX_FAILURE(rc))
                            break;
                        if (!pszNext)
                        {
                            if (ppChild)
                                *ppChild = pChild;
                            break;
                        }

                    }
                    /* if last component fail */
                    else if (!pszNext)
                    {
                        rc = VERR_CFGM_NODE_EXISTS;
                        break;
                    }

                    /* next */
                    pNode = pChild;
                    psz = pszNext;
                }
                RTStrFree(pszDup);
            }
            else
                rc = VERR_NO_TMP_MEMORY;
        }
        /*
         * Not multicomponent, just make sure it's a non-zero name.
         */
        else if (*pszName)
        {
            /*
             * Check if already exists and find last node in chain.
             */
            size_t cchName = strlen(pszName);
            PCFGMNODE pPrev = pNode->pFirstChild;
            if (pPrev)
            {
                for (;; pPrev = pPrev->pNext)
                {
                    if (    cchName == pPrev->cchName
                        &&  !memcmp(pszName, pPrev->szName, cchName))
                        return VERR_CFGM_NODE_EXISTS;
                    if (!pPrev->pNext)
                        break;
                }
            }

            /*
             * Allocate and init node.
             */
            PCFGMNODE pNew = (PCFGMNODE)MMR3HeapAlloc(pNode->pVM, MM_TAG_CFGM, sizeof(*pNew) + cchName);
            if (pNew)
            {
                pNew->pParent       = pNode;
                pNew->pFirstChild   = NULL;
                pNew->pFirstLeaf    = NULL;
                pNew->pVM           = pNode->pVM;
                pNew->fRestrictedRoot = false;
                pNew->cchName       = cchName;
                memcpy(pNew->szName, pszName, cchName + 1);

                /*
                 * Insert into child list.
                 */
                pNew->pNext         = NULL;
                pNew->pPrev         = pPrev;
                if (pPrev)
                    pPrev->pNext    = pNew;
                else
                    pNode->pFirstChild = pNew;
                if (ppChild)
                    *ppChild = pNew;
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else
        {
            rc = VERR_CFGM_INVALID_NODE_PATH;
            AssertMsgFailed(("Invalid path %s\n", pszName));
        }
    }
    else
    {
        rc = VERR_CFGM_NO_PARENT;
        AssertMsgFailed(("No parent! path %s\n", pszName));
    }

    return rc;
}


/**
 * Insert a node, format string name.
 *
 * @returns VBox status     code.
 * @param   pNode           Parent node.
 * @param   ppChild         Where to store the address of the new child node. (optional)
 * @param   pszNameFormat   Name of or path the new child node.
 * @param   ...             Name format arguments.
 */
CFGMR3DECL(int) CFGMR3InsertNodeF(PCFGMNODE pNode, PCFGMNODE *ppChild, const char *pszNameFormat, ...)
{
    va_list Args;
    va_start(Args,  pszNameFormat);
    int rc = CFGMR3InsertNodeFV(pNode, ppChild, pszNameFormat, Args);
    va_end(Args);
    return rc;
}


/**
 * Insert a node, format string name.
 *
 * @returns VBox status code.
 * @param   pNode           Parent node.
 * @param   ppChild         Where to store the address of the new child node. (optional)
 * @param   pszNameFormat   Name or path of the new child node.
 * @param   Args            Name format arguments.
 */
CFGMR3DECL(int) CFGMR3InsertNodeFV(PCFGMNODE pNode, PCFGMNODE *ppChild, const char *pszNameFormat, va_list Args)
{
    int     rc;
    char   *pszName;
    RTStrAPrintfV(&pszName, pszNameFormat, Args);
    if (pszName)
    {
        rc = CFGMR3InsertNode(pNode, pszName, ppChild);
        RTStrFree(pszName);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Marks the node as the root of a restricted subtree, i.e. the end of
 * a CFGMR3GetParent() journey.
 *
 * @param   pNode       The node to mark.
 */
CFGMR3DECL(void) CFGMR3SetRestrictedRoot(PCFGMNODE pNode)
{
    if (pNode)
        pNode->fRestrictedRoot = true;
}


/**
 * Insert a node.
 *
 * @returns VBox status code.
 * @param   pNode       Parent node.
 * @param   pszName     Name of the new child node.
 * @param   ppLeaf      Where to store the new leaf.
 *                      The caller must fill in the enmType and Value fields!
 */
static int cfgmR3InsertLeaf(PCFGMNODE pNode, const char *pszName, PCFGMLEAF *ppLeaf)
{
    int rc;
    if (*pszName)
    {
        if (pNode)
        {
            /*
             * Check if already exists and find last node in chain.
             */
            size_t cchName = strlen(pszName);
            PCFGMLEAF pPrev = pNode->pFirstLeaf;
            if (pPrev)
            {
                for (;; pPrev = pPrev->pNext)
                {
                    if (    cchName == pPrev->cchName
                        &&  !memcmp(pszName, pPrev->szName, cchName))
                        return VERR_CFGM_LEAF_EXISTS;
                    if (!pPrev->pNext)
                        break;
                }
            }

            /*
             * Allocate and init node.
             */
            PCFGMLEAF pNew = (PCFGMLEAF)MMR3HeapAlloc(pNode->pVM, MM_TAG_CFGM, sizeof(*pNew) + cchName);
            if (pNew)
            {
                pNew->cchName       = cchName;
                memcpy(pNew->szName, pszName, cchName + 1);

                /*
                 * Insert into child list.
                 */
                pNew->pNext         = NULL;
                pNew->pPrev         = pPrev;
                if (pPrev)
                    pPrev->pNext    = pNew;
                else
                    pNode->pFirstLeaf = pNew;
                *ppLeaf = pNew;
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_CFGM_NO_PARENT;
    }
    else
        rc = VERR_CFGM_INVALID_CHILD_PATH;
    return rc;
}


/**
 * Remove a node.
 *
 * @param   pNode       Parent node.
 */
CFGMR3DECL(void) CFGMR3RemoveNode(PCFGMNODE pNode)
{
    if (pNode)
    {
        /*
         * Free children.
         */
        while (pNode->pFirstChild)
            CFGMR3RemoveNode(pNode->pFirstChild);

        /*
         * Free leafs.
         */
        while (pNode->pFirstLeaf)
            cfgmR3RemoveLeaf(pNode, pNode->pFirstLeaf);

        /*
         * Unlink ourselves.
         */
        if (pNode->pPrev)
            pNode->pPrev->pNext = pNode->pNext;
        else
        {
            if (pNode->pParent)
                pNode->pParent->pFirstChild = pNode->pNext;
            else if (pNode == pNode->pVM->cfgm.s.pRoot) /* might be a different tree */
                pNode->pVM->cfgm.s.pRoot = NULL;
        }
        if (pNode->pNext)
            pNode->pNext->pPrev = pNode->pPrev;

        /*
         * Free ourselves. (bit of paranoia first)
         */
        pNode->pVM = NULL;
        pNode->pNext = NULL;
        pNode->pPrev = NULL;
        pNode->pParent = NULL;
        MMR3HeapFree(pNode);
    }
}


/**
 * Removes a leaf.
 *
 * @param   pNode   Parent node.
 * @param   pLeaf   Leaf to remove.
 */
static void cfgmR3RemoveLeaf(PCFGMNODE pNode, PCFGMLEAF pLeaf)
{
    if (pNode && pLeaf)
    {
        /*
         * Unlink.
         */
        if (pLeaf->pPrev)
            pLeaf->pPrev->pNext = pLeaf->pNext;
        else
            pNode->pFirstLeaf = pLeaf->pNext;
        if (pLeaf->pNext)
            pLeaf->pNext->pPrev = pLeaf->pPrev;

        /*
         * Free value and node.
         */
        cfgmR3FreeValue(pLeaf);
        pLeaf->pNext = NULL;
        pLeaf->pPrev = NULL;
        MMR3HeapFree(pLeaf);
    }
}


/**
 * Frees whatever resources the leaf value is owning.
 *
 * Use this before assigning a new value to a leaf.
 * The caller must either free the leaf or assign a new value to it.
 *
 * @param   pLeaf       Pointer to the leaf which value should be free.
 */
static void cfgmR3FreeValue(PCFGMLEAF pLeaf)
{
    if (pLeaf)
    {
        switch (pLeaf->enmType)
        {
            case CFGMVALUETYPE_BYTES:
                MMR3HeapFree(pLeaf->Value.Bytes.pau8);
                pLeaf->Value.Bytes.pau8 = NULL;
                pLeaf->Value.Bytes.cb = 0;
                break;

            case CFGMVALUETYPE_STRING:
                MMR3HeapFree(pLeaf->Value.String.psz);
                pLeaf->Value.String.psz = NULL;
                pLeaf->Value.String.cch = 0;
                break;

            case CFGMVALUETYPE_INTEGER:
                break;
        }
        pLeaf->enmType = (CFGMVALUETYPE)0;
    }
}


/**
 * Inserts a new integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Parent node.
 * @param   pszName         Value name.
 * @param   u64Integer      The value.
 */
CFGMR3DECL(int) CFGMR3InsertInteger(PCFGMNODE pNode, const char *pszName, uint64_t u64Integer)
{
    PCFGMLEAF pLeaf;
    int rc = cfgmR3InsertLeaf(pNode, pszName, &pLeaf);
    if (VBOX_SUCCESS(rc))
    {
        pLeaf->enmType = CFGMVALUETYPE_INTEGER;
        pLeaf->Value.Integer.u64 = u64Integer;
    }
    return rc;
}


/**
 * Inserts a new string value.
 *
 * @returns VBox status code.
 * @param   pNode           Parent node.
 * @param   pszName         Value name.
 * @param   pszString       The value.
 */
CFGMR3DECL(int) CFGMR3InsertString(PCFGMNODE pNode, const char *pszName, const char *pszString)
{
    int rc;
    if (pNode)
    {
        /*
         * Allocate string object first.
         */
        size_t cchString = strlen(pszString) + 1;
        char *pszStringCopy = (char *)MMR3HeapAlloc(pNode->pVM, MM_TAG_CFGM_STRING, RT_ALIGN_Z(cchString, 16));
        if (pszStringCopy)
        {
            memcpy(pszStringCopy, pszString, cchString);

            /*
             * Create value leaf and set it to string type.
             */
            PCFGMLEAF pLeaf;
            rc = cfgmR3InsertLeaf(pNode, pszName, &pLeaf);
            if (VBOX_SUCCESS(rc))
            {
                pLeaf->enmType = CFGMVALUETYPE_STRING;
                pLeaf->Value.String.psz = pszStringCopy;
                pLeaf->Value.String.cch = cchString;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_CFGM_NO_PARENT;

    return rc;
}



/**
 * Inserts a new integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Parent node.
 * @param   pszName         Value name.
 * @param   pvBytes         The value.
 * @param   cbBytes         The value size.
 */
CFGMR3DECL(int) CFGMR3InsertBytes(PCFGMNODE pNode, const char *pszName, const void *pvBytes, size_t cbBytes)
{
    int rc;
    if (pNode)
    {
        if (cbBytes == (RTUINT)cbBytes)
        {
            /*
             * Allocate string object first.
             */
            void *pvCopy = MMR3HeapAlloc(pNode->pVM, MM_TAG_CFGM_STRING, RT_ALIGN_Z(cbBytes, 16));
            if (pvCopy || !cbBytes)
            {
                memcpy(pvCopy, pvBytes, cbBytes);

                /*
                 * Create value leaf and set it to string type.
                 */
                PCFGMLEAF pLeaf;
                rc = cfgmR3InsertLeaf(pNode, pszName, &pLeaf);
                if (VBOX_SUCCESS(rc))
                {
                    pLeaf->enmType = CFGMVALUETYPE_BYTES;
                    pLeaf->Value.Bytes.cb   = cbBytes;
                    pLeaf->Value.Bytes.pau8 = (uint8_t *)pvCopy;
                }
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_OUT_OF_RANGE;
    }
    else
        rc = VERR_CFGM_NO_PARENT;

    return rc;
}


/**
 * Remove a value.
 *
 * @returns VBox status code.
 * @param   pNode       Parent node.
 * @param   pszName     Name of the new child node.
 */
CFGMR3DECL(int) CFGMR3RemoveValue(PCFGMNODE pNode, const char *pszName)
{
    PCFGMLEAF pLeaf;
    int rc = cfgmR3ResolveLeaf(pNode, pszName, &pLeaf);
    if (VBOX_SUCCESS(rc))
        cfgmR3RemoveLeaf(pNode, pLeaf);
    return rc;
}



/*
 *  -+- helper apis -+-
 */


/**
 * Query unsigned 64-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu64            Where to store the integer value.
 */
CFGMR3DECL(int) CFGMR3QueryU64(PCFGMNODE pNode, const char *pszName, uint64_t *pu64)
{
    return CFGMR3QueryInteger(pNode, pszName, pu64);
}


/**
 * Query unsigned 64-bit integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu64            Where to store the integer value.
 * @param   u64Def          The default value.
 */
CFGMR3DECL(int) CFGMR3QueryU64Def(PCFGMNODE pNode, const char *pszName, uint64_t *pu64, uint64_t u64Def)
{
    return CFGMR3QueryIntegerDef(pNode, pszName, pu64, u64Def);
}


/**
 * Query signed 64-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi64            Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryS64(PCFGMNODE pNode, const char *pszName, int64_t *pi64)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
        *pi64 = (int64_t)u64;
    return rc;
}


/**
 * Query signed 64-bit integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi64            Where to store the value.
 * @param   i64Def          The default value.
 */
CFGMR3DECL(int) CFGMR3QueryS64Def(PCFGMNODE pNode, const char *pszName, int64_t *pi64, int64_t i64Def)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, i64Def);
    if (VBOX_SUCCESS(rc))
        *pi64 = (int64_t)u64;
    return rc;
}


/**
 * Query unsigned 32-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu32            Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryU32(PCFGMNODE pNode, const char *pszName, uint32_t *pu32)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
    {
        if (!(u64 & UINT64_C(0xffffffff00000000)))
            *pu32 = (uint32_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query unsigned 32-bit integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu32            Where to store the value.
 * @param   u32Def          The default value.
 */
CFGMR3DECL(int) CFGMR3QueryU32Def(PCFGMNODE pNode, const char *pszName, uint32_t *pu32, uint32_t u32Def)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, u32Def);
    if (VBOX_SUCCESS(rc))
    {
        if (!(u64 & UINT64_C(0xffffffff00000000)))
            *pu32 = (uint32_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query signed 32-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi32            Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryS32(PCFGMNODE pNode, const char *pszName, int32_t *pi32)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
    {
        if (   !(u64 & UINT64_C(0xffffffff80000000))
            ||  (u64 & UINT64_C(0xffffffff80000000)) == UINT64_C(0xffffffff80000000))
            *pi32 = (int32_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query signed 32-bit integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi32            Where to store the value.
 * @param   i32Def          The default value.
 */
CFGMR3DECL(int) CFGMR3QueryS32Def(PCFGMNODE pNode, const char *pszName, int32_t *pi32, int32_t i32Def)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, i32Def);
    if (VBOX_SUCCESS(rc))
    {
        if (   !(u64 & UINT64_C(0xffffffff80000000))
            ||  (u64 & UINT64_C(0xffffffff80000000)) == UINT64_C(0xffffffff80000000))
            *pi32 = (int32_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query unsigned 16-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu16            Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryU16(PCFGMNODE pNode, const char *pszName, uint16_t *pu16)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
    {
        if (!(u64 & UINT64_C(0xffffffffffff0000)))
            *pu16 = (int16_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query unsigned 16-bit integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu16            Where to store the value.
 * @param   i16Def          The default value.
 */
CFGMR3DECL(int) CFGMR3QueryU16Def(PCFGMNODE pNode, const char *pszName, uint16_t *pu16, uint16_t u16Def)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, u16Def);
    if (VBOX_SUCCESS(rc))
    {
        if (!(u64 & UINT64_C(0xffffffffffff0000)))
            *pu16 = (int16_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query signed 16-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi16            Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryS16(PCFGMNODE pNode, const char *pszName, int16_t *pi16)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
    {
        if (   !(u64 & UINT64_C(0xffffffffffff8000))
            ||  (u64 & UINT64_C(0xffffffffffff8000)) == UINT64_C(0xffffffffffff8000))
            *pi16 = (int16_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query signed 16-bit integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi16            Where to store the value.
 * @param   i16Def          The default value.
 */
CFGMR3DECL(int) CFGMR3QueryS16Def(PCFGMNODE pNode, const char *pszName, int16_t *pi16, int16_t i16Def)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, i16Def);
    if (VBOX_SUCCESS(rc))
    {
        if (   !(u64 & UINT64_C(0xffffffffffff8000))
            ||  (u64 & UINT64_C(0xffffffffffff8000)) == UINT64_C(0xffffffffffff8000))
            *pi16 = (int16_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query unsigned 8-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu8             Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryU8(PCFGMNODE pNode, const char *pszName, uint8_t *pu8)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
    {
        if (!(u64 & UINT64_C(0xffffffffffffff00)))
            *pu8 = (uint8_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query unsigned 8-bit integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu8             Where to store the value.
 * @param   u8Def           The default value.
 */
CFGMR3DECL(int) CFGMR3QueryU8Def(PCFGMNODE pNode, const char *pszName, uint8_t *pu8, uint8_t u8Def)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, u8Def);
    if (VBOX_SUCCESS(rc))
    {
        if (!(u64 & UINT64_C(0xffffffffffffff00)))
            *pu8 = (uint8_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query signed 8-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi8             Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryS8(PCFGMNODE pNode, const char *pszName, int8_t *pi8)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
    {
        if (   !(u64 & UINT64_C(0xffffffffffffff80))
            ||  (u64 & UINT64_C(0xffffffffffffff80)) == UINT64_C(0xffffffffffffff80))
            *pi8 = (int8_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query signed 8-bit integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi8             Where to store the value.
 * @param   i8Def           The default value.
 */
CFGMR3DECL(int) CFGMR3QueryS8Def(PCFGMNODE pNode, const char *pszName, int8_t *pi8, int8_t i8Def)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, i8Def);
    if (VBOX_SUCCESS(rc))
    {
        if (   !(u64 & UINT64_C(0xffffffffffffff80))
            ||  (u64 & UINT64_C(0xffffffffffffff80)) == UINT64_C(0xffffffffffffff80))
            *pi8 = (int8_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query boolean integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pf              Where to store the value.
 * @remark  This function will interpret any non-zero value as true.
 */
CFGMR3DECL(int) CFGMR3QueryBool(PCFGMNODE pNode, const char *pszName, bool *pf)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
        *pf = u64 ? true : false;
    return rc;
}


/**
 * Query boolean integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pf              Where to store the value.
 * @param   fDef            The default value.
 * @remark  This function will interpret any non-zero value as true.
 */
CFGMR3DECL(int) CFGMR3QueryBoolDef(PCFGMNODE pNode, const char *pszName, bool *pf, bool fDef)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, fDef);
    if (VBOX_SUCCESS(rc))
        *pf = u64 ? true : false;
    return rc;
}


/**
 * Query I/O port address value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pPort           Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryPort(PCFGMNODE pNode, const char *pszName, PRTIOPORT pPort)
{
    AssertCompileSize(RTIOPORT, 2);
    return CFGMR3QueryU16(pNode, pszName, pPort);
}


/**
 * Query I/O port address value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pPort           Where to store the value.
 * @param   PortDef         The default value.
 */
CFGMR3DECL(int) CFGMR3QueryPortDef(PCFGMNODE pNode, const char *pszName, PRTIOPORT pPort, RTIOPORT PortDef)
{
    AssertCompileSize(RTIOPORT, 2);
    return CFGMR3QueryU16Def(pNode, pszName, pPort, PortDef);
}


/**
 * Query pointer integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   ppv             Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryPtr(PCFGMNODE pNode, const char *pszName, void **ppv)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
    {
        uintptr_t u = (uintptr_t)u64;
        if (u64 == u)
            *ppv = (void *)u;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query pointer integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   ppv             Where to store the value.
 * @param   pvDef           The default value.
 */
CFGMR3DECL(int) CFGMR3QueryPtrDef(PCFGMNODE pNode, const char *pszName, void **ppv, void *pvDef)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, (uintptr_t)pvDef);
    if (VBOX_SUCCESS(rc))
    {
        uintptr_t u = (uintptr_t)u64;
        if (u64 == u)
            *ppv = (void *)u;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query Guest Context pointer integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pGCPtr          Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryGCPtr(PCFGMNODE pNode, const char *pszName, PRTGCPTR pGCPtr)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
    {
        RTGCPTR u = (RTGCPTR)u64;
        if (u64 == u)
            *pGCPtr = u;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query Guest Context pointer integer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pGCPtr          Where to store the value.
 * @param   GCPtrDef        The default value.
 */
CFGMR3DECL(int) CFGMR3QueryGCPtrDef(PCFGMNODE pNode, const char *pszName, PRTGCPTR pGCPtr, RTGCPTR GCPtrDef)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, GCPtrDef);
    if (VBOX_SUCCESS(rc))
    {
        RTGCPTR u = (RTGCPTR)u64;
        if (u64 == u)
            *pGCPtr = u;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query Guest Context unsigned pointer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pGCPtr          Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryGCPtrU(PCFGMNODE pNode, const char *pszName, PRTGCUINTPTR pGCPtr)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
    {
        RTGCUINTPTR u = (RTGCUINTPTR)u64;
        if (u64 == u)
            *pGCPtr = u;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query Guest Context unsigned pointer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pGCPtr          Where to store the value.
 * @param   GCPtrDef        The default value.
 */
CFGMR3DECL(int) CFGMR3QueryGCPtrUDef(PCFGMNODE pNode, const char *pszName, PRTGCUINTPTR pGCPtr, RTGCUINTPTR GCPtrDef)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, GCPtrDef);
    if (VBOX_SUCCESS(rc))
    {
        RTGCUINTPTR u = (RTGCUINTPTR)u64;
        if (u64 == u)
            *pGCPtr = u;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query Guest Context signed pointer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pGCPtr          Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryGCPtrS(PCFGMNODE pNode, const char *pszName, PRTGCINTPTR pGCPtr)
{
    uint64_t u64;
    int rc = CFGMR3QueryInteger(pNode, pszName, &u64);
    if (VBOX_SUCCESS(rc))
    {
        RTGCINTPTR u = (RTGCINTPTR)u64;
        if (u64 == (uint64_t)u)
            *pGCPtr = u;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query Guest Context signed pointer value with default.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pGCPtr          Where to store the value.
 * @param   GCPtrDef        The default value.
 */
CFGMR3DECL(int) CFGMR3QueryGCPtrSDef(PCFGMNODE pNode, const char *pszName, PRTGCINTPTR pGCPtr, RTGCINTPTR GCPtrDef)
{
    uint64_t u64;
    int rc = CFGMR3QueryIntegerDef(pNode, pszName, &u64, GCPtrDef);
    if (VBOX_SUCCESS(rc))
    {
        RTGCINTPTR u = (RTGCINTPTR)u64;
        if (u64 == (uint64_t)u)
            *pGCPtr = u;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}


/**
 * Query zero terminated character value storing it in a
 * buffer allocated from the MM heap.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Value name. This value must be of zero terminated character string type.
 * @param   ppszString      Where to store the string pointer.
 *                          Free this using MMR3HeapFree().
 */
CFGMR3DECL(int) CFGMR3QueryStringAlloc(PCFGMNODE pNode, const char *pszName, char **ppszString)
{
    size_t cch;
    int rc = CFGMR3QuerySize(pNode, pszName, &cch);
    if (VBOX_SUCCESS(rc))
    {
        char *pszString = (char *)MMR3HeapAlloc(pNode->pVM, MM_TAG_CFGM_USER, cch);
        if (pszString)
        {
            rc = CFGMR3QueryString(pNode, pszName, pszString, cch);
            if (VBOX_SUCCESS(rc))
                *ppszString = pszString;
            else
                MMR3HeapFree(pszString);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}


/**
 * Query zero terminated character value storing it in a
 * buffer allocated from the MM heap.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Value name. This value must be of zero terminated character string type.
 * @param   ppszString      Where to store the string pointer.
 *                          Free this using MMR3HeapFree().
 */
CFGMR3DECL(int) CFGMR3QueryStringAllocDef(PCFGMNODE pNode, const char *pszName, char **ppszString, const char *pszDef)
{
    size_t cch;
    int rc = CFGMR3QuerySize(pNode, pszName, &cch);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
    {
        cch = strlen(pszDef) + 1;
        rc = VINF_SUCCESS;
    }
    if (VBOX_SUCCESS(rc))
    {
        char *pszString = (char *)MMR3HeapAlloc(pNode->pVM, MM_TAG_CFGM_USER, cch);
        if (pszString)
        {
            rc = CFGMR3QueryStringDef(pNode, pszName, pszString, cch, pszDef);
            if (VBOX_SUCCESS(rc))
                *ppszString = pszString;
            else
                MMR3HeapFree(pszString);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}


/**
 * Dumps the configuration (sub)tree to the release log.
 *
 * @param   pRoot   The root node of the dump.
 */
CFGMR3DECL(void) CFGMR3Dump(PCFGMNODE pRoot)
{
    LogRel(("************************* CFGM dump *************************\n"));
    cfgmR3Info(pRoot->pVM, DBGFR3InfoLogRelHlp(), NULL);
    LogRel(("********************* End of CFGM dump **********************\n"));
}


/**
 * Info handler, internal version.
 *
 * @param   pVM         The VM handle.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) cfgmR3Info(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /*
     * Figure where to start.
     */
    PCFGMNODE pRoot = pVM->cfgm.s.pRoot;
    if (pszArgs && *pszArgs)
    {
        int rc = cfgmR3ResolveNode(pRoot, pszArgs, &pRoot);
        if (VBOX_FAILURE(rc))
        {
            pHlp->pfnPrintf(pHlp, "Failed to resolve CFGM path '%s', %Vrc", pszArgs, rc);
            return;
        }
    }

    /*
     * Dump the specified tree.
     */
    pHlp->pfnPrintf(pHlp, "pRoot=%p:{", pRoot);
    cfgmR3DumpPath(pRoot, pHlp);
    pHlp->pfnPrintf(pHlp, "}\n");
    cfgmR3Dump(pRoot, 0, pHlp);
}


/**
 * Recursivly prints a path name.
 */
static void cfgmR3DumpPath(PCFGMNODE pNode, PCDBGFINFOHLP pHlp)
{
    if (pNode->pParent)
        cfgmR3DumpPath(pNode->pParent, pHlp);
    pHlp->pfnPrintf(pHlp, "%s/", pNode->szName);
}


/**
 * Dumps a branch of a tree.
 */
static void cfgmR3Dump(PCFGMNODE pRoot, unsigned iLevel, PCDBGFINFOHLP pHlp)
{
    /*
     * Path.
     */
    pHlp->pfnPrintf(pHlp, "[");
    cfgmR3DumpPath(pRoot, pHlp);
    pHlp->pfnPrintf(pHlp, "] (level %d)%s\n", iLevel, pRoot->fRestrictedRoot ? " (restricted root)" : "");

    /*
     * Values.
     */
    PCFGMLEAF pLeaf;
    unsigned cchMax = 0;
    for (pLeaf = CFGMR3GetFirstValue(pRoot); pLeaf; pLeaf = CFGMR3GetNextValue(pLeaf))
        cchMax = RT_MAX(cchMax, pLeaf->cchName);
    for (pLeaf = CFGMR3GetFirstValue(pRoot); pLeaf; pLeaf = CFGMR3GetNextValue(pLeaf))
    {
        switch (CFGMR3GetValueType(pLeaf))
        {
            case CFGMVALUETYPE_INTEGER:
                pHlp->pfnPrintf(pHlp, "  %-*s <integer> = %#018llx (%lld)\n", cchMax, pLeaf->szName, pLeaf->Value.Integer.u64, pLeaf->Value.Integer.u64);
                break;

            case CFGMVALUETYPE_STRING:
                pHlp->pfnPrintf(pHlp, "  %-*s <string>  = \"%s\" (cch=%d)\n", cchMax, pLeaf->szName, pLeaf->Value.String.psz, pLeaf->Value.String.cch);
                break;

            case CFGMVALUETYPE_BYTES:
                pHlp->pfnPrintf(pHlp, "  %-*s <bytes>   = \"%.*Vhxs\" (cb=%d)\n", cchMax, pLeaf->szName, pLeaf->Value.Bytes.cb, pLeaf->Value.Bytes.pau8, pLeaf->Value.Bytes.cb);
                break;

            default:
                AssertMsgFailed(("bad leaf!\n"));
                break;
        }
    }
    pHlp->pfnPrintf(pHlp, "\n");

    /*
     * Children.
     */
    for (PCFGMNODE pChild = CFGMR3GetFirstChild(pRoot); pChild; pChild = CFGMR3GetNextChild(pChild))
    {
        Assert(pChild->pNext != pChild);
        Assert(pChild->pPrev != pChild);
        Assert(pChild->pPrev != pChild->pNext || !pChild->pPrev);
        Assert(pChild->pFirstChild != pChild);
        Assert(pChild->pParent != pChild);
        cfgmR3Dump(pChild, iLevel + 1, pHlp);
    }
}

