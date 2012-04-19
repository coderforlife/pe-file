// Adapted from the uSTL library:
// Copyright (c) 2005 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once

#include "uutility.h"

#include "umemory.h"
//#include "upredalgo.h"


//TODO: the iterators do not become 'invalidated' ever

template <typename T>
class vector {
public:
	typedef T					value_type;
	//TODO: typdef ?????			allocator_type;
	typedef size_t				size_type;
	typedef ptrdiff_t			difference_type;
	typedef value_type&			reference;
	typedef const value_type&	const_reference;
	typedef value_type*			pointer;
	typedef const value_type*	const_pointer;
	typedef pointer				iterator;
	typedef const_pointer		const_iterator;
	typedef ::reverse_iterator<iterator>		reverse_iterator;
	typedef ::reverse_iterator<const_iterator>	const_reverse_iterator;
protected:
	typedef vector<T>&			my_ref;
	typedef const vector<T>&	const_my_ref;

public:
	inline explicit			vector()									: _x(NULL), _size(0), _capacity(0)	{ }						// TODO: supposed to have an Allocator param
	inline explicit			vector(size_type n)							: _x(NULL), _size(0), _capacity(0)	{ this->grow(n, T()); }
							vector(size_type n, const T& v)				: _x(NULL), _size(0), _capacity(0)	{ this->grow(n, v); }	// TODO: supposed to have an Allocator param
							vector(const_my_ref v)						: _x(NULL), _size(0), _capacity(0)	{ this->copy(v); }
							vector(const_iterator a, const_iterator b)	: _x(NULL), _size(0), _capacity(0)	{ this->assign(a, b); } // TODO: supposed to have an Allocator param and be InputIterator
	inline					~vector() throw();
	inline vector<T>&		operator=(const_my_ref v)		{ if (this != &v) { this->copy(v); } return *this; }
	inline void				assign(size_type n, const T& v)	{ this->clear(); this->grow(n, v); }
	inline void				assign(const_iterator a, const_iterator b);									// TODO: supposed to be InputIterator
	inline bool				operator==(const_my_ref v) const;
	inline bool				operator< (const_my_ref v) const;
	//TODO: inline allocator_type	get_allocator() const;
	inline reference		at(size_type i)					{ assert(i<size()); return this->_x[i]; }	// TODO: supposed to always bounds check
	inline const_reference	at(size_type i) const			{ assert(i<size()); return this->_x[i]; }	// TODO: supposed to always bounds check
	inline reference		operator[](size_type i)			{ assert(i<size()); return this->_x[i]; }
	inline const_reference	operator[](size_type i) const	{ assert(i<size()); return this->_x[i]; }
	inline reference		front()							{ assert(!empty()); return this->_x[0]; }
	inline const_reference	front() const					{ assert(!empty()); return this->_x[0]; }
	inline reference		back()							{ assert(!empty()); return this->_x[this->_size-1]; }
	inline const_reference	back() const					{ assert(!empty()); return this->_x[this->_size-1]; }
	inline pointer			data()							{ return this->_x; }
	inline const_pointer	data() const					{ return this->_x; }
	inline iterator			begin()							{ return iterator(this->_x); }
	inline const_iterator	begin() const					{ return const_iterator(this->_x); }
	inline iterator			end()							{ return iterator(this->_x + this->_size); }
	inline const_iterator	end() const						{ return const_iterator(this->_x + this->_size); }
	inline reverse_iterator			rbegin()				{ return reverse_iterator(end());		}
	inline const_reverse_iterator	rbegin() const			{ return const_reverse_iterator(end());	}
	inline reverse_iterator			rend()					{ return reverse_iterator(begin());		}
	inline const_reverse_iterator	rend() const			{ return const_reverse_iterator(begin());	}
	inline bool				empty() const					{ return this->_size == 0; }
	inline size_type		size() const					{ return this->_size; }
	inline size_type		max_size() const				{ return SIZE_MAX / sizeof(T); }
	inline void				reserve(size_type n);
	inline size_type		capacity() const				{ return this->_capacity; }
	inline void				clear()							{ this->shrink(0); }
	inline iterator			insert(iterator i, const T& v);
	inline void				insert(iterator i, size_type n, const T& v);
	inline void				insert(iterator i, const_iterator a, const_iterator b);						// TODO: supposed to be InputIterator
	inline iterator			erase(const_iterator i);
	inline iterator			erase(const_iterator a, const_iterator b);
	inline void				push_back(const T& v)			{ this->grow(this->_size + 1, v); }
	inline void				pop_back()						{ assert(0<size()); this->shrink(this->_size - 1); }
	inline void				resize(size_type n, const T& v = T())	{ if (this->_size != n) { if (this->_size > n) shrink(n); else grow(n, v); } }
	inline void				swap(vector<T>& v);
protected:
	//TODO: supposed to construct all new items
	inline void				copy(const vector<T>& v)		{ this->clear(); this->reserve(this->_size = v._size); memcpy(this->_x, v._x, this->_size*sizeof(T)); }
	//TODO: destroy all elements from n to this->_size
	inline void				shrink(size_type n)				{ this->_size = n; }
	inline void				grow(size_type n, const T& v)	{ this->reserve(n); for (size_type i = this->_size; i < n; ++i) this->_x[i] = v; this->_size = n; }
private:
	value_type *_x;
	size_type _size, _capacity;
};

template<typename T>
inline vector<T>::~vector() throw() {
	this->clear();
	destroy(iterator(this->_x), iterator(this->_x+this->_capacity));
	free(this->_x);
	this->_x = NULL;
	this->_capacity = 0;
}

template<typename T>
inline bool vector<T>::operator==(const_my_ref v) const {
	if (this->_size != v._size) return false;
	if (this->_x == v._x) return true;
	for (size_type i = 0; i < this->_size; ++i)
		if (this->_x[i] != v._x[i])
			return false;
	return true;
}

template<typename T>
inline bool vector<T>::operator<(const_my_ref v) const {
	bool smaller = this->_size < v._size;
	if (this->_x == v._x) return smaller;
	size_type sz = smaller ? this->_size : v._size;
	for (size_type i = 0; i < sz; ++i) {
		if (this->_x[i] < v._x[i])
			return true;
		else if (v._x[i] < this->_x[i])
			return false;
	}
	return smaller;
}

template<typename T>
inline void vector<T>::assign(const_iterator a, const_iterator b) {
    assert(a <= b);
	this->clear();
	this->reserve(distance(a, b));
	for (iterator i = this->begin(); a != b; ++a, ++i)
		*i = *a;
}

template <typename T>
inline void vector<T>::reserve(size_type n) {
	if (n > this->_capacity) {
		size_t old_capacity = this->_capacity;
		if (this->_capacity == 0) { this->_capacity = 8; }
		while (n > this->_capacity) { this->_capacity <<= 1; }
		size_type nb = this->_capacity*sizeof(T);
		//TODO: this is supposed to re-construct all items that already existed
		this->_x = (T*)(this->_x ? realloc(this->_x, nb) : malloc(nb));
		construct(iterator(this->_x+old_capacity), iterator(this->_x+this->_capacity));
	}
}

template <typename T>
inline typename vector<T>::iterator vector<T>::insert(iterator i, const T& v) {
	assert(this->begin() <= i && i <= this->end());
	size_type x = i-this->_x, s = this->_size-x;
	this->reserve(this->_size+1); // may cause bad pointers
	i = this->_x + x;
	if (s != 0) {
		//TODO: this is supposed to re-construct all items that are moved
		memmove(i+1, i, s*sizeof(T));
	}
	*i = v;
	++this->_size;
	return iterator(i);
}

template <typename T>
inline void vector<T>::insert(iterator i, size_type n, const T& v) {
	assert(this->begin() <= i && i <= this->end());
	if (n > 0) {
		size_type x = i-this->_x, s = this->_size-x;
		this->reserve(this->_size+n); // may cause bad pointers
		i = this->_x + x;
		if (s != 0) {
			//TODO: this is supposed to re-construct all items that are moved
			memmove(i+n, i, s*sizeof(T));
		}
		for (iterator last = i + n; i != last; ++i)
			*i = v;
		this->_size += n;
	}
}

template <typename T>
inline void vector<T>::insert(iterator i, const_iterator a, const_iterator b) {
	assert(this->begin() <= i && i <= this->end() && a <= b);
	size_type n = distance(a, b);
	if (n > 0) {
		size_type x = i-this->_x, s = this->_size-x;
		this->reserve(this->_size+n); // may cause bad pointers
		i = this->_x + x;
		if (s != 0) {
			//TODO: this is supposed to re-construct all items that are moved
			memmove(i+n, i, s*sizeof(T));
		}
		for (; a != b; ++i, ++a)
			*i = *a;
		this->_size += n;
	}
}

template <typename T>
inline typename vector<T>::iterator vector<T>::erase(const_iterator i) {
	assert(this->begin() <= i && i < this->end());
	//TODO: destroy element at i
	size_type s = this->_x+this->_size-i-1;
	if (s != 0) {
		//TODO: this is supposed to re-construct all items that are moved
		memmove((void*)i, i+1, s*sizeof(T));
	}
	--this->_size;
	return iterator(i);
}

template <typename T>
inline typename vector<T>::iterator vector<T>::erase(const_iterator a, const_iterator b) {
	assert(this->begin() <= a && a < this->end() && a <= b && b <= this->end());
	size_type n = b-a;
	if (n > 0) {
		//TODO: destroy all elements from [a, b)
		size_type s = this->_x+this->_size-b;
		if (s != 0) {
			//TODO: this is supposed to re-construct all items that are moved
			memmove((void*)a, b, s*sizeof(T));
		}
		this->_size -= n;
	}
	return iterator(a);
}

template <typename T>
inline void vector<T>::swap(vector<T>& v) {
	if (this != &v) {
		this->reserve(v._size);
		v.reserve(this->_size);

		//TODO: this is supposed to re-construct all items that are copied
		size_type nb = this->_size*sizeof(T);
		value_type *temp = (value_type*)malloc(nb);
		memcpy(temp, this->_x, nb);
		memcpy(this->_x, v._x, v._size*sizeof(T));
		memcpy(v._x, temp, nb);

		size_type s = this->_size;
		this->_size = v._size;
		v._size = s;
	}
}

// TODO: optimized version for vector<bool>
