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
#include "PEFile.h"

#ifdef USE_WINDOWS_API
#ifdef ARRAYSIZE
#undef ARRAYSIZE
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef ABSOLUTE
#define set_err(e) SetLastError(e)
#define get_err()  GetLastError()
#else
#include <errno.h>
#define ERROR_INVALID_DATA EFFORM
#define set_err(e) errno = e
#define get_err()  errno
#endif

using namespace PE;
using namespace PE::Image;
using namespace PE::Internal;
using namespace PE::Version;


#pragma region GetResourceDirect and get just the offset of the resource
///////////////////////////////////////////////////////////////////////////////
///// GetResourceDirect - For performance reasons (and get just the offset of the resource)
///////////////////////////////////////////////////////////////////////////////
#define FIRST_ENTRY ((resid)-1)
inline static ResourceDirectory *GetDir(const ResourceDirectoryEntry *entry, const_bytes rsrc) {
	return (ResourceDirectory*)(rsrc + entry->OffsetToDirectory);
}
inline static ResourceDirectoryEntry *FirstEntry(const ResourceDirectory *dir) {
	return ((dir->NumberOfIdEntries + dir->NumberOfNamedEntries) < 1) ? NULL : (ResourceDirectoryEntry*)(dir+1);
}
inline static ResourceDirectory *FirstEntry(const ResourceDirectory *dir, const_bytes rsrc, const_resid *out = NULL) {
	const ResourceDirectoryEntry *entry = FirstEntry(dir);
	if (entry && out)
		*out = entry->NameIsString ? ((const_resid)(rsrc+entry->NameOffset+sizeof(uint16_t))) : MakeResID(entry->Id);
	return entry ? GetDir(entry, rsrc) : NULL;
}
inline static ResourceDirectory *FindEntryString(const ResourceDirectory *dir, const_resid id, const_bytes rsrc) {
	const ResourceDirectoryEntry *entries = (ResourceDirectoryEntry*)(dir+1);
	for (uint16_t i = 0; i < dir->NumberOfNamedEntries; i++)
		if (wcscmp(id, (resid)(rsrc+entries[i].NameOffset+sizeof(uint16_t))) == 0)
			return GetDir(entries+i, rsrc);
	return NULL;
}
inline static ResourceDirectory *FindEntryInt(const ResourceDirectory *dir, uint16_t id, const_bytes rsrc) {
	const ResourceDirectoryEntry *entries = (ResourceDirectoryEntry*)(dir+1)+dir->NumberOfNamedEntries;
	for (uint16_t i = 0; i < dir->NumberOfIdEntries; i++)
		if (entries[i].Id == id)
			return GetDir(entries+i, rsrc);
	return NULL;
}
inline static ResourceDirectory *FindEntry(const ResourceDirectory *dir, const_resid id, const_bytes rsrc, const_resid *out = NULL) {
	return (id == FIRST_ENTRY) ? FirstEntry(dir, rsrc, out) : (IsIntResID(id) ? FindEntryInt(dir, ResID2Int(id), rsrc) : FindEntryString(dir, id, rsrc));
}
static void* GetResourceDirectInRsrc(bytes data, const SectionHeader *rsrcSect, const_resid type, const_resid name, const_resid *out_name = NULL, uint16_t *lang = NULL, size_t *size = NULL) {
	if (!rsrcSect || rsrcSect->PointerToRawData == 0 || rsrcSect->SizeOfRawData == 0)	{ return NULL; }

	// Get the bytes for the RSRC section
	bytes rsrc = data + rsrcSect->PointerToRawData;
	
	// Get the type and name directories
	const ResourceDirectory *dir = (ResourceDirectory*)rsrc;
	if ((dir = FindEntry(dir, type, rsrc, NULL)) == NULL)				{ return NULL; }
	if ((dir = FindEntry(dir, name, rsrc, out_name)) == NULL)			{ return NULL; }

	// Assume the first language
	const ResourceDirectoryEntry *entry;
	if ((entry = FirstEntry(dir)) == NULL || entry->DataIsDirectory)	{ return NULL; }
	const ResourceDataEntry *dataEntry = (ResourceDataEntry*)(rsrc+entry->OffsetToData);

	// Get the language and size of the resource
	if (lang) *lang = entry->Id;
	if (size) *size = dataEntry->Size;

	// Return the size and data
	return rsrc+dataEntry->OffsetToData-rsrcSect->VirtualAddress;
}
void* File::GetResourceDirect(void* _data, const_resid type, const_resid name) {
	bytes data = (bytes)_data;

	// Load and check headers
	const DOSHeader *dosh = (DOSHeader*)data;
	if (dosh->e_magic != DOSHeader::SIGNATURE)				{ return NULL; }
	int32_t peOffset = dosh->e_lfanew;
	const NTHeaders32 *nth = (NTHeaders*)(data+peOffset);
	if (nth->Signature != NTHeaders::SIGNATURE)				{ return NULL; }
	const FileHeader* header = &nth->FileHeader; // identical for 32 and 64 bits
	const OptionalHeader* opt = &nth->OptionalHeader;
	bool is64bit = !(header->Characteristics & FileHeader::MACHINE_32BIT);
	if ((is64bit && opt->Magic != OptionalHeader64::SIGNATURE) || (!is64bit && opt->Magic != OptionalHeader32::SIGNATURE)) { return NULL; }

	// Get the RSRC section
	const SectionHeader *sections = (SectionHeader*)(data+peOffset+sizeof(uint32_t)+sizeof(FileHeader)+header->SizeOfOptionalHeader);
	const SectionHeader *rsrcSect = NULL;
	for (uint16_t i = 0; i < header->NumberOfSections; ++i)
		if (strncmp((const char*)sections[i].Name, ".rsrc", ARRAYSIZE(sections[i].Name)) == 0) { rsrcSect = sections+i; break; }

	// Get the resource within the RSRC section
	return GetResourceDirectInRsrc(data, rsrcSect, type, name);
}
#pragma endregion

#pragma region Loading Functions
///////////////////////////////////////////////////////////////////////////////
///// Loading Functions
///////////////////////////////////////////////////////////////////////////////
File::File(void* data, size_t size, bool readonly) : data(new RawDataSource(data, size, readonly)), res(NULL), modified(false) {
	if (!this->data.isopen() || !this->load()) { this->unload(); }
}
File::File(const_str file, bool readonly) : data(new MemoryMappedDataSource(file, readonly)), res(NULL), modified(false) {
	if (!this->data.isopen() || !this->load()) { this->unload(); }
}
File::File(DataSource data) : data(data), res(NULL), modified(false) {
	if (!this->data.isopen() || !this->load()) { this->unload(); }
}
bool File::load() {
	this->dosh = (dyn_ptr<DOSHeader>)(this->data + 0);
	if (this->dosh->e_magic != DOSHeader::SIGNATURE)	{ set_err(ERROR_INVALID_DATA); return false; }
	this->peOffset = this->dosh->e_lfanew;

	this->nth32 = (dyn_ptr<NTHeaders32>)(this->data + this->peOffset);
	this->nth64 = (dyn_ptr<NTHeaders64>)(this->data + this->peOffset);
	if (this->nth32->Signature != NTHeaders::SIGNATURE)	{ set_err(ERROR_INVALID_DATA); return false; }
	this->header = dyn_ptr<FileHeader>(this->dosh, &this->nth32->FileHeader); // identical for 32 and 64 bits
	this->opt = dyn_ptr<OptionalHeader>(this->dosh, &this->nth32->OptionalHeader); // beginning is identical for 32 and 64 bits
	bool is64bit = this->is64bit(), is32bit = this->is32bit();

	if ((is64bit && this->opt->Magic != OptionalHeader64::SIGNATURE) ||
		(is32bit && this->opt->Magic != OptionalHeader32::SIGNATURE) ||
		(is64bit == is32bit))							{ set_err(ERROR_INVALID_DATA); return false; }

	this->dataDir = dyn_ptr<DataDirectory>(this->dosh, is64bit ? this->nth64->OptionalHeader.DataDirectory : this->nth32->OptionalHeader.DataDirectory);
	this->sections = (dyn_ptr<SectionHeader>)(this->data+this->peOffset+sizeof(uint32_t)+sizeof(FileHeader)+this->header->SizeOfOptionalHeader);

	// Load resources
	dyn_ptr<SectionHeader> rsrc = this->getSectionHeader(".rsrc");

	// Create resources object
	if ((this->res = Rsrc::createFromRSRCSection(this->data+0, this->data.size(), rsrc)) == NULL)
		return false;

	// Get the current version and modification information from the resources
	FileVersionBasicInfo *v = FileVersionBasicInfo::Get(GetResourceDirectInRsrc(this->data+0, rsrc, ResType::VERSION, FIRST_ENTRY));
	if (v) {
		this->version = v->FileVersion;
		this->modified = (v->FileFlagsMask & v->FileFlags & (FileVersionBasicInfo::PATCHED | FileVersionBasicInfo::SPECIALBUILD)) > 0;
	}

	return true;
}
File::~File() { unload(); }
void File::unload() {
	uint32_t err = get_err();
	if (this->res) { delete this->res; this->res = NULL; }
	if (this->data.isopen()) { this->data.close(); }
	this->sections = nulldp;
	set_err(err);
}
bool File::isLoaded() const { return this->data.isopen(); }
bool File::isReadOnly() const { return this->data.isreadonly(); }
#pragma endregion

#pragma region Header Functions
///////////////////////////////////////////////////////////////////////////////
///// Header Functions
///////////////////////////////////////////////////////////////////////////////
bool File::is32bit() const { return (this->header->Characteristics & FileHeader::MACHINE_32BIT) != 0; }
bool File::is64bit() const { return (this->header->Characteristics & FileHeader::MACHINE_32BIT) == 0; }
uint64_t File::getImageBase() const { return this->is64bit() ? this->opt->ImageBase64 : this->opt->ImageBase32; }

dyn_ptr<FileHeader> File::getFileHeader() { return this->header; }
dyn_ptr<NTHeaders32> File::getNtHeaders32() { return this->nth32; }
dyn_ptr<NTHeaders64> File::getNtHeaders64() { return this->nth64; }

const dyn_ptr<FileHeader> File::getFileHeader() const { return this->header; }
const dyn_ptr<NTHeaders32> File::getNtHeaders32() const { return this->nth32; }
const dyn_ptr<NTHeaders64> File::getNtHeaders64() const { return this->nth64; }

uint32_t File::getDataDirectoryCount() const { return this->is64bit() ? this->nth64->OptionalHeader.NumberOfRvaAndSizes : this->nth32->OptionalHeader.NumberOfRvaAndSizes; }
dyn_ptr<DataDirectory> File::getDataDirectory(int i) { return this->dataDir+i; }
const dyn_ptr<DataDirectory> File::getDataDirectory(int i) const { return this->dataDir+i; }

int File::getSectionHeaderCount() const { return this->header->NumberOfSections; }
dyn_ptr<SectionHeader> File::getSectionHeader(int i) { return this->sections+i; }
dyn_ptr<SectionHeader> File::getSectionHeader(const char *str, int *index) {
	for (uint16_t i = 0; i < this->header->NumberOfSections; i++) {
		if (strncmp((const char*)this->sections[i].Name, str, ARRAYSIZE(this->sections[i].Name)) == 0) {
			if (index) *index = i;
			return this->sections+i;
		}
	}
	return nulldp;
}
dyn_ptr<SectionHeader> File::getSectionHeaderByRVA(uint32_t rva, int *index) {
	for (uint16_t i = 0; i < this->header->NumberOfSections; i++)
		if (this->sections[i].VirtualAddress <= rva && rva < this->sections[i].VirtualAddress + this->sections[i].VirtualSize) {
			if (index) *index = i;
			return this->sections+i;
		}
	return nulldp;
}
dyn_ptr<SectionHeader> File::getSectionHeaderByVA(uint64_t va, int *index) { return this->getSectionHeaderByRVA((uint32_t)(va - this->getImageBase()), index); }
const dyn_ptr<SectionHeader> File::getSectionHeader(int i) const { return this->sections+i; }
const dyn_ptr<SectionHeader> File::getSectionHeader(const char *str, int *index) const {
	for (uint16_t i = 0; i < this->header->NumberOfSections; i++) {
		if (strncmp((const char*)this->sections[i].Name, str, ARRAYSIZE(this->sections[i].Name)) == 0) {
			if (index) *index = i;
			return this->sections+i;
		}
	}
	return nulldp;
}
const dyn_ptr<SectionHeader> File::getSectionHeaderByRVA(uint32_t rva, int *index) const {
	for (uint16_t i = 0; i < this->header->NumberOfSections; i++)
		if (this->sections[i].VirtualAddress <= rva && rva < this->sections[i].VirtualAddress + this->sections[i].VirtualSize) {
			if (index) *index = i;
			return this->sections+i;
		}
	return nulldp;
}
const dyn_ptr<SectionHeader> File::getSectionHeaderByVA(uint64_t va, int *index) const { return this->getSectionHeaderByRVA((uint32_t)(va - this->getImageBase()), index); }
#pragma endregion

#pragma region Special Section Header Functions
///////////////////////////////////////////////////////////////////////////////
///// Special Section Header Functions
///////////////////////////////////////////////////////////////////////////////
dyn_ptr<SectionHeader> File::getExpandedSectionHdr(int i, uint32_t room) {
	if (i >= this->header->NumberOfSections)				{ return nulldp; }

	dyn_ptr<SectionHeader> sect = this->sections+i;
	uint32_t size = sect->SizeOfRawData, vs = sect->VirtualSize, min_size = vs + room;
	
	// Check if expansion is necessary
	if (min_size <= size)									{ return sect; }

	// Get the file and section alignment values
	uint32_t salign = this->opt->SectionAlignment, falign = this->opt->FileAlignment;

	// Check if expansion is possible
	if (roundUpTo(vs, salign) < min_size)					{ return nulldp; }

	// Move by a multiple of "file alignment"
	uint32_t new_size = (uint32_t)roundUpTo(min_size, falign), move = new_size - size;
	
	// Increase file size (invalidates all local pointers to the file data)
	if (!this->setSize(this->data.size()+move))				{ return nulldp; }
	sect = this->sections+i; // update the section header pointer 

	// Shift data and fill space with zeros
	uint32_t end = sect->PointerToRawData + size;
	if (!this->shift(end, move) || !this->zero(move, end))	{ return nulldp; }

	// Update section headers
	sect->SizeOfRawData += move; // update the size of the expanding section header
	for (uint16_t s = (uint16_t)i + 1; s < this->header->NumberOfSections; ++s) // update the location of all subsequent sections
		this->sections[s].PointerToRawData += move;
	dyn_ptr<DataDirectory> dir; // update the certificate entry if it exists
	if (this->getDataDirectoryCount() > DataDirectory::SECURITY && (dir = this->dataDir+DataDirectory::SECURITY) != nulldp && dir->VirtualAddress && dir->Size)
		dir->VirtualAddress += move;

	// Update NT header with new size information
	uint32_t chars = sect->Characteristics;
	this->opt->SizeOfImage += move;
	if (chars & SectionHeader::CNT_CODE)				this->opt->SizeOfCode += move;
	if (chars & SectionHeader::CNT_INITIALIZED_DATA)	this->opt->SizeOfInitializedData += move;
	if (chars & SectionHeader::CNT_UNINITIALIZED_DATA)	this->opt->SizeOfUninitializedData += move;

	this->flush();

	return sect;
}
dyn_ptr<SectionHeader> File::getExpandedSectionHdr(char *str, uint32_t room) {
	int i = 0;
	return this->getSectionHeader(str, &i) ? this->getExpandedSectionHdr(i, room) : dyn_ptr<SectionHeader>();
}
dyn_ptr<SectionHeader> File::createSection(int i, const char *name, uint32_t room, SectionHeader::CharacteristicFlags chars) {
	// Check if section already exists. If it does, expand it and return it
	int j = 0;
	dyn_ptr<SectionHeader> sect = this->getSectionHeader(name, &j);
	if (sect)											{ return this->getExpandedSectionHdr(j, room); }

	// Get the file and section alignment values
	uint32_t salign = this->opt->SectionAlignment, falign = this->opt->FileAlignment;

	// Get general information about the header
	uint16_t nSects = this->header->NumberOfSections;
	dyn_ptr<SectionHeader> last_sect = this->sections + nSects - 1;
	uint32_t header_used_size = (uint32_t)((dyn_ptr<byte>)(last_sect+1) - this->data), header_raw_size = (uint32_t)roundUpTo(header_used_size, falign), header_space = header_raw_size - header_used_size;
	if (header_space < sizeof(SectionHeader))										{ return nulldp; }	// no room in header to store a new SectionHeader

	// Get information about where this new section will be placed
	bool at_end = i >= nSects, no_sects = nSects == 0;
	sect = at_end ? (no_sects ? dyn_ptr<SectionHeader>() : last_sect) : this->sections + i;
	if (at_end) i = nSects;
	uint32_t pos = at_end ? header_used_size : (uint32_t)((dyn_ptr<byte>)sect - this->data);

	// Get the size, position, and address of the new section
	uint32_t raw_size = (uint32_t)roundUpTo(room, falign), move_va = (uint32_t)roundUpTo(raw_size, salign);
	uint32_t va = (uint32_t)roundUpTo(at_end ? (no_sects ? header_used_size : sect->VirtualAddress + sect->VirtualSize) : sect->VirtualAddress, salign);
	uint32_t pntr = (uint32_t)roundUpTo(at_end ? (no_sects ? header_used_size : sect->PointerToRawData + sect->SizeOfRawData) : sect->PointerToRawData, falign);

	// Create the new section header
	SectionHeader s = { {0, 0, 0, 0, 0, 0, 0, 0}, 0, va, raw_size, pntr, 0, 0, 0, 0, chars };
	size_t name_len = strlen(name);
	if (name_len >= ARRAYSIZE(s.Name))
		memcpy(s.Name, name, ARRAYSIZE(s.Name));
	else {
		memcpy(s.Name, name, name_len);
		memset(s.Name+name_len, 0, ARRAYSIZE(s.Name)-name_len);
	}

	// Increase file size (invalidates all local pointers to the file data)
	if (!this->setSize(this->data.size() + raw_size))								{ return nulldp; }
	// cannot use sect or last_sect unless they are updated!

	// Shift data and fill space with zeros
	if (!this->shift(pntr, raw_size) || !this->zero(raw_size, pntr))				{ return nulldp; }

	// Update the section headers
	if (!at_end && !this->move(pos, header_used_size-pos, sizeof(SectionHeader)))	{ return nulldp; }
	if (!this->set(&s, sizeof(SectionHeader), pos))									{ return nulldp; }
	++this->header->NumberOfSections;
	for (uint16_t s = (uint16_t)i + 1; s <= nSects; ++s) { // update the location and VA of all subsequent sections
		this->sections[s].VirtualAddress += move_va;
		this->sections[s].PointerToRawData += raw_size;
	}
	uint32_t ddCount = this->getDataDirectoryCount();
	for (uint32_t d = 0; d < ddCount; ++d) { // update the VA of all subsequent data directories
		if (d == DataDirectory::SECURITY) {
			if (this->dataDir[d].VirtualAddress >= pntr)
				this->dataDir[d].VirtualAddress += raw_size;
		} else {
			if (this->dataDir[d].VirtualAddress >= va)
				this->dataDir[d].VirtualAddress += move_va;
		}
	}
	
	// Update NT header with new size information
	this->opt->SizeOfImage += raw_size;
	if (chars & SectionHeader::CNT_CODE)				this->opt->SizeOfCode += raw_size;
	if (chars & SectionHeader::CNT_INITIALIZED_DATA)	this->opt->SizeOfInitializedData += raw_size;
	if (chars & SectionHeader::CNT_UNINITIALIZED_DATA)	this->opt->SizeOfUninitializedData += raw_size;

	this->flush();

	return this->sections+i;
}

dyn_ptr<SectionHeader> File::createSection(const char *str, const char *name, uint32_t room, SectionHeader::CharacteristicFlags chars) {
	int i = this->header->NumberOfSections;
	return (str == NULL || this->getSectionHeader(str, &i)) ? this->createSection(i, name, room, chars) : dyn_ptr<SectionHeader>();
}
dyn_ptr<SectionHeader> File::createSection(const char *name, uint32_t room, SectionHeader::CharacteristicFlags chars) {
	int i = this->header->NumberOfSections;
	this->getSectionHeader(".reloc", &i); // if it doesn't exist, i will remain unchanged
	return this->createSection(i, name, room, chars);
}
#pragma endregion

#pragma region Size Functions
///////////////////////////////////////////////////////////////////////////////
///// Size Functions 
///////////////////////////////////////////////////////////////////////////////
size_t File::getSize() const { return this->data.size(); }
bool File::setSize(size_t dwSize, bool grow_only) {
	if (this->data.isreadonly()) { return false; }
	if (dwSize == this->data.size() || (grow_only && dwSize < this->data.size()))	{ return true; }
	if (!this->data.resize(dwSize)) { this->unload(); return false; }
	return true;
}
#pragma endregion

#pragma region Resource Shortcut Functions
///////////////////////////////////////////////////////////////////////////////
///// Resource Shortcut Functions
///////////////////////////////////////////////////////////////////////////////
#ifdef EXPOSE_DIRECT_RESOURCES
Rsrc *File::getResources() { return this->res; }
const Rsrc *File::getResources() const { return this->res; }
#endif
bool File::resourceExists(const_resid type, const_resid name, uint16_t lang) const { return this->res->exists(type, name, lang); }
bool File::resourceExists(const_resid type, const_resid name, uint16_t* lang) const { return this->res->exists(type, name, lang); }
void* File::getResource(const_resid type, const_resid name, uint16_t lang, size_t* size) const { return this->res->get(type, name, lang, size); }
void* File::getResource(const_resid type, const_resid name, uint16_t* lang, size_t* size) const { return this->res->get(type, name, lang, size); }
bool File::removeResource(const_resid type, const_resid name, uint16_t lang) { return !this->data.isreadonly() && this->res->remove(type, name, lang); }
bool File::addResource(const_resid type, const_resid name, uint16_t lang, const void* data, size_t size, Overwrite overwrite) { return !this->data.isreadonly() && this->res->add(type, name, lang, data, size, overwrite); }
#pragma endregion

#pragma region Direct Data Functions
///////////////////////////////////////////////////////////////////////////////
///// Direct Data Functions
///////////////////////////////////////////////////////////////////////////////
dyn_ptr<byte> File::get(uint32_t dwOffset, uint32_t *dwSize) { if (dwSize) *dwSize = (uint32_t)this->data.size() - dwOffset; return this->data + dwOffset; }
const dyn_ptr<byte> File::get(uint32_t dwOffset, uint32_t *dwSize) const { if (dwSize) *dwSize = (uint32_t)this->data.size() - dwOffset; return this->data + dwOffset; }
bool File::set(const void* lpBuffer, uint32_t dwSize, uint32_t dwOffset) { return !this->data.isreadonly() && (dwOffset + dwSize <= this->data.size()) && memcpy(this->data + dwOffset, lpBuffer, dwSize); }
bool File::zero(uint32_t dwSize, uint32_t dwOffset) { return !this->data.isreadonly() && (dwOffset + dwSize <= this->data.size()) && memset(this->data + dwOffset, 0, dwSize); }
bool File::move(uint32_t dwOffset, uint32_t dwSize, int32_t dwDistanceToMove) { return !this->data.isreadonly() && (dwOffset + dwSize + dwDistanceToMove <= this->data.size()) && memmove(this->data+dwOffset+dwDistanceToMove, this->data+dwOffset, dwSize); }
bool File::shift(uint32_t dwOffset, int32_t dwDistanceToMove) { return move(dwOffset, (uint32_t)this->data.size() - dwOffset - dwDistanceToMove, dwDistanceToMove); }
bool File::flush() { return this->data.flush(); }
#pragma endregion

#pragma region General Query and Settings Functions
///////////////////////////////////////////////////////////////////////////////
///// General Query and Settings Functions
///////////////////////////////////////////////////////////////////////////////
#define CHK_SUM_FOLD(c) (((c)&0xffff) + ((c)>>16))
#define CHK_SUM_OFFSET	(peOffset+sizeof(uint32_t)+sizeof(FileHeader)+offsetof(OptionalHeader, CheckSum))
bool File::UpdatePEChkSum(bytes data, size_t dwSize, size_t peOffset, uint32_t dwOldCheck) {
	uint16_t *ptr = (uint16_t*)data;
	size_t len = dwSize/sizeof(uint16_t);
	uint32_t c = 0;
	while (len) {
		size_t l = (len < 0x4000) ? len : 0x4000;
		len -= l;
		for (size_t j=0; j<l; ++j)
			c += *ptr++;
		c = CHK_SUM_FOLD(c);
	}
	uint32_t dwCheck = (uint32_t)(uint16_t)CHK_SUM_FOLD(c);
	if (dwSize & 1) {
		dwCheck += data[dwSize-1];
		dwCheck = CHK_SUM_FOLD(dwCheck);
	}
	dwCheck = ((dwCheck-1<dwOldCheck)?(dwCheck-1):dwCheck) - dwOldCheck;
	dwCheck = CHK_SUM_FOLD(dwCheck);
	dwCheck = CHK_SUM_FOLD(dwCheck);
	*(uint32_t*)(data+CHK_SUM_OFFSET) = (uint32_t)(dwCheck + dwSize);
	return true;
}
bool File::updatePEChkSum() { return !this->data.isreadonly() && UpdatePEChkSum(this->data+0, this->data.size(), this->peOffset, this->is64bit() ? this->nth64->OptionalHeader.CheckSum : this->nth32->OptionalHeader.CheckSum) && this->flush(); }
//------------------------------------------------------------------------------
static const byte TinyDosStub[] = {0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD, 0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x57, 0x69, 0x6E, 0x20, 0x4F, 0x6E, 0x6C, 0x79, 0x0D, 0x0A, 0x24, 0x00, 0x00, 0x00};
bool File::hasExtraData() const { return this->dosh->e_crlc == 0x0000 && this->dosh->e_cparhdr == 0x0002 && this->dosh->e_lfarlc == 0x0020; }
dyn_ptr<void> File::getExtraData(uint32_t *size) {
	uint32_t sz = this->peOffset - sizeof(DOSHeader);
	if (!this->hasExtraData()) {
		if (this->data.isreadonly()) { return nulldp; }
		// Create the new header and fill the old stub in with 0s
		this->dosh->e_crlc		= 0x0000;	// number of relocations
		this->dosh->e_cparhdr	= 0x0002;	// size of header in 16 byte paragraphs
		this->dosh->e_lfarlc	= 0x0020;	// location of relocation table (end of tiny header)
		memcpy((dyn_ptr<byte>)this->dosh+0x20, TinyDosStub, sizeof(TinyDosStub));
		memset(this->data + sizeof(DOSHeader), 0, sz);
		this->flush();
	}
	*size = sz;
	return this->data + sizeof(DOSHeader);
}
//------------------------------------------------------------------------------
bool File::clearCertificateTable() {
	if (this->data.isreadonly()) { return false; }
	DataDirectory d = this->dataDir[DataDirectory::SECURITY];
	if (d.VirtualAddress && d.Size) {
		// Zero out the certificate
		memset(this->data + d.VirtualAddress, 0, d.Size);
		
		// Find out if the certificate was at the end
		uint32_t i;
		for (i = d.VirtualAddress + d.Size; i < this->data.size() && !this->data[i-1]; ++i);
		if (i >= this->data.size() && !this->setSize(d.VirtualAddress, false))
			return false;

		// Update the header
		this->dataDir[DataDirectory::SECURITY].VirtualAddress = 0;
		this->dataDir[DataDirectory::SECURITY].Size = 0;
		
		// Flush the changes
		this->flush();
	}
	return true;
}
//------------------------------------------------------------------------------
PE::Version::Version File::getFileVersion() const { return this->version; }
//------------------------------------------------------------------------------
bool File::isAlreadyModified() const { return this->modified; }
bool File::setModifiedFlag() {
	if (!this->data.isreadonly() && !this->modified) {
		const_resid name = NULL;
		uint16_t lang = 0;
		size_t size = 0;
		void* ver = GetResourceDirectInRsrc(this->data+0, this->getSectionHeader(".rsrc"), ResType::VERSION, FIRST_ENTRY, &name, &lang, &size);
		FileVersionBasicInfo *v = FileVersionBasicInfo::Get(ver);
		if (v) {
			v->FileFlags = (FileVersionBasicInfo::Flags)(v->FileFlags | (v->FileFlagsMask & (FileVersionBasicInfo::PATCHED | FileVersionBasicInfo::SPECIALBUILD)));
			this->modified = this->res->add(ResType::VERSION, name, lang, ver, size, ONLY);
			this->flush();
		}
	}
	return this->modified;
}
//-----------------------------------------------------------------------------
typedef union _Reloc {
	uint16_t Reloc;
	struct {
		uint16_t Offset : 12;
		uint16_t Type : 4;
	};
} Reloc;
#define RELOCS(e)		(dyn_ptr<Reloc>)((dyn_ptr<byte>)e+sizeof(BaseRelocation))
#define NEXT_RELOCS(e)	(dyn_ptr<BaseRelocation>)((dyn_ptr<byte>)e+e->SizeOfBlock)
#define COUNT_RELOCS(e)	(e->SizeOfBlock - sizeof(BaseRelocation)) / sizeof(uint16_t)
bool File::removeRelocs(uint32_t start, uint32_t end, bool reverse) {
	if (end < start)							{ return false; }

	// Read the relocs
	//DataDirectory dir = f->getDataDirectory(DataDirectory::BASERELOC);
	//uint32 size = dir.Size, pntr = dir.VirtualAddress;
	dyn_ptr<SectionHeader> sect = this->getSectionHeader(".reloc");
	if (!sect)									{ return true; } // no relocations exist, so nothing to remove!

	uint32_t size = sect->SizeOfRawData, pntr = sect->PointerToRawData;
	dyn_ptr<byte> data = this->get(pntr);

	//ABSOLUTE	= IMAGE_REL_I386_ABSOLUTE or IMAGE_REL_AMD64_ABSOLUTE
	//HIGHLOW	=> ??? or IMAGE_REL_AMD64_ADDR32NB (32-bit address w/o image base (RVA))
	//DIR64		=> IMAGE_REL_AMD64_SSPAN32 (32 bit signed span-dependent value applied at link time)
	uint16_t new_type = reverse ? (this->is64bit() ? BaseRelocation::DIR64 : BaseRelocation::HIGHLOW) : BaseRelocation::ABSOLUTE;

	// Remove everything that is between start and end
	// We do a thorough search for possible relocations and do not assume that they are in order
	dyn_ptr<void> entry_end = data + size;
	for (dyn_ptr<BaseRelocation> entry = (dyn_ptr<BaseRelocation>)data; entry < entry_end && entry->SizeOfBlock > 0; entry = NEXT_RELOCS(entry)) {

		// Check that the ranges overlap
		if (entry->VirtualAddress+0xFFF < start || entry->VirtualAddress > end) continue;

		// Go through each reloc in this entry
		uint32_t count = COUNT_RELOCS(entry);
		dyn_ptr<Reloc> relocs = RELOCS(entry);
		for (uint32_t i = 0; i < count; ++i) {
			// Already 'removed'
			if ((!reverse && relocs[i].Type == BaseRelocation::ABSOLUTE) ||
				(reverse && (relocs[i].Type != BaseRelocation::ABSOLUTE || relocs[i].Offset == 0))) continue;

			// Check the virtual address and possibly clear it
			uint32_t va = entry->VirtualAddress + relocs[i].Offset;
			if (va >= start && va <= end) {
				//relocs[i].Reloc = 0;
				relocs[i].Type = new_type;
			}
		}
	}
	return true;
}
#pragma endregion

#pragma region Saving Functions
///////////////////////////////////////////////////////////////////////////////
///// Saving Functions
///////////////////////////////////////////////////////////////////////////////
size_t File::getSizeOf(uint32_t cnt, int rsrcIndx, size_t rsrcRawSize) const {
	size_t size = 0;
	for (uint16_t i = 0; i < this->header->NumberOfSections; i++)
		if (this->sections[i].Characteristics & cnt)
			size += (i == (uint16_t)rsrcIndx) ? rsrcRawSize : this->sections[i].SizeOfRawData;
	return size;
}
inline static void adjustAddr(uint32_t &addr, size_t rAddr, size_t rNewSize, size_t rOldSize) {
	if (addr != 0 && addr >= rAddr + rOldSize)
		addr = (uint32_t)((addr + rNewSize) - rOldSize); // subtraction needs to be last b/c these are unsigned
}
bool File::save() {
	if (this->data.isreadonly()) { return false; }

	// Compile the .rsrc, get its size, and get all the information about it
	bool is64bit = this->is64bit();
	uint32_t fAlign = is64bit ? this->getNtHeaders64()->OptionalHeader.FileAlignment    : this->getNtHeaders32()->OptionalHeader.FileAlignment;
	uint32_t sAlign = is64bit ? this->getNtHeaders64()->OptionalHeader.SectionAlignment : this->getNtHeaders32()->OptionalHeader.SectionAlignment;
	int rIndx = 0;
	dyn_ptr<SectionHeader> rSect = this->getSectionHeader(".rsrc", &rIndx);
	if (!rSect) {
		this->createSection(".rsrc", 0, INIT_DATA_SECTION_R);
		rSect = this->getSectionHeader(".rsrc", &rIndx);
	}
	size_t rSize = 0;
	void* rsrc = this->res->compile(&rSize, rSect->VirtualAddress);
	size_t rRawSize = roundUpTo(rSize, fAlign);
	size_t rVirSize = roundUpTo(rSize, sAlign);
	//size_t rSizeOld = rSect->Misc.VirtualSize;
	size_t rRawSizeOld = rSect->SizeOfRawData;
	size_t rVirSizeOld = roundUpTo(rSect->VirtualSize, sAlign);
	uint32_t pntr = rSect->PointerToRawData;
	uint32_t imageSize = 0; //, imageSizeOld = 0;
	uint32_t fileSize = 0, fileSizeOld = (uint32_t)this->data.size();

	// Update PointerToSymbolTable
	adjustAddr(this->header->PointerToSymbolTable, rSect->VirtualAddress, rVirSize, rVirSizeOld);

	// Update Optional Header
	this->opt->SizeOfInitializedData = (uint32_t)this->getSizeOf(SectionHeader::CNT_INITIALIZED_DATA, rIndx, rRawSize);
	adjustAddr(this->opt->AddressOfEntryPoint, rSect->VirtualAddress, rVirSize, rVirSizeOld);
	adjustAddr(this->opt->BaseOfCode, rSect->VirtualAddress, rVirSize, rVirSizeOld);
	//imageSizeOld = this->opt->SizeOfImage;

	// Update the Data Directories
	this->dataDir[DataDirectory::RESOURCE].Size = (uint32_t)rSize;
	uint32_t ddCount = this->getDataDirectoryCount();
	for (uint32_t i = 0; i < ddCount; i++) {
		if (i == DataDirectory::SECURITY) { // the virtual address of DataDirectory::SECURITY is actually a file address, not a virtual address
			adjustAddr(this->dataDir[i].VirtualAddress, rSect->PointerToRawData, rRawSize, rRawSizeOld);
			if (this->dataDir[i].VirtualAddress + this->dataDir[i].Size > fileSize)
				fileSize = this->dataDir[i].VirtualAddress + this->dataDir[i].Size;
		} else {
			adjustAddr(this->dataDir[i].VirtualAddress, rSect->VirtualAddress, rVirSize, rVirSizeOld);
			if (this->dataDir[i].VirtualAddress + this->dataDir[i].Size > imageSize)
				imageSize = this->dataDir[i].VirtualAddress + this->dataDir[i].Size;
		}
	}
	
	// Update all section headers
	for (uint16_t i = (uint16_t)rIndx; i < this->header->NumberOfSections; i++) {
		if (strncmp((const char*)this->sections[i].Name, ".rsrc", ARRAYSIZE(this->sections[i].Name)) == 0) {
			this->sections[i].VirtualSize = (uint32_t)rSize;
			this->sections[i].SizeOfRawData = (uint32_t)rRawSize;
		} else {
			adjustAddr(this->sections[i].VirtualAddress, rSect->VirtualAddress, rVirSize, rVirSizeOld);
			adjustAddr(this->sections[i].PointerToRawData, rSect->PointerToRawData, rRawSize, rRawSizeOld);
		}
		adjustAddr(this->sections[i].PointerToLinenumbers, rSect->PointerToRawData, rRawSize, rRawSizeOld);
		adjustAddr(this->sections[i].PointerToRelocations, rSect->PointerToRawData, rRawSize, rRawSizeOld);
		if (this->sections[i].VirtualAddress + this->sections[i].VirtualSize > imageSize)
			imageSize = this->sections[i].VirtualAddress + this->sections[i].VirtualSize;
		if (this->sections[i].PointerToRawData + this->sections[i].SizeOfRawData > fileSize)
			fileSize = this->sections[i].PointerToRawData + this->sections[i].SizeOfRawData;
	}
	
	// Update the ImageSize
	this->opt->SizeOfImage = (uint32_t)roundUpTo(imageSize, sAlign);

	// Increase file size (invalidates all local pointers to the file data)
	if (fileSize > fileSizeOld && !this->setSize(fileSize))			{ free(rsrc); return false; }

	// Move all sections after resources and save resources
	dyn_ptr<byte> dp = this->data+pntr;
	if (rRawSize != rRawSizeOld && fileSize-rRawSize-pntr > 0)
		memmove(dp+rRawSize, dp+rRawSizeOld, fileSize-rRawSize-pntr);
	if (rRawSize > rSize)
		memset(dp+rSize, 0, rRawSize-rSize);
	memcpy(dp, rsrc, rSize);
	free(rsrc);

	// Decrease file size (invalidates all local pointers to the file data)
	if (fileSize < fileSizeOld && !this->setSize(fileSize, false))	{ return false; }

	// Finish Up
	return updatePEChkSum();
}
#pragma endregion
