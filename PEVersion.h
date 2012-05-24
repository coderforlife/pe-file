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

// Implements a few utility functions for dealing with version information in PE files

#ifndef PE_VERSION_H
#define PE_VERSION_H

#include "PEDataTypes.h"

#include <map>
#include <string>

namespace PE { namespace Version {
	struct Version {
		Version();
		uint16_t Minor, Major, Revision, Build;
	};
	struct SmallVersion {
		SmallVersion();
		uint16_t Minor, Major;
	};
	struct LangAndCodePage {
		LangAndCodePage();
		uint16_t Language, CodePage;
		bool operator <(const LangAndCodePage& other) const; 
	};

	// This data is modifiable, changes in ver show up here and changes here show up in ver
	struct FileVersionBasicInfo {
		static const uint32_t SIGNATURE = 0xFEEF04BD;

		static FileVersionBasicInfo* Get(void* ver);

		uint32_t Signature;
		SmallVersion StrucVersion;
		Version FileVersion;
		Version ProductVersion;

		typedef enum _FileFlags : uint32_t { // FLAGS
			DEBUG        = 0x00000001,
			PRERELEASE   = 0x00000002,
			PATCHED      = 0x00000004,
			PRIVATEBUILD = 0x00000008,
			INFOINFERRED = 0x00000010,
			SPECIALBUILD = 0x00000020,
		} Flags;
		Flags FileFlagsMask;
		Flags FileFlags;

		typedef enum _OS : uint32_t { // FLAGS, kinda
			UNKNOWN_OS    = 0x00000000,

			DOS           = 0x00010000,
			OS216         = 0x00020000,
			OS232         = 0x00030000,
			NT            = 0x00040000,

			WINDOWS16     = 0x00000001,
			PM16          = 0x00000002,
			PM32          = 0x00000003,
			WINDOWS32     = 0x00000004,

			DOS_WINDOWS16 = 0x00010001,
			DOS_WINDOWS32 = 0x00010004,
			OS216_PM16    = 0x00020002,
			OS232_PM32    = 0x00030003,
			NT_WINDOWS32  = 0x00040004,
		} OS;
		OS FileOS;

		typedef enum _Type : uint32_t  {
			UNKNOWN_TYPE = 0x00000000,
			APP          = 0x00000001,
			DLL          = 0x00000002,
			DRV          = 0x00000003,
			FONT         = 0x00000004,
			VXD          = 0x00000005,
			STATIC_LIB   = 0x00000007,
		} Type;
		Type FileType;

		typedef enum _SubType : uint32_t {
			UNKNOWN_SUB_TYPE      = 0x00000000,

			DRV_PRINTER           = 0x00000001,
			DRV_KEYBOARD          = 0x00000002,
			DRV_LANGUAGE          = 0x00000003,
			DRV_DISPLAY           = 0x00000004,
			DRV_MOUSE             = 0x00000005,
			DRV_NETWORK           = 0x00000006,
			DRV_SYSTEM            = 0x00000007,
			DRV_INSTALLABLE       = 0x00000008,
			DRV_SOUND             = 0x00000009,
			DRV_COMM              = 0x0000000A,
			DRV_VERSIONED_PRINTER = 0x0000000C,

			FONT_RASTER           = 0x00000001,
			FONT_VECTOR           = 0x00000002,
			FONT_TRUETYPE         = 0x00000003,
		} SubType;
		SubType FileSubtype;

		uint64_t FileDate;
	};

	// This data is not modifiable (except the basic info), all strings are copies
	struct FileVersionInfo {
		FileVersionInfo(void* ver);

		FileVersionBasicInfo* Basic;

		typedef std::map<std::wstring, std::wstring> StringFileInfo;
		typedef std::map<LangAndCodePage, StringFileInfo> StringFileInfos;
		StringFileInfos Strings;
	};
} }

#endif
