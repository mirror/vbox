


#pragma pack(4) /* Misdesigned header, not 8-byte aligned size. */
typedef struct XARHEADER
{
    /** The magic number 'xar!' (XAR_HEADER_MAGIC). */
    uint32_t    u32Magic;
    /** The size of this header structure. */
    uint16_t    cbHeader;
    /** The header version structure. */
    uint16_t    uVersion;
    /** The size of the compressed table of content (TOC). */
    uint64_t    cbTocCompressed;
    /** The size of the table of context (TOC) when not compressed. */
    uint64_t    cbTocUncompressed;
    /** Which cryptographic hash function is used (XAR_HASH_XXX). */
    uint32_t    uHashFunction;
} XARHEADER;
#pragma pack()
/** Pointer to a XAR header. */
typedef XARHEADER *PXARHEADER;
/** Pointer to a const XAR header. */
typedef XARHEADER const *PCXARHEADER;

/** XAR magic value (on disk endian). */
#define XAR_HEADER_MAGIC        RT_H2LE_U32(RT_MAKE_U32_FROM_U8('x', 'a', 'r', '!'))
/** The current header version value (host endian). */
#define XAR_HEADER_VERSION      1

/** @name XAR hashing functions.
 * @{ */
#define XAR_HASH_NONE           0
#define XAR_HASH_SHA1           1
#define XAR_HASH_MD5            2
#define XAR_HASH_MAX            2
/** @} */

