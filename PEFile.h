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

// Implements the classes and functions for dealing with general features of PE files

#ifndef PE_FILE_H
#define PE_FILE_H

#define EXPOSE_DIRECT_RESOURCES

#include "PEDataTypes.h"
#include "PEFileResources.h"
#include "PEDataSource.h"
#include "PEVersion.h"

namespace PE {

class File {
protected:
	PE::DataSource data;

	Image::DOSHeader *dosh;
	long peOffset;
	Image::NTHeaders32 *nth32;
	Image::NTHeaders64 *nth64;
	Image::FileHeader *header;		// part of nth32/nth64 header
	Image::OptionalHeader *opt;		// part of nth32/nth64 header
	Image::DataDirectory *dataDir;	// part of nth32/nth64 header
	Image::SectionHeader *sections;
	Rsrc *res;

	PE::Version::Version version;
	bool modified;

	size_t getSizeOf(uint32_t cnt, int rsrcIndx, size_t rsrcRawSize) const;

	bool load(bool incRes);
	void unload();
public:
	File(pntr data, size_t size, bool readonly = false); // data is freed when the PEFile is deleted
	File(const_str filename, bool readonly = false);
	~File();
	bool isLoaded() const;
	bool isReadOnly() const;

	bool save(); // flushes

	bool is32bit() const;
	bool is64bit() const;
	uint64_t getImageBase() const;

	Image::FileHeader *getFileHeader();				// pointer can modify the file
	Image::NTHeaders32 *getNtHeaders32();			// pointer can modify the file
	Image::NTHeaders64 *getNtHeaders64();			// pointer can modify the file
	const Image::FileHeader *getFileHeader() const;
	const Image::NTHeaders32 *getNtHeaders32() const;
	const Image::NTHeaders64 *getNtHeaders64() const;

	uint32_t getDataDirectoryCount() const;
	Image::DataDirectory *getDataDirectory(int i);	// pointer can modify the file
	const Image::DataDirectory *getDataDirectory(int i) const;

	Image::SectionHeader *getSectionHeader(int i);							// pointer can modify the file
	Image::SectionHeader *getSectionHeader(const char *str, int *i = NULL);	// pointer can modify the file
	Image::SectionHeader *getSectionHeaderByRVA(uint32_t rva, int *i);		// pointer can modify the file
	Image::SectionHeader *getSectionHeaderByVA(uint64_t va, int *i);		// pointer can modify the file
	const Image::SectionHeader *getSectionHeader(int i) const;
	const Image::SectionHeader *getSectionHeader(const char *str, int *i = NULL) const;
	const Image::SectionHeader *getSectionHeaderByRVA(uint32_t rva, int *i) const;
	const Image::SectionHeader *getSectionHeaderByVA(uint64_t va, int *i) const;
	int getSectionHeaderCount() const;

	Image::SectionHeader *getExpandedSectionHdr(int i, uint32_t room);		// pointer can modify the file, invalidates all pointers returned by functions, flushes
	Image::SectionHeader *getExpandedSectionHdr(char *str, uint32_t room);	// as above

	static const Image::SectionHeader::CharacteristicFlags CHARS_CODE_SECTION   = (Image::SectionHeader::CharacteristicFlags)(Image::SectionHeader::CNT_CODE | Image::SectionHeader::MEM_EXECUTE | Image::SectionHeader::MEM_READ);
	static const Image::SectionHeader::CharacteristicFlags INIT_DATA_SECTION_R  = (Image::SectionHeader::CharacteristicFlags)(Image::SectionHeader::CNT_INITIALIZED_DATA | Image::SectionHeader::MEM_READ);
	static const Image::SectionHeader::CharacteristicFlags INIT_DATA_SECTION_RW = (Image::SectionHeader::CharacteristicFlags)(Image::SectionHeader::CNT_INITIALIZED_DATA | Image::SectionHeader::MEM_READ | Image::SectionHeader::MEM_WRITE);

	Image::SectionHeader *createSection(int i, const char *name, uint32_t room, Image::SectionHeader::CharacteristicFlags chars);			// pointer can modify the file, invalidates all pointers returned by functions, flushes
	Image::SectionHeader *createSection(const char *str, const char *name, uint32_t room, Image::SectionHeader::CharacteristicFlags chars);	// as above, adds before the section named str
	Image::SectionHeader *createSection(const char *name, uint32_t room, Image::SectionHeader::CharacteristicFlags chars);					// as above, adds before ".reloc" if exists or at the very end

	size_t getSize() const;
	bool setSize(size_t dwSize, bool grow_only = true);				// invalidates all pointers returned by functions, flushes

	bytes get(uint32_t dwOffset = 0, uint32_t *dwSize = NULL);				// pointer can modify the file
	const_bytes get(uint32_t dwOffset = 0, uint32_t *dwSize = NULL) const;
	bool set(const_pntr lpBuffer, uint32_t dwSize, uint32_t dwOffset);		// shorthand for memcpy(f->get(dwOffset), lpBuffer, dwSize) with bounds checking
	bool zero(uint32_t dwSize, uint32_t dwOffset);							// shorthand for memset(f->get(dwOffset), 0, dwSize) with bounds checking
	bool move(uint32_t dwOffset, uint32_t dwSize, int32_t dwDistanceToMove);// shorthand for x = f->get(dwOffset); memmove(x+dwDistanceToMove, x, dwSize) with bounds checking
	bool shift(uint32_t dwOffset, int32_t dwDistanceToMove);				// shorthand for f->move(dwOffset, f->getSize() - dwOffset - dwDistanceToMove, dwDistanceToMove)
	bool flush();

	bool updatePEChkSum();				// flushes
	bool hasExtraData() const;
	pntr getExtraData(uint32_t *size);	// pointer can modify the file, when first enabling it will flush
	bool clearCertificateTable();		// may invalidate all pointers returned by functions, flushes
	PE::Version::Version getFileVersion() const;
	bool isAlreadyModified() const;
	bool setModifiedFlag();				// flushes
	bool removeRelocs(uint32_t start, uint32_t end, bool reverse = false);

#ifdef EXPOSE_DIRECT_RESOURCES
	Rsrc *getResources();
	const Rsrc *getResources() const;
#endif
	bool resourceExists(const_resid type, const_resid name, uint16_t lang) const;
	bool resourceExists(const_resid type, const_resid name, uint16_t* lang = NULL) const;
	pntr getResource   (const_resid type, const_resid name, uint16_t lang, size_t* size) const;  // must be freed
	pntr getResource   (const_resid type, const_resid name, uint16_t* lang, size_t* size) const; // must be freed
	bool removeResource(const_resid type, const_resid name, uint16_t lang);
	bool addResource   (const_resid type, const_resid name, uint16_t lang, const_pntr data, size_t size, Overwrite overwrite = ALWAYS);
	
	static pntr GetResourceDirect(pntr data, const_resid type, const_resid name); // must be freed, massively performance enhanced for a single retrieval, no editing, and no buffer checks // lang? size?
	static bool UpdatePEChkSum(bytes data, size_t dwSize, size_t peOffset, uint32_t dwOldCheck);
};

}

#endif