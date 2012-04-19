#define _DECLARE_ALL_PE_FILE_RESOURCES_
#include "PEFileResources.h"

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

// Raised when resources have a problem loading
class ResLoadFailure {};
static ResLoadFailure resLoadFailure;

bool ResCmp::operator()(LPCWSTR a, LPCWSTR b) const { return IS_INTRESOURCE(a) ? (IS_INTRESOURCE(b) ? (RESID2WORD(a) < RESID2WORD(b)) : false) : (IS_INTRESOURCE(b) ? true : wcscmp(a, b) < 0); }
//int ResCmp::operator()(LPCWSTR a, LPCWSTR b) const { return (IS_INTRESOURCE(a) ? (IS_INTRESOURCE(b) ? ((WORD)a - (WORD)b) : 1) : (IS_INTRESOURCE(b) ? -1 : wcscmp(a, b)); }

inline static LPWSTR dup(LPCWSTR id) { return IS_INTRESOURCE(id) ? MAKEINTRESOURCE(RESID2WORD(id)) : _wcsdup(id); }
inline static void free_id(LPWSTR id) { if (!IS_INTRESOURCE(id)) free(id); }

#pragma region RSRC Utility Functions

static IMAGE_RESOURCE_DIRECTORY_ENTRY *GetEntries(const LPBYTE data, size_t size, size_t offset, DWORD *nEntries) {
	if (offset + sizeof(IMAGE_RESOURCE_DIRECTORY) >= size) { throw resLoadFailure; }
	IMAGE_RESOURCE_DIRECTORY dir = *(IMAGE_RESOURCE_DIRECTORY*)(data+offset);
	offset += sizeof(IMAGE_RESOURCE_DIRECTORY);
	*nEntries = dir.NumberOfIdEntries+dir.NumberOfNamedEntries;
	if (offset + *nEntries*sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY) >= size) { throw resLoadFailure; }
	return (IMAGE_RESOURCE_DIRECTORY_ENTRY*)(data+offset);
}
static LPWSTR GetResourceName(const LPBYTE data, size_t size, size_t offset, const IMAGE_RESOURCE_DIRECTORY_ENTRY & entry) {
	if (entry.NameIsString) {
		offset += entry.NameOffset;

		if (offset + sizeof(WORD) > size) { return NULL; }
		WORD len = *(WORD*)(data+offset);
		offset += sizeof(WORD);

		if (offset + sizeof(WCHAR)*len > size) { return NULL; }
		WCHAR *str = wcsncpy((WCHAR*)malloc((len+1)*sizeof(WCHAR)), (WCHAR*)(data+offset), len);
		str[len] = 0;

		return str;
	} else {
		return MAKEINTRESOURCE(entry.Id);
	}
}
static void WriteResDir(LPBYTE data, size_t& pos, WORD nNamed, WORD nId) {
	IMAGE_RESOURCE_DIRECTORY dir;
	dir.Characteristics = 0;
	dir.TimeDateStamp = 0;
	dir.MajorVersion = 4;
	dir.MinorVersion = 0;
	dir.NumberOfNamedEntries = nNamed;
	dir.NumberOfIdEntries = nId;
	memcpy(data+pos, &dir, sizeof(IMAGE_RESOURCE_DIRECTORY));
	pos += sizeof(IMAGE_RESOURCE_DIRECTORY);
}
template <class Iterator>
static void WriteResDir(LPBYTE data, size_t &pos, Iterator start, Iterator end) {
	WORD nNamed = 0, nId = 0;
	for (Iterator i = start; i != end; ++i) {
		if (IS_INTRESOURCE(i->first))
			nId += 1;
		else
			nNamed += 1;
	}
	WriteResDir(data, pos, nNamed, nId);
}
static void WriteResDirEntry(LPBYTE data, LPCWSTR name, size_t headerSize, size_t &pos, size_t &posDir, size_t &posData) {
	IMAGE_RESOURCE_DIRECTORY_ENTRY entry;
	entry.DataIsDirectory = TRUE;
	entry.OffsetToDirectory = posDir;
	posDir += headerSize;

	entry.NameIsString = !IS_INTRESOURCE(name);
	if (entry.NameIsString) {
		entry.NameOffset = posData;
		WORD len = (WORD)wcslen(name);
		memcpy(data+posData, &len, sizeof(WORD));
		memcpy(data+posData+sizeof(WORD), name, len*sizeof(WCHAR));
		posData += roundUpTo<4>(len*sizeof(WCHAR)+sizeof(WORD));
	} else {
		entry.Name = (DWORD)RESID2WORD(name);
	}

	memcpy(data+pos, &entry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
	pos += sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);
}
#pragma endregion

#pragma region RES Utility Functions

typedef struct _RESHEADER {
	DWORD DataSize;
	DWORD HeaderSize;
	LPCWSTR Type;
	LPCWSTR Name;
	DWORD DataVersion;
	WORD  MemoryFlags;
	WORD  LanguageId;
	DWORD Version;
	DWORD Characteristics;
} RESHEADER;

static const size_t RESHeaderSize = sizeof(DWORD)*7 + sizeof(WORD)*2;

static size_t ReadRESHeaderID(const LPBYTE data, LPCWSTR *id) {
	WCHAR* chars = (WCHAR*)data;
	size_t len;
	if (chars[0] == 0xFFFF) {
		len = 2 * sizeof(WCHAR);
		*id = MAKEINTRESOURCE(chars[1]);
	} else {
		len = (wcslen((LPCWSTR)chars) + 1) * sizeof(WCHAR);
		*id = (LPCWSTR)chars;
	}
	return len;
}
static RESHEADER* ReadRESHeader(const LPBYTE data, size_t size, size_t& pos) {
	if (pos + sizeof(DWORD)*2 >= size) { return NULL; }

	RESHEADER *h = (RESHEADER*)malloc(sizeof(RESHEADER));
	memcpy(h, data+pos, sizeof(DWORD)*2);

	if (pos + h->HeaderSize >= size) { free(h); return NULL; }

	pos += sizeof(DWORD)*2;
	pos += ReadRESHeaderID(data + pos, &h->Type);
	pos += ReadRESHeaderID(data + pos, &h->Name);

	memcpy(&h->DataVersion, data + pos, sizeof(RESHEADER) - offsetof(RESHEADER, DataVersion));
	pos += sizeof(RESHEADER) - offsetof(RESHEADER, DataVersion);

	return h;
}

static const WORD DefResMemoryFlags = 0x0030; // or possibly 0x0000 ?
static const WORD ResMemoryFlags[] = {
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

static size_t GetRESHeaderIDExtraLen(LPCWSTR id) { return IS_INTRESOURCE(id) ? 0 : (wcslen(id) - 1) * sizeof(WCHAR); }
static size_t WriteRESHeaderID(LPBYTE data, LPCWSTR id, DWORD *hdrSize) {
	if (IS_INTRESOURCE(id)) {
		*((DWORD*)data) = (((DWORD)RESID2WORD(id)) << 16) | 0xFFFF;
		return 0;
	} else {
		size_t len = (wcslen(id) + 1) * sizeof(WCHAR);
		memcpy(data, id, len);
		len -= sizeof(DWORD);
		*hdrSize += (DWORD)len;
		return len;
	}
}
static size_t WriteRESHeader(LPBYTE data, LPCWSTR type, LPCWSTR name, WORD lang, size_t dataSize) {
	*(DWORD*)data = (DWORD)dataSize;
	DWORD *hdrSize = ((DWORD*)data)+1;
	*hdrSize = (DWORD)RESHeaderSize;
	data += sizeof(DWORD)*2;
	data += WriteRESHeaderID(data, type, hdrSize) + sizeof(DWORD);
	data += WriteRESHeaderID(data, name, hdrSize) + sizeof(DWORD);
	memset(data, 0, sizeof(DWORD)*3 + sizeof(WORD)*2); // DataVersion, *, Version, Characteristics
	((WORD*)data)[2] = (IS_INTRESOURCE(type) && (RESID2WORD(type) < ARRAYSIZE(ResMemoryFlags))) ? ResMemoryFlags[RESID2WORD(type)] : DefResMemoryFlags; // MemoryFlags
	((WORD*)data)[3] = lang;
	return *hdrSize;
}
#pragma endregion

#pragma region Rsrc
///////////////////////////////////////////////////////////////////////////////
///// Rsrc
///////////////////////////////////////////////////////////////////////////////
Rsrc* Rsrc::createFromRSRCSection(const LPBYTE data, size_t size, IMAGE_SECTION_HEADER *section) { try { return (!data || !size || !section) ? NULL : new Rsrc(data, size, section); } catch (ResLoadFailure&) { return NULL; } }
Rsrc::Rsrc(const LPBYTE data, size_t size, IMAGE_SECTION_HEADER *section) {
	DWORD nEntries;
	IMAGE_RESOURCE_DIRECTORY_ENTRY *entries = GetEntries(data, size, section->PointerToRawData, &nEntries);
	for (WORD i = 0; i < nEntries; i++) {
		LPWSTR type = GetResourceName(data, size, section->PointerToRawData, entries[i]);
		//this->types.set(type), new ResourceType(type, data, size, section->PointerToRawData, section->VirtualAddress, entries[i]));
		this->types[type] = new ResourceType(type, data, size, section->PointerToRawData, section->VirtualAddress, entries[i]);
	}
	this->cleanup();
}
Rsrc* Rsrc::createFromRESFile(const LPBYTE data, size_t size) { try { return (!data || !size) ? NULL : new Rsrc(data, size); } catch (ResLoadFailure&) { return NULL; } }
Rsrc::Rsrc(const LPBYTE data, size_t size) {
	RESHEADER *h;
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
LPCWSTR Rsrc::getId() const { return NULL; }
bool Rsrc::cleanup() {
	for (TypeMap::iterator i = this->types.begin(); i != this->types.end(); ++i) {
		if (i->second->cleanup()) {
			free_id(i->first);
			delete i->second;
			i = this->types.erase(i);
		}
	}
	return this->isEmpty();
}
bool Rsrc::exists(LPCWSTR type, LPCWSTR name, WORD lang) const {
	TypeMap::const_iterator iter = this->types.find((const LPWSTR)type);
	return iter == this->types.end() ? false : iter->second->exists(name, lang);
}
bool Rsrc::exists(LPCWSTR type, LPCWSTR name, WORD *lang) const {
	TypeMap::const_iterator iter = this->types.find((const LPWSTR)type);
	return iter == this->types.end() ? false : iter->second->exists(name, lang);
}
LPVOID Rsrc::get(LPCWSTR type, LPCWSTR name, WORD lang, size_t *size) const {
	TypeMap::const_iterator iter = this->types.find((const LPWSTR)type);
	return iter == this->types.end() ? NULL : iter->second->get(name, lang, size);
}
LPVOID Rsrc::get(LPCWSTR type, LPCWSTR name, WORD *lang, size_t *size) const {
	TypeMap::const_iterator iter = this->types.find((const LPWSTR)type);
	return iter == this->types.end() ? NULL : iter->second->get(name, lang, size);
}
bool Rsrc::remove(LPCWSTR type, LPCWSTR name, WORD lang) {
	TypeMap::iterator iter = this->types.find((const LPWSTR)type);
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
bool Rsrc::add(LPCWSTR type, LPCWSTR name, WORD lang, const LPVOID data, size_t size, DWORD overwrite) {
	TypeMap::iterator iter = this->types.find((const LPWSTR)type);
	if (iter == types.end() && (overwrite == OVERWRITE_ALWAYS || overwrite == OVERWRITE_NEVER)) {
		//types.set(dup(type), new ResourceType(type, name, lang, data, size));
		types[dup(type)] = new ResourceType(type, name, lang, data, size);
		return true;
	}
	return iter->second->add(name, lang, data, size, overwrite);
}
bool Rsrc::isEmpty() const { return this->types.size() == 0; }
ResourceType* Rsrc::operator[](LPCWSTR type) {
	TypeMap::iterator iter = this->types.find((const LPWSTR)type);
	return iter == this->types.end() ? NULL : iter->second;
}
const ResourceType* Rsrc::operator[](LPCWSTR type) const {
	TypeMap::const_iterator iter = this->types.find((const LPWSTR)type);
	return iter == this->types.end() ? NULL : iter->second;
}
vector<LPCWSTR> Rsrc::getTypes() const {
	vector<LPCWSTR> v;
	v.reserve(this->types.size());
	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i)
		v.push_back(i->first);
	return v;
}
vector<LPCWSTR> Rsrc::getNames(LPCWSTR type) const {
	TypeMap::const_iterator iter = this->types.find((const LPWSTR)type);
	return iter == types.end() ? vector<LPCWSTR>() : iter->second->getNames();
}
vector<WORD> Rsrc::getLangs(LPCWSTR type, LPCWSTR name) const {
	TypeMap::const_iterator iter = this->types.find((const LPWSTR)type);
	return iter == this->types.end() ? vector<WORD>() : iter->second->getLangs(name);
}
size_t Rsrc::getDataSize() const {
	size_t size = 0;
	for (TypeMap::const_iterator i = this->types.begin(); i != this->types.end(); ++i) {
		if (!IS_INTRESOURCE(i->first))
			size += roundUpTo<4>(sizeof(WORD)+wcslen(i->first)*sizeof(WCHAR));
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
size_t Rsrc::getThisHeaderSize() const { return sizeof(IMAGE_RESOURCE_DIRECTORY)+this->types.size()*sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY); }
LPVOID Rsrc::compile(size_t *size, DWORD startVA) {
	this->cleanup();

	size_t dataSize = this->getDataSize();
	size_t headerSize = roundUpTo<4>(this->getHeaderSize()); // DWORD alignment
	
	*size = headerSize + dataSize;

	LPBYTE data = (LPBYTE)memset(malloc(*size), 0, *size);
	
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
LPVOID Rsrc::compileRES(size_t *size) {
	this->cleanup();

	*size = this->getRESSize();
	LPBYTE data = (LPBYTE)memset(malloc(*size), 0, *size);
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
ResourceType::ResourceType(LPCWSTR type, const LPBYTE data, size_t size, DWORD start, DWORD startVA, IMAGE_RESOURCE_DIRECTORY_ENTRY entry) : type(dup(type)) {
	DWORD nEntries;
	IMAGE_RESOURCE_DIRECTORY_ENTRY *entries = GetEntries(data, size, start+entry.OffsetToDirectory, &nEntries);
	for (WORD i = 0; i < nEntries; i++) {
		LPWSTR name = GetResourceName(data, size, start, entries[i]);
		//this->names.set(name, new ResourceName(name, data, size, start, startVA, entries[i]));
		this->names[name] = new ResourceName(name, data, size, start, startVA, entries[i]);
	}
}
ResourceType::ResourceType(LPCWSTR type, LPCWSTR name, WORD lang, const LPVOID data, size_t size) : type(dup(type)) {
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
LPCWSTR ResourceType::getId() const { return this->type; }
bool ResourceType::cleanup() {
	for (NameMap::iterator i = this->names.begin(); i != this->names.end(); ++i) {
		if (i->second->cleanup()) {
			free_id(i->first);
			delete i->second;
			i = this->names.erase(i);
		}
	}
	return this->isEmpty();
}
bool ResourceType::exists(LPCWSTR name, WORD lang) const {
	NameMap::const_iterator iter = this->names.find((const LPWSTR)name);
	return iter == this->names.end() ? false : iter->second->exists(lang);
}
bool ResourceType::exists(LPCWSTR name, WORD *lang) const {
	NameMap::const_iterator iter = this->names.find((const LPWSTR)name);
	return iter == this->names.end() ? false : iter->second->exists(lang);
}
LPVOID ResourceType::get(LPCWSTR name, WORD lang, size_t *size) const {
	NameMap::const_iterator iter = this->names.find((const LPWSTR)name);
	return iter == this->names.end() ? NULL : iter->second->get(lang, size);
}
LPVOID ResourceType::get(LPCWSTR name, WORD *lang, size_t *size) const {
	NameMap::const_iterator iter = this->names.find((const LPWSTR)name);
	return iter == this->names.end() ? NULL : iter->second->get(lang, size);
}
bool ResourceType::remove(LPCWSTR name, WORD lang) {
	NameMap::iterator iter = this->names.find((const LPWSTR)name);
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
bool ResourceType::add(LPCWSTR name, WORD lang, const LPVOID data, size_t size, DWORD overwrite) {
	NameMap::iterator iter = this->names.find((const LPWSTR)name);
	if (iter == this->names.end() && (overwrite == OVERWRITE_ALWAYS || overwrite == OVERWRITE_NEVER)) {
		//this->names.set(dup(name), new ResourceName(name, lang, data, size));
		this->names[dup(name)] = new ResourceName(name, lang, data, size);
		return true;
	}
	return iter->second->add(lang, data, size, overwrite);
}
bool ResourceType::isEmpty() const { return this->names.empty(); }
ResourceName* ResourceType::operator[](LPCWSTR name) {
	NameMap::iterator iter = this->names.find((const LPWSTR)name);
	return iter == this->names.end() ? NULL : iter->second;
}
const ResourceName* ResourceType::operator[](LPCWSTR name) const {
	NameMap::const_iterator iter = this->names.find((const LPWSTR)name);
	return iter == this->names.end() ? NULL : iter->second;
}
vector<LPCWSTR> ResourceType::getNames() const {
	vector<LPCWSTR> v;
	v.reserve(this->names.size());
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		v.push_back(i->first);
	return v;
}
vector<WORD> ResourceType::getLangs(LPCWSTR name) const {
	NameMap::const_iterator iter = this->names.find((const LPWSTR)name);
	return iter == this->names.end() ? vector<WORD>() : iter->second->getLangs();
}
size_t ResourceType::getDataSize() const {
	size_t size = 0;
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i) {
		if (!IS_INTRESOURCE(i->first))
			size += roundUpTo<4>(sizeof(WORD)+wcslen(i->first)*sizeof(WCHAR));
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
size_t ResourceType::getThisHeaderSize() const { return sizeof(IMAGE_RESOURCE_DIRECTORY)+this->names.size()*sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY); }
void ResourceType::writeNameDirs(LPBYTE data, size_t& pos, size_t& posDir, size_t& posData) const {
	WriteResDir(data, pos, this->names.begin(), this->names.end());
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		WriteResDirEntry(data, i->first, i->second->getThisHeaderSize(), pos, posDir, posData);
}
void ResourceType::writeLangDirs(LPBYTE data, size_t& pos, size_t& posDir) const {
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		i->second->writeLangDirs(data, pos, posDir);
}
void ResourceType::writeData(LPBYTE data, size_t& posDataEntry, size_t& posData, DWORD startVA) const {
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		i->second->writeData(data, posDataEntry, posData, startVA);
}
size_t ResourceType::getRESSize() const {
	size_t xlen = GetRESHeaderIDExtraLen(this->type), size = 0;
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		size += i->second->getRESSize(xlen);
	return size;
}
void ResourceType::writeRESData(LPBYTE data, size_t& pos) const {
	for (NameMap::const_iterator i = this->names.begin(); i != this->names.end(); ++i)
		i->second->writeRESData(data, pos, this->type);
}
#pragma endregion

#pragma region ResourceName
///////////////////////////////////////////////////////////////////////////////
///// ResourceName
///////////////////////////////////////////////////////////////////////////////
ResourceName::ResourceName(LPCWSTR name, const LPBYTE data, size_t size, DWORD start, DWORD startVA, IMAGE_RESOURCE_DIRECTORY_ENTRY entry) : name(dup(name)) {
	DWORD nEntries;
	IMAGE_RESOURCE_DIRECTORY_ENTRY *entries = GetEntries(data, size, start+entry.OffsetToDirectory, &nEntries);
	for (WORD i = 0; i < nEntries; i++)
		//this->langs.set(entries[i].Id, new ResourceLang(entries[i].Id, data, size, start, startVA, entries[i]));
		this->langs[entries[i].Id] = new ResourceLang(entries[i].Id, data, size, start, startVA, entries[i]);
}
ResourceName::ResourceName(LPCWSTR name, WORD lang, const LPVOID data, size_t size) : name(dup(name)) {
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
LPCWSTR ResourceName::getId() const { return this->name; }
bool ResourceName::cleanup() {
	for (LangMap::iterator i = this->langs.begin(); i != this->langs.end(); ++i) {
		if (i->second->getDataSize() == 0) {
			delete i->second;
			i = this->langs.erase(i);
		}
	}
	return this->isEmpty();
}
bool ResourceName::exists(WORD lang) const { return this->langs.find(lang) != this->langs.end(); }
bool ResourceName::exists(WORD *lang) const {
	if (this->langs.size() > 0) {
		*lang = this->langs.begin()->first;
		return true;
	}
	return false;
}
LPVOID ResourceName::get(WORD lang, size_t *size) const {
	LangMap::const_iterator iter = this->langs.find(lang);
	return iter == this->langs.end() ? NULL : iter->second->get(size);
}
LPVOID ResourceName::get(WORD *lang, size_t *size) const {
	if (this->langs.size() > 0) {
		LangMap::const_iterator iter = this->langs.begin();
		*lang = iter->first;
		return iter->second->get(size);
	}
	return NULL;
}
bool ResourceName::remove(WORD lang) {
	LangMap::iterator iter = this->langs.find(lang);
	if (iter == this->langs.end())
		return false;
	delete iter->second;
	this->langs.erase(iter);
	return true;
}
bool ResourceName::add(WORD lang, const LPVOID data, size_t size, DWORD overwrite) {
	LangMap::iterator iter = this->langs.find(lang);
	if (iter == this->langs.end() && (overwrite == OVERWRITE_ALWAYS || overwrite == OVERWRITE_NEVER)) {
		//this->langs.set(lang, new ResourceLang(lang, data, size));
		this->langs[lang] = new ResourceLang(lang, data, size);
		return true;
	} else if (overwrite == OVERWRITE_ALWAYS || overwrite == OVERWRITE_ONLY) {
		return iter->second->set(data, size);
	}
	return false;
}
bool ResourceName::isEmpty() const { return this->langs.empty(); }
ResourceLang* ResourceName::operator[](WORD lang) {
	LangMap::iterator iter = this->langs.find(lang);
	return iter == this->langs.end() ? NULL : iter->second;
}
const ResourceLang* ResourceName::operator[](WORD lang) const {
	LangMap::const_iterator iter = this->langs.find(lang);
	return iter == this->langs.end() ? NULL : iter->second;
}
vector<WORD> ResourceName::getLangs() const {
	vector<WORD> v;
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
size_t ResourceName::getThisHeaderSize() const { return sizeof(IMAGE_RESOURCE_DIRECTORY)+this->langs.size()*sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY); }
void ResourceName::writeLangDirs(LPBYTE data, size_t& pos, size_t& posDir) const {
	WriteResDir(data, pos, 0, (WORD)this->langs.size());
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i) {
		IMAGE_RESOURCE_DIRECTORY_ENTRY entry;
		entry.DataIsDirectory = FALSE;
		entry.OffsetToDirectory = posDir;
		entry.NameIsString = FALSE;
		entry.Name = i->first;

		memcpy(data+pos, &entry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));

		posDir += i->second->getThisHeaderSize();
		pos += sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);
	}
}
void ResourceName::writeData(LPBYTE data, size_t& posDataEntry, size_t& posData, DWORD startVA) const {
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i)
		i->second->writeData(data, posDataEntry, posData, startVA);
}
size_t ResourceName::getRESSize(size_t addl_hdr_size) const {
	size_t xlen = addl_hdr_size + GetRESHeaderIDExtraLen(this->name), size = 0;
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i)
		size += i->second->getRESSize(xlen);
	return size;
}
void ResourceName::writeRESData(LPBYTE data, size_t& pos, LPCWSTR type) const {
	for (LangMap::const_iterator i = this->langs.begin(); i != this->langs.end(); ++i)
		i->second->writeRESData(data, pos, type, this->name);
}

#pragma endregion

#pragma region ResourceLang
///////////////////////////////////////////////////////////////////////////////
///// ResourceLang
///////////////////////////////////////////////////////////////////////////////
ResourceLang::ResourceLang(WORD lang, const LPBYTE data, size_t size, DWORD start, DWORD startVA, IMAGE_RESOURCE_DIRECTORY_ENTRY entry) : lang(lang) {
	if (start+entry.OffsetToData+sizeof(IMAGE_RESOURCE_DATA_ENTRY) > size) { throw resLoadFailure; }
	IMAGE_RESOURCE_DATA_ENTRY de = *(IMAGE_RESOURCE_DATA_ENTRY*)(data+start+entry.OffsetToData);
	if (start+de.OffsetToData-startVA+de.Size > size) { throw resLoadFailure; }
	this->data = malloc(this->length = de.Size);
	if (this->length == 0) { return; }
	memcpy(this->data, data+start+de.OffsetToData-startVA, this->length);
}
ResourceLang::ResourceLang(WORD lang, const LPVOID data, size_t size) : lang(lang), length(size) {
	this->data = memcpy(malloc(size), data, length);
}
ResourceLang::~ResourceLang() { free(this->data); }
LPCWSTR ResourceLang::getId() const { return MAKEINTRESOURCE(this->lang); }
LPVOID ResourceLang::get(size_t *size) const { return memcpy(malloc(this->length), this->data, *size = this->length); }
bool ResourceLang::set(const LPVOID data, size_t size) {
	if (this->length != size)
	{
		free(this->data);
		this->data = malloc(this->length = size);
	}
	memcpy(this->data, data, size);
	return true;
}
size_t ResourceLang::getDataSize() const		{ return this->length; }
size_t ResourceLang::getHeaderSize() const		{ return sizeof(IMAGE_RESOURCE_DATA_ENTRY); }
size_t ResourceLang::getThisHeaderSize() const	{ return sizeof(IMAGE_RESOURCE_DATA_ENTRY); }
void ResourceLang::writeData(LPBYTE data, size_t& posDataEntry, size_t& posData, size_t startVA) const {
	IMAGE_RESOURCE_DATA_ENTRY de = {(DWORD)(posData+startVA), (DWORD)this->length, 0, 0}; // needs to be an RVA
	memcpy(data+posDataEntry, &de, sizeof(IMAGE_RESOURCE_DATA_ENTRY));
	posDataEntry += sizeof(IMAGE_RESOURCE_DATA_ENTRY);
	memcpy(data+posData, this->data, this->length);
	posData += roundUpTo<4>(this->length);
}
size_t ResourceLang::getRESSize(size_t addl_hdr_size) const { return roundUpTo<4>(this->length + RESHeaderSize + addl_hdr_size); }
void ResourceLang::writeRESData(LPBYTE data, size_t& pos, LPCWSTR type, LPCWSTR name) const {
	pos += WriteRESHeader(data+pos, type, name, this->lang, this->length);
	memcpy(data+pos, this->data, this->length);
	pos = roundUpTo<4>(pos + this->length);
}
#pragma endregion
