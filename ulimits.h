// Adapted from the uSTL library:
// Copyright (c) 2005 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once

#include "utypes.h"

#define __limits_digits(T)		(sizeof(T)*8)

template <typename T> 
struct numeric_limits {
	static inline T min() { return T(0); }
	static inline T max() { return T(0); }
	static const bool is_signed = false;
	static const bool is_integer = false;
	static const bool is_integral = false;
	static const unsigned digits = __limits_digits(T);		///number of bits in T
};

template <typename T>
struct numeric_limits<T*> {
    static inline T* min() { return NULL; }
    static inline T* max() { return reinterpret_cast<T*>(UINTPTR_MAX); }
    static const bool is_signed = false;
    static const bool is_integer = true;
    static const bool is_integral = true;
    static const unsigned digits = __limits_digits(T*);
};

#define NUMERIC_LIMITS(T, minVal, maxVal, bSigned, bInteger, bIntegral)	\
template <>													\
struct numeric_limits<T> {									\
    static inline T min() { return minVal; }				\
    static inline T max() { return maxVal; }				\
    static const bool is_signed = bSigned;					\
    static const bool is_integer = bInteger;				\
    static const bool is_integral = bIntegral;				\
    static const unsigned digits = __limits_digits(T);		\
}

//--------------------------------------------------------------------------------------
//				type				min			max			signed	integer	integral
//--------------------------------------------------------------------------------------
NUMERIC_LIMITS( bool,				false,		true,		false,	true,	true);
NUMERIC_LIMITS( char,				CHAR_MIN,	CHAR_MAX,	true,	true,	true);
NUMERIC_LIMITS( int,				INT_MIN,	INT_MAX,	true,	true,	true);
NUMERIC_LIMITS( short,				SHRT_MIN,	SHRT_MAX,	true,	true,	true);
NUMERIC_LIMITS( long,				LONG_MIN,	LONG_MAX,	true,	true,	true);
NUMERIC_LIMITS( signed char,		SCHAR_MIN,	SCHAR_MAX,	true,	true,	true);
NUMERIC_LIMITS( unsigned char,		0,			UCHAR_MAX,	false,	true,	true);
NUMERIC_LIMITS( unsigned int,		0,			UINT_MAX,	false,	true,	true);
NUMERIC_LIMITS( unsigned short,		0,			USHRT_MAX,	false,	true,	true);
NUMERIC_LIMITS( unsigned long,		0,			ULONG_MAX,	false,	true,	true);
NUMERIC_LIMITS( wchar_t,			0,			WCHAR_MAX,	false,	true,	true);
NUMERIC_LIMITS( float,				FLT_MIN,	FLT_MAX,	true,	false,	true);
NUMERIC_LIMITS( double,				DBL_MIN,	DBL_MAX,	true,	false,	true);
NUMERIC_LIMITS( long double,		LDBL_MIN,	LDBL_MAX,	true,	false,	true);
NUMERIC_LIMITS( long long,			LLONG_MIN,	LLONG_MAX,	true,	true,	true);
NUMERIC_LIMITS( unsigned long long,	0,			ULLONG_MAX,	false,	true,	true);
//--------------------------------------------------------------------------------------
