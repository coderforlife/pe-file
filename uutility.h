// Adapted from the uSTL library:
// Copyright (c) 2005 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once

#include "utypes.h"
//#include "traits.h"
#include "ulimits.h"
#include <assert.h>

namespace ustl {

template <typename T1, typename T2> inline T1 min(const T1& a, const T2& b) { return (a < b ? a : b); }
template <typename T1, typename T2> inline T1 max(const T1& a, const T2& b) { return (b < a ? a : b); }

template <typename T> inline T* NullPointer() { return (T*)NULL; }
template <typename T> inline T& NullValue() { return *NullPointer<T>(); }

template <typename T>	inline T			advance(T i,			ssize_t offset)	{ return i + offset; }
template <>				inline const void*	advance(const void* p,	ssize_t offset)	{ assert(p || !offset); return reinterpret_cast<const uint8_t*>(p) + offset; }
template <>				inline void*		advance(void* p,		ssize_t offset)	{ assert(p || !offset); return reinterpret_cast<uint8_t*>(p) + offset; }

template <typename T1, typename T2> inline ptrdiff_t distance(T1 a, T2 b) { assert(a<b); return b-a; }
#define UNVOID_DISTANCE(T1const,T2const) \
template <> inline ptrdiff_t distance(T1const void* a, T2const void* b) { assert(a<b); return (T2const uint8_t*)b-(T1const uint8_t*)a; }
UNVOID_DISTANCE(,)
UNVOID_DISTANCE(const,const)
UNVOID_DISTANCE(,const)
UNVOID_DISTANCE(const,)
#undef UNVOID_DISTANCE

template <typename T, bool IsSigned>	struct __is_negative			{ inline bool operator()(const T& v) const { return v<0; } };
template <typename T>					struct __is_negative<T,false>	{ inline bool operator()(const T&) const { return false; } };
template <typename T> inline bool is_negative(const T& v) { return __is_negative<T,numeric_limits<T>::is_signed>()(v); }
template <typename T> inline T absv(T v) { return is_negative(v) ? -v : v; }
template <typename T> inline T sign(T v) { return (0 < v) - is_negative(v); }

template <typename T1, typename T2> inline size_t abs_distance(T1 a, T2 b) { return absv(distance(a, b)); }

template <typename T> inline size_t size_of_elements(size_t n, const T*) { return n*sizeof(T); }

template <typename T> inline void Delete(T*& p) { delete p; p = NULL; }
template <typename T> inline void DeleteVector(T*& p) { delete [] p; p = NULL; }

template <typename T> inline bool operator!=(const T& x, const T& y) { return !(x == y); }
template <typename T> inline bool operator> (const T& x, const T& y) { return   y < x;  }
template <typename T> inline bool operator<=(const T& x, const T& y) { return !(y < x); }
template <typename T> inline bool operator>=(const T& x, const T& y) { return !(x < y); }
}
