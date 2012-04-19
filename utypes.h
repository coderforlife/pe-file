// Adapted from the uSTL library:
// Copyright (c) 2005 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#define _CRT_SECURE_NO_WARNINGS

#include <stddef.h>
#include <limits.h>
#include <float.h>
#include <sys/types.h>
#if _MSC_VER >= 1600 || (defined(_WIN32) && defined(__GNUC__))
#include <stdint.h>
#else
#include <yvals.h>
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
#endif
#ifndef WCHAR_MIN
#define WCHAR_MIN	0x0000
#define WCHAR_MAX	0xffff
#endif
#ifdef _MSC_VER
#if _WIN64
typedef __int64 ssize_t;
#else
typedef _W64 int ssize_t;
#endif
#endif

typedef size_t		uoff_t;			///< A type for storing offsets into blocks measured by size_t.
typedef uint32_t	hashvalue_t;	///< Value type returned by the hash functions.
typedef size_t		streamsize;		///< Size of stream data
typedef uoff_t		streamoff;		///< Offset into a stream
