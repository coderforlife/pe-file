// Adapted from the uSTL library:
// Copyright (c) 2005 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once

#include <new> //#include "unew.h"
//#if HAVE_ALLOCA_H
//#include <alloca.h>
//#else
//#include <stdlib.h>
//#endif
//#include "upair.h"
#include "uiterator.h"
#include "ulimits.h"

namespace ustl {

template <typename T>
class auto_ptr {
public:
	typedef T	value_type;
	typedef T*	pointer;
	typedef T&	reference;
public:
	/// Takes ownership of \p p.
	inline explicit		auto_ptr(pointer p = NULL)	: _p(p)		{}
	/// Takes ownership of pointer in \p p. \p p relinquishes ownership.
	inline				auto_ptr(auto_ptr<T>& p)	: _p(p.release()) {}
	/// Deletes the owned pointer.
	inline				~auto_ptr()								{ delete this->_p; }
	/// Returns the pointer without relinquishing ownership.
	inline pointer		get() const								{ return this->_p; }
	/// Returns the pointer and gives up ownership.
	inline pointer		release()								{ pointer rv(this->_p); this->_p = NULL; return rv; }
	/// Deletes the pointer and sets it equal to \p p.
	inline void			reset(pointer p)						{ if (p != this->_p) { delete this->_p; this->_p = p; } }
	/// Takes ownership of \p p.
	inline auto_ptr<T>&	operator=(pointer p)					{ reset(p); return *this; }
	/// Takes ownership of pointer in \p p. \p p relinquishes ownership.
	inline auto_ptr<T>&	operator=(auto_ptr<T>& p)				{ reset(p.release()); return *this; }
	inline reference	operator*(void) const					{ return *this->_p; }
	inline pointer		operator->(void) const					{ return this->_p; }
	inline bool			operator==(const pointer p) const		{ return this->_p == p; }
	inline bool			operator==(const auto_ptr<T>& p) const	{ return this->_p == p._p; }
	inline bool			operator<(const auto_ptr<T>& p) const	{ return p._p < this->_p; }
private:
	pointer _p;
};

/// Calls the placement new on \p p.
template <typename T> inline void construct(T* p) { new (p) T; }

/// Calls the placement new on \p p.
#pragma warning(push)
#pragma warning(disable:4127) //conditional expression is constant
template <typename ForwardIterator>
inline void construct (ForwardIterator first, ForwardIterator last) {
	typedef typename iterator_traits<ForwardIterator>::value_type value_type;
	if (numeric_limits<value_type>::is_integral)
		memset(first, 0, distance(first,last)*sizeof(value_type));
	else
		for (; first != last; ++first)
			construct(&*first);
}
#pragma warning(pop)

/// Calls the placement new on \p p.
template <typename T> inline void construct(T* p, const T& value) { new (p) T(value); }

/// Calls the destructor of \p p without calling delete.
#pragma warning(push)
#pragma warning(disable:4100) //unreferenced formal parameter
template <typename T> inline void destroy(T* p) throw() { p->~T(); }
#pragma warning(pop)

// Helper templates to not instantiate anything for integral types.
template <typename T> void dtors(T first, T last) throw() { for (; first != last; ++first) destroy(&*first); }
template <typename T, bool bIntegral> struct Sdtorsr { inline void operator()(T first, T last) const throw() { dtors(first, last); } };
template <typename T> struct Sdtorsr<T,true> { inline void operator()(T, T) const throw() {} };

/// Calls the destructor on elements in range [first, last) without calling delete.
template <typename ForwardIterator>
inline void destroy (ForwardIterator first, ForwardIterator last) throw() {
	typedef typename iterator_traits<ForwardIterator>::value_type value_type;
	Sdtorsr<ForwardIterator,numeric_limits<value_type>::is_integral>()(first, last);
}

}
