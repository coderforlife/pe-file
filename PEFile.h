#pragma once

#define EXPOSE_DIRECT_RESOURCES

#include "PEFileResources.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define LARGE_PATH 32767

class PEFile {
protected:
	IMAGE_DOS_HEADER *dosh;
	LONG peOffset;
	IMAGE_NT_HEADERS32 *nth32;
	IMAGE_NT_HEADERS64 *nth64;
	IMAGE_FILE_HEADER *header;		// part of nth32/nth64 header
	IMAGE_DATA_DIRECTORY *dataDir;	// part of nth32/nth64 header
	IMAGE_SECTION_HEADER *sections;
	Rsrc *res;
	
	bool readonly;
	WCHAR original[LARGE_PATH];
	HANDLE hFile, hMap;
	size_t size;
	LPBYTE orig_data, data;

	ULONGLONG version;
	bool modified;

	size_t getSizeOf(DWORD cnt, int rsrcIndx, size_t rsrcRawSize) const;

	bool map();
	void unmap();
	
	bool usesMemoryMappedFile() const;

	bool load(bool incRes);
	void unload();
public:
	PEFile(LPVOID data, size_t size, bool readonly = false); // data is freed when the PEFile is deleted
	PEFile(LPCWSTR filename, bool readonly = false);
	~PEFile();
	bool isLoaded() const;
	bool isReadOnly() const;

	bool save(); // flushes

	bool is32bit() const;
	bool is64bit() const;
	ULONGLONG getImageBase() const;

	IMAGE_FILE_HEADER *getFileHeader();				// pointer can modify the file
	IMAGE_NT_HEADERS32 *getNtHeaders32();			// pointer can modify the file
	IMAGE_NT_HEADERS64 *getNtHeaders64();			// pointer can modify the file
	const IMAGE_FILE_HEADER *getFileHeader() const;
	const IMAGE_NT_HEADERS32 *getNtHeaders32() const;
	const IMAGE_NT_HEADERS64 *getNtHeaders64() const;

	DWORD getDataDirectoryCount() const;
	IMAGE_DATA_DIRECTORY *getDataDirectory(int i);	// pointer can modify the file
	const IMAGE_DATA_DIRECTORY *getDataDirectory(int i) const;

	IMAGE_SECTION_HEADER *getSectionHeader(int i);							// pointer can modify the file
	IMAGE_SECTION_HEADER *getSectionHeader(const char *str, int *i = NULL);	// pointer can modify the file
	IMAGE_SECTION_HEADER *getSectionHeaderByRVA(DWORD rva, int *i);			// pointer can modify the file
	IMAGE_SECTION_HEADER *getSectionHeaderByVA(ULONGLONG va, int *i);		// pointer can modify the file
	const IMAGE_SECTION_HEADER *getSectionHeader(int i) const;
	const IMAGE_SECTION_HEADER *getSectionHeader(const char *str, int *i = NULL) const;
	const IMAGE_SECTION_HEADER *getSectionHeaderByRVA(DWORD rva, int *i) const;
	const IMAGE_SECTION_HEADER *getSectionHeaderByVA(ULONGLONG va, int *i) const;
	int getSectionHeaderCount() const;

	IMAGE_SECTION_HEADER *getExpandedSectionHdr(int i, DWORD room);		// pointer can modify the file, invalidates all pointers returned by functions, flushes
	IMAGE_SECTION_HEADER *getExpandedSectionHdr(char *str, DWORD room);	// as above

#define CHARS_CODE_SECTION			IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ
#define CHARS_INIT_DATA_SECTION_R	IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ
#define CHARS_INIT_DATA_SECTION_RW	IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE

	IMAGE_SECTION_HEADER *createSection(int i, const char *name, DWORD room, DWORD chars);				// pointer can modify the file, invalidates all pointers returned by functions, flushes
	IMAGE_SECTION_HEADER *createSection(const char *str, const char *name, DWORD room, DWORD chars);	// as above, adds before the section named str
	IMAGE_SECTION_HEADER *createSection(const char *name, DWORD room, DWORD chars);						// as above, adds before ".reloc" if exists or at the very end

	size_t getSize() const;
	bool setSize(size_t dwSize, bool grow_only = true);				// invalidates all pointers returned by functions, flushes

	LPBYTE get(DWORD dwOffset = 0, DWORD *dwSize = NULL);			// pointer can modify the file
	const LPBYTE get(DWORD dwOffset = 0, DWORD *dwSize = NULL) const;
	bool set(const LPVOID lpBuffer, DWORD dwSize, DWORD dwOffset);	// shorthand for memcpy(f->get(dwOffset), lpBuffer, dwSize) with bounds checking
	bool zero(DWORD dwSize, DWORD dwOffset);						// shorthand for memset(f->get(dwOffset), 0, dwSize) with bounds checking
	bool move(DWORD dwOffset, DWORD dwSize, int dwDistanceToMove);	// shorthand for x = f->get(dwOffset); memmove(x+dwDistanceToMove, x, dwSize) with bounds checking
	bool shift(DWORD dwOffset, int dwDistanceToMove);				// shorthand for f->move(dwOffset, f->getSize() - dwOffset - dwDistanceToMove, dwDistanceToMove)
	bool flush();

	bool updatePEChkSum();				// flushes
	bool hasExtraData() const;
	LPVOID getExtraData(DWORD *size);	// pointer can modify the file, when first enabling it will flush
	bool clearCertificateTable();		// may invalidate all pointers returned by functions, flushes
	ULONGLONG getFileVersion() const;
	bool isAlreadyModified() const;
	bool setModifiedFlag();				// flushes
	bool removeRelocs(DWORD start, DWORD end, bool reverse = false);

#ifdef EXPOSE_DIRECT_RESOURCES
	Rsrc *getResources();
	const Rsrc *getResources() const;
#endif
	bool resourceExists(LPCWSTR type, LPCWSTR name, WORD lang) const;
	bool resourceExists(LPCWSTR type, LPCWSTR name, WORD* lang) const;
	LPVOID getResource (LPCWSTR type, LPCWSTR name, WORD lang, size_t* size) const;	// must be freed
	LPVOID getResource (LPCWSTR type, LPCWSTR name, WORD* lang, size_t* size) const;	// must be freed
	bool removeResource(LPCWSTR type, LPCWSTR name, WORD lang);
	bool addResource   (LPCWSTR type, LPCWSTR name, WORD lang, const LPVOID data, size_t size, DWORD overwrite = OVERWRITE_ALWAYS);
	
	static const LPVOID GetResourceDirect(const LPVOID data, LPCWSTR type, LPCWSTR name); // must be freed, massively performance enhanced for a single retrieval and no editing // lang? size?
	static bool UpdatePEChkSum(LPBYTE data, size_t dwSize, size_t peOffset, DWORD dwOldCheck);
	static bool GetVersionInfo(const LPVOID ver, LPCWSTR query, LPVOID *buffer, PUINT len);
	static VS_FIXEDFILEINFO *GetVersionInfo(const LPVOID ver);
	static void UnmapAllViewsOfFile(LPCWSTR file);
};
