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

#define _DECLARE_ALL_PE_FILE_RESOURCES_
#include "PEFileResources.h"

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <wchar.h>

using namespace PE;
using namespace PE::Image;
using namespace PE::Internal;

// Raised when resources have a problem loading
class ResLoadFailure {};
static ResLoadFailure resLoadFailure;

bool ResCmp::operator()(const_resid a, const_resid b) const { return IsIntResID(a) ? (IsIntResID(b) ? (ResID2Int(a) < ResID2Int(b)) : false) : (IsIntResID(b) ? true : wcscmp(a, b) < 0); }
//int ResCmp::operator()(const_resid a, const_resid b) const { return (IsIntResID(a) ? (IsIntResID(b) ? ((uint16)a - (uint16)b) : 1) : (IsIntResID(b) ? -1 : wcscmp(a, b)); }

inline static resid dup(const_resid id) { return IsIntResID(id) ? MakeResID(ResID2Int(id)) : _wcsdup(id); }
inline static void free_id(resid id) { if (!IsIntResID(id)) free(id); }

#pragma region RSRC Utility Functions

static ResourceDirectoryEntry *GetEntries(const_bytes data, size_t size, size_t offset, uint32_t *nEntries) {
	if (offset + sizeof(ResourceDirectory) >= size) { throw resLoadFailure; }
	ResourceDirectory dir = *(ResourceDirectory*)(data+offset);
	*nEntries = dir.NumberOfIdEntries+dir.NumberOfNamedEntries;
	offset += sizeof(ResourceDirectory);
	if (offset + (*nEntries)*sizeof(ResourceDirectoryEntry) >= size) { throw resLoadFailure; }
	return (ResourceDirectoryEntry*)(data+offset);
}
static resid GetResourceName(const_bytes data, size_t size, size_t offset, const ResourceDirectoryEntry & entry) {
	if (entry.NameIsString) {
		offset += entry.NameOffset;

		if (offset + sizeof(uint16_t) > size) { return NULL; }
		uint16_t len = *(uint16_t*)(data+offset);
		offset += sizeof(uint16_t);

		if (offset + sizeof(wchar_t)*len > size) { return NULL; }
		resid str = wcsncpy((wchar_t*)malloc((len+1)*sizeof(wchar_t)), (wchar_t*)(data+offset), len);
		str[len] = 0;

		return str;
	} else {
		return MakeResID(entry.Id);
	}
}
static void WriteResDir(bytes data, size_t& pos, uint16_t nNamed, uint16_t nId) {
	ResourceDirectory dir;
	dir.Characteristics = 0;
	dir.TimeDateStamp = 0;
	dir.MajorVersion = 4;
	dir.MinorVersion = 0;
	dir.NumberOfNamedEntries = nNamed;
	dir.NumberOfIdEntries = nId;
	memcpy(data+pos, &dir, sizeof(ResourceDirectory));
	pos += sizeof(ResourceDirectory);
}
template <class Iterator>
static void WriteResDir(bytes data, size_t &pos, Iterator start, Iterator end) {
	uint16_t nNamed = 0, nId = 0;
	for (Iterator i = start; i != end; ++i) {
		if (IsIntResID(i->first))
			nId += 1;
		else
			nNamed += 1;
	}
	WriteResDir(data, pos, nNamed, nId);
}
static void WriteResDirEntry(bytes data, const_resid name, size_t headerSize, size_t &pos, size_t &posDir, size_t &posData) {
	ResourceDirectoryEntry entry;
	entry.DataIsDirectory = 1;
	entry.OffsetToDirectory = posDir;
	posDir += headerSize;

	entry.NameIsString = !IsIntResID(name);
	if (entry.NameIsString) {
		entry.NameOffset = posData;
		uint16_t len = (uint16_t)wcslen(name);
		memcpy(data+posData, &len, sizeof(uint16_t));
		memcpy(data+posData+sizeof(uint16_t), name, len*sizeof(wchar_t));
		posData += roundUpTo<4>(len*sizeof(wchar_t)+sizeof(uint16_t));
	} else {
		entry.Name = (uint32_t)ResID2Int(name);
	}

	memcpy(data+pos, &entry, sizeof(ResourceDirectoryEntry));
	pos += sizeof(ResourceDirectoryEntry);
}
#pragma endregion

#pragma region RES Utility Functions

typedef struct _RESHEADER {
	uint32_t DataSize;
	uint32_t HeaderSize;
	resid Type;
	resid Name;
	uint32_t DataVersion;
	uint16_t MemoryFlags;
	uint16_t LanguageId;
	uint32_t Version;
	uint32_t Characteristics;
} RESHeader;

static const size_t RESHeaderSize = sizeof(uint32_t)*7 + sizeof(uint16_t)*2;

static size_t ReadRESHeaderID(const_bytes data, resid *id) {
	wchar_t* chars = (wchar_t*)data;
	size_t len;
	if (chars[0] == 0xFFFF) {
		len = 2 * sizeof(wchar_t);
		*id = MakeResID(chars[1]);
	} else {
		len = (wcslen((resid)chars) + 1) * sizeof(wchar_t);
		*id = (resid)chars;
	}
	return len;
}
static RESHeader* ReadRESHeader(const_bytes data, size_t size, size_t& pos) {
	if (pos + sizeof(uint32_t)*2 >= size) { return NULL; }

	RESHeader *h = (RESHeader*)malloc(sizeof(RESHeader));
	memcpy(h, data+pos, sizeof(uint32_t)*2);

	if (pos + h->HeaderSize >= size) { free(h); return NULL; }

	pos += sizeof(uint32_t)*2;
	pos += ReadRESHeaderID(data + pos, &h->Type);
	pos += ReadRESHeaderID(data + pos, &h->Name);

	memcpy(&h->DataVersion, data + pos, sizeof(RESHeader) - offsetof(RESHeader, DataVersion));
	pos += sizeof(RESHeader) - offsetof(RESHeader, DataVersion);

	return h;
}

static const uint16_t DefResMemoryFlags = 0x0030; // or possibly 0x0000 ?
static const uint16_t ResMemoryFlags[] = {
	0x0000, // Nothing
	0x1010, // Cursor
	0x0030, // Bitmap
	0x1010, // Icon
	0x1030, // Menu
	0x1030, // Dialog
	0x1030, // String
	0x1030, // Fontdir
	0x1030, // Font
	0x0030, // Accelerator
	0x0030, // RCData
	0x0030, // Message Table
	0x1030, // Group Cursor
	DefResMemoryFlags,
	0x1030, // Group Icon
	DefResMemoryFlags,
	0x0030, // Version
	0x1030, // Dlg Include		(no examples)
	DefResMemoryFlags,
	0x0030, // Plug Play		(no examples) (obsolete)
	0x0030, // VXD				(obsolete)
	0x0030, // Ani-Cursor
	0x0030, // Ani-Icon			(no examples)
	0x0030, // HTML
	0x0030, // Manifest
};

static size_t GetRESHeaderIDExtraLen(const_resid id) { return IsIntResID(id) ? 0 : (wcslen(id) - 1) * sizeof(wchar_t); }
static size_t WriteRESHeaderID(bytes data, const_resid id, uint32_t *hdrSize) {
	if (IsIntResID(id)) {
		*((uint32_t*)data) = (((uint32_t)ResID2Int(id)) << 16) | 0xFFFF;
		return 0;
	} else {
		size_t len = (wcslen(id) + 1) * sizeof(wchar_t);
		memcpy(data, id, len);
		len -= sizeof(uint32_t);
		*hdrSize += (uint32_t)len;
		return len;
	}
}
static size_t WriteRESHeader(bytes data, const_resid type, const_resid name, uint16_t lang, size_t dataSize) {
	*(uint32_t*)data = (uint32_t)dataSize;
	uint32_t *hdrSize = ((uint32_t*)data)+1;
	*hdrSize = (uint32_t)RESHeaderSize;
	data += sizeof(uint32_t)*2;
	data += WriteRESHeaderID(data, type, hdrSize) + sizeof(uint32_t);
	data += WriteRESHeaderID(data, name, hdrSize) + sizeof(uint32_t);
	memset(data, 0, sizeof(uint32_t)*3 + sizeof(uint16_t)*2); // DataVersion, *, Version, Characteristics
	((uint16_t*)data)[2] = (IsIntResID(type) && (ResID2Int(type) < ARRAYSIZE(ResMemoryFlags))) ? ResMemoryFlags[ResID2Int(type)] : DefResMemoryFlags; // MemoryFlags
	((uint16_t*)data)[3] = lang;
	return *hdrSize;
}
#pragma endregion

#pragma region Rsrc
///////////////////////////////////////////////////////////////////////////////
///// Rsrc
///////////////////////////////////////////////////////////////////////////////
Rsrc* Rsrc::createFromRSRCSection(const_bytes data, size_t size, SectionHeader *section) { try { return (!data || !size || !section) ? NULL : new Rsrc(data, size, section); } catch (ResLoadFailure&) { return NULL; } }
Rsrc::Rsrc(const_bytes data, size_t size, SectionHeader *section) {
	uint32_t nEntries;
	ResourceDirectoryEntry *entries = GetEntries(data, size, section->PointerToRawData, &nEntries);
	for (uint16_t i = 0; i < nEntries; i++) {
		resid type = GetResourceName(data, size, section->PointerToRawData, entries[i]);
		//this->types.set(type), new ResourceType(type, data, size, section->PointerToRawData, section->VirtualAddress, entries[i]));
		this->types[type] = new ResourceType(type, data, size, section->PointerToRawData, section->VirtualAddress, entries[i]);
	}
	this->cleanup();
}
Rsrc* Rsrc::createFromRESFile(const_bytes data, size_t size) { try { return (!data || !size) ? NULL : new Rsrc(data, size); } catch (ResLoadFailure&) { return NULL; } }
Rsrc::Rsrc(const_bytes data, size_t size) {
	RESHeader *h;
	size_t pos = 0;
	while ((h = ReadRESHeader(data, size, pos)) != NULL) {
		if (h->Type || h->Name || h->LanguageId || h->DataSize) {
			this->add(h->Type, h->Name, h->LanguageId, data + pos, h->DataSize);
			pos += roundUpTo<4>(h->HeaderSize + h->DataSize) - h->HeaderSize;
		}
	}
	this->cleanup();
}
Rsrc* Rsrc::createEmpty() { return new Rsrc(); }
Rsrc::Rsrc() {  }
Rsrc::~Rsrc() {
	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i) {
		free_id(i->first);
		delete i->second;
	}
	this->types.clear();
}
const_resid Rsrc::getId() const { return NULL; }
bool Rsrc::cleanup() {
	for (TypeMap::iterator i = this->types.begin(); i != this->types.end(); ) {
		if (i->second->cleanup()) {
			free_id(i->first);
			delete i->second;
			this->types.erase(i++);
		} else { ++i; }
	}
	return this->isEmpty();
}
bool Rsrc::exists(const_resid type, const_resid name, uint16_t lang) const {
	TypeMap::const_iterator iter = this->types.find((resid)type);
	return iter == this->types.end() ? false : iter->second->exists(name, lang);
}
bool Rsrc::exists(const_resid type, const_resid name, uint16_t *lang) const {
	TypeMap::const_iterator iter = this->types.find((resid)type);
	return iter == this->types.end() ? false : iter->second->exists(name, lang);
}
void* Rsrc::get(const_resid type, const_resid name, uint16_t lang, size_t *size) const {
	TypeMap::const_iterator iter = this->types.find((resid)type);
	return iter == this->types.end() ? NULL : iter->second->get(name, lang, size);
}
void* Rsrc::get(const_resid type, const_resid name, uint16_t *lang, size_t *size) const {
	TypeMap::const_iterator iter = this->types.find((resid)type);
	return iter == this->types.end() ? NULL : iter->second->get(name, lang, size);
}
bool Rsrc::remove(const_resid type, const_resid name, uint16_t lang) {
	TypeMap::iterator iter = this->types.find((resid)type);
	if (iter == this->types.end())
		return false;
	bool b = iter->second->remove(name, lang);
	if (iter->second->isEmpty()) {
		free_id(iter->first);
		delete iter->second;
		this->types.erase(iter);
	}
	return b;
}
bool Rsrc::add(const_resid type, const_resid name, uint16_t lang, const void* data, size_t size, Overwrite overwrite) {
	TypeMap::iterator iter = this->types.find((resid)type);
	if (iter == types.end() && (overwrite == ALWAYS || overwrite == NEVER)) {
		//types.set(dup(type), new ResourceType(type, name, lang, data, size));
		types[dup(type)] = new ResourceType(type, name, lang, data, size);
		return true;
	}
	return iter->second->add(name, lang, data, size, overwrite);
}
bool Rsrc::isEmpty() const { return this->types.size() == 0; }
ResourceType* Rsrc::operator[](const_resid type) {
	TypeMap::iterator iter = this->types.find((resid)type);
	return iter == this->types.end() ? NULL : iter->second;
}
const ResourceType* Rsrc::operator[](const_resid type) const {
	TypeMap::const_iterator iter = this->types.find((resid)type);
	return iter == this->types.end() ? NULL : iter->second;
}
std::vector<const_resid> Rsrc::getTypes() const {
	std::vector<const_resid> v;
	v.reserve(this->types.size());
	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i)
		v.push_back(i->first);
	return v;
}
std::vector<const_resid> Rsrc::getNames(const_resid type) const {
	TypeMap::const_iterator iter = this->types.find((resid)type);
	return iter == types.end() ? std::vector<const_resid>() : iter->second->getNames();
}
std::vector<uint16_t> Rsrc::getLangs(const_resid type, const_resid name) const {
	TypeMap::const_iterator iter = this->types.find((resid)type);
	return iter == this->types.end() ? std::vector<uint16_t>() : iter->second->getLangs(name);
}
size_t Rsrc::getDataSize() const {
	size_t size = 0;
	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i) {
		if (!IsIntResID(i->first))
			size += roundUpTo<4>(sizeof(uint16_t)+wcslen(i->first)*sizeof(wchar_t));
		size += i->second->getDataSize();
	}
	return size;
}
size_t Rsrc::getHeaderSize() const {
	size_t size = this->getThisHeaderSize();
	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i)
		size += i->second->getHeaderSize();
	return size;
}
size_t Rsrc::getThisHeaderSize() const { return sizeof(ResourceDirectory)+this->types.size()*sizeof(ResourceDirectoryEntry); }
void* Rsrc::compile(size_t *size, uint32_t startVA) {
	this->cleanup();

	size_t dataSize = this->getDataSize();
	size_t headerSize = roundUpTo<4>(this->getHeaderSize()); // uint32 alignment
	
	*size = headerSize + dataSize;

	bytes data = (bytes)memset(malloc(*size), 0, *size);
	
	size_t pos = 0;
	size_t posDir = this->getThisHeaderSize();
	size_t posData = headerSize;

	WriteResDir(data, pos, this->types.begin(), this->types.end());

	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i)
		WriteResDirEntry(data, i->first, i->second->getThisHeaderSize(), pos, posDir, posData);

	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i)
		i->second->writeNameDirs(data, pos, posDir, posData);
	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i)
		i->second->writeLangDirs(data, pos, posDir);
	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i)
		i->second->writeData(data, pos, posData, startVA);

	return data;
}
size_t Rsrc::getRESSize() const {
	size_t size = RESHeaderSize;
	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i)
		size += i->second->getRESSize();
	return size;
}
void* Rsrc::compileRES(size_t *size) {
	this->cleanup();

	*size = this->getRESSize();
	bytes data = (bytes)memset(malloc(*size), 0, *size);
	size_t pos = WriteRESHeader(data, 0, 0, 0, 0);

	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i)
		i->second->writeRESData(data, pos);

	return data;
}
#pragma endregion

#pragma region ResourceType
///////////////////////////////////////////////////////////////////////////////
///// ResourceType
///////////////////////////////////////////////////////////////////////////////
ResourceType::ResourceType(const_resid type, const_bytes data, size_t size, uint32_t start, uint32_t startVA, ResourceDirectoryEntry entry) : type(dup(type)) {
	uint32_t nEntries;
	ResourceDirectoryEntry *entries = GetEntries(data, size, start+entry.OffsetToDirectory, &nEntries);
	for (uint16_t i = 0; i < nEntries; i++) {
		str name = GetResourceName(data, size, start, entries[i]);
		//this->names.set(name, new ResourceName(name, data, size, start, startVA, entries[i]));
		this->names[name] = new ResourceName(name, data, size, start, startVA, entries[i]);
	}
}
ResourceType::ResourceType(const_resid type, const_resid name, uint16_t lang, const void* data, size_t size) : type(dup(type)) {
	//this->names.set(dup(name), new ResourceName(name, lang, data, size));
	this->names[dup(name)] = new ResourceName(name, lang, data, size);
}
ResourceType::~ResourceType() {
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i) {
		free_id(i->first);
		delete i->second;
	}
	this->names.clear();
	free_id(type);
}
const_resid ResourceType::getId() const { return this->type; }
bool ResourceType::cleanup() {
	for (NameMap::iterator i = this->names.begin(); i != this->names.end(); ) {
		if (i->second->cleanup()) {
			free_id(i->first);
			delete i->second;
			this->names.erase(i++);
		} else { ++i; }
	}
	return this->isEmpty();
}
bool ResourceType::exists(const_resid name, uint16_t lang) const {
	NameMap::const_iterator iter = this->names.find((resid)name);
	return iter == this->names.end() ? false : iter->second->exists(lang);
}
bool ResourceType::exists(const_resid name, uint16_t *lang) const {
	NameMap::const_iterator iter = this->names.find((resid)name);
	return iter == this->names.end() ? false : iter->second->exists(lang);
}
void* ResourceType::get(const_resid name, uint16_t lang, size_t *size) const {
	NameMap::const_iterator iter = this->names.find((resid)name);
	return iter == this->names.end() ? NULL : iter->second->get(lang, size);
}
void* ResourceType::get(const_resid name, uint16_t *lang, size_t *size) const {
	NameMap::const_iterator iter = this->names.find((resid)name);
	return iter == this->names.end() ? NULL : iter->second->get(lang, size);
}
bool ResourceType::remove(const_resid name, uint16_t lang) {
	NameMap::iterator iter = this->names.find((resid)name);
	if (iter == this->names.end())
		return false;
	bool b = iter->second->remove(lang);
	if (iter->second->isEmpty()) {
		free_id(iter->first);
		delete iter->second;
		this->names.erase(iter);
	}
	return b;
}
bool ResourceType::add(const_resid name, uint16_t lang, const void* data, size_t size, Overwrite overwrite) {
	NameMap::iterator iter = this->names.find((resid)name);
	if (iter == this->names.end() && (overwrite == ALWAYS || overwrite == NEVER)) {
		//this->names.set(dup(name), new ResourceName(name, lang, data, size));
		this->names[dup(name)] = new ResourceName(name, lang, data, size);
		return true;
	}
	return iter->second->add(lang, data, size, overwrite);
}
bool ResourceType::isEmpty() const { return this->names.empty(); }
ResourceName* ResourceType::operator[](const_resid name) {
	NameMap::iterator iter = this->names.find((resid)name);
	return iter == this->names.end() ? NULL : iter->second;
}
const ResourceName* ResourceType::operator[](const_resid name) const {
	NameMap::const_iterator iter = this->names.find((resid)name);
	return iter == this->names.end() ? NULL : iter->second;
}
std::vector<const_resid> ResourceType::getNames() const {
	std::vector<const_resid> v;
	v.reserve(this->names.size());
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		v.push_back(i->first);
	return v;
}
std::vector<uint16_t> ResourceType::getLangs(const_resid name) const {
	NameMap::const_iterator iter = this->names.find((resid)name);
	return iter == this->names.end() ? std::vector<uint16_t>() : iter->second->getLangs();
}
size_t ResourceType::getDataSize() const {
	size_t size = 0;
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i) {
		if (!IsIntResID(i->first))
			size += roundUpTo<4>(sizeof(uint16_t)+wcslen(i->first)*sizeof(wchar_t));
		size += i->second->getDataSize();
	}
	return size;
}
size_t ResourceType::getHeaderSize() const {
	size_t size = this->getThisHeaderSize();
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		size += i->second->getHeaderSize();
	return size;
}
size_t ResourceType::getThisHeaderSize() const { return sizeof(ResourceDirectory)+this->names.size()*sizeof(ResourceDirectoryEntry); }
void ResourceType::writeNameDirs(bytes data, size_t& pos, size_t& posDir, size_t& posData) const {
	WriteResDir(data, pos, this->names.begin(), this->names.end());
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		WriteResDirEntry(data, i->first, i->second->getThisHeaderSize(), pos, posDir, posData);
}
void ResourceType::writeLangDirs(bytes data, size_t& pos, size_t& posDir) const {
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		i->second->writeLangDirs(data, pos, posDir);
}
void ResourceType::writeData(bytes data, size_t& posDataEntry, size_t& posData, uint32_t startVA) const {
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		i->second->writeData(data, posDataEntry, posData, startVA);
}
size_t ResourceType::getRESSize() const {
	size_t xlen = GetRESHeaderIDExtraLen(this->type), size = 0;
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		size += i->second->getRESSize(xlen);
	return size;
}
void ResourceType::writeRESData(bytes data, size_t& pos) const {
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		i->second->writeRESData(data, pos, this->type);
}
#pragma endregion

#pragma region ResourceName
///////////////////////////////////////////////////////////////////////////////
///// ResourceName
///////////////////////////////////////////////////////////////////////////////
ResourceName::ResourceName(const_resid name, const_bytes data, size_t size, uint32_t start, uint32_t startVA, ResourceDirectoryEntry entry) : name(dup(name)) {
	uint32_t nEntries;
	ResourceDirectoryEntry *entries = GetEntries(data, size, start+entry.OffsetToDirectory, &nEntries);
	for (uint16_t i = 0; i < nEntries; i++)
		//this->langs.set(entries[i].Id, new ResourceLang(entries[i].Id, data, size, start, startVA, entries[i]));
		this->langs[entries[i].Id] = new ResourceLang(entries[i].Id, data, size, start, startVA, entries[i]);
}
ResourceName::ResourceName(const_resid name, uint16_t lang, const void* data, size_t size) : name(dup(name)) {
	//this->langs.set(lang, new ResourceLang(lang, data, size));
	this->langs[lang] = new ResourceLang(lang, data, size);
}
ResourceName::~ResourceName() {
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i) {
		delete i->second;
	}
	this->langs.clear();
	free_id(this->name);
}
const_resid ResourceName::getId() const { return this->name; }
bool ResourceName::cleanup() {
	for (LangMap::iterator i = this->langs.begin(); i != this->langs.end(); ) {
		if (i->second->getDataSize() == 0) {
			delete i->second;
			this->langs.erase(i++);
		} else { ++i; }
	}
	return this->isEmpty();
}
bool ResourceName::exists(uint16_t lang) const { return this->langs.find(lang) != this->langs.end(); }
bool ResourceName::exists(uint16_t *lang) const {
	if (this->langs.size() > 0) {
		if (lang) *lang = this->langs.begin()->first;
		return true;
	}
	return false;
}
void* ResourceName::get(uint16_t lang, size_t *size) const {
	LangMap::const_iterator iter = this->langs.find(lang);
	return iter == this->langs.end() ? NULL : iter->second->get(size);
}
void* ResourceName::get(uint16_t *lang, size_t *size) const {
	if (this->langs.size() > 0) {
		LangMap::const_iterator iter = this->langs.begin();
		if (lang) *lang = iter->first;
		return iter->second->get(size);
	}
	return NULL;
}
bool ResourceName::remove(uint16_t lang) {
	LangMap::iterator iter = this->langs.find(lang);
	if (iter == this->langs.end())
		return false;
	delete iter->second;
	this->langs.erase(iter);
	return true;
}
bool ResourceName::add(uint16_t lang, const void* data, size_t size, Overwrite overwrite) {
	LangMap::iterator iter = this->langs.find(lang);
	if (iter == this->langs.end() && (overwrite == ALWAYS || overwrite == NEVER)) {
		//this->langs.set(lang, new ResourceLang(lang, data, size));
		this->langs[lang] = new ResourceLang(lang, data, size);
		return true;
	} else if (overwrite == ALWAYS || overwrite == ONLY) {
		return iter->second->set(data, size);
	}
	return false;
}
bool ResourceName::isEmpty() const { return this->langs.empty(); }
ResourceLang* ResourceName::operator[](uint16_t lang) {
	LangMap::iterator iter = this->langs.find(lang);
	return iter == this->langs.end() ? NULL : iter->second;
}
const ResourceLang* ResourceName::operator[](uint16_t lang) const {
	LangMap::const_iterator iter = this->langs.find(lang);
	return iter == this->langs.end() ? NULL : iter->second;
}
std::vector<uint16_t> ResourceName::getLangs() const {
	std::vector<uint16_t> v;
	v.reserve(this->langs.size());
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i)
		v.push_back(i->first);
	return v;
}
size_t ResourceName::getDataSize() const {
	size_t size = 0;
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i)
		size += roundUpTo<4>(i->second->getDataSize());
	return size;
}
size_t ResourceName::getHeaderSize() const {
	size_t size = this->getThisHeaderSize();
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i)
		size += i->second->getHeaderSize();
	return size;
}
size_t ResourceName::getThisHeaderSize() const { return sizeof(ResourceDirectory)+this->langs.size()*sizeof(ResourceDirectoryEntry); }
void ResourceName::writeLangDirs(bytes data, size_t& pos, size_t& posDir) const {
	WriteResDir(data, pos, 0, (uint16_t)this->langs.size());
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i) {
		ResourceDirectoryEntry entry;
		entry.DataIsDirectory = 0;
		entry.OffsetToDirectory = posDir;
		entry.NameIsString = 0;
		entry.Name = i->first;

		memcpy(data+pos, &entry, sizeof(ResourceDirectoryEntry));

		posDir += i->second->getThisHeaderSize();
		pos += sizeof(ResourceDirectoryEntry);
	}
}
void ResourceName::writeData(bytes data, size_t& posDataEntry, size_t& posData, uint32_t startVA) const {
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i)
		i->second->writeData(data, posDataEntry, posData, startVA);
}
size_t ResourceName::getRESSize(size_t addl_hdr_size) const {
	size_t xlen = addl_hdr_size + GetRESHeaderIDExtraLen(this->name), size = 0;
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i)
		size += i->second->getRESSize(xlen);
	return size;
}
void ResourceName::writeRESData(bytes data, size_t& pos, const_resid type) const {
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i)
		i->second->writeRESData(data, pos, type, this->name);
}

#pragma endregion

#pragma region ResourceLang
///////////////////////////////////////////////////////////////////////////////
///// ResourceLang
///////////////////////////////////////////////////////////////////////////////
ResourceLang::ResourceLang(uint16_t lang, const_bytes data, size_t size, uint32_t start, uint32_t startVA, ResourceDirectoryEntry entry) : lang(lang) {
	if (start+entry.OffsetToData+sizeof(ResourceDataEntry) > size) { throw resLoadFailure; }
	ResourceDataEntry de = *(ResourceDataEntry*)(data+start+entry.OffsetToData);
	if (start+de.OffsetToData-startVA+de.Size > size) { throw resLoadFailure; }
	this->data = malloc(this->length = de.Size);
	if (this->length == 0) { return; }
	memcpy(this->data, data+start+de.OffsetToData-startVA, this->length);
}
ResourceLang::ResourceLang(uint16_t lang, const void* data, size_t size) : lang(lang), length(size) {
	this->data = memcpy(malloc(size), data, length);
}
ResourceLang::~ResourceLang() { free(this->data); }
const_resid ResourceLang::getId() const { return MakeResID(this->lang); }
void* ResourceLang::get(size_t *size) const { return memcpy(malloc(this->length), this->data, *size = this->length); }
bool ResourceLang::set(const void* dat, size_t size) {
	if (this->length != size)
	{
		free(this->data);
		this->data = malloc(this->length = size);
	}
	memcpy(this->data, dat, size);
	return true;
}
size_t ResourceLang::getDataSize() const		{ return this->length; }
size_t ResourceLang::getHeaderSize() const		{ return sizeof(ResourceDataEntry); }
size_t ResourceLang::getThisHeaderSize() const	{ return sizeof(ResourceDataEntry); }
void ResourceLang::writeData(bytes dat, size_t& posDataEntry, size_t& posData, size_t startVA) const {
	ResourceDataEntry de = {(uint32_t)(posData+startVA), (uint32_t)this->length, 0, 0}; // needs to be an RVA
	memcpy(dat+posDataEntry, &de, sizeof(ResourceDataEntry));
	posDataEntry += sizeof(ResourceDataEntry);
	memcpy(dat+posData, this->data, this->length);
	posData += roundUpTo<4>(this->length);
}
size_t ResourceLang::getRESSize(size_t addl_hdr_size) const { return roundUpTo<4>(this->length + RESHeaderSize + addl_hdr_size); }
void ResourceLang::writeRESData(bytes dat, size_t& pos, const_resid type, const_resid name) const {
	pos += WriteRESHeader(dat+pos, type, name, this->lang, this->length);
	memcpy(dat+pos, this->data, this->length);
	pos = roundUpTo<4>(pos + this->length);
}
#pragma endregion
