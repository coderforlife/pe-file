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

#include "PEVersion.h"

#include <string.h>
#include <wchar.h>
#include <vector>

using namespace PE;
using namespace PE::Internal;
using namespace PE::Version;

PE::Version::Version::Version() : Minor(0), Major(0), Revision(0), Build(0) { }
SmallVersion::SmallVersion() : Minor(0), Major(0) { }
LangAndCodePage::LangAndCodePage() : Language(0), CodePage(0) { }
bool LangAndCodePage::operator <(const LangAndCodePage& other) const {
	return (this->Language == other.Language) ? this->CodePage < other.CodePage : this->Language < other.Language;
}

struct Block16 {
	uint16_t size;     // size including key, value, and children
	uint16_t val_size;
	char* key;
	bytes value;
	std::vector<Block16> children;
};
typedef std::vector<Block16>::iterator B16iter;

struct Block32 {
	uint16_t size;     // size including key, value, and children
	uint16_t val_size;
	uint16_t type;     // 0x0000 for a binary value, 0x0001 for a string value
	wchar_t* key;
	bytes value;
	std::vector<Block32> children;
};
typedef std::vector<Block32>::iterator B32iter;

static Block16 GetBlock16(const_pntr ver, bool recurse) {
	uint16_t* words = (uint16_t*)ver;

	Block16 b = { words[0], words[1], (char*)(words+2) };
	b.value = ((bytes)ver) + roundUpTo<sizeof(uint32_t)>(2 * sizeof(uint16_t) + (strlen(b.key) + 1) * sizeof(char));
	
	//if (b.key[0] < ' ') { /* error: actually a 32-bit block*/ }
	
	if (recurse) {
		bytes end = (bytes)ver+b.size;
		bytes child = b.value + roundUpTo<sizeof(uint32_t)>(b.val_size);
		while (child < end) {
			Block16 c = GetBlock16(child, true);
			b.children.push_back(c);
			child += c.size;
		}
	}

	return b;
}

static Block32 GetBlock32(const_pntr ver, bool recurse) {
	uint16_t* words = (uint16_t*)ver;

	Block32 b = { words[0], words[1], words[2], (wchar_t*)(words+3) };
	b.value = ((bytes)ver) + roundUpTo<sizeof(uint32_t)>(3 * sizeof(uint16_t) + (wcslen(b.key) + 1) * sizeof(wchar_t));
	
	//if (*((char*)&b.type) >= ' ') { /* error: actually a 16-bit block*/ }

	if (recurse) {
		bytes end = (bytes)ver+b.size;
		bytes child = b.value + roundUpTo<sizeof(uint32_t)>(b.val_size);
		while (child < end) {
			Block32 c = GetBlock32(child, true);
			b.children.push_back(c);
			child += c.size;
		}
	}

	return b;
}

static FileVersionBasicInfo *GetFileVersionBasicInfo(Block32 root) {
	if (wcscmp(root.key, L"VS_VERSION_INFO") != 0 || root.type != 0x0000 || root.val_size != 52) { return NULL; } // error!
	FileVersionBasicInfo *v = (FileVersionBasicInfo*)root.value;
	if (v->Signature != FileVersionBasicInfo::SIGNATURE || v->StrucVersion.Major != 1 || v->StrucVersion.Minor != 0) { return NULL; } // error!
	return v;
}
FileVersionBasicInfo *FileVersionBasicInfo::Get(pntr ver) { return ver ? GetFileVersionBasicInfo(GetBlock32(ver, false)) : NULL; }

FileVersionInfo::FileVersionInfo(pntr ver) : Basic(NULL) {
	if (ver == NULL) { return; }
	Block32 root = GetBlock32(ver, true);
	this->Basic = GetFileVersionBasicInfo(root);
	if (this->Basic == NULL) { return; } // error!

	for (B32iter i = root.children.begin(); i != root.children.end(); ++i) {
		if (wcscmp(i->key, L"StringFileInfo") == 0) {
			for (B32iter j = i->children.begin(); j != i->children.end(); ++j) {
				if (j->val_size == 0) {
					LangAndCodePage lcp; // TODO: convert j->key to LCP
					StringFileInfo& sfi = this->Strings[lcp];
					for (B32iter k = j->children.begin(); k != j->children.end(); ++k) {
						sfi[k->key] = std::wstring((wchar_t*)k->value, k->val_size / sizeof(wchar_t));
					}
				}
			}
		} else if (wcscmp(i->key, L"VarFileInfo") == 0) {
			for (B32iter j = i->children.begin(); j != i->children.end(); ++j) {
				if (wcscmp(j->key, L"Translation") == 0 && j->val_size == sizeof(LangAndCodePage)) {
					this->Strings.insert(make_pair(*(LangAndCodePage*)j->value, FileVersionInfo::StringFileInfo()));
				}
			}
		} else { /* Ignore the unknown block */ }
	}
}
