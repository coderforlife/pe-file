#pragma once

#ifndef OVERWRITE_ALWAYS
#define OVERWRITE_ALWAYS	0 //always adds the resource, even if it already exists
#define OVERWRITE_NEVER		1 //only adds a resource is it does not already exist
#define OVERWRITE_ONLY		2 //only adds a resource if it will overwrite another resource
#endif

#ifdef EXPOSE_DIRECT_RESOURCES
#define _DECLARE_ALL_PE_FILE_RESOURCES_
#endif

#ifndef _DECLARE_ALL_PE_FILE_RESOURCES_

class Rsrc;

#else

#include "umap.h"
#include "uvector.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Functions / Macros used by both PEFile and PEFileResources
#define RESID2WORD(ID) ((WORD)((ULONG_PTR)(ID)))
template<unsigned int MULT> inline static size_t roundUpTo(size_t x) { size_t mod = x % MULT; return (mod == 0) ? x : (x + MULT - mod); }
template<> inline size_t roundUpTo<2>(size_t x) { return (x + 1) & ~0x1; }
template<> inline size_t roundUpTo<4>(size_t x) { return (x + 3) & ~0x3; }
inline static size_t roundUpTo(size_t x, size_t mult) { size_t mod = x % mult; return (mod == 0) ? x : (x + mult - mod); }

// A comparator for resource names
struct ResCmp { bool operator()(LPCWSTR a, LPCWSTR b) const; };

// A resource (directory) entry
class Resource {
public:
	virtual LPCWSTR getId() const = 0;
private:
	virtual size_t getDataSize() const = 0;
	virtual size_t getHeaderSize() const = 0;
	virtual size_t getThisHeaderSize() const = 0;
};

// Forward Declarations
//class ResourceLang;
//class ResourceName;
//class ResourceType;
//class Rsrc;

// The final resource directory, contains the data for the resource
class ResourceLang : Resource {
	friend class ResourceName;

	WORD lang;
	LPVOID data;
	size_t length;

	ResourceLang(WORD lang, const LPBYTE data, size_t size, DWORD start, DWORD startVA, IMAGE_RESOURCE_DIRECTORY_ENTRY entry);
	ResourceLang(WORD lang, const LPVOID data, size_t size);
public:
	~ResourceLang();

	LPCWSTR getId() const;

	bool isLoaded() const;
	LPVOID get(size_t* size) const; // must be freed
	bool set(const LPVOID data, size_t size);

private:
	virtual size_t getDataSize() const;
	virtual size_t getHeaderSize() const;
	virtual size_t getThisHeaderSize() const;
	void writeData(LPBYTE data, size_t& posDataEntry, size_t& posData, size_t startVA) const;

	virtual size_t getRESSize(size_t addl_hdr_size) const;
	void writeRESData(LPBYTE data, size_t& pos, LPCWSTR type, LPCWSTR name) const;
};

typedef map<WORD, ResourceLang*> LangMap;

// The named resource directory, the second level
class ResourceName : Resource {
	friend class ResourceType;

	LPWSTR name;
	LangMap langs;

	ResourceName(LPCWSTR name, const LPBYTE data, size_t size, DWORD start, DWORD startVA, IMAGE_RESOURCE_DIRECTORY_ENTRY entry);
	ResourceName(LPCWSTR name, WORD lang, const LPVOID data, size_t size);
public:
	~ResourceName();

	LPCWSTR getId() const;

	bool exists(WORD lang) const;
	bool exists(WORD* lang) const;
	LPVOID get(WORD lang, size_t* size) const;
	LPVOID get(WORD* lang, size_t* size) const;
	bool remove(WORD lang);
	bool add(WORD lang, const LPVOID data, size_t size, DWORD overwrite = OVERWRITE_ALWAYS);

	bool isEmpty() const;
	
	ResourceLang* operator[](WORD lang);
	const ResourceLang* operator[](WORD lang) const;

	vector<WORD> getLangs() const;

private:
	bool cleanup();
	virtual size_t getDataSize() const;
	virtual size_t getHeaderSize() const;
	virtual size_t getThisHeaderSize() const;
	void writeLangDirs(LPBYTE data, size_t& pos, size_t& posDir) const;
	void writeData(LPBYTE data, size_t& posDataEntry, size_t& posData, DWORD startVA) const;

	virtual size_t getRESSize(size_t addl_hdr_size) const;
	void writeRESData(LPBYTE data, size_t& pos, LPCWSTR type) const;
};

typedef map<LPWSTR, ResourceName*, ResCmp> NameMap;

// The typed resource directory, the first level
class ResourceType : Resource {
	friend class Rsrc;

	LPWSTR type;
	NameMap names;

	ResourceType(LPCWSTR type, const LPBYTE data, size_t size, DWORD start, DWORD startVA, IMAGE_RESOURCE_DIRECTORY_ENTRY entry);
	ResourceType(LPCWSTR type, LPCWSTR name, WORD lang, const LPVOID data, size_t size);
public:
	~ResourceType();

	LPCWSTR getId() const;

	bool exists(LPCWSTR name, WORD lang) const;
	bool exists(LPCWSTR name, WORD* lang) const;
	LPVOID get (LPCWSTR name, WORD lang, size_t* size) const;
	LPVOID get (LPCWSTR name, WORD* lang, size_t* size) const;
	bool remove(LPCWSTR name, WORD lang);
	bool add(LPCWSTR name, WORD lang, const LPVOID data, size_t size, DWORD overwrite = OVERWRITE_ALWAYS);

	bool isEmpty() const;
	
	ResourceName* operator[](LPCWSTR name);
	const ResourceName* operator[](LPCWSTR name) const;

	vector<LPCWSTR> getNames() const;
	vector<WORD> getLangs(LPCWSTR name) const;

private:
	bool cleanup();
	virtual size_t getDataSize() const;
	virtual size_t getHeaderSize() const;
	virtual size_t getThisHeaderSize() const;
	void writeNameDirs(LPBYTE data, size_t& pos, size_t& posDir, size_t& posData) const;
	void writeLangDirs(LPBYTE data, size_t& pos, size_t& posDir) const;
	void writeData(LPBYTE data, size_t& posDataEntry, size_t& posData, DWORD startVA) const;

	virtual size_t getRESSize() const;
	void writeRESData(LPBYTE data, size_t& pos) const;
};

typedef map<LPWSTR, ResourceType*, ResCmp> TypeMap;

class Rsrc : Resource {
	TypeMap types;

	Rsrc(const LPBYTE data, size_t size, IMAGE_SECTION_HEADER *section); // creates from ".rsrc" section in PE file
	Rsrc(const LPBYTE data, size_t size); // creates from RES file
	Rsrc(); // creates empty
public:
	~Rsrc();
	
	static Rsrc* createFromRSRCSection(const LPBYTE data, size_t size, IMAGE_SECTION_HEADER *section);
	static Rsrc* createFromRESFile(const LPBYTE data, size_t size);
	static Rsrc* createEmpty();

	LPCWSTR getId() const;

	bool exists(LPCWSTR type, LPCWSTR name, WORD lang) const;
	bool exists(LPCWSTR type, LPCWSTR name, WORD* lang) const;
	LPVOID get (LPCWSTR type, LPCWSTR name, WORD lang, size_t* size) const;
	LPVOID get (LPCWSTR type, LPCWSTR name, WORD* lang, size_t* size) const;
	bool remove(LPCWSTR type, LPCWSTR name, WORD lang);
	bool add(LPCWSTR type, LPCWSTR name, WORD lang, const LPVOID data, size_t size, DWORD overwrite = OVERWRITE_ALWAYS);
	
	bool isEmpty() const;
	
	ResourceType* operator[](LPCWSTR type);
	const ResourceType* operator[](LPCWSTR type) const;

	vector<LPCWSTR> getTypes() const;
	vector<LPCWSTR> getNames(LPCWSTR type) const;
	vector<WORD> getLangs(LPCWSTR type, LPCWSTR name) const;

	bool cleanup();
	LPVOID compile(size_t* size, DWORD startVA); // calls cleanup
	LPVOID compileRES(size_t* size); // calls cleanup

private:
	virtual size_t getDataSize() const;
	virtual size_t getHeaderSize() const;
	virtual size_t getThisHeaderSize() const;
	virtual size_t getRESSize() const;
};

#endif
