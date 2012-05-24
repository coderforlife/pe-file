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

// Implements the classes and functions for dealing with resources in PE files or RES files

#ifndef PE_FILE_RESOURCES_H
#define PE_FILE_RESOURCES_H

#ifdef EXPOSE_DIRECT_RESOURCES
#define _DECLARE_ALL_PE_FILE_RESOURCES_
#endif

#ifndef _DECLARE_ALL_PE_FILE_RESOURCES_

class Rsrc;

#else

#include "PEDataTypes.h"

#include <map>
#include <vector>

namespace PE {

// A comparator for resource names
struct ResCmp { bool operator()(const_resid a, const_resid b) const; };

// A resource (directory) entry
class Resource {
public:
	virtual const_resid getId() const = 0;
private:
	virtual size_t getDataSize() const = 0;
	virtual size_t getHeaderSize() const = 0;
	virtual size_t getThisHeaderSize() const = 0;
};

// The final resource directory, contains the data for the resource
class ResourceLang : Resource {
	friend class ResourceName;

	uint16_t lang;
	void* data;
	size_t length;

	ResourceLang(uint16_t lang, const_bytes data, size_t size, uint32_t start, uint32_t startVA, Image::ResourceDirectoryEntry entry);
	ResourceLang(uint16_t lang, const void* data, size_t size);
public:
	~ResourceLang();

	const_resid getId() const;

	bool isLoaded() const;
	void* get(size_t* size) const; // must be freed
	bool set(const void* data, size_t size);

private:
	virtual size_t getDataSize() const;
	virtual size_t getHeaderSize() const;
	virtual size_t getThisHeaderSize() const;
	void writeData(bytes data, size_t& posDataEntry, size_t& posData, size_t startVA) const;

	virtual size_t getRESSize(size_t addl_hdr_size) const;
	void writeRESData(bytes data, size_t& pos, const_resid type, const_resid name) const;
};

// The named resource directory, the second level
class ResourceName : Resource {
	friend class ResourceType;

	resid name;

	typedef std::map<uint16_t, ResourceLang*> LangMap;
	LangMap langs;

	ResourceName(const_resid name, const_bytes data, size_t size, uint32_t start, uint32_t startVA, Image::ResourceDirectoryEntry entry);
	ResourceName(const_resid name, uint16_t lang, const void* data, size_t size);
public:
	~ResourceName();

	const_resid getId() const;

	bool exists(uint16_t lang) const;
	bool exists(uint16_t* lang) const;
	void* get(uint16_t lang, size_t* size) const;
	void* get(uint16_t* lang, size_t* size) const;
	bool remove(uint16_t lang);
	bool add(uint16_t lang, const void* data, size_t size, Overwrite overwrite = ALWAYS);

	bool isEmpty() const;
	
	ResourceLang* operator[](uint16_t lang);
	const ResourceLang* operator[](uint16_t lang) const;

	std::vector<uint16_t> getLangs() const;

private:
	bool cleanup();
	virtual size_t getDataSize() const;
	virtual size_t getHeaderSize() const;
	virtual size_t getThisHeaderSize() const;
	void writeLangDirs(bytes data, size_t& pos, size_t& posDir) const;
	void writeData(bytes data, size_t& posDataEntry, size_t& posData, uint32_t startVA) const;

	virtual size_t getRESSize(size_t addl_hdr_size) const;
	void writeRESData(bytes data, size_t& pos, const_resid type) const;
};

// The typed resource directory, the first level
class ResourceType : Resource {
	friend class Rsrc;

	resid type;
	typedef std::map<resid, ResourceName*, ResCmp> NameMap;
	NameMap names;

	ResourceType(const_resid type, const_bytes data, size_t size, uint32_t start, uint32_t startVA, Image::ResourceDirectoryEntry entry);
	ResourceType(const_resid type, const_resid name, uint16_t lang, const void* data, size_t size);
public:
	~ResourceType();

	const_resid getId() const;

	bool exists(const_resid name, uint16_t lang) const;
	bool exists(const_resid name, uint16_t* lang) const;
	void* get (const_resid name, uint16_t lang, size_t* size) const;
	void* get (const_resid name, uint16_t* lang, size_t* size) const;
	bool remove(const_resid name, uint16_t lang);
	bool add(const_resid name, uint16_t lang, const void* data, size_t size, Overwrite overwrite = ALWAYS);

	bool isEmpty() const;
	
	ResourceName* operator[](const_resid name);
	const ResourceName* operator[](const_resid name) const;

	std::vector<const_resid> getNames() const;
	std::vector<uint16_t> getLangs(const_resid name) const;

private:
	bool cleanup();
	virtual size_t getDataSize() const;
	virtual size_t getHeaderSize() const;
	virtual size_t getThisHeaderSize() const;
	void writeNameDirs(bytes data, size_t& pos, size_t& posDir, size_t& posData) const;
	void writeLangDirs(bytes data, size_t& pos, size_t& posDir) const;
	void writeData(bytes data, size_t& posDataEntry, size_t& posData, uint32_t startVA) const;

	virtual size_t getRESSize() const;
	void writeRESData(bytes data, size_t& pos) const;
};

class Rsrc : Resource {
	typedef std::map<resid, ResourceType*, ResCmp> TypeMap;
	TypeMap types;

	Rsrc(const_bytes data, size_t size, Image::SectionHeader *section); // creates from ".rsrc" section in PE file
	Rsrc(const_bytes data, size_t size); // creates from RES file
	Rsrc(); // creates empty
public:
	~Rsrc();
	
	static Rsrc* createFromRSRCSection(const_bytes data, size_t size, Image::SectionHeader *section);
	static Rsrc* createFromRESFile(const_bytes data, size_t size);
	static Rsrc* createEmpty();

	const_resid getId() const;

	bool exists(const_resid type, const_resid name, uint16_t lang) const;
	bool exists(const_resid type, const_resid name, uint16_t* lang) const;
	void* get (const_resid type, const_resid name, uint16_t lang, size_t* size) const;
	void* get (const_resid type, const_resid name, uint16_t* lang, size_t* size) const;
	bool remove(const_resid type, const_resid name, uint16_t lang);
	bool add(const_resid type, const_resid name, uint16_t lang, const void* data, size_t size, Overwrite overwrite = ALWAYS);
	
	bool isEmpty() const;
	
	ResourceType* operator[](const_resid type);
	const ResourceType* operator[](const_resid type) const;

	std::vector<const_resid> getTypes() const;
	std::vector<const_resid> getNames(const_resid type) const;
	std::vector<uint16_t> getLangs(const_resid type, const_resid name) const;

	bool cleanup();
	void* compile(size_t* size, uint32_t startVA); // calls cleanup
	void* compileRES(size_t* size); // calls cleanup

private:
	virtual size_t getDataSize() const;
	virtual size_t getHeaderSize() const;
	virtual size_t getThisHeaderSize() const;
	virtual size_t getRESSize() const;
};

}

#endif
#endif