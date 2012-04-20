// Adapted from the uSTL library:
// Copyright (c) 2005 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once

#include <string.h>

namespace ustl {

//----------------------------------------------------------------------
// Standard functors
//----------------------------------------------------------------------

template <typename Result>
struct void_function {
	typedef Result	result_type;
};

template <typename Arg, typename Result>
struct unary_function {
	typedef Arg		argument_type;
    typedef Result	result_type;
};

template <typename Arg1, typename Arg2, typename Result>
struct binary_function {
    typedef Arg1	first_argument_type;
    typedef Arg2	second_argument_type;
    typedef Result	result_type;
};

#define STD_BINARY_FUNCTOR(name, rv, func)	template <class T>			struct name : public binary_function<T,T,rv>	{ inline rv operator()(const T& a, const T& b) const { return func; } };
#define STD_UNARY_FUNCTOR(name, rv, func)	template <class T>			struct name : public unary_function<T,rv>		{ inline rv operator()(const T& a) const { return func; } }; 
#define STD_CONVERSION_FUNCTOR(name, func)	template <class S, class D>	struct name : public unary_function<S,D>		{ inline D operator()(const S& a) const { return func; } };

STD_BINARY_FUNCTOR (plus,			T,		(a + b))
STD_BINARY_FUNCTOR (minus,			T,		(a - b))
STD_BINARY_FUNCTOR (divides,		T,		(a / b))
STD_BINARY_FUNCTOR (modulus,		T,		(a % b))
STD_BINARY_FUNCTOR (multiplies,		T,		(a * b))
STD_BINARY_FUNCTOR (logical_and,	T,		(a && b))
STD_BINARY_FUNCTOR (logical_or,		T,		(a || b))
STD_UNARY_FUNCTOR  (logical_not,	T,		(!a))
STD_BINARY_FUNCTOR (bitwise_or,		T,		(a | b))
STD_BINARY_FUNCTOR (bitwise_and,	T,		(a & b))
STD_BINARY_FUNCTOR (bitwise_xor,	T,		(a ^ b))
STD_UNARY_FUNCTOR  (bitwise_not,	T,		(~a))
STD_UNARY_FUNCTOR  (negate,			T,		(-a))
STD_BINARY_FUNCTOR (equal_to,		bool,	(a == b))
STD_BINARY_FUNCTOR (not_equal_to,	bool,	(!(a == b)))
STD_BINARY_FUNCTOR (greater,		bool,	(b < a))
STD_BINARY_FUNCTOR (less,			bool,	(a < b))
STD_BINARY_FUNCTOR (greater_equal,	bool,	(!(a < b)))
STD_BINARY_FUNCTOR (less_equal,		bool,	(!(b < a)))
STD_BINARY_FUNCTOR (compare,		int,	(a < b ? -1 : (b < a)))
STD_UNARY_FUNCTOR  (identity,		T,		(a))

template <class T1, class T2> struct project1st	: public binary_function<T1,T2,T1> { inline const T1& operator()(const T1& a, const T2&) const { return (a); } };
template <class T1, class T2> struct project2nd	: public binary_function<T1,T2,T2> { inline const T2& operator()(const T1&, const T2& a) const { return (a); } };

template<> class less<char*>	{ public: inline bool operator()(const char*& a, const char*& b) const { return strcmp(a, b) < 0; } };
//template<> class less<LPCSTR>	{ public: inline bool operator()(const LPCSTR& a, const LPCSTR& b) const { return strcmp(a, b) < 0; } };
template<> class less<wchar_t*>	{ public: inline bool operator()(const wchar_t*& a, const wchar_t*& b) const { return wcscmp(a, b) < 0; } };
//template<> class less<LPCWSTR>	{ public: inline bool operator()(const LPCWSTR& a, const LPCWSTR& b) const { return wcscmp(a, b) < 0; } };

}
