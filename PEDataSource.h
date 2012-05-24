// pe-file: library for reading and manipulating pe-files
// Copyright (C) 2012  Jeffrey Bush  jeff@coderforlife.com
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Implements the classes and functions for reading and writing data

#ifndef PE_DATA_SOURCE_H
#define PE_DATA_SOURCE_H

#include "PEDataTypes.h"

namespace PE {
	class DataSourceImp {
	public:
		virtual bool isreadonly() const = 0;
		virtual void close() = 0;
		virtual bool flush() = 0;
		virtual void* data() = 0;
		virtual size_t size() const = 0;
		virtual bool resize(size_t new_size) = 0;
	};
	
	class RawDataSource : public DataSourceImp {
		bool readonly;
		void *d, *orig_data;
		size_t sz;
	public:
		RawDataSource(void* data, size_t size, bool readonly = false);
		~RawDataSource();
		virtual bool isreadonly() const;
		virtual void* data();
		virtual size_t size() const;
		virtual void close();
		virtual bool resize(size_t new_size);
		virtual bool flush();
	};

	class MemoryMappedDataSource : public DataSourceImp {
		bool readonly;
		wchar_t original[LARGE_PATH];
#ifdef USE_WINDOWS_API
		void *hFile, *hMap;
#else
		int fd;
#endif
		void* d;
		size_t sz;

		bool map();
		void unmap();
	public:
		MemoryMappedDataSource(const_str file, bool readonly = false);
		~MemoryMappedDataSource();
		virtual bool isreadonly() const;
		virtual void* data();
		virtual size_t size() const;
		virtual void close();
		virtual bool resize(size_t new_size);
		virtual bool flush();
		
		static void UnmapAllViewsOfFile(const_str file);
	};

	class DataSource {
		DataSourceImp* ds;
		bool readonly;
		void* data;
		size_t sz;

		inline void update() {
			if (this->ds) { this->data = this->ds->data(); this->sz = this->ds->size(); }
			else { this->data = NULL; this->sz = 0; }
		}

	public:
		inline DataSource(DataSourceImp* ds) : ds(ds), readonly(ds && ds->isreadonly()), data(ds ? ds->data() : NULL), sz(ds ? ds->size() : 0) { }
		//inline static DataSource create(pntr data, size_t size, bool readonly = false) { return DataSource(new RawDataSource(data, size, readonly)); }
		//inline static DataSource create(const_str file, bool readonly = false) { return DataSource(new MemoryMappedDataSource(file, readonly)); }

		inline bool isopen() const { return this->data != NULL; }
		inline bool isreadonly() const { return this->readonly; }

		inline size_t size() const { return this->sz; }

		inline bool flush() { return this->ds->flush(); }
		inline void close() { if (this->ds) { this->ds->close(); delete this->ds; this->ds = NULL; this->update(); } }
		inline bool resize(size_t new_size) { bool retval = this->ds->resize(new_size); if (retval) { this->update(); } return retval; }

		//inline operator bool() const { return this->data != NULL; } // returns if the data is open

		//inline operator pntr() { return this->data; }
		//inline operator bytes() { return (bytes)this->data; }
		//inline operator const_pntr() const { return this->data; }
		//inline operator const_bytes() const { return (const_bytes)this->data; }

		inline       dyn_ptr<byte> operator +(const size_t& off)       { return dyn_ptr<byte>(&this->data, off); }
		inline const dyn_ptr<byte> operator +(const size_t& off) const { return dyn_ptr<byte>(&this->data, off); }

		inline ptrdiff_t operator -(const_bytes b) const { return (const_bytes)this->data - b; }

		inline       byte& operator[](const size_t& off)       { return       ((bytes)this->data)[off]; }
		inline const byte& operator[](const size_t& off) const { return ((const_bytes)this->data)[off]; }
	};

	inline ptrdiff_t operator -(const_bytes a, const DataSource& b) { return a - (b + 0); }
}


#endif