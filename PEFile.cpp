#define _DECLARE_ALL_PE_FILE_RESOURCES_
#include "PEFileResources.h"
#include "PEFile.h"

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#pragma comment(lib, "Version.lib")		// for VerQueryValueW to read file versions

#define SAVE_ERR()			DWORD _err_ = GetLastError()
#define SET_ERR()			SetLastError(_err_)

#pragma region GetResourceDirect and get just the offset of the resource
///////////////////////////////////////////////////////////////////////////////
///// GetResourceDirect - For performance reasons (and get just the offset of the resource)
///////////////////////////////////////////////////////////////////////////////
#define FIRST_ENTRY ((LPWSTR)-1)
inline static IMAGE_RESOURCE_DIRECTORY *GetDir(const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry, const LPBYTE rsrc) {
	return (IMAGE_RESOURCE_DIRECTORY*)(rsrc + entry->OffsetToDirectory);
}
inline static IMAGE_RESOURCE_DIRECTORY_ENTRY *FirstEntry(const IMAGE_RESOURCE_DIRECTORY *dir) {
	return ((dir->NumberOfIdEntries + dir->NumberOfNamedEntries) < 1) ? NULL : (IMAGE_RESOURCE_DIRECTORY_ENTRY*)(dir+1);
}
inline static IMAGE_RESOURCE_DIRECTORY *FirstEntry(const IMAGE_RESOURCE_DIRECTORY *dir, const LPBYTE rsrc, LPCWSTR *out = NULL) {
	const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry = FirstEntry(dir);
	if (entry && out)
		*out = entry->NameIsString ? ((LPCWSTR)(rsrc+entry->NameOffset+sizeof(WORD))) : MAKEINTRESOURCE(entry->Id);
	return entry ? GetDir(entry, rsrc) : NULL;
}
inline static IMAGE_RESOURCE_DIRECTORY *FindEntryString(const IMAGE_RESOURCE_DIRECTORY *dir, const LPCWSTR id, const LPBYTE rsrc) {
	const IMAGE_RESOURCE_DIRECTORY_ENTRY *entries = (IMAGE_RESOURCE_DIRECTORY_ENTRY*)(dir+1);
	for (WORD i = 0; i < dir->NumberOfNamedEntries; i++)
		if (wcscmp(id, (LPWSTR)(rsrc+entries[i].NameOffset+sizeof(WORD))) == 0)
			return GetDir(entries+i, rsrc);
	return NULL;
}
inline static IMAGE_RESOURCE_DIRECTORY *FindEntryInt(const IMAGE_RESOURCE_DIRECTORY *dir, WORD id, const LPBYTE rsrc) {
	const IMAGE_RESOURCE_DIRECTORY_ENTRY *entries = (IMAGE_RESOURCE_DIRECTORY_ENTRY*)(dir+1)+dir->NumberOfNamedEntries;
	for (WORD i = 0; i < dir->NumberOfIdEntries; i++)
		if (entries[i].Id == id)
			return GetDir(entries+i, rsrc);
	return NULL;
}
inline static IMAGE_RESOURCE_DIRECTORY *FindEntry(const IMAGE_RESOURCE_DIRECTORY *dir, LPCWSTR id, const LPBYTE rsrc, LPCWSTR *out = NULL) {
	return (id == FIRST_ENTRY) ? FirstEntry(dir, rsrc, out) : (IS_INTRESOURCE(id) ? FindEntryInt(dir, RESID2WORD(id), rsrc) : FindEntryString(dir, id, rsrc));
}
static const LPVOID GetResourceDirectInRsrc(const LPBYTE data, const IMAGE_SECTION_HEADER *rsrcSect, LPCWSTR type, LPCWSTR name, LPCWSTR *out_name = NULL, WORD *lang = NULL, size_t *size = NULL) {
	if (!rsrcSect || rsrcSect->PointerToRawData == 0 || rsrcSect->SizeOfRawData == 0)	{ return NULL; }

	// Get the bytes for the RSRC section
	const LPBYTE rsrc = data + rsrcSect->PointerToRawData;
	
	// Get the type and name directories
	const IMAGE_RESOURCE_DIRECTORY *dir = (IMAGE_RESOURCE_DIRECTORY*)rsrc;
	if ((dir = FindEntry(dir, type, rsrc, NULL)) == NULL)				{ return NULL; }
	if ((dir = FindEntry(dir, name, rsrc, out_name)) == NULL)			{ return NULL; }

	// Assume the first language
	const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
	if ((entry = FirstEntry(dir)) == NULL || entry->DataIsDirectory)	{ return NULL; }
	const IMAGE_RESOURCE_DATA_ENTRY *dataEntry = (IMAGE_RESOURCE_DATA_ENTRY*)(rsrc+entry->OffsetToData);

	// Get the language and size of the resource
	if (lang) *lang = entry->Id;
	if (size) *size = dataEntry->Size;

	// Return the size and data
	return rsrc+dataEntry->OffsetToData-rsrcSect->VirtualAddress;
}
const LPVOID PEFile::GetResourceDirect(const LPVOID _data, LPCWSTR type, LPCWSTR name) {
	const LPBYTE data = (const LPBYTE)_data;

	// Load and check headers
	const IMAGE_DOS_HEADER *dosh = (IMAGE_DOS_HEADER*)data;
	if (dosh->e_magic != IMAGE_DOS_SIGNATURE)						{ return NULL; }
	LONG peOffset = dosh->e_lfanew;
	const IMAGE_NT_HEADERS32 *nth32 = (IMAGE_NT_HEADERS32*)(data+peOffset);
	const IMAGE_NT_HEADERS64 *nth64 = (IMAGE_NT_HEADERS64*)(data+peOffset);
	IMAGE_FILE_HEADER header = nth32->FileHeader; // identical for 32 and 64 bits
	if (nth32->Signature != IMAGE_NT_SIGNATURE)						{ return NULL; }
	bool is64bit = !(header.Characteristics & IMAGE_FILE_32BIT_MACHINE);
	if ((is64bit && nth64->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) || (!is64bit && nth32->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)) { return NULL; }

	// Get the RSRC section
	const IMAGE_SECTION_HEADER *sections = (IMAGE_SECTION_HEADER*)(data+peOffset+4+IMAGE_SIZEOF_FILE_HEADER+header.SizeOfOptionalHeader);
	const IMAGE_SECTION_HEADER *rsrcSect = NULL;
	for (WORD i = 0; i < header.NumberOfSections; ++i)
		if (strncmp((CHAR*)sections[i].Name, ".rsrc", IMAGE_SIZEOF_SHORT_NAME) == 0) { rsrcSect = sections+i; break; }

	// Get the resource within the RSRC section
	return GetResourceDirectInRsrc(data, rsrcSect, type, name);
}
#pragma endregion

#pragma region Memory Map Management Functions
///////////////////////////////////////////////////////////////////////////////
///// Memory Map Management Functions
///////////////////////////////////////////////////////////////////////////////
#include "umap.h"
#include "uvector.h"
typedef BOOL (WINAPI *UNMAP_OR_CLOSE)(void*);
typedef map<LPCWSTR, vector<LPVOID> > MMFViews;
static MMFViews mmfHndls, mmfViews;
static HANDLE AddMMF    (LPCWSTR file, HANDLE hMap) { if (hMap != NULL) mmfHndls[file].push_back(hMap); return hMap; }
static LPVOID AddMMFView(LPCWSTR file, LPVOID view) { if (view != NULL) mmfViews[file].push_back(view); return view; }
static void _RemoveMMF(MMFViews &mmfs, LPCWSTR file, LPVOID x) {
	MMFViews::iterator v = mmfs.find(file);
	if (v != mmfs.end()) {
		size_t size = v->second.size();
		for (size_t i = 0; i < size; ++i) {
			if (v->second[i] == x) {
				if (size == 1) {
					mmfs.erase(v);
				} else {
					if (i != size-1) // make the removed element the last element
						v->second[i] = v->second[size-1];
					v->second.pop_back(); // remove the last element
				}
				break;
			}
		}
	}
}
static void RemoveMMF    (LPCWSTR file, HANDLE hMap) { _RemoveMMF(mmfHndls, file, hMap); }
static void RemoveMMFView(LPCWSTR file, LPVOID view) { _RemoveMMF(mmfViews, file, view); }
static void _UnmapAll(MMFViews &mmfs, LPCWSTR file, UNMAP_OR_CLOSE func) {
	MMFViews::iterator v = mmfs.find(file);
	if (v != mmfs.end()) {
		size_t size = v->second.size();
		for (size_t i = 0; i < size; ++i)
			func(v->second[i]);
		mmfs.erase(v);
	}
}
void PEFile::UnmapAllViewsOfFile(LPCWSTR file) {
	_UnmapAll(mmfHndls, file, &CloseHandle);
	_UnmapAll(mmfViews, file, (UNMAP_OR_CLOSE)&UnmapViewOfFile);
}
#pragma endregion

#pragma region Loading Functions
///////////////////////////////////////////////////////////////////////////////
///// Loading Functions
///////////////////////////////////////////////////////////////////////////////
PEFile::PEFile(LPVOID data, size_t size, bool readonly)
	: sections(NULL), res(NULL), readonly(readonly), hFile(NULL), hMap(NULL), size(size), orig_data((LPBYTE)data), data(NULL), version(0), modified(false) {
	this->original[0] = 0;
	if (!map() || !this->load(true))
		this->unload();
}
PEFile::PEFile(LPCWSTR file, bool readonly)
	: sections(NULL), res(NULL), readonly(readonly), hFile(NULL), hMap(NULL), size(0), orig_data(NULL), data(NULL), version(0), modified(false) {
	this->original[0] = 0;
	if (!GetFullPathName(file, ARRAYSIZE(this->original), this->original, NULL) ||
		(this->hFile = CreateFile(this->original, (readonly ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE)), FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE ||
		(this->size = GetFileSize(this->hFile, 0)) == INVALID_FILE_SIZE ||
		!this->map() || !this->load(true)) {
			this->unload();
	}
}
bool PEFile::map() { // only call this from an unmapped state
	if (this->orig_data) {
		if (this->readonly) {
			DWORD old_protect = 0;
			return (this->data = (LPBYTE)VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) != NULL &&
				memcpy(this->data, this->orig_data, size) && VirtualProtect(this->data, size, PAGE_READONLY, &old_protect);
		} else {
			this->data = this->orig_data;
			return true;
		}
	} else {
		return (this->hMap = AddMMF(this->original, CreateFileMapping(this->hFile, NULL, (this->readonly ? PAGE_READONLY : PAGE_READWRITE), 0, 0, NULL))) != NULL &&
			(this->data = (LPBYTE)AddMMFView(this->original, MapViewOfFile(this->hMap, (this->readonly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS), 0, 0, 0))) != NULL;
	}
}
void PEFile::unmap() {
	this->flush();
	if (this->orig_data && this->data) {
		if (this->readonly) { VirtualFree(this->data, 0, MEM_RELEASE); }
		this->data = NULL;
	}
	if (this->hMap) {
		if (this->data) { UnmapViewOfFile(this->data); RemoveMMFView(this->original, this->data); this->data = NULL; }
		RemoveMMF(this->original, this->hMap); CloseHandle(this->hMap); this->hMap = NULL;
	}
}
bool PEFile::load(bool incRes) {
	this->dosh = (IMAGE_DOS_HEADER*)this->data;
	if (this->dosh->e_magic != IMAGE_DOS_SIGNATURE)					{ SetLastError(ERROR_INVALID_DATA); return false; }
	this->peOffset = this->dosh->e_lfanew;

	this->nth32 = (IMAGE_NT_HEADERS32*)(this->data+this->peOffset);
	this->nth64 = (IMAGE_NT_HEADERS64*)(this->data+this->peOffset);
	if (this->nth32->Signature != IMAGE_NT_SIGNATURE)				{ SetLastError(ERROR_INVALID_DATA); return false; }
	this->header = &this->nth32->FileHeader; // identical for 32 and 64 bits
	bool is64bit = this->is64bit(), is32bit = this->is32bit();

	if ((is64bit && this->nth64->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) ||
		(is32bit && this->nth32->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) ||
		(is64bit == is32bit))										{ SetLastError(ERROR_INVALID_DATA); return false; }

	this->dataDir = is64bit ? this->nth64->OptionalHeader.DataDirectory : this->nth32->OptionalHeader.DataDirectory;
	this->sections = (IMAGE_SECTION_HEADER*)(this->data+this->peOffset+4+IMAGE_SIZEOF_FILE_HEADER+this->header->SizeOfOptionalHeader);

	// Load resources
	if (incRes) {
		IMAGE_SECTION_HEADER *sect = this->getSectionHeader(".rsrc");

		// Create resources object
		if ((this->res = Rsrc::createFromRSRCSection(this->data, this->size, sect)) == NULL)
			return false;

		// Get the current version and modification information from the resources
		VS_FIXEDFILEINFO *v = GetVersionInfo(GetResourceDirectInRsrc(this->data, sect, RT_VERSION, FIRST_ENTRY));
		if (v) {
			this->version = ((ULONGLONG)v->dwFileVersionMS << 32) | v->dwFileVersionLS;
			this->modified = (v->dwFileFlagsMask & v->dwFileFlags & (VS_FF_PATCHED | VS_FF_SPECIALBUILD)) > 0;
		}
	}

	return true;
}
PEFile::~PEFile() { unload(); if (this->orig_data) free(this->orig_data); }
void PEFile::unload() {
	SAVE_ERR();
	if (this->res) { delete this->res; this->res = NULL; }
	unmap();
	if (this->hFile && this->hFile != INVALID_HANDLE_VALUE) { CloseHandle(this->hFile); this->hFile = NULL; }
	this->sections = NULL;
	SET_ERR();
}
bool PEFile::isLoaded() const { return this->sections != NULL; }
bool PEFile::isReadOnly() const { return this->readonly; }
bool PEFile::usesMemoryMappedFile() const { return this->orig_data == NULL; }
#pragma endregion

#pragma region Header Functions
///////////////////////////////////////////////////////////////////////////////
///// Header Functions
///////////////////////////////////////////////////////////////////////////////
bool PEFile::is32bit() const { return (this->header->Characteristics & IMAGE_FILE_32BIT_MACHINE) != 0; }
bool PEFile::is64bit() const { return (this->header->Characteristics & IMAGE_FILE_32BIT_MACHINE) == 0; }
ULONGLONG PEFile::getImageBase() const { return this->is64bit() ? this->nth64->OptionalHeader.ImageBase : this->nth32->OptionalHeader.ImageBase; }

IMAGE_FILE_HEADER *PEFile::getFileHeader() { return this->header; }
IMAGE_NT_HEADERS32 *PEFile::getNtHeaders32() { return this->nth32; }
IMAGE_NT_HEADERS64 *PEFile::getNtHeaders64() { return this->nth64; }

const IMAGE_FILE_HEADER *PEFile::getFileHeader() const { return this->header; }
const IMAGE_NT_HEADERS32 *PEFile::getNtHeaders32() const { return this->nth32; }
const IMAGE_NT_HEADERS64 *PEFile::getNtHeaders64() const { return this->nth64; }

DWORD PEFile::getDataDirectoryCount() const { return this->is64bit() ? this->nth64->OptionalHeader.NumberOfRvaAndSizes : this->nth32->OptionalHeader.NumberOfRvaAndSizes; }
IMAGE_DATA_DIRECTORY *PEFile::getDataDirectory(int i) { return this->dataDir+i; }
const IMAGE_DATA_DIRECTORY *PEFile::getDataDirectory(int i) const { return this->dataDir+i; }

int PEFile::getSectionHeaderCount() const { return this->header->NumberOfSections; }
IMAGE_SECTION_HEADER *PEFile::getSectionHeader(int i) { return this->sections+i; }
IMAGE_SECTION_HEADER *PEFile::getSectionHeader(const char *str, int *index) {
	for (WORD i = 0; i < this->header->NumberOfSections; i++) {
		if (strncmp((CHAR*)this->sections[i].Name, str, IMAGE_SIZEOF_SHORT_NAME) == 0) {
			if (index) *index = i;
			return this->sections+i;
		}
	}
	return NULL;
}
IMAGE_SECTION_HEADER *PEFile::getSectionHeaderByRVA(DWORD rva, int *index) {
	for (WORD i = 0; i < this->header->NumberOfSections; i++)
		if (this->sections[i].VirtualAddress <= rva && rva < this->sections[i].VirtualAddress + this->sections[i].Misc.VirtualSize) {
			if (index) *index = i;
			return this->sections+i;
		}
	return NULL;
}
IMAGE_SECTION_HEADER *PEFile::getSectionHeaderByVA(ULONGLONG va, int *index) { return this->getSectionHeaderByRVA((DWORD)(va - this->getImageBase()), index); }
const IMAGE_SECTION_HEADER *PEFile::getSectionHeader(int i) const { return this->sections+i; }
const IMAGE_SECTION_HEADER *PEFile::getSectionHeader(const char *str, int *index) const {
	for (WORD i = 0; i < this->header->NumberOfSections; i++) {
		if (strncmp((CHAR*)this->sections[i].Name, str, IMAGE_SIZEOF_SHORT_NAME) == 0) {
			if (index) *index = i;
			return this->sections+i;
		}
	}
	return NULL;
}
const IMAGE_SECTION_HEADER *PEFile::getSectionHeaderByRVA(DWORD rva, int *index) const {
	for (WORD i = 0; i < this->header->NumberOfSections; i++)
		if (this->sections[i].VirtualAddress <= rva && rva < this->sections[i].VirtualAddress + this->sections[i].Misc.VirtualSize) {
			if (index) *index = i;
			return this->sections+i;
		}
	return NULL;
}
const IMAGE_SECTION_HEADER *PEFile::getSectionHeaderByVA(ULONGLONG va, int *index) const { return this->getSectionHeaderByRVA((DWORD)(va - this->getImageBase()), index); }
#pragma endregion

#pragma region Special Section Header Functions
///////////////////////////////////////////////////////////////////////////////
///// Special Section Header Functions
///////////////////////////////////////////////////////////////////////////////
IMAGE_SECTION_HEADER *PEFile::getExpandedSectionHdr(int i, DWORD room) {
	if (i >= this->header->NumberOfSections)			{ return NULL; }

	IMAGE_SECTION_HEADER *sect = this->sections+i;
	DWORD size = sect->SizeOfRawData, vs = sect->Misc.VirtualSize, min_size = vs + room, salign, falign;
	
	// Check if expansion is necessary
	if (min_size <= size)								{ return sect; }

	// Get the file and section alignment values
	if (this->is64bit()) {
		IMAGE_OPTIONAL_HEADER64 *h = &this->nth64->OptionalHeader;
		salign = h->SectionAlignment;
		falign = h->FileAlignment;
	} else {
		IMAGE_OPTIONAL_HEADER32 *h = &this->nth32->OptionalHeader;
		salign = h->SectionAlignment;
		falign = h->FileAlignment;
	}

	// Check if expansion is possible
	if (roundUpTo(vs, salign) < min_size)				{ return NULL; }

	// Move by a multiple of "file alignment"
	DWORD new_size = (DWORD)roundUpTo(min_size, falign), move = new_size - size;
	
	// Increase file size (invalidates all local pointers to the file data)
	if (!this->setSize(this->size+move))				{ return NULL; }
	sect = this->sections+i; // update the section header pointer 

	// Shift data and fill space with zeros
	DWORD end = sect->PointerToRawData + size;
	if (!this->shift(end, move) || !this->zero(move, end))	{ return NULL; }

	// Update section headers
	sect->SizeOfRawData += move; // update the size of the expanding section header
	for (WORD s = (WORD)i + 1; s < this->header->NumberOfSections; ++s) // update the location of all subsequent sections
		this->sections[s].PointerToRawData += move;
	IMAGE_DATA_DIRECTORY *dir; // update the certificate entry if it exists
	if (this->getDataDirectoryCount() > IMAGE_DIRECTORY_ENTRY_SECURITY && (dir = this->dataDir+IMAGE_DIRECTORY_ENTRY_SECURITY) != NULL && dir->VirtualAddress && dir->Size)
		dir->VirtualAddress += move;

	// Update NT header with new size information
	DWORD chars = sect->Characteristics;
	if (this->is64bit()) {
		IMAGE_OPTIONAL_HEADER64 *h = &this->nth64->OptionalHeader;
		h->SizeOfImage += move;
		if (chars & IMAGE_SCN_CNT_CODE)					h->SizeOfCode += move;
		if (chars & IMAGE_SCN_CNT_INITIALIZED_DATA)		h->SizeOfInitializedData += move;
		if (chars & IMAGE_SCN_CNT_UNINITIALIZED_DATA)	h->SizeOfUninitializedData += move;
	} else {
		IMAGE_OPTIONAL_HEADER32 *h = &this->nth32->OptionalHeader;
		h->SizeOfImage += move;
		if (chars & IMAGE_SCN_CNT_CODE)					h->SizeOfCode += move;
		if (chars & IMAGE_SCN_CNT_INITIALIZED_DATA)		h->SizeOfInitializedData += move;
		if (chars & IMAGE_SCN_CNT_UNINITIALIZED_DATA)	h->SizeOfUninitializedData += move;
	}

	this->flush();

	return sect;
}
IMAGE_SECTION_HEADER *PEFile::getExpandedSectionHdr(char *str, DWORD room) {
	int i = 0;
	return this->getSectionHeader(str, &i) ? this->getExpandedSectionHdr(i, room) : NULL;
}
IMAGE_SECTION_HEADER *PEFile::createSection(int i, const char *name, DWORD room, DWORD chars) {
	// Check if section already exists. If it does, expand it and return it
	int j;
	IMAGE_SECTION_HEADER *sect = this->getSectionHeader(name, &j);
	if (sect)											{ return this->getExpandedSectionHdr(j, room); }

	// Get the file and section alignment values
	DWORD salign, falign;
	if (this->is64bit()) {
		IMAGE_OPTIONAL_HEADER64 *h = &this->nth64->OptionalHeader;
		salign = h->SectionAlignment;
		falign = h->FileAlignment;
	} else {
		IMAGE_OPTIONAL_HEADER32 *h = &this->nth32->OptionalHeader;
		salign = h->SectionAlignment;
		falign = h->FileAlignment;
	}

	// Get general information about the header
	WORD nSects = this->header->NumberOfSections;
	IMAGE_SECTION_HEADER *last_sect = this->sections + nSects - 1;
	DWORD header_used_size = (DWORD)((LPBYTE)(last_sect+1) - this->data), header_raw_size = (DWORD)roundUpTo(header_used_size, falign), header_space = header_raw_size - header_used_size;
	if (header_space < sizeof(IMAGE_SECTION_HEADER))	{ return NULL; }	// no room in header to store a new IMAGE_SECTION_HEADER

	// Get information about where this new section will be placed
	bool at_end = i >= nSects, no_sects = nSects == 0;
	sect = at_end ? (no_sects ? NULL : last_sect) : this->sections + i;
	if (at_end) i = nSects;
	DWORD pos = at_end ? header_used_size : (DWORD)((LPBYTE)sect - this->data);

	// Get the size, position, and address of the new section
	DWORD raw_size = (DWORD)roundUpTo(room, falign), move_va = (DWORD)roundUpTo(raw_size, salign);
	DWORD va = (DWORD)roundUpTo(at_end ? (no_sects ? header_used_size : sect->VirtualAddress + sect->Misc.VirtualSize) : sect->VirtualAddress, salign);
	DWORD pntr = (DWORD)roundUpTo(at_end ? (no_sects ? header_used_size : sect->PointerToRawData + sect->SizeOfRawData) : sect->PointerToRawData, falign);

	// Create the new section header
	IMAGE_SECTION_HEADER s = { {0, 0, 0, 0, 0, 0, 0, 0}, {0}, va, raw_size, pntr, 0, 0, 0, 0, chars };
	size_t name_len = strlen(name);
	if (name_len >= IMAGE_SIZEOF_SHORT_NAME)
		memcpy(s.Name, name, IMAGE_SIZEOF_SHORT_NAME);
	else {
		memcpy(s.Name, name, name_len);
		memset(s.Name+name_len, 0, IMAGE_SIZEOF_SHORT_NAME-name_len);
	}

	// Increase file size (invalidates all local pointers to the file data)
	if (!this->setSize(this->size + raw_size))							{ return NULL; }
	// cannot use sect or last_sect unless they are updated!

	// Shift data and fill space with zeros
	if (!this->shift(pntr, raw_size) || !this->zero(raw_size, pntr))	{ return NULL; }

	// Update the section headers
	if (!at_end && !this->move(pos, header_used_size-pos, sizeof(IMAGE_SECTION_HEADER)))	{ return NULL; }
	if (!this->set(&s, sizeof(IMAGE_SECTION_HEADER), pos))				{ return NULL; }
	++this->header->NumberOfSections;
	for (WORD s = (WORD)i + 1; s <= nSects; ++s) { // update the location and VA of all subsequent sections
		this->sections[s].VirtualAddress += move_va;
		this->sections[s].PointerToRawData += raw_size;
	}
	DWORD ddCount = this->getDataDirectoryCount();
	for (DWORD d = 0; d < ddCount; ++d) { // update the VA of all subsequent data directories
		if (d == IMAGE_DIRECTORY_ENTRY_SECURITY) {
			if (this->dataDir[d].VirtualAddress >= pntr)
				this->dataDir[d].VirtualAddress += raw_size;
		} else {
			if (this->dataDir[d].VirtualAddress >= va)
				this->dataDir[d].VirtualAddress += move_va;
		}
	}
	
	// Update NT header with new size information
	if (this->is64bit()) {
		IMAGE_OPTIONAL_HEADER64 *h = &this->nth64->OptionalHeader;
		h->SizeOfImage += raw_size;
		if (chars & IMAGE_SCN_CNT_CODE)					h->SizeOfCode += raw_size;
		if (chars & IMAGE_SCN_CNT_INITIALIZED_DATA)		h->SizeOfInitializedData += raw_size;
		if (chars & IMAGE_SCN_CNT_UNINITIALIZED_DATA)	h->SizeOfUninitializedData += raw_size;
	} else {
		IMAGE_OPTIONAL_HEADER32 *h = &this->nth32->OptionalHeader;
		h->SizeOfImage += raw_size;
		if (chars & IMAGE_SCN_CNT_CODE)					h->SizeOfCode += raw_size;
		if (chars & IMAGE_SCN_CNT_INITIALIZED_DATA)		h->SizeOfInitializedData += raw_size;
		if (chars & IMAGE_SCN_CNT_UNINITIALIZED_DATA)	h->SizeOfUninitializedData += raw_size;
	}

	this->flush();

	return this->sections+i;
}

IMAGE_SECTION_HEADER *PEFile::createSection(const char *str, const char *name, DWORD room, DWORD chars) {
	int i = this->header->NumberOfSections;
	return (str == NULL || this->getSectionHeader(str, &i)) ? this->createSection(i, name, room, chars) : NULL;
}
IMAGE_SECTION_HEADER *PEFile::createSection(const char *name, DWORD room, DWORD chars) {
	int i = this->header->NumberOfSections;
	this->getSectionHeader(".reloc", &i); // if it doesn't exist, i will remain unchanged
	return this->createSection(i, name, room, chars);
}
#pragma endregion

#pragma region Size Functions
///////////////////////////////////////////////////////////////////////////////
///// Size Functions 
///////////////////////////////////////////////////////////////////////////////
size_t PEFile::getSize() const { return this->size; }
bool PEFile::setSize(size_t dwSize, bool grow_only) {
	if (this->readonly)										{ return false; }
	bool shrinking = dwSize < this->size;
	if (dwSize == this->size || (grow_only && shrinking))	{ return true; }
	this->unmap();
	bool retval = (this->orig_data ?
		((this->orig_data = (LPBYTE)realloc(this->orig_data, dwSize)) != NULL) :
		SetFilePointer(this->hFile, (DWORD)dwSize, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER && SetEndOfFile(this->hFile)) && this->map();
	if (retval) {
		this->size = dwSize;
		if (!shrinking)
			memset(this->data+this->size, 0, dwSize-this->size); // set new memory to 0
		retval = this->load(false);
	}
	if (!retval) this->unload();
	return retval;
}
#pragma endregion

#pragma region Resource Shortcut Functions
///////////////////////////////////////////////////////////////////////////////
///// Resource Shortcut Functions
///////////////////////////////////////////////////////////////////////////////
#ifdef EXPOSE_DIRECT_RESOURCES
Rsrc *PEFile::getResources() { return this->res; }
const Rsrc *PEFile::getResources() const { return this->res; }
#endif
bool PEFile::resourceExists(LPCWSTR type, LPCWSTR name, WORD lang) const { return this->res->exists(type, name, lang); }
bool PEFile::resourceExists(LPCWSTR type, LPCWSTR name, WORD* lang) const { return this->res->exists(type, name, lang); }
LPVOID PEFile::getResource(LPCWSTR type, LPCWSTR name, WORD lang, size_t* size) const { return this->res->get(type, name, lang, size); }
LPVOID PEFile::getResource(LPCWSTR type, LPCWSTR name, WORD* lang, size_t* size) const { return this->res->get(type, name, lang, size); }
bool PEFile::removeResource(LPCWSTR type, LPCWSTR name, WORD lang) { return !this->readonly && this->res->remove(type, name, lang); }
bool PEFile::addResource(LPCWSTR type, LPCWSTR name, WORD lang, const LPVOID data, size_t size, DWORD overwrite) { return !this->readonly && this->res->add(type, name, lang, data, size, overwrite); }
#pragma endregion

#pragma region Direct Data Functions
///////////////////////////////////////////////////////////////////////////////
///// Direct Data Functions
///////////////////////////////////////////////////////////////////////////////
LPBYTE PEFile::get(DWORD dwOffset, DWORD *dwSize) { if (dwSize) *dwSize = (DWORD)this->size - dwOffset; return this->data + dwOffset; }
const LPBYTE PEFile::get(DWORD dwOffset, DWORD *dwSize) const { if (dwSize) *dwSize = (DWORD)this->size - dwOffset; return this->data + dwOffset; }
bool PEFile::set(const LPVOID lpBuffer, DWORD dwSize, DWORD dwOffset) { return !this->readonly && (dwOffset + dwSize <= this->size) && memcpy(this->data + dwOffset, lpBuffer, dwSize); }
bool PEFile::zero(DWORD dwSize, DWORD dwOffset) { return !this->readonly && (dwOffset + dwSize <= this->size) && memset(this->data + dwOffset, 0, dwSize); }
bool PEFile::move(DWORD dwOffset, DWORD dwSize, int dwDistanceToMove) { return !this->readonly && (dwOffset + dwSize + dwDistanceToMove <= this->size) && memmove(this->data+dwOffset+dwDistanceToMove, this->data+dwOffset, dwSize); }
bool PEFile::shift(DWORD dwOffset, int dwDistanceToMove) { return move(dwOffset, (DWORD)this->size - dwOffset - dwDistanceToMove, dwDistanceToMove); }
bool PEFile::flush() { return !this->readonly && (!this->hMap || FlushViewOfFile(this->data, 0)) && (!this->hFile || FlushFileBuffers(this->hFile)); }
#pragma endregion

#pragma region General Query and Settings Functions
///////////////////////////////////////////////////////////////////////////////
///// General Query and Settings Functions
///////////////////////////////////////////////////////////////////////////////
#define CHK_SUM_FOLD(c) (((c)&0xffff) + ((c)>>16))
#define CHK_SUM_OFFSET	(peOffset+sizeof(DWORD)+sizeof(IMAGE_FILE_HEADER)+offsetof(IMAGE_OPTIONAL_HEADER, CheckSum))
bool PEFile::UpdatePEChkSum(LPBYTE data, size_t dwSize, size_t peOffset, DWORD dwOldCheck) {
	USHORT *ptr = (USHORT*)data;
	size_t len = dwSize/sizeof(USHORT);
	DWORD c = 0;
	while (len) {
		size_t l = (len < 0x4000) ? len : 0x4000;
		len -= l;
		for (size_t j=0; j<l; ++j)
			c += *ptr++;
		c = CHK_SUM_FOLD(c);
	}
	DWORD dwCheck = (DWORD)(WORD)CHK_SUM_FOLD(c);
	if (dwSize & 1) {
		dwCheck += data[dwSize-1];
		dwCheck = CHK_SUM_FOLD(dwCheck);
	}
	dwCheck = ((dwCheck-1<dwOldCheck)?(dwCheck-1):dwCheck) - dwOldCheck;
	dwCheck = CHK_SUM_FOLD(dwCheck);
	dwCheck = CHK_SUM_FOLD(dwCheck);
	*(DWORD*)(data+CHK_SUM_OFFSET) = (DWORD)(dwCheck + dwSize);
	return true;
}
bool PEFile::updatePEChkSum() { return !this->readonly && UpdatePEChkSum(this->data, this->size, this->peOffset, this->is64bit() ? this->nth64->OptionalHeader.CheckSum : this->nth32->OptionalHeader.CheckSum) && this->flush(); }
//------------------------------------------------------------------------------
static const BYTE TinyDosStub[] = {0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD, 0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x57, 0x69, 0x6E, 0x20, 0x4F, 0x6E, 0x6C, 0x79, 0x0D, 0x0A, 0x24, 0x00, 0x00, 0x00};
bool PEFile::hasExtraData() const { return this->dosh->e_crlc == 0x0000 && this->dosh->e_cparhdr == 0x0002 && this->dosh->e_lfarlc == 0x0020; }
LPVOID PEFile::getExtraData(DWORD *size) {
	DWORD sz = this->peOffset - sizeof(IMAGE_DOS_HEADER);
	if (!this->hasExtraData()) {
		if (this->readonly) { return NULL; }
		// Create the new header and fill the old stub in with 0s
		this->dosh->e_crlc		= 0x0000;	// number of relocations
		this->dosh->e_cparhdr	= 0x0002;	// size of header in 16 byte paragraphs
		this->dosh->e_lfarlc	= 0x0020;	// location of relocation table (end of tiny header)
		memcpy(((BYTE*)this->dosh)+0x20, TinyDosStub, sizeof(TinyDosStub));
		memset(this->data + sizeof(IMAGE_DOS_HEADER), 0, sz);
		this->flush();
	}
	*size = sz;
	return this->data + sizeof(IMAGE_DOS_HEADER);
}
//------------------------------------------------------------------------------
bool PEFile::clearCertificateTable() {
	if (this->readonly) { return false; }
	IMAGE_DATA_DIRECTORY d = this->dataDir[IMAGE_DIRECTORY_ENTRY_SECURITY];
	if (d.VirtualAddress && d.Size) {
		// Zero out the certificate
		memset(this->data + d.VirtualAddress, 0, d.Size);
		
		// Find out if the certificate was at the end
		DWORD i;
		for (i = d.VirtualAddress + d.Size; i < this->size && !this->data[i-1]; ++i);
		if (i >= this->size && !this->setSize(d.VirtualAddress, false))
			return false;

		// Update the header
		this->dataDir[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress = 0;
		this->dataDir[IMAGE_DIRECTORY_ENTRY_SECURITY].Size = 0;
		
		// Flush the changes
		this->flush();
	}
	return true;
}
//------------------------------------------------------------------------------
bool PEFile::GetVersionInfo(const LPVOID ver, LPCWSTR query, LPVOID *buffer, PUINT len) { return VerQueryValueW(ver, query, buffer, len) != 0; }
VS_FIXEDFILEINFO *PEFile::GetVersionInfo(const LPVOID ver) { VS_FIXEDFILEINFO *v = NULL; UINT count; return (ver && VerQueryValueW(ver, L"\\", (LPVOID*)&v, &count)) ? v : NULL; }
ULONGLONG PEFile::getFileVersion() const { return this->version; }
//------------------------------------------------------------------------------
bool PEFile::isAlreadyModified() const { return this->modified; }
bool PEFile::setModifiedFlag() {
	if (!this->readonly && !this->modified) {
		LPCWSTR name = NULL;
		WORD lang = 0;
		size_t size = 0;
		LPVOID ver = GetResourceDirectInRsrc(this->data, this->getSectionHeader(".rsrc"), RT_VERSION, FIRST_ENTRY, &name, &lang, &size);
		VS_FIXEDFILEINFO *v = PEFile::GetVersionInfo(ver);
		if (ver && v) {
			v->dwFileFlags |= v->dwFileFlagsMask & (VS_FF_PATCHED | VS_FF_SPECIALBUILD);
			this->modified = this->res->add(RT_VERSION, name, lang, ver, size, OVERWRITE_ONLY);
			this->flush();
		}
	}
	return this->modified;
}
//-----------------------------------------------------------------------------
typedef union _Reloc {
	WORD Reloc;
	struct {
		WORD Offset : 12;
		WORD Type : 4;
	};
} Reloc;
#define RELOCS(e)		(Reloc*)((LPBYTE)e+sizeof(IMAGE_BASE_RELOCATION))
#define NEXT_RELOCS(e)	(IMAGE_BASE_RELOCATION*)((LPBYTE)e+e->SizeOfBlock)
#define COUNT_RELOCS(e)	(e->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD)
bool PEFile::removeRelocs(DWORD start, DWORD end, bool reverse) {
	if (end < start)							{ return false; }

	// Read the relocs
	//IMAGE_DATA_DIRECTORY dir = f->getDataDirectory(IMAGE_DIRECTORY_ENTRY_BASERELOC);
	//DWORD size = dir.Size, pntr = dir.VirtualAddress;
	IMAGE_SECTION_HEADER *sect = this->getSectionHeader(".reloc");
	if (sect == NULL)							{ return true; } // no relocations exist, so nothing to remove!

	DWORD size = sect->SizeOfRawData, pntr = sect->PointerToRawData;
	LPBYTE data = this->get(pntr);

	//IMAGE_REL_BASED_ABSOLUTE	= IMAGE_REL_I386_ABSOLUTE or IMAGE_REL_AMD64_ABSOLUTE
	//IMAGE_REL_BASED_HIGHLOW	=> ??? or IMAGE_REL_AMD64_ADDR32NB (32-bit address w/o image base (RVA))
	//IMAGE_REL_BASED_DIR64		=> IMAGE_REL_AMD64_SSPAN32 (32 bit signed span-dependent value applied at link time)
	WORD new_type = reverse ? (this->is64bit() ? IMAGE_REL_BASED_DIR64 : IMAGE_REL_BASED_HIGHLOW) : IMAGE_REL_BASED_ABSOLUTE;

	// Remove everything that is between start and end
	// We do a thorough search for possible relocations and do not assume that they are in order
	void *entry_end = data + size;
	for (IMAGE_BASE_RELOCATION *entry = (IMAGE_BASE_RELOCATION*)data; entry < entry_end && entry->SizeOfBlock > 0; entry = NEXT_RELOCS(entry)) {

		// Check that the ranges overlap
		if (entry->VirtualAddress+0xFFF < start || entry->VirtualAddress > end) continue;

		// Go through each reloc in this entry
		DWORD count = COUNT_RELOCS(entry);
		Reloc *relocs = RELOCS(entry);
		for (DWORD i = 0; i < count; ++i) {
			// Already 'removed'
			if ((!reverse && relocs[i].Type == IMAGE_REL_BASED_ABSOLUTE) ||
				(reverse && (relocs[i].Type != IMAGE_REL_BASED_ABSOLUTE || relocs[i].Offset == 0))) continue;

			// Check the virtual address and possibly clear it
			DWORD va = entry->VirtualAddress + relocs[i].Offset;
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
size_t PEFile::getSizeOf(DWORD cnt, int rsrcIndx, size_t rsrcRawSize) const {
	size_t size = 0;
	for (WORD i = 0; i < this->header->NumberOfSections; i++)
		if (this->sections[i].Characteristics & cnt)
			size += (i == (WORD)rsrcIndx) ? rsrcRawSize : this->sections[i].SizeOfRawData;
	return size;
}
inline static void adjustAddr(DWORD &addr, size_t rAddr, size_t rNewSize, size_t rOldSize) {
	if (addr != 0 && addr >= rAddr + rOldSize)
		addr = (DWORD)((addr + rNewSize) - rOldSize); // subtraction needs to be last b/c these are unsigned
}
bool PEFile::save() {
	if (this->readonly) { return false; }

	// Compile the .rsrc, get its size, and get all the information about it
	bool is64bit = this->is64bit();
	DWORD fAlign = is64bit ? this->getNtHeaders64()->OptionalHeader.FileAlignment    : this->getNtHeaders32()->OptionalHeader.FileAlignment;
	DWORD sAlign = is64bit ? this->getNtHeaders64()->OptionalHeader.SectionAlignment : this->getNtHeaders32()->OptionalHeader.SectionAlignment;
	int rIndx = 0;
	IMAGE_SECTION_HEADER *rSect = this->getSectionHeader(".rsrc", &rIndx);
	if (!rSect) {
		this->createSection(".rsrc", 0, CHARS_INIT_DATA_SECTION_R);
		rSect = this->getSectionHeader(".rsrc", &rIndx);
	}
	size_t rSize = 0;
	LPVOID rsrc = this->res->compile(&rSize, rSect->VirtualAddress);
	size_t rRawSize = roundUpTo(rSize, fAlign);
	size_t rVirSize = roundUpTo(rSize, sAlign);
	//size_t rSizeOld = rSect->Misc.VirtualSize;
	size_t rRawSizeOld = rSect->SizeOfRawData;
	size_t rVirSizeOld = roundUpTo(rSect->Misc.VirtualSize, sAlign);
	DWORD pntr = rSect->PointerToRawData;
	DWORD imageSize = 0; //, imageSizeOld = 0;
	DWORD fileSize = 0, fileSizeOld = (DWORD)this->size;

	// Update PointerToSymbolTable
	adjustAddr(this->header->PointerToSymbolTable, rSect->VirtualAddress, rVirSize, rVirSizeOld);

	// Update Optional Header
	if (is64bit) {
		this->nth64->OptionalHeader.SizeOfInitializedData = (DWORD)this->getSizeOf(IMAGE_SCN_CNT_INITIALIZED_DATA, rIndx, rRawSize);
		adjustAddr(this->nth64->OptionalHeader.AddressOfEntryPoint, rSect->VirtualAddress, rVirSize, rVirSizeOld);
		adjustAddr(this->nth64->OptionalHeader.BaseOfCode, rSect->VirtualAddress, rVirSize, rVirSizeOld);
		//imageSizeOld = this->nth64->OptionalHeader.SizeOfImage;
	} else {
		this->nth32->OptionalHeader.SizeOfInitializedData = (DWORD)this->getSizeOf(IMAGE_SCN_CNT_INITIALIZED_DATA, rIndx, rRawSize);
		adjustAddr(this->nth32->OptionalHeader.AddressOfEntryPoint, rSect->VirtualAddress, rVirSize, rVirSizeOld);
		adjustAddr(this->nth32->OptionalHeader.BaseOfCode, rSect->VirtualAddress, rVirSize, rVirSizeOld);
		adjustAddr(this->nth32->OptionalHeader.BaseOfData, rSect->VirtualAddress, rVirSize, rVirSizeOld);
		//imageSizeOld = this->nth32->OptionalHeader.SizeOfImage;
	}

	// Update the Data Directories
	this->dataDir[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size = (DWORD)rSize;
	DWORD ddCount = this->getDataDirectoryCount();
	for (DWORD i = 0; i < ddCount; i++) {
		if (i == IMAGE_DIRECTORY_ENTRY_SECURITY) { // the virtual address of IMAGE_DIRECTORY_ENTRY_SECURITY is actually a file address, not a virtual address
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
	for (WORD i = (WORD)rIndx; i < this->header->NumberOfSections; i++) {
		if (strncmp((CHAR*)this->sections[i].Name, ".rsrc", IMAGE_SIZEOF_SHORT_NAME) == 0) {
			this->sections[i].Misc.VirtualSize = (DWORD)rSize;
			this->sections[i].SizeOfRawData = (DWORD)rRawSize;
		} else {
			adjustAddr(this->sections[i].VirtualAddress, rSect->VirtualAddress, rVirSize, rVirSizeOld);
			adjustAddr(this->sections[i].PointerToRawData, rSect->PointerToRawData, rRawSize, rRawSizeOld);
		}
		adjustAddr(this->sections[i].PointerToLinenumbers, rSect->PointerToRawData, rRawSize, rRawSizeOld);
		adjustAddr(this->sections[i].PointerToRelocations, rSect->PointerToRawData, rRawSize, rRawSizeOld);
		if (this->sections[i].VirtualAddress + this->sections[i].Misc.VirtualSize > imageSize)
			imageSize = this->sections[i].VirtualAddress + this->sections[i].Misc.VirtualSize;
		if (this->sections[i].PointerToRawData + this->sections[i].SizeOfRawData > fileSize)
			fileSize = this->sections[i].PointerToRawData + this->sections[i].SizeOfRawData;
	}
	
	// Update the ImageSize
	imageSize = (DWORD)roundUpTo(imageSize, sAlign);
	if (is64bit)	this->nth64->OptionalHeader.SizeOfImage = imageSize;
	else			this->nth32->OptionalHeader.SizeOfImage = imageSize;

	// Increase file size (invalidates all local pointers to the file data)
	if (fileSize > fileSizeOld && !this->setSize(fileSize))			{ free(rsrc); return false; }

	// Move all sections after resources and save resources
	LPBYTE dp = this->data + pntr;
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
