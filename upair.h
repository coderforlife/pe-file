// Adapted from the uSTL library:
// Copyright (c) 2005 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once

#include "utypes.h"

template <typename T1, typename T2>
class pair {
public:
	typedef T1		first_type;
	typedef T2		second_type;
public:
	inline pair(void)						: first(T1()), second(T2()) {}
	inline pair(const T1& a, const T2& b)	: first(a), second(b) {}
	inline pair& operator=(const pair<T1, T2>& p2) { first = p2.first; second = p2.second; return *this; }
	template <typename T3, typename T4>
	inline pair& operator=(const pair<T3, T4>& p2) { first = p2.first; second = p2.second; return *this; }
public:
	first_type		first;
	second_type		second;
};

template <typename T1, typename T2>
inline pair<T1,T2> make_pair(const T1& a, const T2& b) { return pair<T1,T2>(a, b); }

template <typename T1, typename T2>
inline bool operator==(const pair<T1,T2>& a, const pair<T1,T2>& b) { return a.first == b.first && a.second == b.second; }

template <typename T1, typename T2>
inline bool operator<(const pair<T1,T2>& a, const pair<T1,T2>& b) { return a.first < b.first || a.first == b.first && a.second < b.second; }
