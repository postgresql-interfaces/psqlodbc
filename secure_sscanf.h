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

#ifndef __SECURESSCANF_H__
#define __SECURESSCANF_H__

#define MAX_STRING_OUTPUT                  4096 * 4
#define ERROR_SUCCESS                      0
#define ERROR_NUMERIC_CONVERSION           -1
#define ERROR_BUFFER_TOO_SMALL             -2
#define ERROR_INVALID_FMT                  -3
#define ERROR_INVALID_TYPE                 -4

enum CheckTypes {
    TYPE_DEFAULT,
    TYPE_FLOAT,
    TYPE_INT,
    TYPE_UINT,
    TYPE_SHORT,
    TYPE_USHORT,
    TYPE_LONG,
    TYPE_ULONG,
    TYPE_LLONG,
    TYPE_ULLONG,
    TYPE_CHAR,
    TYPE_UCHAR,
    TYPE_STRING
};

#define PUT_VALUE(ptr, value) \
    do { if (ptr) *(ptr) = (value); } while (0)

#define VALIDATE_FMT_TYPE(type) \
    do { if (iType != (type)) { PUT_VALUE(pError, ERROR_INVALID_TYPE); return pCurrent; } } while (0)

#define ARG_FLOAT(arg)                     TYPE_FLOAT, (arg)
#define ARG_INT(arg)                       TYPE_INT, (arg)
#define ARG_UINT(arg)                      TYPE_UINT, (arg)
#define ARG_SHORT(arg)                     TYPE_SHORT, (arg)
#define ARG_USHORT(arg)                    TYPE_USHORT, (arg)
#define ARG_LONG(arg)                      TYPE_LONG, (arg)
#define ARG_ULONG(arg)                     TYPE_ULONG, (arg)
#define ARG_LLONG(arg)                     TYPE_LLONG, (arg)
#define ARG_ULLONG(arg)                    TYPE_ULLONG, (arg)
#define ARG_CHAR(arg)                      TYPE_CHAR, (arg)
#define ARG_UCHAR(arg)                     TYPE_UCHAR, (arg)
#define ARG_STR(arg, size)                 TYPE_STRING, (arg), (size)

#if defined(__GNUC__) && __GNUC__ >= 7
    #define FALL_THROUGH __attribute__ ((fallthrough))
#else
    #define FALL_THROUGH ((void)0)
#endif /* __GNUC__ >= 7 */

#ifdef _MSC_VER
    #define ALWAYS_INLINE __forceinline
#elif defined(__GNUC__)
    #define ALWAYS_INLINE __attribute__((always_inline)) inline
#else
    #define ALWAYS_INLINE inline
#endif

int secure_sscanf(const char *pInputString, int *pStatus, const char *pFmt, ... );

#endif /* __SECURESSCANF_H__ */
