//  Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Library General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Library General Public License for more details.
//
//  You should have received a copy of the GNU Library General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

/** 
* NAME 
*    secure_sscanf - secure replacement for sscanf()
* 
* SYNOPSIS 
*    #include "secure_sscanf.h"
*
*    int 
*    secure_sscanf(const char *pInputString, int *pStatus, const char *pFmt, ... ) 
* 
* DESCRIPTION 
*    The secure_sscanf() function is a secure replacement for sscanf(). It
*    scans formatted input from the string str according to the format string
*    format, as described in the FORMATS section below. secure_sscanf()
*    differs from sscanf() in the following ways:
*
*    1. Types are passed using ARG_TYPE() macros.
*    2. Output values are initialized even on errors.
*    3. Numeric conversion errors will be propagated.
*    4. The '%s' format specifier takes an additional argument, a size_t of the
*       destination buffer.
*    5. Output is guaranteed to be \0 terminated.
*    6. Max string output is capped to MAX_STRING_OUTPUT (4 * PAGE_SIZE).
* 
* FORMATS
*    The format string is composed of zero or more directives: one or more
*    white-space characters, an ordinary character (neither '%' nor a white-
*    space character), or a conversion specification.  Each conversion
*    specification is introduced by the character '%' after which the following
*    appear in sequence:
* 
*    %c - matches a character or a sequence of characters 
*    %s - matches a sequence of non-whitespace characters (a string) 
*    %d - Matches an optionally signed decimal integer 
*    %u - Matches an unsigned decimal integer 
*    %x - Matches an unsigned hexadecimal integer 
*    %f - Matches an optionally signed floating-point number 
* 
*    The arguments for the format string specifiers are passed through the following macros:
*
*    ARG_STR(&buf, sizeof(buf)) for strings (%s)
*    ARG_INT(&int1) for integers (%d, %u, %x)
*    ARG_FLOAT(&float1) for floats (%f)
*    ARG_CHAR(&char1) for char (%c)

* RETURN VALUE
*    The secure_sscanf() function returns the number of input items
*    successfully matched and assigned; this can be fewer than provided for,
*    or even zero, in the event of an early matching failure between the input
*    string and a directive.
*
*    The pStatus argument will be set to one of the following values:
*
*    ERROR_SUCCESS
*    ERROR_NUMERIC_CONVERSION
*    ERROR_BUFFER_TOO_SMALL
*    ERROR_INVALID_FMT
*    ERROR_INVALID_TYPE 
*
* EXAMPLES
*
*    ret = secure_sscanf(pInput, &status, "%s %f %c", 
*                                              ARG_STR(&string1, sizeof(string1)),
*                                              ARG_FLOAT(&float1),
*                                              ARG_CHAR(&char1));
*
*    if (ret != 3 || status != ERROR_SUCCESS)
*        // handle error
*
* SEE ALSO 
*    sscanf(3) 
* 
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <limits.h>

#include "secure_sscanf.h"

ALWAYS_INLINE char *
skip_spaces(const char *pInputString)
{
    while (isspace(*pInputString)) {
        ++pInputString;
    }

    return (char *)pInputString;
}

const char *
parse_arg(int *pError, const char *pFmt, const char *pInputString, va_list *args)
{
    const char *pCurrent                    = pInputString;
    const char *pParse                      = pInputString;
    char trimCurr[MAX_STRING_OUTPUT]        = { 0 };

    char *pEnd                              = NULL;
    char *pFmtEnd                           = NULL;

    void *pOutput                           = NULL;

    float fRet                              = 0.0f;
    long lRet                               = 0;
    long long llRet                         = 0;
    unsigned long ulRet                     = 0;
    unsigned long long ullRet               = 0;

    int iBase                               = 10;
    int iType                               = 0;

    size_t cbSize                           = 0;

    unsigned int uWidth                     = 0;
    unsigned char useTrim                   = 0;

    PUT_VALUE(pError, ERROR_SUCCESS);

    lRet = strtol(pFmt, &pFmtEnd, 10);

    if (((lRet == HUGE_VALF || lRet == -HUGE_VALF) && (errno == ERANGE)) || (lRet > UINT_MAX)) {
        PUT_VALUE(pError, ERROR_INVALID_FMT);
        return pCurrent;
    }

    uWidth = lRet;
    if (uWidth > 0) {
        useTrim = 1;
        strncpy(trimCurr, pCurrent, uWidth);
        trimCurr[uWidth] = '\0';
        pParse = trimCurr;
    } else {
        pParse = pCurrent;
    }

    pFmt = pFmtEnd;

    iType = va_arg(*args, int);

    long spType = TYPE_DEFAULT;
    switch (*pFmt) {
        case 'h':
            {
                pFmt++;
                if (*pFmt == 'h') {
                    pFmt++;
                    spType = TYPE_CHAR;
                } else {
                    spType = TYPE_SHORT;
                }
                break;
            }

        case 'I':
            {
                pFmt++;
                if (*pFmt == '6') {
                    pFmt++;
                    if (*pFmt == '4') {
                        pFmt++;
                        spType = TYPE_LLONG;
                        break;
                    } 
                }
                PUT_VALUE(pError, ERROR_INVALID_FMT);
                return skip_spaces(pCurrent);
            }

        case 'l':
            {
                pFmt++;
                if (*pFmt == 'l') {
                    pFmt++;
                    spType = TYPE_LLONG;
                } else {
                    spType = TYPE_LONG;
                }
                break;
            }
        default:
            break;
    }

    switch (*pFmt) {
        case 'f':
            {
                VALIDATE_FMT_TYPE(TYPE_FLOAT);
                pOutput = (void *)va_arg(*args, float *);
                PUT_VALUE((float *)pOutput, 0.0f);

                fRet = strtof(pParse, &pEnd);
                if (pEnd == pParse) {
                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                    return skip_spaces(pCurrent);
                }

                if (useTrim) {
                    while (uWidth-- > 0 && *pParse != *pEnd) {
                        pCurrent++;
                        pParse++;
                    } 
                } else {
                    pCurrent = pEnd;
                }

                if (((fRet == HUGE_VALF || fRet == -HUGE_VALF)) && (errno == ERANGE)) {
                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                    return skip_spaces(pCurrent);
                }

                PUT_VALUE((float *)pOutput, fRet);
                break;
            }

        case 'i':   
            {
                if (*pCurrent == '0') {
                    if (*(pCurrent + 1) == 'x') {
                        iBase = 16;
                    } else {
                        iBase = 8;
                    }
                }
                FALL_THROUGH;
            }

        case 'd':
            {
                VALIDATE_FMT_TYPE(spType == TYPE_DEFAULT ? TYPE_INT : spType);
                switch (spType) {
                    case TYPE_CHAR:
                        {
                            pOutput = (void *)va_arg(*args, char *);
                            PUT_VALUE((char *)pOutput, 0);
                            lRet = strtol(pParse, &pEnd, iBase);
                            break;
                        }

                    case TYPE_SHORT:
                        {
                            pOutput = (void *)va_arg(*args, short *);
                            PUT_VALUE((short *)pOutput, 0);
                            lRet = strtol(pParse, &pEnd, iBase);
                            break;
                        }

                    case TYPE_LONG:
                        {
                            pOutput = (void *)va_arg(*args, long *);
                            PUT_VALUE((long *)pOutput, 0);
                            lRet = strtol(pParse, &pEnd, iBase);
                            break;
                        }

                    case TYPE_LLONG:
                        {
                            pOutput = (void *)va_arg(*args, long long *);
                            PUT_VALUE((long long *)pOutput, 0);
                            llRet = strtoll(pParse, &pEnd, iBase);
                            break;
                        }

                    case TYPE_DEFAULT:
                        {
                            FALL_THROUGH;
                        }
                    default:
                        {
                            pOutput = (void *)va_arg(*args, unsigned int *);
                            PUT_VALUE((int *)pOutput, 0);
                            lRet = strtol(pParse, &pEnd, iBase);
                        }
                }

                if (pEnd == pParse) {
                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                    return skip_spaces(pCurrent);
                }

                if (useTrim) {
                    while (uWidth-- > 0 && *pParse != *pEnd) {
                        pCurrent++;
                        pParse++;
                    } 
                } else {
                    pCurrent = pEnd;
                }

                if ((lRet == HUGE_VAL) && (errno == ERANGE)) {
                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                    return skip_spaces(pCurrent);
                }

                switch (spType) {
                        case TYPE_CHAR:
                            {
                                if (lRet > CHAR_MAX) {
                                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                                    return skip_spaces(pCurrent);
                                }
                                PUT_VALUE((char *)pOutput, lRet);
                                break;
                            }

                        case TYPE_SHORT:
                            {
                                if (lRet > SHRT_MAX) {
                                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                                    return skip_spaces(pCurrent);
                                }
                                PUT_VALUE((short *)pOutput, lRet);
                                break;
                            }

                        case TYPE_LONG:
                            {
                                if (lRet > LONG_MAX) {
                                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                                    return skip_spaces(pCurrent);
                                }
                                PUT_VALUE((long *)pOutput, lRet);
                                break;
                            }

                        case TYPE_LLONG:
                            {
                                if (llRet > LLONG_MAX) {
                                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                                    return skip_spaces(pCurrent);
                                }
                                PUT_VALUE((long long *)pOutput, llRet);
                                break;
                            }

                        case TYPE_DEFAULT:
                            {
                                FALL_THROUGH;
                            }
                        default:
                            {
                                if (lRet > INT_MAX) {
                                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                                    return skip_spaces(pCurrent);
                                }
                                PUT_VALUE((int *)pOutput, lRet);
                            }
                }
                break;
            }

        case 'X':
            {
                FALL_THROUGH;
            }

        case 'x':
            {
                iBase = 16;
                FALL_THROUGH;    
            }

        case 'u':
            {
                // Up by 1 to "unsigned" type
                unsigned long type = spType == TYPE_DEFAULT ? TYPE_UINT : spType + 1;
                VALIDATE_FMT_TYPE(type);
                switch (type) {
                    case TYPE_UCHAR:
                        {
                            pOutput = (void *)va_arg(*args, unsigned char *);
                            PUT_VALUE((unsigned char *)pOutput, 0);
                            ulRet = strtoul(pParse, &pEnd, iBase);
                            break;
                        }

                    case TYPE_USHORT:
                        {
                            pOutput = (void *)va_arg(*args, unsigned short *);
                            PUT_VALUE((unsigned short *)pOutput, 0);
                            ulRet = strtoul(pParse, &pEnd, iBase);
                            break;
                        }

                    case TYPE_ULONG:
                        {
                            pOutput = (void *)va_arg(*args, unsigned long *);
                            PUT_VALUE((unsigned long *)pOutput, 0);
                            ulRet = strtoul(pParse, &pEnd, iBase);
                            break;
                        }

                    case TYPE_ULLONG:
                        {
                            pOutput = (void *)va_arg(*args, unsigned long long *);
                            PUT_VALUE((unsigned long long *)pOutput, 0);
                            ullRet = strtoull(pParse, &pEnd, iBase);
                            break;
                        }

                    case TYPE_DEFAULT:
                        {
                            FALL_THROUGH;
                        }
                    default:
                        {
                            pOutput = (void *)va_arg(*args, unsigned int *);
                            PUT_VALUE((unsigned int *)pOutput, 0);
                            ulRet = strtoul(pParse, &pEnd, iBase);
                        }
                }

                if (pEnd == pCurrent) {
                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                    return skip_spaces(pCurrent);
                }

                if (useTrim) {
                    while (uWidth-- > 0 && *pParse != *pEnd) {
                        pCurrent++;
                        pParse++;
                    }
                } else {
                    pCurrent = pEnd;
                }

                if ((ulRet == HUGE_VAL) && (errno == ERANGE)) {
                    PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                    return skip_spaces(pCurrent);
                }

                switch (type) {
                    case TYPE_UCHAR:
                        {
                            if (ulRet > UCHAR_MAX) {
                                PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                                return skip_spaces(pCurrent);
                            }
                            PUT_VALUE((unsigned char *)pOutput, ulRet);
                            break;
                        }

                    case TYPE_USHORT:
                        {
                            if (ulRet > USHRT_MAX) {
                                PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                                return skip_spaces(pCurrent);
                            }
                            PUT_VALUE((unsigned short *)pOutput, ulRet);
                            break;
                        }

                    case TYPE_ULONG:
                        {
                            if (ulRet > ULONG_MAX) {
                                PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                                return skip_spaces(pCurrent);
                            }
                            PUT_VALUE((unsigned long *)pOutput, ulRet);
                            break;
                        }

                    case TYPE_ULLONG:
                        {
                            if (ullRet > ULLONG_MAX) {
                                PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                                return skip_spaces(pCurrent);
                            }
                            PUT_VALUE((unsigned long long *)pOutput, ullRet);
                            break;
                        }

                    case TYPE_DEFAULT:
                        {
                            FALL_THROUGH;
                        }
                    default:
                        {
                            if (lRet > UINT_MAX) {
                                PUT_VALUE(pError, ERROR_NUMERIC_CONVERSION);
                                return skip_spaces(pCurrent);
                            }
                            PUT_VALUE((unsigned int *)pOutput, ulRet);
                        }
                }
                break;
            }

        case 'c':
            {
                VALIDATE_FMT_TYPE(TYPE_CHAR);
                pOutput = (void *)va_arg(*args, unsigned char *);
                PUT_VALUE((char *)pOutput, *pInputString);
                break;
            }

        case 's':
            {
                VALIDATE_FMT_TYPE(TYPE_STRING);
                pOutput = (void *)va_arg(*args, char *);
                cbSize  = va_arg(*args, size_t);

                if (!pOutput)
                    return pCurrent;

                if (cbSize > MAX_STRING_OUTPUT) {
                    PUT_VALUE(pError, ERROR_BUFFER_TOO_SMALL);
                    return pCurrent;
                }

                if (cbSize) {
                    memset(pOutput, '\0', useTrim ? uWidth : cbSize);
                    cbSize--;
                    while (pCurrent[0] != '\0' && !isspace(pCurrent[0]) && cbSize != 0 && (useTrim ? uWidth-- > 0 : 1)) {
                        #ifdef _MSC_VER
                            *((char*)pOutput)++ = *pCurrent++;
                        #else
                            *(char *)pOutput++ = *pCurrent++;
                        #endif
                        cbSize--;
                    }

                    if (cbSize == 0) {
                        /* Truncated ... Should this condition be propogated ? */
                        while (pCurrent[0] != '\0' && !isspace(pCurrent[0]))
                            pCurrent++;
                    }
                }

                break;
            }

        default:
            {
                PUT_VALUE(pError, ERROR_INVALID_FMT);
                return pCurrent;
            }
    }

    return skip_spaces(pCurrent);
}

int
secure_sscanf(const char *pInputString, int *pStatus, const char *pFmt, ... )
{
    const char *pFmtEnd = NULL;

    int error           = 0;
    int ret             = 0;

    if (!pInputString || !pFmt || !pStatus)
        return 0;

    *pStatus = ERROR_SUCCESS;

    va_list args;
    va_start(args, pFmt);

    pFmtEnd = pFmt + strlen(pFmt);

    while (pFmt[0] != '\0' && pFmt < pFmtEnd && pInputString[0] != '\0') {
        if (pFmt[0] == '%') {
            const char *tmp = parse_arg(&error, &pFmt[1], pInputString, &args);

            if (error != ERROR_SUCCESS) {
                *pStatus = error;
                return ret;
            }

            if (tmp == pInputString) {
                break;
            }

            if (pFmt[1] != '%') {
                ++ret;
            }

            ++pFmt;

            while ((pFmt[0] >= '0' && pFmt[0] <= '9') ||pFmt[0] == 'h' || pFmt[0] == 'l') {
                ++pFmt;
            }

            ++pFmt;
            pInputString = tmp;

        } else if (isspace(pFmt[0])) {
            ++pFmt;
            pInputString = skip_spaces(pInputString);
        } else if (pFmt[0] == pInputString[0]) {
            ++pFmt;
            ++pInputString;
        } else {
            break;
        }
    }

    va_end(args);

    return ret;
}
