/** @file
 * CFGM - Configuration Manager
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___VBox_cfgm_h
#define ___VBox_cfgm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/stdarg.h>

/** @defgroup   grp_cfgm        The Configuration Manager API
 * @{
 */

/** Configuration manager tree node - A key. */
typedef struct CFGMNODE *PCFGMNODE;

/** Configuration manager tree leaf - A value. */
typedef struct CFGMLEAF *PCFGMLEAF;

/**
 * Configuration manager value type.
 */
typedef enum CFGMVALUETYPE
{
    /** Integer value. */
    CFGMVALUETYPE_INTEGER = 1,
    /** String value. */
    CFGMVALUETYPE_STRING,
    /** Bytestring value. */
    CFGMVALUETYPE_BYTES
} CFGMVALUETYPE;
/** Pointer to configuration manager property type. */
typedef CFGMVALUETYPE *PCFGMVALUETYPE;



__BEGIN_DECLS

#ifdef IN_RING3
/** @defgroup   grp_cfgm_r3     The CFGM Host Context Ring-3 API
 * @ingroup grp_cfgm
 * @{
 */

typedef enum CFGMCONFIGTYPE
{
    /** pvConfig points to nothing, use defaults. */
    CFGMCONFIGTYPE_NONE = 0,
    /** pvConfig points to a IMachine interface. */
    CFGMCONFIGTYPE_IMACHINE
} CFGMCONFIGTYPE;


/**
 * CFGM init callback for constructing the configuration tree.
 *
 * This is called from the emulation thread, and the one interfacing the VM
 * can make any necessary per-thread initializations at this point.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pvUser          The argument supplied to VMR3Create().
 */
typedef DECLCALLBACK(int) FNCFGMCONSTRUCTOR(PVM pVM, void *pvUser);
/** Pointer to a FNCFGMCONSTRUCTOR(). */
typedef FNCFGMCONSTRUCTOR *PFNCFGMCONSTRUCTOR;


/**
 * Constructs the configuration for the VM.
 *
 * @returns VBox status code.
 * @param   pVM                 Pointer to VM which configuration has not yet been loaded.
 * @param   pfnCFGMConstructor  Pointer to callback function for constructing the VM configuration tree.
 *                              This is called in the EM.
 * @param   pvUser              The user argument passed to pfnCFGMConstructor.
 */
CFGMR3DECL(int) CFGMR3Init(PVM pVM, PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUser);

/**
 * Terminates the configuration manager.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 */
CFGMR3DECL(int) CFGMR3Term(PVM pVM);


/** Tree Navigation and Enumeration.
 * @{
 */

/**
 * Gets the root node for the VM.
 *
 * @returns Pointer to root node.
 * @param   pVM             VM handle.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetRoot(PVM pVM);

/**
 * Gets the parent of a CFGM node.
 *
 * @returns Pointer to the parent node.
 * @returns NULL if pNode is Root or pNode is the start of a
 *          restricted subtree (use CFGMr3GetParentEx() for that).
 *
 * @param   pNode           The node which parent we query.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetParent(PCFGMNODE pNode);

/**
 * Gets the parent of a CFGM node.
 *
 * @returns Pointer to the parent node.
 * @returns NULL if pNode is Root or pVM is not correct.
 *
 * @param   pVM             The VM handle, used as token that the caller is trusted.
 * @param   pNode           The node which parent we query.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetParentEx(PVM pVM, PCFGMNODE pNode);

/**
 * Query a child node.
 *
 * @returns Pointer to the specified node.
 * @returns NULL if node was not found or pNode is NULL.
 * @param   pNode           Node pszPath is relative to.
 * @param   pszPath         Path to the child node or pNode.
 *                          It's good style to end this with '/'.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetChild(PCFGMNODE pNode, const char *pszPath);

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
CFGMR3DECL(PCFGMNODE) CFGMR3GetChildF(PCFGMNODE pNode, const char *pszPathFormat, ...);

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
CFGMR3DECL(PCFGMNODE) CFGMR3GetChildFV(PCFGMNODE pNode, const char *pszPathFormat, va_list Args);

/**
 * Gets the first child node.
 * Use this to start an enumeration of child nodes.
 *
 * @returns Pointer to the first child.
 * @returns NULL if no children.
 * @param   pNode           Node to enumerate children for.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetFirstChild(PCFGMNODE pNode);

/**
 * Gets the next sibling node.
 * Use this to continue an enumeration.
 *
 * @returns Pointer to the first child.
 * @returns NULL if no children.
 * @param   pCur            Node to returned by a call to CFGMR3GetFirstChild()
 *                          or successive calls to this function.
 */
CFGMR3DECL(PCFGMNODE) CFGMR3GetNextChild(PCFGMNODE pCur);

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
CFGMR3DECL(int) CFGMR3GetName(PCFGMNODE pCur, char *pszName, size_t cchName);

/**
 * Gets the length of the current node's name.
 * (Needed for enumeration.)
 *
 * @returns Node name length in bytes including the terminating null char.
 * @returns 0 if pCur is NULL.
 * @param   pCur            Node returned by a call to CFGMR3GetFirstChild()
 *                          or successive calls to CFGMR3GetNextChild().
 */
CFGMR3DECL(int) CFGMR3GetNameLen(PCFGMNODE pCur);

/**
 * Validates that the child nodes are within a set of valid names.
 *
 * @returns true if all names are found in pszzAllowed.
 * @returns false if not.
 * @param   pNode           The node which values should be examined.
 * @param   pszzValid       List of valid names separated by '\\0' and ending with
 *                          a double '\\0'.
 */
CFGMR3DECL(bool) CFGMR3AreChildrenValid(PCFGMNODE pNode, const char *pszzValid);


/**
 * Gets the first value of a node.
 * Use this to start an enumeration of values.
 *
 * @returns Pointer to the first value.
 * @param   pCur            The node (Key) which values to enumerate.
 */
CFGMR3DECL(PCFGMLEAF) CFGMR3GetFirstValue(PCFGMNODE pCur);

/**
 * Gets the next value in enumeration.
 *
 * @returns Pointer to the next value.
 * @param   pCur            The current value as returned by this function or CFGMR3GetFirstValue().
 */
CFGMR3DECL(PCFGMLEAF) CFGMR3GetNextValue(PCFGMLEAF pCur);

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
CFGMR3DECL(int) CFGMR3GetValueName(PCFGMLEAF pCur, char *pszName, size_t cchName);

/**
 * Gets the length of the current node's name.
 * (Needed for enumeration.)
 *
 * @returns Value name length in bytes including the terminating null char.
 * @returns 0 if pCur is NULL.
 * @param   pCur            Value returned by a call to CFGMR3GetFirstValue()
 *                          or successive calls to CFGMR3GetNextValue().
 */
CFGMR3DECL(int) CFGMR3GetValueNameLen(PCFGMLEAF pCur);

/**
 * Gets the value type.
 * (For enumeration.)
 *
 * @returns VBox status code.
 * @param   pCur            Value returned by a call to CFGMR3GetFirstValue()
 *                          or successive calls to CFGMR3GetNextValue().
 */
CFGMR3DECL(CFGMVALUETYPE) CFGMR3GetValueType(PCFGMLEAF pCur);

/**
 * Validates that the values are within a set of valid names.
 *
 * @returns true if all names are found in pszzAllowed.
 * @returns false if not.
 * @param   pNode           The node which values should be examined.
 * @param   pszzValid       List of valid names separated by '\\0' and ending with
 *                          a double '\\0'.
 */
CFGMR3DECL(bool) CFGMR3AreValuesValid(PCFGMNODE pNode, const char *pszzValid);

/** @} */

/**
 * Query value type.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   penmType        Where to store the type.
 */
CFGMR3DECL(int) CFGMR3QueryType(PCFGMNODE pNode, const char *pszName, PCFGMVALUETYPE penmType);

/**
 * Query value size.
 * This works on all types of values.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pcb             Where to store the value size.
 */
CFGMR3DECL(int) CFGMR3QuerySize(PCFGMNODE pNode, const char *pszName, size_t *pcb);

/**
 * Query integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu64            Where to store the integer value.
 */
CFGMR3DECL(int) CFGMR3QueryInteger(PCFGMNODE pNode, const char *pszName, uint64_t *pu64);

/**
 * Query zero terminated character value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of a zero terminate character value.
 * @param   pszString       Where to store the string.
 * @param   cchString       Size of the string buffer. (Includes terminator.)
 */
CFGMR3DECL(int) CFGMR3QueryString(PCFGMNODE pNode, const char *pszName, char *pszString, size_t cchString);

/**
 * Query byte string value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of a byte string value.
 * @param   pvData          Where to store the binary data.
 * @param   cbData          Size of buffer pvData points too.
 */
CFGMR3DECL(int) CFGMR3QueryBytes(PCFGMNODE pNode, const char *pszName, void *pvData, size_t cbData);


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
CFGMR3DECL(PCFGMNODE) CFGMR3CreateTree(PVM pVM);

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
CFGMR3DECL(int) CFGMR3InsertSubTree(PCFGMNODE pNode, const char *pszName, PCFGMNODE pSubTree, PCFGMNODE *ppChild);

/**
 * Insert a node.
 *
 * @returns VBox status code.
 * @returns VERR_CFGM_NODE_EXISTS if the final child node name component exists.
 * @param   pNode       Parent node.
 * @param   pszName     Name or path of the new child node.
 * @param   ppChild     Where to store the address of the new child node. (optional)
 */
CFGMR3DECL(int) CFGMR3InsertNode(PCFGMNODE pNode, const char *pszName, PCFGMNODE *ppChild);

/**
 * Insert a node, format string name.
 *
 * @returns VBox status     code.
 * @param   pNode           Parent node.
 * @param   ppChild         Where to store the address of the new child node. (optional)
 * @param   pszNameFormat   Name or path of the new child node.
 * @param   ...             Name format arguments.
 */
CFGMR3DECL(int) CFGMR3InsertNodeF(PCFGMNODE pNode, PCFGMNODE *ppChild, const char *pszNameFormat, ...);

/**
 * Insert a node, format string name.
 *
 * @returns VBox status code.
 * @param   pNode           Parent node.
 * @param   ppChild         Where to store the address of the new child node. (optional)
 * @param   pszNameFormat   Name or path of the new child node.
 * @param   Args            Name format arguments.
 */
CFGMR3DECL(int) CFGMR3InsertNodeFV(PCFGMNODE pNode, PCFGMNODE *ppChild, const char *pszNameFormat, va_list Args);

/**
 * Marks the node as the root of a restricted subtree, i.e. the end of
 * a CFGMR3GetParent() journey.
 *
 * @param   pNode       The node to mark.
 */
CFGMR3DECL(void) CFGMR3SetRestrictedRoot(PCFGMNODE pNode);

/**
 * Remove a node.
 *
 * @param   pNode       Parent node.
 */
CFGMR3DECL(void) CFGMR3RemoveNode(PCFGMNODE pNode);


/**
 * Inserts a new integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Parent node.
 * @param   pszName         Value name.
 * @param   u64Integer      The value.
 */
CFGMR3DECL(int) CFGMR3InsertInteger(PCFGMNODE pNode, const char *pszName, uint64_t u64Integer);

/**
 * Inserts a new string value.
 *
 * @returns VBox status code.
 * @param   pNode           Parent node.
 * @param   pszName         Value name.
 * @param   pszString       The value.
 */
CFGMR3DECL(int) CFGMR3InsertString(PCFGMNODE pNode, const char *pszName, const char *pszString);

/**
 * Inserts a new integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Parent node.
 * @param   pszName         Value name.
 * @param   pvBytes         The value.
 * @param   cbBytes         The value size.
 */
CFGMR3DECL(int) CFGMR3InsertBytes(PCFGMNODE pNode, const char *pszName, const void *pvBytes, size_t cbBytes);

/**
 * Remove a value.
 *
 * @returns VBox status code.
 * @param   pNode       Parent node.
 * @param   pszName     Name of the new child node.
 */
CFGMR3DECL(int) CFGMR3RemoveValue(PCFGMNODE pNode, const char *pszName);



/** Helpers
 * @{
 */
/**
 * Query unsigned 64-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu64            Where to store the integer value.
 */
CFGMR3DECL(int) CFGMR3QueryU64(PCFGMNODE pNode, const char *pszName, uint64_t *pu64);

/**
 * Query signed 64-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi64            Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryS64(PCFGMNODE pNode, const char *pszName, int64_t *pi64);

/**
 * Query unsigned 32-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu32            Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryU32(PCFGMNODE pNode, const char *pszName, uint32_t *pu32);

/**
 * Query signed 32-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi32            Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryS32(PCFGMNODE pNode, const char *pszName, int32_t *pi32);

/**
 * Query unsigned 16-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu16            Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryU16(PCFGMNODE pNode, const char *pszName, uint16_t *pu16);

/**
 * Query signed 16-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi16            Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryS16(PCFGMNODE pNode, const char *pszName, int16_t *pi16);

/**
 * Query unsigned 8-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pu8             Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryU8(PCFGMNODE pNode, const char *pszName, uint8_t *pu8);

/**
 * Query signed 8-bit integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pi8             Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryS8(PCFGMNODE pNode, const char *pszName, int8_t *pi8);

/**
 * Query boolean integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pf              Where to store the value.
 * @remark  This function will interpret any non-zero value as true.
 */
CFGMR3DECL(int) CFGMR3QueryBool(PCFGMNODE pNode, const char *pszName, bool *pf);

/**
 * Query pointer integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   ppv             Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryPtr(PCFGMNODE pNode, const char *pszName, void **ppv);

/**
 * Query Guest Context pointer integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pGCPtr          Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryGCPtr(PCFGMNODE pNode, const char *pszName, PRTGCPTR pGCPtr);

/**
 * Query Guest Context unsigned pointer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pGCPtr          Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryGCPtrU(PCFGMNODE pNode, const char *pszName, PRTGCUINTPTR pGCPtr);

/**
 * Query Guest Context signed pointer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pGCPtr          Where to store the value.
 */
CFGMR3DECL(int) CFGMR3QueryGCPtrS(PCFGMNODE pNode, const char *pszName, PRTGCINTPTR pGCPtr);

/**
 * Query boolean integer value.
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pvValue         Where to store the value.
 * @param   cbValue         The size of the integer value (in bytes).
 * @param   fSigned         Whether the integer is signed (true) or not (false).
 * @remark  This function will interpret any non-zero value as true.
 */
DECLINLINE(int) CFGMR3QueryIntegerBySize(PCFGMNODE pNode, const char *pszName, void *pvValue, size_t cbValue, bool fSigned)
{
    int rc;
    if (fSigned)
    {
        switch (cbValue)
        {
            case  8: rc = CFGMR3QueryS8(pNode, pszName, (int8_t *)pvValue);   break;
            case 16: rc = CFGMR3QueryS16(pNode, pszName, (int16_t *)pvValue); break;
            case 32: rc = CFGMR3QueryS32(pNode, pszName, (int32_t *)pvValue); break;
            case 64: rc = CFGMR3QueryS64(pNode, pszName, (int64_t *)pvValue); break;
            default: rc = -1 /* VERR_GENERAL_FAILURE*/; break;
        }
    }
    else
    {
        switch (cbValue)
        {
            case  8: rc = CFGMR3QueryU8(pNode, pszName, (uint8_t *)pvValue);   break;
            case 16: rc = CFGMR3QueryU16(pNode, pszName, (uint16_t *)pvValue); break;
            case 32: rc = CFGMR3QueryU32(pNode, pszName, (uint32_t *)pvValue); break;
            case 64: rc = CFGMR3QueryU64(pNode, pszName, (uint64_t *)pvValue); break;
            default: rc = -1 /* VERR_GENERAL_FAILURE*/; break;
        }
    }
    return rc;
}


/**
 * Query I/O port address value (integer).
 *
 * @returns VBox status code.
 * @param   pNode           Which node to search for pszName in.
 * @param   pszName         Name of an integer value.
 * @param   pPort           Where to store the value.
 */
DECLINLINE(int) CFGMR3QueryPort(PCFGMNODE pNode, const char *pszName, PRTIOPORT pPort)
{
    return CFGMR3QueryIntegerBySize(pNode, pszName, pPort, sizeof(*pPort), false);
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
CFGMR3DECL(int) CFGMR3QueryStringAlloc(PCFGMNODE pNode, const char *pszName, char **ppszString);

/** @} */


/**
 * Dumps the configuration (sub)tree.
 *
 * @param   pRoot   The root node of the dump.
 */
CFGMR3DECL(void) CFGMR3Dump(PCFGMNODE pRoot);

/** @} */
#endif  /* IN_RING3 */


__END_DECLS

/** @} */

#endif
