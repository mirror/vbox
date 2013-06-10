/* $Id$ */
/** @file
 * VBoxDef2LazyLoad - Lazy Library Loader Generator.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iprt/types.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct MYEXPORT
{
    struct MYEXPORT    *pNext;
    bool                fNoName;
    unsigned            uOrdinal;
    char                szName[1];
} MYEXPORT;
typedef MYEXPORT *PMYEXPORT;



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** @name Options
 * @{ */
static const char  *g_pszOutput   = NULL;
static const char  *g_pszInput    = NULL;
static const char  *g_pszLibrary  = NULL;
static bool         g_fIgnoreData = true;
/** @} */

/** Pointer to the export name list head. */
static PMYEXPORT    g_pExpHead  = NULL;
/** Pointer to the next pointer for insertion. */
static PMYEXPORT   *g_ppExpNext = &g_pExpHead;



static const char *leftStrip(const char *psz)
{
    while (isspace(*psz))
        psz++;
    return psz;
}


static char *leftStrip(char *psz)
{
    while (isspace(*psz))
        psz++;
    return psz;
}


static unsigned wordLength(const char *pszWord)
{
    unsigned off = 0;
    char ch;
    while (   (ch = pszWord[off]) != '\0'
           && ch                  != '='
           && ch                  != ','
           && ch                  != ':'
           && !isspace(ch) )
        off++;
    return off;
}


/**
 * Parses the module definition file, collecting export information.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE, in the latter case full
 *          details has been displayed.
 * @param   pInput              The input stream.
 */
static RTEXITCODE parseInputInner(FILE *pInput)
{
    /*
     * Process the file line-by-line.
     */
    bool        fInExports = false;
    unsigned    iLine      = 0;
    char        szLine[16384];
    while (fgets(szLine, sizeof(szLine), pInput))
    {
        iLine++;

        /*
         * Strip leading and trailing spaces from the line as well as
         * trailing comments.
         */
        char *psz = leftStrip(szLine);
        if (*psz == ';')
            continue; /* comment line. */

        char *pszComment = strchr(psz, ';');
        if (pszComment)
            *pszComment = '\0';

        unsigned cch = (unsigned)strlen(psz);
        while (cch > 0 && (isspace(psz[cch - 1]) || psz[cch - 1] == '\r' || psz[cch - 1] == '\n'))
            psz[--cch] = '\0';

        if (!cch)
            continue;

        /*
         * Check for known directives.
         */
        size_t cchWord0 = wordLength(psz);
#define WORD_CMP(pszWord1, cchWord1, szWord2) \
            ( (cchWord1) == sizeof(szWord2) - 1 && memcmp(pszWord1, szWord2, sizeof(szWord2) - 1) == 0 )
fprintf(stderr,"word: %u: cchWord0=%d '%s'\n", iLine, cchWord0, psz);
        if (WORD_CMP(psz, cchWord0, "EXPORTS"))
        {
            fInExports = true;

            /* In case there is an export on the same line. (Really allowed?) */
            psz = leftStrip(psz + sizeof("EXPORTS") - 1);
            if (!*psz)
                continue;
        }
        /* Directives that we don't care about, but need to catch in order to
           terminate the EXPORTS section in a timely manner. */
        else if (   WORD_CMP(psz, cchWord0, "NAME")
                 || WORD_CMP(psz, cchWord0, "LIBRARY")
                 || WORD_CMP(psz, cchWord0, "DESCRIPTION")
                 || WORD_CMP(psz, cchWord0, "STACKSIZE")
                 || WORD_CMP(psz, cchWord0, "SECTIONS")
                 || WORD_CMP(psz, cchWord0, "SEGMENTS")
                 || WORD_CMP(psz, cchWord0, "VERSION")
                )
        {
            fInExports = false;
        }

        /*
         * Process exports:
         *  entryname[=internalname] [@ordinal[ ][NONAME]] [DATA] [PRIVATE]
         */
        if (fInExports)
        {
            const char *pchName = psz;
            unsigned    cchName = wordLength(psz);

            psz = leftStrip(psz + cchName);
            if (*psz == '=')
            {
                psz = leftStrip(psz + 1);
                psz = leftStrip(psz + wordLength(psz));
            }

            bool     fNoName  = true;
            unsigned uOrdinal = ~0U;
            if (*psz == '@')
            {
                psz++;
                if (!isdigit(*psz))
                {
                    fprintf(stderr, "%s:%u: error: Invalid ordinal spec.\n", g_pszInput, iLine);
                    return RTEXITCODE_FAILURE;
                }
                uOrdinal = *psz++ - '0';
                while (isdigit(*psz))
                {
                    uOrdinal *= 10;
                    uOrdinal += *psz++ - '0';
                }
                psz = leftStrip(psz);
                cch = wordLength(psz);
                if (WORD_CMP(psz, cch, "NONAME"))
                {
#if 0
                    fNoName = true;
                    psz = leftStrip(psz + cch);
#else
                    fprintf(stderr, "%s:%u: error: NONAME export not implemented.\n", g_pszInput, iLine);
                    return RTEXITCODE_FAILURE;
#endif
                }
            }

            while (*psz)
            {
                cch = wordLength(psz);
                if (WORD_CMP(psz, cch, "DATA"))
                {
                    if (!g_fIgnoreData)
                    {
                        fprintf(stderr, "%s:%u: error: Cannot wrap up DATA export '%.*s'.\n",
                                g_pszInput, iLine, cchName, pchName);
                        return RTEXITCODE_SUCCESS;
                    }
                }
                else if (!WORD_CMP(psz, cch, "PRIVATE"))
                {
                    fprintf(stderr, "%s:%u: error: Cannot wrap up DATA export '%.*s'.\n",
                            g_pszInput, iLine, cchName, pchName);
                    return RTEXITCODE_SUCCESS;
                }
                psz = leftStrip(psz + cch);
            }

            /*
             * Add the export.
             */
            PMYEXPORT pExp = (PMYEXPORT)malloc(sizeof(*pExp) + cchName);
            if (!pExp)
            {
                fprintf(stderr, "%s:%u: error: Out of memory.\n", g_pszInput, iLine);
                return RTEXITCODE_SUCCESS;
            }
            memcpy(pExp->szName, pchName, cchName);
            pExp->szName[cchName] = '\0';
            pExp->uOrdinal = uOrdinal;
            pExp->fNoName  = fNoName;
            pExp->pNext    = NULL;
            *g_ppExpNext   = pExp;
            g_ppExpNext    = &pExp->pNext;
        }
    }

    /*
     * Why did we quit the loop, EOF or error?
     */
    if (feof(pInput))
        return RTEXITCODE_SUCCESS;
    fprintf(stderr, "error: Read while reading '%s' (iLine=%u).\n", g_pszInput, iLine);
    return RTEXITCODE_FAILURE;
}


/**
 * Parses g_pszInput, populating the list pointed to by g_pExpHead.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE, in the latter case full
 *          details has been displayed.
 */
static RTEXITCODE  parseInput(void)
{
    RTEXITCODE rcExit = RTEXITCODE_FAILURE;
    FILE *pInput = fopen(g_pszInput, "r");
    if (pInput)
    {
        rcExit = parseInputInner(pInput);
        fclose(pInput);
        if (rcExit == RTEXITCODE_SUCCESS && !g_pExpHead)
        {
            fprintf(stderr, "error: Found no exports in '%s'.\n", g_pszInput);
            rcExit = RTEXITCODE_FAILURE;
        }
    }
    else
        fprintf(stderr, "error: Failed to open '%s' for reading.\n", g_pszInput);
    return rcExit;
}


static RTEXITCODE generateOutputInner(FILE *pOutput)
{
    fprintf(pOutput,
            ";; Autogenerated from '%s'. DO NOT EDIT!\n"
            "\n"
            "%%include \"iprt/asmdefs.mac\"\n"
            "\n"
            "BEGINCODE\n",
            g_pszInput);

    for (PMYEXPORT pExp = g_pExpHead; pExp; pExp = pExp->pNext)
    {
        fprintf(pOutput,
                "BEGINDATA\n"
                "%%ifdef ASM_FORMAT_PE\n"
                "global __imp_%s\n"
                "__imp_%s:\n"
                "%%endif\n"
                "g_pfn%s RTCCPTR_DEF ___LazyLoad___%s\n"
                "BEGINCODE\n"
                "BEGINPROC %s\n"
                "    jmp RTCCPTR_PRE [g_pfn%s xWrtRIP]\n"
                "ENDPROC   %s\n"
                "___LazyLoad___%s:\n"
                /* "int3\n" */
                "%%ifdef RT_ARCH_AMD64\n"
                "    lea     rax, [.szName wrt rip]\n"
                "    lea     r10, [g_pfn%s wrt rip]\n"
                "%%elifdef RT_ARCH_X86\n"
                "    push    .szName\n"
                "    push    g_pfn%s\n"
                "%%else\n"
                " %%error \"Unsupported architecture\"\n"
                "%%endif\n"
                "    call    NAME(LazyLoadResolver)\n"
                "%%ifdef RT_ARCH_X86\n"
                "    add     esp, 8h\n"
                "%%endif\n"
                "    jmp     NAME(%s)\n"
                ".szName db '%s',0\n"
                "\n"
                ,
                pExp->szName,
                pExp->szName,
                pExp->szName, pExp->szName,
                pExp->szName,
                pExp->szName,
                pExp->szName,
                pExp->szName,
                pExp->szName,
                pExp->szName,
                pExp->szName,
                pExp->szName);
    }

    /*
     * The code that does the loading and resolving.
     */
    fprintf(pOutput,
            "BEGINDATA\n"
            "g_hMod RTCCPTR_DEF 0\n"
            "\n"
            "BEGINCODE\n");

    /*
     * How we load the module needs to be selectable later on.
     *
     * The LazyLoading routine returns the module handle in RCX/ECX, caller
     * saved all necessary registers.
     */
    fprintf(pOutput,
            ";\n"
            ";SUPR3DECL(int) SUPR3HardenedLdrLoadAppPriv(const char *pszFilename, PRTLDRMOD phLdrMod, \n"
            ";                                           uint32_t fFlags, PRTERRINFO pErrInfo);\n"
            ";\n"
            "extern IMPNAME(SUPR3HardenedLdrLoadAppPriv)\n"
            "\n"
            "BEGINPROC LazyLoading\n"
            "    mov     xCX, [g_hMod xWrtRIP]\n"
            "    or      xCX, xCX\n"
            "    jnz     .return\n"
            "\n"
            "%%ifdef ASM_CALL64_GCC\n"
            "    xor     rcx, rcx               ; pErrInfo (local load)\n"
            "    xor     rdx, rdx               ; fFlags (local load)\n"
            "    lea     rsi, [g_hMod wrt rip]  ; phLdrMod\n"
            "    lea     rdi, [.szLib wrt rip]  ; pszFilename\n"
            "    call    IMP2(SUPR3HardenedLdrLoadAppPriv)\n"
            "\n"
            "%%elifdef ASM_CALL64_MSC\n"
            "    xor     r9, r9                 ; pErrInfo (local load)\n"
            "    xor     r8, r8                 ; fFlags (local load)\n"
            "    lea     rdx, [g_hMod wrt rip]  ; phLdrMod\n"
            "    lea     rcx, [.szLib wrt rip]  ; pszFilename\n"
            "    sub     rsp, 20h\n"
            "    call    IMP2(SUPR3HardenedLdrLoadAppPriv)\n"
            "    add     rsp, 20h\n"
            "\n"
            "%%elifdef RT_ARCH_X86\n"
            "    push    0              ; pErrInfo\n"
            "    push    0              ; fFlags (local load)\n"
            "    push    g_hMod         ; phLdrMod\n"
            "    push    .szLib         ; pszFilename\n"
            "    call    IMP2(SUPR3HardenedLdrLoadAppPriv)\n"
            "    add     esp, 10h\n"
            "%%else\n"
            " %%error \"Unsupported architecture\"\n"
            "%%endif\n"
            "    or      eax, eax\n"
            "    jz      .loadok\n"
            ".badload:\n"
            "    int3\n"
            "    jmp     .badload\n"
            ".loadok:\n"
            "    mov     xCX, [g_hMod xWrtRIP]\n"
            ".return:\n"
            "    ret\n"
            ".szLib db '%s',0\n"
            "ENDPROC   LazyLoading\n"
            , g_pszLibrary);


    fprintf(pOutput,
            "\n"
            ";\n"
            ";RTDECL(int) RTLdrGetSymbol(RTLDRMOD hLdrMod, const char *pszSymbol, void **ppvValue);\n"
            ";\n"
            "extern IMPNAME(RTLdrGetSymbol)\n"
            "BEGINPROC LazyLoadResolver\n"
            "%%ifdef RT_ARCH_AMD64\n"
            "    push    rbp\n"
            "    mov     rbp, rsp\n"
            "    push    r15\n"
            "    push    r14\n"
            "    mov     r15, rax       ; name\n"
            "    mov     r14, r10       ; ppfn\n"
            "    push    r9\n"
            "    push    r8\n"
            "    push    rcx\n"
            "    push    rdx\n"
            " %%ifdef ASM_CALL64_GCC\n"
            "    push    rdi\n"
            "    push    rsi\n"
            " %%else\n"
            "    sub     rsp, 20h\n"
            " %%endif\n"
            "\n"
            "    call    NAME(LazyLoading) ; returns handle in rcx\n"
            " %%ifdef ASM_CALL64_GCC\n"
            "    mov     rdi, rcx       ; hLdrMod\n"
            "    mov     rsi, r15       ; pszSymbol\n"
            "    mov     rdx, r14       ; ppvValue\n"
            " %%else\n"
            "    mov     rdx, r15       ; pszSymbol\n"
            "    mov     r8, r14        ; ppvValue\n"
            " %%endif\n"
            "    call    IMP2(RTLdrGetSymbol)\n"
            "    or      eax, eax\n"
            ".badsym:\n"
            "    jz      .symok\n"
            "    int3\n"
            "    jmp     .badsym\n"
            ".symok:\n"
            "\n"
            " %%ifdef ASM_CALL64_GCC\n"
            "    pop     rdi\n"
            "    pop     rsi\n"
            " %%else\n"
            "    add     rsp, 20h\n"
            " %%endif\n"
            "    pop     rdx\n"
            "    pop     rcx\n"
            "    pop     r8\n"
            "    pop     r9\n"
            "    pop     r14\n"
            "    pop     r15\n"
            "    leave\n"
            "\n"
            "%%elifdef RT_ARCH_X86\n"
            "    push    ebp\n"
            "    mov     ebp, esp\n"
            "    push    eax\n"
            "    push    ecx\n"
            "    push    edx\n"
            "\n"
            ".loaded:\n"
            "    mov     eax, [ebp + 4] ; value addr\n"
            "    push    eax\n"
            "    mov     edx, [ebp + 8] ; symbol name\n"
            "    push    edx\n"
            "    call    NAME(LazyLoading) ; returns handle in ecx\n"
            "    mov     ecx, [g_hMod]\n"
            "    call    IMP2(RTLdrGetSymbol)\n"
            "    add     esp, 12\n"
            "    or      eax, eax\n"
            ".badsym:\n"
            "    jz      .symok\n"
            "    int3\n"
            "    jmp     .badsym\n"
            ".symok:\n"
            "    pop     edx\n"
            "    pop     ecx\n"
            "    pop     eax\n"
            "    leave\n"
            "%%else\n"
            " %%error \"Unsupported architecture\"\n"
            "%%endif\n"
            "    ret\n"
            "ENDPROC   LazyLoadResolver\n"
            );

    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE generateOutput(void)
{
    RTEXITCODE rcExit = RTEXITCODE_FAILURE;
    FILE *pOutput = fopen(g_pszOutput, "w");
    if (pOutput)
    {
        rcExit = generateOutputInner(pOutput);
        if (fclose(pOutput))
        {
            fprintf(stderr, "error: Error closing '%s'.\n", g_pszOutput);
            rcExit = RTEXITCODE_FAILURE;
        }
    }
    else
        fprintf(stderr, "error: Failed to open '%s' for writing.\n", g_pszOutput);
    return rcExit;
}


static int usage(const char *pszArgv0)
{
    printf("usage: %s --libary <loadname> --output <lazyload.asm> <input.def>\n",
           "\n"
           "Copyright (C) 2013 Oracle Corporation\n"
           , pszArgv0);

    return RTEXITCODE_SUCCESS;
}


int main(int argc, char **argv)
{
    /*
     * Parse options.
     */
    for (int i = 1; i < argc; i++)
    {
        const char *psz = argv[i];
        if (*psz == '-')
        {
            if (!strcmp(psz, "--output") || !strcmp(psz, "-o"))
            {
                if (++i >= argc)
                {
                    fprintf(stderr, "syntax error: File name expected after '%s'.\n", psz);
                    return RTEXITCODE_SYNTAX;
                }
                g_pszOutput = argv[i];
            }
            else if (!strcmp(psz, "--library") || !strcmp(psz, "-l"))
            {
                if (++i >= argc)
                {
                    fprintf(stderr, "syntax error: Library name expected after '%s'.\n", psz);
                    return RTEXITCODE_SYNTAX;
                }
                g_pszLibrary = argv[i];
            }
            else if (   !strcmp(psz, "--help")
                     || !strcmp(psz, "-help")
                     || !strcmp(psz, "-h")
                     || !strcmp(psz, "-?") )
                return usage(argv[0]);
            else if (   !strcmp(psz, "--version")
                     || !strcmp(psz, "-V"))
            {
                printf("$Revision$\n");
                return RTEXITCODE_SUCCESS;
            }
            else
            {
                fprintf(stderr, "syntax error: Unknown option '%s'.\n", psz);
                return RTEXITCODE_SYNTAX;
            }

        }
        else
        {
            if (g_pszInput)
            {
                fprintf(stderr, "syntax error: Already specified '%s' as the input file.\n", g_pszInput);
                return RTEXITCODE_SYNTAX;
            }
            g_pszInput = argv[i];
        }
    }
    if (!g_pszInput)
    {
        fprintf(stderr, "syntax error: No input file specified.\n");
        return RTEXITCODE_SYNTAX;
    }
    if (!g_pszOutput)
    {
        fprintf(stderr, "syntax error: No output file specified.\n");
        return RTEXITCODE_SYNTAX;
    }
    if (!g_pszLibrary)
    {
        fprintf(stderr, "syntax error: No library name specified.\n");
        return RTEXITCODE_SYNTAX;
    }

    /*
     * Do the job.
     */
    RTEXITCODE rcExit = parseInput();
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = generateOutput();
    return rcExit;
}

