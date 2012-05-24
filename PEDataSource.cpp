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


#ifdef __cplusplus_cli
#pragma unmanaged
#endif
#include "PEDataSource.h"

#include <stdlib.h>
#include <memory.h>

#ifdef USE_WINDOWS_API
#ifdef ARRAYSIZE
#undef ARRAYSIZE
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#endif

using namespace PE;

RawDataSource::RawDataSource(void* data, size_t size, bool readonly) : readonly(readonly), orig_data(data), sz(size) {
	if (readonly) {
#ifdef USE_WINDOWS_API
		DWORD old_protect = 0;
		if ((this->d = (bytes)VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) == NULL ||
			memcpy(this->d, this->orig_data, size) == NULL || !VirtualProtect(this->d, size, PAGE_READONLY, &old_protect)) { this->close(); }
#else
		if ((this->d = (bytes)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED ||
			memcpy(this->d, this->orig_data, size) == NULL || mprotect(this->d, size, PROT_READ) == -1) { if (this->d == MAP_FAILED) { this->d = NULL; } this->close(); }
#endif
	} else {
		this->d = this->orig_data;
	}
}
RawDataSource::~RawDataSource() { this->close(); }
bool RawDataSource::isreadonly() const { return this->readonly; };
bool RawDataSource::flush() { return true; }
void* RawDataSource::data() { return this->d; }
size_t RawDataSource::size() const { return this->sz; }
void RawDataSource::close() {
	if (this->d) {
		this->flush();
		if (this->readonly) {
#ifdef USE_WINDOWS_API
			VirtualFree(this->d, 0, MEM_RELEASE);
#else
			munmap(this->d, this->sz);
#endif
		}
		this->d = NULL;
	}
	if (this->orig_data) { free(this->orig_data); this->orig_data = NULL; }
	this->sz = 0;
}
bool RawDataSource::resize(size_t new_size) {
	if (this->readonly)			{ return false; }
	if (new_size == this->sz)	{ return true; }
	this->flush();
	this->d = (bytes)realloc(this->orig_data, new_size);
	if (!this->d) { this->close(); return false; }
	this->orig_data = this->d;
	if (new_size < this->sz)
		memset((bytes)this->d+this->sz, 0, new_size-this->sz); // set new memory to 0
	this->sz = new_size;
	return true;
}

#pragma region Memory Map Management Functions
///////////////////////////////////////////////////////////////////////////////
///// Memory Map Management Functions
///////////////////////////////////////////////////////////////////////////////
#include <map>
#include <vector>
using namespace std;
typedef map<const_str, vector<void*> > MMFs;
static MMFs mmfs;
static void _RemoveMMF(MMFs &mmfs, const_str file, void* x) {
	MMFs::iterator v = mmfs.find(file);
	if (v != mmfs.end()) {
		size_t size = v->second.size();
		for (size_t i = 0; i < size; ++i) {
			if (v->second[i] == x) {
				if (size == 1) {
					mmfs.erase(v);
				} else {
					if (i != size-1) // move the last element up
						v->second[i] = v->second[size-1];
					v->second.pop_back(); // remove the last element
				}
				break;
			}
		}
	}
}
static void* AddMMF  (const_str file, void* mm) { if (mm != NULL && mm != (void*)-1) { mmfs[file].push_back(mm); } return mm; }
static void RemoveMMF(const_str file, void* mm) { _RemoveMMF(mmfs, file, mm); }

#ifdef USE_WINDOWS_API
static MMFs mmfViews;
typedef BOOL (WINAPI *UNMAP_OR_CLOSE)(void*);
static void* AddMMFView(const_str file, void* view)   { if (view != NULL) mmfViews[file].push_back(view); return view; }
static void RemoveMMFView(const_str file, void* view) { _RemoveMMF(mmfViews, file, view); }
static void _UnmapAll(MMFs &mmfs, const_str file, UNMAP_OR_CLOSE func) {
	MMFs::iterator v = mmfs.find(file);
	if (v != mmfs.end()) {
		size_t size = v->second.size();
		for (size_t i = 0; i < size; ++i)
			func(v->second[i]);
		mmfs.erase(v);
	}
}
void MemoryMappedDataSource::UnmapAllViewsOfFile(const_str file) {
	_UnmapAll(mmfs, file, &CloseHandle);
	_UnmapAll(mmfViews, file, (UNMAP_OR_CLOSE)&UnmapViewOfFile);
}
#else
void MemoryMappedDataSource::UnmapAllViewsOfFile(const_str file) {
	MMFs::iterator v = mmfs.find(file);
	if (v != mmfs.end()) {
		size_t size = v->second.size();
		for (size_t i = 0; i < size; ++i)
			munmap(v->second[i]);
		mmfs.erase(v);
	}
}
#endif
#pragma endregion

bool MemoryMappedDataSource::map() {
#ifdef USE_WINDOWS_API
	return
		(this->hMap = AddMMF(this->original, CreateFileMapping(this->hFile, NULL, (readonly ? PAGE_READONLY : PAGE_READWRITE), 0, 0, NULL))) != NULL &&
		(this->d = AddMMFView(this->original, MapViewOfFile(this->hMap, (readonly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS), 0, 0, 0))) != NULL;
#else
	return (this->d = AddMMF(this->original, mmap(NULL, this->sz, (readonly ? PROT_READ : PROT_READ | PROT_WRITE), (readonly ? MAP_PRIVATE : MAP_SHARED), this->fd, 0))) != MAP_FAILED;
#endif
}
void MemoryMappedDataSource::unmap() {
#ifdef USE_WINDOWS_API
	if (this->hMap) {
		if (this->d)
		{
			this->flush();
			UnmapViewOfFile(this->d);
			RemoveMMFView(this->original, this->d);
			this->d = NULL;
		}
		RemoveMMF(this->original, this->hMap);
		CloseHandle(this->hMap);
		this->hMap = NULL;
	}
#else
	if (this->d == MAP_FAILED) { this->d = NULL; }
	else if (this->d)
	{
		this->flush();
		munmap(this->d, this->sz);
		RemoveMMF(this->original, this->d);
		this->d = NULL;
	}
#endif
}
#ifdef USE_WINDOWS_API
MemoryMappedDataSource::MemoryMappedDataSource(const_str file, bool readonly) : readonly(readonly), hFile(INVALID_HANDLE_VALUE), hMap(NULL), d(NULL), sz(0) {
#else
MemoryMappedDataSource::MemoryMappedDataSource(const_str file, bool readonly) : readonly(readonly), fd(-1), d(NULL), sz(0) {
#endif
	this->original[0] = 0;
#ifdef USE_WINDOWS_API
	if (!GetFullPathName(file, ARRAYSIZE(this->original), this->original, NULL) ||
		(this->hFile = CreateFile(this->original, (readonly ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE)), FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE ||
		(this->sz = GetFileSize(this->hFile, 0)) == INVALID_FILE_SIZE)
	{
		this->close();
	}
#else
	struct stat sb;
	if ((_wrealpath(file, this->original) == NULL) ||
		(this->fd = _wopen(this->original, (readonly ? O_RDONLY : O_RDWR))) == -1 ||
		fstat(this->fd, &sb) == -1)
	{
		this->close();
	}
	this->sz = sb.st_size;
#endif
	if (!map())
		this->close();
}
MemoryMappedDataSource::~MemoryMappedDataSource() { this->close(); }
	
bool MemoryMappedDataSource::isreadonly() const { return this->readonly; };
bool MemoryMappedDataSource::flush() {
#ifdef USE_WINDOWS_API
	return !this->readonly && FlushViewOfFile(this->d, 0) && FlushFileBuffers(this->hFile);
#else
	return !this->readonly && msync(this->d, this->sz, MS_SYNC | MS_INVALIDATE) != -1;
#endif
}

void* MemoryMappedDataSource::data() { return this->d; }
size_t MemoryMappedDataSource::size() const { return this->sz; }
void MemoryMappedDataSource::close() {
	this->unmap();
#ifdef USE_WINDOWS_API
	if (this->hFile != INVALID_HANDLE_VALUE) { CloseHandle(this->hFile); this->hFile = INVALID_HANDLE_VALUE; }
#else
	if (this->fd != -1) { close(this->fd); this->fd = -1; }
#endif
	this->sz = 0;
}
bool MemoryMappedDataSource::resize(size_t new_size) {
	if (this->readonly)			{ return false; }
	if (new_size == this->sz)	{ return true; }
	this->unmap();
#ifdef USE_WINDOWS_API
	if (SetFilePointer(this->hFile, (uint32_t)new_size, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER || !SetEndOfFile(this->hFile) || !this->map()) { this->close(); return false; }
	if (new_size > this->sz)
		memset((bytes)this->d+this->sz, 0, new_size-this->sz); // set new memory to 0 (I am unsure if Windows does this automatically like Linux does)
#else
	if (ftruncate(this->fd, new_size) == -1 || !this->map()) { this->close(); return false; }
#endif
	this->sz = new_size;
	return true;
}
