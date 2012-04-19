// Adapted from the uSTL library:
// Copyright (c) 2005 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once

#include "utypes.h"

template <typename Iterator>
struct iterator_traits {
	typedef typename Iterator::value_type		value_type;
	typedef typename Iterator::difference_type	difference_type;
	typedef typename Iterator::pointer			pointer;
	typedef typename Iterator::reference		reference;
};

template <typename T>
struct iterator_traits<T*> {
	typedef T			value_type;
	typedef ptrdiff_t	difference_type;
	typedef const T*	const_pointer;
	typedef T*			pointer;
	typedef const T&	const_reference;
	typedef T&			reference;
};

template <typename T>
struct iterator_traits<const T*> {
	typedef T			value_type;
	typedef ptrdiff_t	difference_type;
	typedef const T*	const_pointer;
	typedef const T*	pointer;
	typedef const T&	const_reference;
	typedef const T&	reference;
};

template <class Iterator>
class reverse_iterator {
public:
	typedef typename iterator_traits<Iterator>::value_type		value_type;
	typedef typename iterator_traits<Iterator>::difference_type	difference_type;
	typedef typename iterator_traits<Iterator>::pointer			pointer;
	typedef typename iterator_traits<Iterator>::reference		reference;
public:
								reverse_iterator()				: i()			{}
	explicit					reverse_iterator(Iterator iter)	: i(iter)		{}
	inline bool					operator==(const reverse_iterator& iter) const	{ return i == iter.i; }
	inline bool					operator<(const reverse_iterator& iter) const	{ return iter.i < i; }
	inline Iterator				base() const									{ return i; }
	inline reference			operator*() const								{ Iterator prev(i); --prev; return *prev; }
	inline pointer				operator->() const								{ Iterator prev(i); --prev; return prev; }
	inline reverse_iterator&	operator++()									{ --i; return *this; }
	inline reverse_iterator&	operator--()									{ ++i; return *this; }
	inline reverse_iterator		operator++(int)									{ reverse_iterator prev(*this); --i; return prev; }
	inline reverse_iterator		operator--(int)									{ reverse_iterator prev(*this); ++i; return prev; }
	inline reverse_iterator&	operator+=(size_t n)							{ i -= n; return *this; }
	inline reverse_iterator&	operator-=(size_t n)							{ i += n; return *this; }
	inline reverse_iterator		operator+(size_t n) const						{ return reverse_iterator(i-n); }
	inline reverse_iterator		operator-(size_t n) const						{ return reverse_iterator(i+n); }
	inline reference			operator[](uoff_t n) const						{ return *(*this + n); }
	inline difference_type		operator-(const reverse_iterator& iter) const	{ return distance(i, iter.i); }
protected:
	Iterator i;
};
