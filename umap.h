// Adapted from the uSTL library:
// Copyright (c) 2005 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once

// TODO: Complexity requirements are not met
// This is only useful for small maps

#include "uvector.h"
#include "upair.h"
#include "ufunction.h"

namespace ustl {

template <typename K, typename T, typename Compare = less<K> >
class map : protected vector<pair<K,T> > {
public:
	typedef K			key_type;
	typedef T			mapped_type;
	typedef Compare		key_compare;
	//TODO: typdef ?????			value_compare;
	//TODO: typdef ?????			allocator_type;
	typedef vector<pair<K,T> >						base_class;
	typedef typename base_class::value_type			value_type;
	typedef typename base_class::size_type			size_type;
	typedef typename base_class::difference_type	difference_type;
	typedef typename base_class::reference			reference;
	typedef typename base_class::const_reference	const_reference;
	typedef typename base_class::pointer			pointer;
	typedef typename base_class::const_pointer		const_pointer;
	typedef typename base_class::iterator			iterator;
	typedef typename base_class::const_iterator		const_iterator;
	typedef typename base_class::reverse_iterator		reverse_iterator;
	typedef typename base_class::const_reverse_iterator	const_reverse_iterator;

protected:
	typedef K&						key_ref;
	typedef T&						mapped_ref;
	typedef map<K,T,Compare>&		my_ref;
	typedef const K&				const_key_ref;
	typedef const T&				const_mapped_ref;
	typedef const map<K,T,Compare>&	const_my_ref;

	typedef const Compare&			comp_ref;

	typedef pair<iterator,iterator>					range_t;
	typedef pair<const_iterator,const_iterator>		const_range_t;
    typedef pair<iterator,bool>						insertrv_t;

public:
	inline explicit map(comp_ref c = Compare())								: base_class(), _c(c)		{}						// TODO: supposed to have an Allocator param
	inline map(const_my_ref v)												: base_class(v), _c(v._c)	{}
	inline map(const_iterator a, const_iterator b, comp_ref c = Compare())	: base_class(), _c(c)		{ this->insert(a, b); }	// TODO: supposed to have an Allocator param and be InputIterator
	inline const_my_ref		operator=(const_my_ref v)			{ base_class::operator=(v); return *this; }
	//TODO: inline bool				operator==(const_my_ref v) const;
	//TODO: inline bool				operator< (const_my_ref v) const;
	inline const_mapped_ref	operator[](const_key_ref k) const	{ assert(this->find(k) != this->end()); return this->find(k)->second; }
	inline mapped_ref		operator[](const_key_ref k);
	//TODO: inline allocator_type		get_allocator() const;
	inline key_compare				key_comp() const			{ return this->_c; }
	//TODO: inline value_compare		value_comp() const;
	inline iterator					begin()				{ return base_class::begin(); }
	inline const_iterator			begin() const		{ return base_class::begin(); }
	inline iterator					end()				{ return base_class::end(); }
	inline const_iterator			end() const			{ return base_class::end(); }
	inline reverse_iterator			rbegin()			{ return base_class::rbegin(); }
	inline const_reverse_iterator	rbegin() const		{ return base_class::rbegin(); }
	inline reverse_iterator			rend()				{ return base_class::rend(); }
	inline const_reverse_iterator	rend() const		{ return base_class::rend(); }
	inline bool						empty() const		{ return base_class::empty(); }
	inline size_type				size() const		{ return base_class::size(); }
	inline size_type				max_size() const	{ return base_class::max_size(); }
	inline void						clear()				{ base_class::clear(); }
	inline insertrv_t		insert(const_reference v);
	inline iterator			insert(iterator p, const_reference v);
	void					insert(const_iterator a, const_iterator b);						// TODO: supposed to be InputIterator
	inline size_type		erase(const_key_ref k)				{ iterator i = this->find(k); if (i != this->end()) { this->erase(i); return 1; } return 0; }
	inline iterator			erase(iterator i)					{ return base_class::erase(i); }
	inline iterator			erase(iterator a, iterator b)		{ return base_class::erase(a, b); }
	inline void				swap(my_ref v);
	inline size_type		count(const_key_ref k) const		{ const_iterator i = this->lower_bound(k); return (i < this->end() && this->_c(k, i->first)) ? 0 : 1; }
    inline iterator			find(const_key_ref k)				{ return const_cast<iterator>(const_cast<const_my_ref>(*this).find(k)); }
	inline const_iterator	find(const_key_ref k) const			{ const_iterator i = this->lower_bound(k); return (i < this->end() && this->_c(k, i->first)) ? end() : i; }
	inline range_t			equal_range(const_key_ref k)		{ return const_cast<range_t>(const_cast<const_my_ref>(*this).equal_range(k)); }
	inline const_range_t	equal_range(const_key_ref k) const;
	inline iterator			lower_bound(const_key_ref k)		{ return const_cast<iterator>(const_cast<const_my_ref>(*this).lower_bound(k)); }
	const_iterator			lower_bound(const_key_ref k) const;
	inline iterator			upper_bound(const_key_ref k)		{ return const_cast<iterator>(const_cast<const_my_ref>(*this).upper_bound(k)); }
	const_iterator			upper_bound(const_key_ref k) const;

private:
	Compare _c;
};

template <typename K, typename V, typename Compare>
inline typename map<K,V,Compare>::mapped_ref map<K,V,Compare>::operator[](const_key_ref k) {
	iterator i = this->lower_bound(k);
	if (i == this->end() || this->_c(k, i->first))
		i = base_class::insert(i, make_pair(k, V()));
	return i->second;
}

template <typename K, typename V, typename Compare>
inline typename map<K,V,Compare>::insertrv_t map<K,V,Compare>::insert(const_reference v) {
	iterator i = this->lower_bound(v.first);
	bool bInserted = i == end() || this->_c(v.first, i->first);
	if (bInserted)
		i = base_class::insert(i, v);
	return make_pair(i, bInserted);
}

template <typename K, typename V, typename Compare>
inline typename map<K,V,Compare>::iterator map<K,V,Compare>::insert(iterator p, const_reference v) {
	UNREFERENCED_PARAMETER(p);
	iterator i = this->lower_bound(v.first);
	return (i == end() || this->_c(v.first, i->first)) ? base_class::insert(i, v) : i;
}

template <typename K, typename V, typename Compare>
void map<K,V,Compare>::insert(const_iterator a, const_iterator b) {
	assert(a <= b);
	base_class::reserve(this->size() + distance(a, b));
	for (iterator i = a; i != b; ++i)
		this->insert(*i);
}

template <typename K, typename V, typename Compare>
inline void map<K,V,Compare>::swap(my_ref v) {
	if (this != &v) {
		base_class::swap(v);

		Compare c(this->_c);
		this->_c = v._c;
		v._c = c;
	}
}

template <typename K, typename V, typename Compare>
inline typename map<K,V,Compare>::const_range_t map<K,V,Compare>::equal_range(const_key_ref k) const {
	const_range_t r;
	r.second = r.first = this->lower_bound(k);
	if (r.second != this->end() && !this->_c(k, r.second->first)) // no while since for maps this will be at most one element because of unique keys
		++r.second;
	return r;
}

template <typename K, typename V, typename Compare>
inline typename map<K,V,Compare>::const_iterator map<K,V,Compare>::lower_bound(const_key_ref k) const {
	const_iterator first(this->begin()), last(this->end());
	while (first != last) {
		const_iterator mid = advance(first, distance(first, last) / 2);
		if (this->_c(mid->first, k))
			first = mid+1;
		else
			last = mid;
	}
	return first;
}

template <typename K, typename V, typename Compare>
inline typename map<K,V,Compare>::const_iterator map<K,V,Compare>::upper_bound(const_key_ref k) const {
	const_iterator first(this->begin()), last(this->end());
	while (first != last) {
		const_iterator mid = advance(first, distance(first, last) / 2);
		if (this->_c(k, mid->first))
			last = mid;
		else
			first = mid+1;
	}
	return last;
}

}
