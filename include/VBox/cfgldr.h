/** @file
 * CFGLDR - Configuration Loader
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

#ifndef ___VBox_cfgldr_h
#define ___VBox_cfgldr_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/string.h>


/** @defgroup   grp_cfgldr        The Configuration Loader API
 * @{
 */

#ifndef IN_RING3
# error "There are no configuration loader APIs available in Ring-0 Context!"
#else /* IN_RING3 */

/** @def IN_CFGLDR_R3
 * Used to indicate whether we're inside the same link module as the
 * Configuration Loader.
 */
#ifdef IN_CFGLDR_R3
# define CFGLDRR3DECL(type)   DECLEXPORT(type) VBOXCALL
#else
# define CFGLDRR3DECL(type)   DECLIMPORT(type) VBOXCALL
#endif

/** @type CFGHANDLE
 * Configuration handle. */
/** @type CFGNODE
 * Configuration node handle. */
#ifdef __cplusplus
class CfgLoader;
typedef CfgLoader *CFGHANDLE;

class CfgNode;
typedef CfgNode *CFGNODE;
#else
struct CfgLoader;
typedef struct CfgLoader *CFGHANDLE;

struct CfgNode;
typedef struct CfgNode *CFGNODE;
#endif

/** Entity type */
typedef enum CFGLDRENTITYTYPE
{
    CFGLDRENTITYTYPE_INVALID = 0,   /**< Invalid type */
    CFGLDRENTITYTYPE_HANDLE,        /**< File handle */
    CFGLDRENTITYTYPE_MEMORY         /**< Memory buffer */
} CFGLDRENTITYTYPE;

/** Entity descriptor for the entity resolver callback */
typedef struct CFGLDRENTITY
{
    /** Entity type */
    CFGLDRENTITYTYPE enmType;

    union
    {
        /** File handle (CFGLDRENTITYTYPE_HANDLE) */
        struct
        {
            RTFILE hFile; /**< Handle of the file opened for reading entity data */
            bool bClose;    /**< Whether to close the handle when no more needed */
        } handle;
        /** Memory buffer (CFGLDRENTITYTYPE_MEMORY) */
        struct
        {
            unsigned char *puchBuf; /**< Byte array containing entity data */
            size_t cbBuf;           /**< Size of the puchBuf array */
            bool bFree; /**< Whether to free puchBuf using RTMemTmpFree()
                             when no more needed */
        } memory;
    } u;

} CFGLDRRESOLVEDENTITY;

/** Pointer to CFGLDRENTITY */
typedef CFGLDRENTITY *PCFGLDRENTITY;
/** Pointer to const CFGLDRENTITY */
typedef const CFGLDRENTITY *PCCFGLDRENTITY;

/**
 *  Callback function used by CFGLDRLoad() to resolve external entities when
 *  loading and parsing the configuration file,
 *
 *  First three arguments are input parameters, describing the entity to resolve.
 *  On return, the callback fills in a CFGLDRENTITY structure pointed to by the
 *  pEntity argument, with values corresponding to the given entity.
 *
 *  @param  pcszPublicId
 *      Public ID of the entity in question (UTF-8)
 *  @param  pcszSystemId
 *      System ID (URI) of the entity in question (UTF-8)
 *  @param  pcszBaseURI
 *      Base URI of the entity necessary to resolve relative URIs (UTF-8)
 *  @param pEntity
 *      Pointer to the entity descriptor structure to fill in
 *
 *  @return
 *      VINF_SUCCESS to indicate that pEntity is successfully filled in by the
 *      callback. Any other value will cause the configuration
 *      loader to use the default entity resolver (see pfnEntityResolver in
 *      CFGLDRLoad()).
 */
typedef DECLCALLBACK(int) FNCFGLDRENTITYRESOLVER(const char *pcszPublicId,
                                                 const char *pcszSystemId,
                                                 const char *pcszBaseURI,
                                                 PCFGLDRENTITY pEntity);
/** Pointer to entity resolver callback. */
typedef FNCFGLDRENTITYRESOLVER *PFNCFGLDRENTITYRESOLVER;

__BEGIN_DECLS

/**
 * Initializes CFGLDR, have to be called at VBox startup before accessing any CFGLDR functions.
 *
 * @return VBox status code.
 */
CFGLDRR3DECL(int) CFGLDRInitialize(void);

/**
 * Deletes all configurations and nodes at VBox termination. After calling this function all CFGLDR functions will be unavailable.
 *
 * @return VBox status code.
 */
CFGLDRR3DECL(void) CFGLDRShutdown(void);

/**
 * Creates a configuration file from scratch. The file then can be saved
 * using the CFGLDRSaveAs() function.
 *
 * @return  VBox status code.
 */
CFGLDRR3DECL(int) CFGLDRCreate(CFGHANDLE *phcfg);

/**
 *  Loads a configuration file.
 *
 *  @param  phcfg
 *      Pointer to a handle that will be used to access the configuration.
 *  @param  pszFilename
 *      Name of the file containing the configuration (UTF-8).
 *  @param  hFileHandle
 *      Handle of the file containing the configuration. If equals to NIL_RTFILE,
 *      an attempt to open the specified file will be made; otherwise
 *      it must be a valid file handle corresponding to the given file name
 *      opened at least for reading.
 *  @param  pszExternalSchemaLocation
 *      Name of the file containing namespaceless XML Schema for the configuration
 *      file or one or more pairs (separated by spaces) consiting of a namespace
 *      and an XML schema file for the given namespace, depending on the
 *      value of bDoNamespaces. The parameter can be NULL to indicate that
 *      no validation of the configuration file should be done.
 *  @param  bDoNamespaces
 *      If TRUE, namespace processing is turned on and pszExternalSchemaLocation
 *      is assumed to contain "namespace schema-file" pairs; otherwise namespaces
 *      are disabled and pszExternalSchemaLocation is assumed to be a single file
 *      name. The parameter ignored if pszExternalSchemaLocation is NULL.
 *  @param  pfnEntityResolver
 *      Callback used to resolve external entities in the configuration file.
 *      Note that the configuration file itself is always accessed directly
 *      (using the given file name or file handle), but all external entities it
 *      refers to, as well as all schema files, are resolved using this callback.
 *      This parameter can be NULL, in which case the default resolver is used
 *      which simply tries to open an entity by its system ID (URI).
 *  @param  ppszErrorMessage
 *      Address of a variable that will receive a pointer to an UTF-8 string
 *      containing all error messages produced by a parser while parsing and
 *      validating the configuration file. If non-NULL pointer is returned, the
 *      caller must free the string using RTStrFree(). The parameter can be
 *      NULL on input (no error string will be returned), and it can point to
 *      NULL on output indicating that no error message was provided.
 *      If the function succeeds, the returned string is always NULL.
 *
 *  @return VBox status code.
 */
CFGLDRR3DECL(int) CFGLDRLoad(CFGHANDLE *phcfg,
                             const char *pszFileName, RTFILE hFileHandle,
                             const char *pszExternalSchemaLocation, bool bDoNamespaces,
                             PFNCFGLDRENTITYRESOLVER pfnEntityResolver,
                             char **ppszErrorMessage);

/**
 * Frees the previously loaded configuration.
 *
 * @return  VBox status code.
 * @param   hcfg            Handle of configuration that was loaded using CFGLDRLoad().
 */
CFGLDRR3DECL(int) CFGLDRFree(CFGHANDLE hcfg);

/**
 * Saves the previously loaded configuration.
 *
 *  @param  hcfg
 *      Handle of the configuration that was loaded using CFGLDRLoad().
 *  @param  ppszErrorMessage
 *      Address of a variable that will receive a pointer to an UTF-8 string
 *      containing all error messages produced by a writer while saving
 *      the configuration file. If non-NULL pointer is returned, the
 *      caller must free the string using RTStrFree(). The parameter can be
 *      NULL on input (no error string will be returned), and it can point to
 *      NULL on output indicating that no error message was provided.
 *      If the function succeeds, the returned string is always NULL.
 *
 *  @return VBox status code.
 */
CFGLDRR3DECL(int) CFGLDRSave(CFGHANDLE hcfg,
                             char **ppszErrorMessage);
/**
 *  Saves the configuration to a specified file.
 *
 *  @param  hcfg
 *      Handle of the configuration that was loaded using CFGLDRLoad().
 *  @param  pszFilename
 *      Name of the file to save the configuration to (UTF-8).
 *  @param  hFileHandle
 *      Handle of the file to save the configuration to. If equals to NIL_RTFILE,
 *      an attempt to open the specified file will be made; otherwise
 *      it must be a valid file handle corresponding to the given file name
 *      opened at least for writting.
 *  @param  ppszErrorMessage
 *      Address of a variable that will receive a pointer to an UTF-8 string
 *      containing all error messages produced by a writer while saving
 *      the configuration file. If non-NULL pointer is returned, the
 *      caller must free the string using RTStrFree(). The parameter can be
 *      NULL on input (no error string will be returned), and it can point to
 *      NULL on output indicating that no error message was provided.
 *      If the function succeeds, the returned string is always NULL.
 *
 * @return  VBox status code.
 */
CFGLDRR3DECL(int) CFGLDRSaveAs(CFGHANDLE hcfg,
                               const char *pszFilename, RTFILE hFileHandle,
                               char **ppszErrorMessage);

/**
 *  Applies an XSLT transformation to the given configuration handle.
 *
 *  @param  hcfg
 *      Handle of the configuration that was loaded using CFGLDRLoad().
 *  @param  pszTemlateLocation
 *      Name of the file containing an XSL template to apply
 *  @param  pfnEntityResolver
 *      Callback used to resolve external entities (the XSL template file).
 *      This parameter can be NULL, in which case the default resolver is used
 *      which simply tries to open an entity by its system ID (URI).
 *  @param  ppszErrorMessage
 *      Address of a variable that will receive a pointer to an UTF-8 string
 *      containing all error messages produced by a parser while parsing and
 *      validating the configuration file. If non-NULL pointer is returned, the
 *      caller must free the string using RTStrFree(). The parameter can be
 *      NULL on input (no error string will be returned), and it can point to
 *      NULL on output indicating that no error message was provided.
 *      If the function succeeds, the returned string is always NULL.
 */
CFGLDRR3DECL(int) CFGLDRTransform (CFGHANDLE hcfg,
                                   const char *pszTemlateLocation,
                                   PFNCFGLDRENTITYRESOLVER pfnEntityResolver,
                                   char **ppszErrorMessage);

/**
 * Get node handle.
 *
 * @return  VBox status code.
 * @param   hcfg            Handle of configuration that was loaded using CFGLDRLoad().
 * @param   pszName         Name of the node (UTF-8).
 * @param   uIndex          Index of the node (0 based).
 * @param   phnode          Pointer to a handle that will be used to access the node attributes, value and child nodes.
 */
CFGLDRR3DECL(int) CFGLDRGetNode(CFGHANDLE hcfg, const char *pszName, unsigned uIndex, CFGNODE *phnode);

/**
 * Get child node handle.
 *
 * @return  VBox status code.
 * @param   hparent         Handle of parent node.
 * @param   pszName         Name of the child node (UTF-8), NULL for any child.
 * @param   uIndex          Index of the child node (0 based).
 * @param   phnode          Pointer to a handle that will be used to access the node attributes, value and child nodes.
 */
CFGLDRR3DECL(int) CFGLDRGetChildNode(CFGNODE hparent, const char *pszName, unsigned uIndex, CFGNODE *phnode);

/**
 * Creates a new node or returns the first one with a given name if
 * already exists.
 *
 * @return  VBox status code.
 * @param   hcfg            Handle of the configuration.
 * @param   pszName         Name of the node to be created (UTF-8).
 * @param   phnode          Pointer to a handle that will be used to access the node attributes, value and child nodes.
 */
CFGLDRR3DECL(int) CFGLDRCreateNode(CFGHANDLE hcfg, const char *pszName, CFGNODE *phnode);

/**
 * Creates a new child node or returns the first one with a given name if
 * already exists.
 *
 * @return  VBox status code.
 * @param   hparent         Handle of parent node.
 * @param   pszName         Name of the child node (UTF-8).
 * @param   phnode          Pointer to a handle that will be used to access the node attributes, value and child nodes.
 */
CFGLDRR3DECL(int) CFGLDRCreateChildNode(CFGNODE hparent, const char *pszName, CFGNODE *phnode);

/**
 * Appends a new child node.
 *
 * @return  VBox status code.
 * @param   hparent         Handle of parent node.
 * @param   pszName         Name of the child node (UTF-8).
 * @param   phnode          Pointer to a handle that will be used to access the node attributes, value and child nodes.
 */
CFGLDRR3DECL(int) CFGLDRAppendChildNode(CFGNODE hparent, const char *pszName, CFGNODE *phnode);

/**
 * Release node handle.
 *
 * @return  VBox status code.
 * @param   hnode           Handle that will not be used anymore.
 */
CFGLDRR3DECL(int) CFGLDRReleaseNode(CFGNODE hnode);

/**
 * Delete node. The handle is released and the entire node is removed.
 *
 * @return  VBox status code.
 * @param   hnode           Handle of node that will be deleted.
 */
CFGLDRR3DECL(int) CFGLDRDeleteNode(CFGNODE hnode);

/**
 * Count child nodes with given name.
 *
 * @return  VBox status code.
 * @param   hparent         Handle of parent node.
 * @param   pszChildName    Name of the child node (UTF-8), NULL for all children.
 * @param   pCount          Pointer to unsigned variable where to store the count.
 */
CFGLDRR3DECL(int) CFGLDRCountChildren(CFGNODE hnode, const char *pszChildName, unsigned *pCount);

/**
 * Query 32 bit unsigned value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pulValue        Pointer to 32 bit variable where to store the value.
 */
CFGLDRR3DECL(int) CFGLDRQueryUInt32(CFGNODE hnode, const char *pszName, uint32_t *pulValue);

/**
 * Set 32 bit unsigned value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   ulValue         The value.
 */
CFGLDRR3DECL(int) CFGLDRSetUInt32(CFGNODE hnode, const char *pszName, uint32_t ulValue);

/**
 * Set 32 bit unsigned value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   ulValue         The value.
 * @patam   uiBase          0, 8 or 16.
 */
CFGLDRR3DECL(int) CFGLDRSetUInt32Ex(CFGNODE hnode, const char *pszName, uint32_t ulValue, unsigned int uiBase);

/**
 * Query 64 bit unsigned value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pullValue       Pointer to 64 bit variable where to store the value.
 */
CFGLDRR3DECL(int) CFGLDRQueryUInt64(CFGNODE hnode, const char *pszName, uint64_t *pullValue);

/**
 * Set 64 bit unsigned value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   ullValue        The value.
 */
CFGLDRR3DECL(int) CFGLDRSetUInt64(CFGNODE hnode, const char *pszName, uint64_t ullValue);

/**
 * Set 64 bit unsigned value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   ullValue        The value.
 * @patam   uiBase          0, 8 or 16.
 */
CFGLDRR3DECL(int) CFGLDRSetUInt64Ex(CFGNODE hnode, const char *pszName, uint64_t ullValue, unsigned int uiBase);

/**
 * Query 32 bit signed integer value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   plValue         Pointer to 32 bit variable where to store the value.
 */
CFGLDRR3DECL(int) CFGLDRQueryInt32(CFGNODE hnode, const char *pszName, int32_t *plValue);

/**
 * Set 32 bit signed integer value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   lValue          The value.
 */
CFGLDRR3DECL(int) CFGLDRSetInt32(CFGNODE hnode, const char *pszName, int32_t lValue);

/**
 * Set 32 bit signed integer value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   lValue          The value.
 * @patam   uiBase          0, 8 or 16.
 */
CFGLDRR3DECL(int) CFGLDRSetInt32Ex(CFGNODE hnode, const char *pszName, int32_t lValue, unsigned int uiBase);

/**
 * Query 64 bit signed integer value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pllValue        Pointer to 64 bit variable where to store the value.
 */
CFGLDRR3DECL(int) CFGLDRQueryInt64(CFGNODE hnode, const char *pszName, int64_t *pllValue);

/**
 * Set 64 bit signed integer value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   llValue         The value.
 */
CFGLDRR3DECL(int) CFGLDRSetInt64(CFGNODE hnode, const char *pszName, int64_t llValue);

/**
 * Set 64 bit signed integer value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   llValue         The value.
 * @patam   uiBase          0, 8 or 16.
 */
CFGLDRR3DECL(int) CFGLDRSetInt64Ex(CFGNODE hnode, const char *pszName, int64_t llValue, unsigned int uiBase);

/**
 * Query 16 bit unsigned integer value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   puhValue        Pointer to 16 bit variable where to store the value.
 */
CFGLDRR3DECL(int) CFGLDRQueryUInt16(CFGNODE hnode, const char *pszName, uint16_t *puhValue);

/**
 * Set 16 bit unsigned integer value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   uhValue         The value.
 */
CFGLDRR3DECL(int) CFGLDRSetUInt16(CFGNODE hnode, const char *pszName, uint16_t uhValue);

/**
 * Set 16 bit unsigned integer value attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   uhValue         The value.
 * @patam   uiBase          0, 8 or 16.
 */
CFGLDRR3DECL(int) CFGLDRSetUInt16Ex(CFGNODE hnode, const char *pszName, uint16_t uhValue, unsigned int uiBase);

/**
 * Query binary data attribute. If the size of the buffer (cbValue)
 * is smaller than the binary data size, the VERR_BUFFER_OVERFLOW status
 * code is returned and the actual data size is stored into the variable
 * pointed to by pcbValue; pvValue is ignored in this case.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pvValue         Where to store the binary data.
 * @param   cbValue         Size of buffer pvValue points to.
 * @param   pcbValue        Where to store the number of bytes actually retrieved.
 */
CFGLDRR3DECL(int) CFGLDRQueryBin(CFGNODE hnode, const char *pszName, void *pvValue, unsigned cbValue, unsigned *pcbValue);

/**
 * Set binary data attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pvValue         The binary data to store.
 * @param   cbValue         Size of buffer pvValue points to.
 */
CFGLDRR3DECL(int) CFGLDRSetBin(CFGNODE hnode, const char *pszName, void *pvValue, unsigned cbValue);

/**
 * Queries a string attribute. If the size of the buffer (cbValue)
 * is smaller than the length of the string, the VERR_BUFFER_OVERFLOW status
 * code is returned and the actual string length is stored into the variable
 * pointed to by pcbValue; pszValue is ignored in this case.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pszValue        Where to store the string (UTF-8).
 * @param   cbValue         Size of buffer pszValue points to, including the terminating zero.
 * @param   pcbValue        Where to store the number of bytes actually retrieved, including the terminating zero.
 */
CFGLDRR3DECL(int) CFGLDRQueryString(CFGNODE hnode, const char *pszName, char *pszValue, unsigned cbValue, unsigned *pcbValue);

#ifdef CFGLDR_HAVE_COM
/**
 * Queries a string attribute and returns it as an allocated OLE BSTR.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   ppuszValue      Where to store the pointer to the allocated BSTR.
 */
CFGLDRR3DECL(int) CFGLDRQueryBSTR(CFGNODE hnode, const char *pszName, BSTR *ppuszValue);
#endif // CFGLDR_HAVE_COM

/**
 * Queries a UUID attribute and returns it into the supplied buffer.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pUUID           Where to store the UUID.
 */
CFGLDRR3DECL(int) CFGLDRQueryUUID(CFGNODE hnode, const char *pszName, PRTUUID pUUID);

/**
 * Set UUID attribute. It will be put inside curly brackets.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pUuid           The uuid.
 */
CFGLDRR3DECL(int) CFGLDRSetUUID(CFGNODE hnode, const char *pszName, PCRTUUID pUuid);

/**
 * Set string attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pszValue        The string (UTF-8).
 * @param   cbValue         Size of buffer pszValue points to.
 */
CFGLDRR3DECL(int) CFGLDRSetString(CFGNODE hnode, const char *pszName, const char *pszValue);

#ifdef CFGLDR_HAVE_COM
/**
 * Set BSTR attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   bstrValue       The string (COM BSTR).
 * @param   cbValue         Size of buffer pszValue points to.
 */
CFGLDRR3DECL(int) CFGLDRSetBSTR(CFGNODE hnode, const char *pszName, const BSTR bstrValue);
#endif // CFGLDR_HAVE_COM

/**
 * Queries a boolean attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pfValue         Where to store the value.
 */
CFGLDRR3DECL(int) CFGLDRQueryBool(CFGNODE hnode, const char *pszName, bool *pfValue);

/**
 * Sets a boolean attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   fValue          The boolean value.
 */
CFGLDRR3DECL(int) CFGLDRSetBool(CFGNODE hnode, const char *pszName, bool fValue);

/**
 * Query 64-bit unix time (in milliseconds since 1970-01-01 UTC).
 * The specified node or attribute must be a string in xsd:dateTime format.
 *
 * Currently, only the UTC ('Z') time zone is supported and must always present.
 * If there is a different timezone, or no timezone at all, VERR_PARSE_ERROR
 * is returned (use XML Schema constarints to limit the value space and prevent
 * this error).
 *
 * Also note that the minimum 64-bit unix date year is
 * about -292269053 and the maximum year is about ~292272993.
 * It's a good idea to limit the max and min date values in the XML Schema
 * as well, to something like -200000000 to 200000000.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be queried.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   pu64Value       Pointer to 64 bit variable where to store the time value.
 */
CFGLDRR3DECL(int) CFGLDRQueryDateTime(CFGNODE hnode, const char *pszName, int64_t *pi64Value);

/**
 * Set 64-bit unix time (in milliseconds since 1970-01-01 UTC).
 * Time is written to the specified node or attribute in the xsd:format.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be set.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 * @param   u64Value        The time value.
 */
CFGLDRR3DECL(int) CFGLDRSetDateTime(CFGNODE hnode, const char *pszName, int64_t i64Value);

/**
 * Delete an attribute.
 *
 * @return  VBox status code.
 * @param   hnode           Node which attribute will be deleted.
 * @param   pszName         Name of the attribute (UTF-8), NULL for the node value.
 */
CFGLDRR3DECL(int) CFGLDRDeleteAttribute(CFGNODE hnode, const char *pszName);

// hack to not have to define BSTR
#ifdef BSTR_REDEFINED
#undef BSTR
#endif

#endif /* IN_RING3 */

__END_DECLS

/** @} */

#endif
