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

// Implements the basic types, structures, functions, and constants used in many of the other files

#ifndef PE_DATA_TYPES_H
#define PE_DATA_TYPES_H

#define _CRT_SECURE_NO_WARNINGS

#ifdef _MSC_VER
#pragma warning(disable : 4201 4480) // nonstandard extension used: [nameless struct/union | specifying underlying type for enum]
#endif

#ifdef _WIN32
#define USE_WINDOWS_API
#elif !defined(__posix)
#error This code requires using either the Windows or POSIX APIs
#endif

#if defined(_MSC_VER) && _MSC_VER <= 1500
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
#else
#include <stdint.h>
#endif

#include <stdlib.h>

#define CASSERT(pred)	switch(0){case 0:case pred:;}

namespace PE {
	typedef uint8_t byte;
	typedef byte* bytes;
	typedef const byte* const_bytes;

	typedef wchar_t* str;
	typedef const wchar_t* const_str;

	typedef wchar_t* resid;
	typedef const wchar_t* const_resid;

	inline static bool IsIntResID(const_resid r) { return (((size_t)r) >> 16) == 0; }
	inline static resid MakeResID(uint16_t i) { return (resid)(size_t)i; }
	inline static uint16_t ResID2Int(const_resid r) { return (uint16_t)(size_t)r; }
	
	static const void*const null = NULL;

	// A class that acts as a pointer but automatically is updated when the underlying memory shifts
	template<typename T> class dyn_ptr;
	template<> class dyn_ptr<void>;
	template<typename T> class dyn_ptr_base {
		template<typename T2> friend class dyn_ptr;

	protected:
		void*const* base;
		size_t off;
		inline       T* get()       { return (      T*)((*(const_bytes*)this->base)+this->off); }
		inline const T* get() const { return (const T*)((*(const_bytes*)this->base)+this->off); }
	
		template<typename T2> friend class dyn_ptr_base;

		inline dyn_ptr_base() : base((void*const*)&null), off(0) { }
		inline dyn_ptr_base(void*const* base, size_t off = 0) : base(base), off(off) { }
		inline dyn_ptr_base(void*const* base, T* val)         : base(base), off((bytes)val - *(const_bytes*)base) { }

	public:
		inline operator       bool ()       { return this->get() != NULL; }
		inline operator       bool () const { return this->get() != NULL; }
		inline bool operator      !() const { return this->get() == NULL; }
		inline operator       T*   ()       { return this->get(); }
		inline operator const T*   () const { return this->get(); }

		inline bool equals(const dyn_ptr_base<T>& b) const { return this->base == b.base && this->off == b.off; }

		inline bool operator ==(const dyn_ptr_base<T>& b) const { return this->get() == b.get(); }
		inline bool operator !=(const dyn_ptr_base<T>& b) const { return this->get() != b.get(); }
		inline bool operator <=(const dyn_ptr_base<T>& b) const { return this->get() <= b.get(); }
		inline bool operator >=(const dyn_ptr_base<T>& b) const { return this->get() >= b.get(); }
		inline bool operator < (const dyn_ptr_base<T>& b) const { return this->get() <  b.get(); }
		inline bool operator > (const dyn_ptr_base<T>& b) const { return this->get() >  b.get(); }
		inline bool operator ==(const T* b) const { return this->get() == b; }
		inline bool operator !=(const T* b) const { return this->get() != b; }
		inline bool operator <=(const T* b) const { return this->get() <= b; }
		inline bool operator >=(const T* b) const { return this->get() >= b; }
		inline bool operator < (const T* b) const { return this->get() <  b; }
		inline bool operator > (const T* b) const { return this->get() >  b; }
		friend inline bool operator ==(const T* a, const dyn_ptr_base<T>& b) { return a == b.get(); }
		friend inline bool operator !=(const T* a, const dyn_ptr_base<T>& b) { return a != b.get(); }
		friend inline bool operator <=(const T* a, const dyn_ptr_base<T>& b) { return a <= b.get(); }
		friend inline bool operator >=(const T* a, const dyn_ptr_base<T>& b) { return a >= b.get(); }
		friend inline bool operator < (const T* a, const dyn_ptr_base<T>& b) { return a <  b.get(); }
		friend inline bool operator > (const T* a, const dyn_ptr_base<T>& b) { return a >  b.get(); }
		//TODO: inline bool operator ==(int b) const { return this->get() == b; }
		//TODO: inline bool operator !=(int b) const { return this->get() == b; }
		//TODO: friend inline bool operator ==(int a, const dyn_ptr_base<T>& b) { return a == b.get(); }
		//TODO: friend inline bool operator !=(int a, const dyn_ptr_base<T>& b) { return a != b.get(); }
	};
	template<typename T> class dyn_ptr : public dyn_ptr_base<T> {
	public:
		inline dyn_ptr() : dyn_ptr_base<T>() { }
		inline dyn_ptr(void*const* base, size_t off = 0) : dyn_ptr_base<T>(base, off) { }
		inline dyn_ptr(void*const* base, T* val)         : dyn_ptr_base<T>(base, val) { }
		                      inline dyn_ptr(const dyn_ptr<T>& b)                 : dyn_ptr_base<T>(b.base, b.off) { }
		template<typename T2> inline dyn_ptr(const dyn_ptr_base<T2>& b)           : dyn_ptr_base<T>(b.base, b.off) { }
		template<typename T2> inline dyn_ptr(const dyn_ptr_base<T2> base, T* val) : dyn_ptr_base<T>(base.base, val) { }
		                      inline dyn_ptr<T>& operator =(const dyn_ptr<T>& b)       { this->base = b.base; this->off = b.off; return *this; }
		template<typename T2> inline dyn_ptr<T>& operator =(const dyn_ptr_base<T2>& b) { this->base = b.base; this->off = b.off; return *this; }

		inline operator       void*()       { return this->get(); }
		inline operator const void*() const { return this->get(); }
		template<typename T2> inline operator       dyn_ptr<T2>()       { return dyn_ptr<T2>(this->base, this->off); }
		template<typename T2> inline operator const dyn_ptr<T2>() const { return dyn_ptr<T2>(this->base, this->off); }

		inline bool operator ==(const dyn_ptr_base<void>& b) const { return this->get() == b.get(); }
		inline bool operator !=(const dyn_ptr_base<void>& b) const { return this->get() != b.get(); }
		inline bool operator <=(const dyn_ptr_base<void>& b) const { return this->get() <= b.get(); }
		inline bool operator >=(const dyn_ptr_base<void>& b) const { return this->get() >= b.get(); }
		inline bool operator < (const dyn_ptr_base<void>& b) const { return this->get() <  b.get(); }
		inline bool operator > (const dyn_ptr_base<void>& b) const { return this->get() >  b.get(); }
		inline bool operator ==(const void* b) const { return this->get() == b; }
		inline bool operator !=(const void* b) const { return this->get() != b; }
		inline bool operator <=(const void* b) const { return this->get() <= b; }
		inline bool operator >=(const void* b) const { return this->get() >= b; }
		inline bool operator < (const void* b) const { return this->get() <  b; }
		inline bool operator > (const void* b) const { return this->get() >  b; }
		friend inline bool operator ==(const void* a, const dyn_ptr_base<T>& b) { return a == b.get(); }
		friend inline bool operator !=(const void* a, const dyn_ptr_base<T>& b) { return a != b.get(); }
		friend inline bool operator <=(const void* a, const dyn_ptr_base<T>& b) { return a <= b.get(); }
		friend inline bool operator >=(const void* a, const dyn_ptr_base<T>& b) { return a >= b.get(); }
		friend inline bool operator < (const void* a, const dyn_ptr_base<T>& b) { return a <  b.get(); }
		friend inline bool operator > (const void* a, const dyn_ptr_base<T>& b) { return a >  b.get(); }
		
		inline       T& operator *()        { return *this->get(); }
		inline const T& operator *() const  { return *this->get(); }
		inline       T* operator ->()       { return this->get();  }
		inline const T* operator ->() const { return this->get();  }
				
		inline ptrdiff_t operator -(const dyn_ptr<T>& b) const { return this->get() - b.get(); }
		inline ptrdiff_t operator -(const T* b)          const { return this->get() - b;       }
		inline ptrdiff_t operator -(      T* b)          const { return this->get() - b;       }
		friend inline ptrdiff_t operator -(const T* a, const dyn_ptr<T>& b) { return a - b.get(); }
		friend inline ptrdiff_t operator -(      T* a, const dyn_ptr<T>& b) { return a - b.get(); }

		inline dyn_ptr<T>& operator ++()    { this->off+=sizeof(T); return *this; }
		inline dyn_ptr<T>& operator --()    { this->off-=sizeof(T); return *this; }
		inline dyn_ptr<T>  operator ++(int) { dyn_ptr<T> p = dyn_ptr<T>(this->base, this->off); this->off+=sizeof(T); return p; }
		inline dyn_ptr<T>  operator --(int) { dyn_ptr<T> p = dyn_ptr<T>(this->base, this->off); this->off-=sizeof(T); return p; }
		
		// [] accessors for many integer sizes, both signed and unsigned
		#define ARRAY_ACCESSORS(I) \
			inline       T& operator [](const I& i)       { return this->get()[i]; } \
			inline const T& operator [](const I& i) const { return this->get()[i]; }
		ARRAY_ACCESSORS(size_t)
		ARRAY_ACCESSORS(ptrdiff_t)
		#if SIZE_MAX > 0xffffffffffffffff
			ARRAY_ACCESSORS(uint64_t)
			ARRAY_ACCESSORS(int64_t)
		#endif
		#if SIZE_MAX > 0xffffffff
			ARRAY_ACCESSORS(uint32_t)
			ARRAY_ACCESSORS(int32_t)
		#endif
		#if SIZE_MAX > 0xffff
			ARRAY_ACCESSORS(uint16_t)
			ARRAY_ACCESSORS(int16_t)
		#endif
		#if SIZE_MAX > 0xff
			ARRAY_ACCESSORS(uint8_t)
			ARRAY_ACCESSORS(int8_t)
		#endif
		#undef ARRAY_ACCESSORS
		
		// +/- operations for many integer sizes, both signed and unsigned
		#define PLUS_MINUS_OPS(I) \
			inline       dyn_ptr<T>  operator + (const I& off)       { return dyn_ptr<T>(this->base, this->off + sizeof(T)*off); } \
			inline const dyn_ptr<T>  operator + (const I& off) const { return dyn_ptr<T>(this->base, this->off + sizeof(T)*off); } \
			inline       dyn_ptr<T>  operator - (const I& off)       { return dyn_ptr<T>(this->base, this->off - sizeof(T)*off); } \
			inline const dyn_ptr<T>  operator - (const I& off) const { return dyn_ptr<T>(this->base, this->off - sizeof(T)*off); } \
			inline       dyn_ptr<T>& operator +=(const I& off)       { this->off += sizeof(T)*off; return *this; } \
			inline       dyn_ptr<T>& operator -=(const I& off)       { this->off -= sizeof(T)*off; return *this; }
		PLUS_MINUS_OPS(size_t)
		PLUS_MINUS_OPS(ptrdiff_t)
		#if SIZE_MAX > 0xffffffffffffffff
			PLUS_MINUS_OPS(uint64_t)
			PLUS_MINUS_OPS(int64_t)
		#endif
		#if SIZE_MAX > 0xffffffff
			PLUS_MINUS_OPS(uint32_t)
			PLUS_MINUS_OPS(int32_t)
		#endif
		#if SIZE_MAX > 0xffff
			PLUS_MINUS_OPS(uint16_t)
			PLUS_MINUS_OPS(int16_t)
		#endif
		#if SIZE_MAX > 0xff
			PLUS_MINUS_OPS(uint8_t)
			PLUS_MINUS_OPS(int8_t)
		#endif
		#undef PLUS_MINUS_OPS

		//TODO: are these possible?
		// ~ &  |  ^  <<  >>
		//   &= |= ^= <<= >>=
		// ->* () , new new[] delete delete[]
	};
	template<> class dyn_ptr<void> : public dyn_ptr_base<void> {
	public:
		inline dyn_ptr() : dyn_ptr_base<void>() { }
		inline dyn_ptr(void*const* base, size_t off = 0) : dyn_ptr_base<void>(base, off) { }
		inline dyn_ptr(void*const* base, void* val)      : dyn_ptr_base<void>(base, val) { }
		                      inline dyn_ptr(const dyn_ptr<void>& b)                 : dyn_ptr_base<void>(b.base, b.off) { }
		template<typename T2> inline dyn_ptr(const dyn_ptr_base<T2>& b)              : dyn_ptr_base<void>(b.base, b.off) { }
		template<typename T2> inline dyn_ptr(const dyn_ptr_base<T2> base, void* val) : dyn_ptr_base<void>(base.base, val) { }
		                      inline dyn_ptr<void>& operator =(const dyn_ptr<void>& b)    { this->base = b.base; this->off = b.off; return *this; }
		template<typename T2> inline dyn_ptr<void>& operator =(const dyn_ptr_base<T2>& b) { this->base = b.base; this->off = b.off; return *this; }

		template<typename T2> inline operator       dyn_ptr<T2>()       { return dyn_ptr<T2>(this->base, this->off); }
		template<typename T2> inline operator const dyn_ptr<T2>() const { return dyn_ptr<T2>(this->base, this->off); }
	};
	static const dyn_ptr<void> nulldp;

	enum Overwrite {
		ALWAYS, // always adds the resource, even if it already exists
		NEVER,  // only adds a resource is it does not already exist
		ONLY,   // only adds a resource if it will overwrite another resource
	};

	namespace Internal {
		#ifndef ARRAYSIZE
		#define ARRAYSIZE(a) sizeof(a)/sizeof(a[0])
		#endif

		template<unsigned int MULT> inline static size_t roundUpTo(size_t x) { size_t mod = x % MULT; return (mod == 0) ? x : (x + MULT - mod); }
		template<> inline size_t roundUpTo<2>(size_t x) { return (x + 1) & ~0x1; }
		template<> inline size_t roundUpTo<4>(size_t x) { return (x + 3) & ~0x3; }
		inline static size_t roundUpTo(size_t x, size_t mult) { size_t mod = x % mult; return (mod == 0) ? x : (x + mult - mod); }
	}

	static const unsigned int LARGE_PATH = 32767;

	namespace Image {
		#include "pshpack2.h"
		struct DOSHeader {      // IMAGE_DOS_HEADER - DOS .EXE header
			const static uint16_t SIGNATURE = 0x5A4D; // MZ

			uint16_t e_magic;    // Magic number
			uint16_t e_cblp;     // Bytes on last page of file
			uint16_t e_cp;       // Pages in file
			uint16_t e_crlc;     // Relocations
			uint16_t e_cparhdr;  // Size of header in paragraphs
			uint16_t e_minalloc; // Minimum extra paragraphs needed
			uint16_t e_maxalloc; // Maximum extra paragraphs needed
			uint16_t e_ss;       // Initial (relative) SS value
			uint16_t e_sp;       // Initial SP value
			uint16_t e_csum;     // Checksum
			uint16_t e_ip;       // Initial IP value
			uint16_t e_cs;       // Initial (relative) CS value
			uint16_t e_lfarlc;   // File address of relocation table
			uint16_t e_ovno;     // Overlay number
			uint16_t e_res[4];   // Reserved words
			uint16_t e_oemid;    // OEM identifier (for e_oeminfo)
			uint16_t e_oeminfo;  // OEM information; e_oemid specific
			uint16_t e_res2[10]; // Reserved words
			int32_t  e_lfanew;   // File address of new exe header
		};
		#include "poppack.h"
	
		//
		// File header format.
		//
		struct FileHeader { // IMAGE_FILE_HEADER
			enum CharacteristicFlags : uint16_t { // IMAGE_FILE_* - FLAGS
				RELOCS_STRIPPED           = 0x0001, // Relocation info stripped from file.
				EXECUTABLE_IMAGE          = 0x0002, // File is executable  (i.e. no unresolved external references).
				LINE_NUMS_STRIPPED        = 0x0004, // Line numbers stripped from file.
				LOCAL_SYMS_STRIPPED       = 0x0008, // Local symbols stripped from file.
				AGGRESIVE_WS_TRIM         = 0x0010, // Aggressively trim working set
				LARGE_ADDRESS_AWARE       = 0x0020, // App can handle >2gb addresses
				BYTES_REVERSED_LO         = 0x0080, // Bytes of machine word are reversed.
				MACHINE_32BIT             = 0x0100, // 32 bit word machine.
				DEBUG_STRIPPED            = 0x0200, // Debugging info stripped from file in .DBG file
				REMOVABLE_RUN_FROM_SWAP   = 0x0400, // If Image is on removable media, copy and run from the swap file.
				NET_RUN_FROM_SWAP         = 0x0800, // If Image is on Net, copy and run from the swap file.
				SYSTEM                    = 0x1000, // System File.
				DLL                       = 0x2000, // File is a DLL.
				UP_SYSTEM_ONLY            = 0x4000, // File should only be run on a UP machine
				BYTES_REVERSED_HI         = 0x8000, // Bytes of machine word are reversed.
			};
			enum MachineType : uint16_t { // IMAGE_FILE_MACHINE_*
				Unknown   = 0,
				I386      = 0x014c, // Intel 386.
				R3000     = 0x0162, // MIPS little-endian, 0x160 big-endian
				R4000     = 0x0166, // MIPS little-endian
				R10000    = 0x0168, // MIPS little-endian
				WCEMIPSV2 = 0x0169, // MIPS little-endian WCE v2
				ALPHA     = 0x0184, // Alpha_AXP
				SH3       = 0x01a2, // SH3 little-endian
				SH3DSP    = 0x01a3,
				SH3E      = 0x01a4, // SH3E little-endian
				SH4       = 0x01a6, // SH4 little-endian
				SH5       = 0x01a8, // SH5
				ARM       = 0x01c0, // ARM Little-Endian
				THUMB     = 0x01c2,
				AM33      = 0x01d3,
				POWERPC   = 0x01F0, // IBM PowerPC Little-Endian
				POWERPCFP = 0x01f1,
				IA64      = 0x0200, // Intel 64
				MIPS16    = 0x0266, // MIPS
				ALPHA64   = 0x0284, // ALPHA64
				MIPSFPU   = 0x0366, // MIPS
				MIPSFPU16 = 0x0466, // MIPS
				AXP64     = ALPHA64,
				TRICORE   = 0x0520, // Infineon
				CEF       = 0x0CEF,
				EBC       = 0x0EBC, // EFI Byte Code
				AMD64     = 0x8664, // AMD64 (K8)
				M32R      = 0x9041, // M32R little-endian
				CEE       = 0xC0EE,
			};

			MachineType Machine;
			uint16_t NumberOfSections;
			uint32_t TimeDateStamp;
			uint32_t PointerToSymbolTable;
			uint32_t NumberOfSymbols;
			uint16_t SizeOfOptionalHeader;
			CharacteristicFlags Characteristics;
		};

		//
		// Directory format.
		//
		struct DataDirectory { // IMAGE_DATA_DIRECTORY
			static const int NUMBER_OF_ENTRIES = 16;
			enum DirectoryEntryType { // IMAGE_DIRECTORY_ENTRY_*
				EXPORT         =  0, // Export Directory
				IMPORT         =  1, // Import Directory
				RESOURCE       =  2, // Resource Directory
				EXCEPTION      =  3, // Exception Directory
				SECURITY       =  4, // Security Directory
				BASERELOC      =  5, // Base Relocation Table
				DEBUG          =  6, // Debug Directory
				//COPYRIGHT      =  7, // (X86 usage)
				ARCHITECTURE   =  7, // Architecture Specific Data
				GLOBALPTR      =  8, // RVA of GP
				TLS            =  9, // TLS Directory
				LOAD_CONFIG    = 10, // Load Configuration Directory
				BOUND_IMPORT   = 11, // Bound Import Directory in headers
				IAT            = 12, // Import Address Table
				DELAY_IMPORT   = 13, // Delay Load Import Descriptors
				COM_DESCRIPTOR = 14, // COM Runtime descriptor
			};

			uint32_t VirtualAddress;
			uint32_t Size;
		};

		//
		// Optional header format.
		//
		struct OptionalHeader { // IMAGE_OPTIONAL_HEADER_*
			enum SubsystemType : uint16_t { // IMAGE_SUBSYSTEM_*
				UNKNOWN                  = 0,  // Unknown subsystem.
				NATIVE                   = 1,  // Image doesn't require a subsystem.
				WINDOWS_GUI              = 2,  // Image runs in the Windows GUI subsystem.
				WINDOWS_CUI              = 3,  // Image runs in the Windows character subsystem.
				OS2_CUI                  = 5,  // image runs in the OS/2 character subsystem.
				POSIX_CUI                = 7,  // image runs in the Posix character subsystem.
				NATIVE_WINDOWS           = 8,  // image is a native Win9x driver.
				WINDOWS_CE_GUI           = 9,  // Image runs in the Windows CE subsystem.
				EFI_APPLICATION          = 10,
				EFI_BOOT_SERVICE_DRIVER  = 11,
				EFI_RUNTIME_DRIVER       = 12,
				EFI_ROM                  = 13,
				XBOX                     = 14,
				WINDOWS_BOOT_APPLICATION = 16,
			};
			enum DllCharacteristicFlags : uint16_t { // IMAGE_DLLCHARACTERISTICS_* - FLAGS
				//IMAGE_LIBRARY_PROCESS_INIT = 0x0001, // Reserved.
				//IMAGE_LIBRARY_PROCESS_TERM = 0x0002, // Reserved.
				//IMAGE_LIBRARY_THREAD_INIT  = 0x0004, // Reserved.
				//IMAGE_LIBRARY_THREAD_TERM  = 0x0008, // Reserved.
				DYNAMIC_BASE          = 0x0040, // DLL can move.
				FORCE_INTEGRITY       = 0x0080, // Code Integrity Image
				NX_COMPAT             = 0x0100, // Image is NX compatible
				NO_ISOLATION          = 0x0200, // Image understands isolation and doesn't want it
				NO_SEH                = 0x0400, // Image does not use SEH.  No SE handler may reside in this image
				NO_BIND               = 0x0800, // Do not bind this image.
				//Reserved            = 0x1000,
				WDM_DRIVER            = 0x2000, // Driver uses WDM model
				//Reserved            = 0x4000,
				TERMINAL_SERVER_AWARE = 0x8000,
			};

			uint16_t Magic;
			byte     MajorLinkerVersion;
			byte     MinorLinkerVersion;
			uint32_t SizeOfCode;
			uint32_t SizeOfInitializedData;
			uint32_t SizeOfUninitializedData;
			uint32_t AddressOfEntryPoint;
			uint32_t BaseOfCode;
			union {
				struct
				{
					uint32_t BaseOfData;
					uint32_t ImageBase32;
				};
				uint64_t ImageBase64;
			};
			uint32_t SectionAlignment;
			uint32_t FileAlignment;
			uint16_t MajorOperatingSystemVersion;
			uint16_t MinorOperatingSystemVersion;
			uint16_t MajorImageVersion;
			uint16_t MinorImageVersion;
			uint16_t MajorSubsystemVersion;
			uint16_t MinorSubsystemVersion;
			uint32_t Win32VersionValue;
			uint32_t SizeOfImage;
			uint32_t SizeOfHeaders;
			uint32_t CheckSum;
			SubsystemType Subsystem;
			DllCharacteristicFlags DllCharacteristics;
		};
		struct OptionalHeader32 : OptionalHeader { // IMAGE_OPTIONAL_HEADER
			static const uint16_t SIGNATURE = 0x10b;
			uint32_t SizeOfStackReserve;
			uint32_t SizeOfStackCommit;
			uint32_t SizeOfHeapReserve;
			uint32_t SizeOfHeapCommit;
			uint32_t LoaderFlags;
			uint32_t NumberOfRvaAndSizes;
			PE::Image::DataDirectory DataDirectory[PE::Image::DataDirectory::NUMBER_OF_ENTRIES];
		};
		struct OptionalHeader64 : OptionalHeader { // IMAGE_OPTIONAL_HEADER64
			static const uint16_t SIGNATURE = 0x20b;
			uint64_t SizeOfStackReserve;
			uint64_t SizeOfStackCommit;
			uint64_t SizeOfHeapReserve;
			uint64_t SizeOfHeapCommit;
			uint32_t LoaderFlags;
			uint32_t NumberOfRvaAndSizes;
			PE::Image::DataDirectory DataDirectory[PE::Image::DataDirectory::NUMBER_OF_ENTRIES];
		};

		struct NTHeaders { // IMAGE_NT_HEADERS_*
			static const uint32_t SIGNATURE = 0x00004550; // PE00
			uint32_t Signature;
			PE::Image::FileHeader FileHeader;
		};
		struct NTHeaders64 : NTHeaders { // IMAGE_NT_HEADERS64
			OptionalHeader64 OptionalHeader;
		};
		struct NTHeaders32 : NTHeaders { // IMAGE_NT_HEADERS
			OptionalHeader32 OptionalHeader;
		};

		//
		// Section header format.
		//
		struct SectionHeader { // IMAGE_SECTION_HEADER
			enum CharacteristicFlags : uint32_t { // IMAGE_SCN_* - FLAGS
				//Reserved: TYPE_REG          = 0x00000000,
				//Reserved: TYPE_DSECT        = 0x00000001,
				//Reserved: TYPE_NOLOAD       = 0x00000002,
				//Reserved: TYPE_GROUP        = 0x00000004,
				TYPE_NO_PAD                   = 0x00000008,
				//Reserved: TYPE_COPY         = 0x00000010,

				CNT_CODE                   = 0x00000020, // Section contains code.
				CNT_INITIALIZED_DATA       = 0x00000040, // Section contains initialized data.
				CNT_UNINITIALIZED_DATA     = 0x00000080, // Section contains uninitialized data.
				
				LNK_OTHER                  = 0x00000100,
				LNK_INFO                   = 0x00000200, // Section contains comments or some other type of information.
				//Reserved: TYPE_OVER      = 0x00000400,
				LNK_REMOVE                 = 0x00000800, // Section contents will not become part of image.
				LNK_COMDAT                 = 0x00001000, // Section contents comdat.

				//Reserved:                = 0x00002000,
				//Obsolete: MEM_PROTECTED  = 0x00004000,
				NO_DEFER_SPEC_EXC          = 0x00004000, // Reset speculative exceptions handling bits in the TLB entries for this section.
				GPREL                      = 0x00008000, // Section content can be accessed relative to GP
				MEM_FARDATA                = 0x00008000,
				//Obsolete: MEM_SYSHEAP    = 0x00010000,
				MEM_PURGEABLE              = 0x00020000,
				MEM_16BIT                  = 0x00020000,
				MEM_LOCKED                 = 0x00040000,
				MEM_PRELOAD                = 0x00080000,
				
				ALIGN_1BYTES               = 0x00100000,
				ALIGN_2BYTES               = 0x00200000,
				ALIGN_4BYTES               = 0x00300000,
				ALIGN_8BYTES               = 0x00400000,
				ALIGN_16BYTES              = 0x00500000, // Default alignment if no others are specified.
				ALIGN_32BYTES              = 0x00600000,
				ALIGN_64BYTES              = 0x00700000,
				ALIGN_128BYTES             = 0x00800000,
				ALIGN_256BYTES             = 0x00900000,
				ALIGN_512BYTES             = 0x00A00000,
				ALIGN_1024BYTES            = 0x00B00000,
				ALIGN_2048BYTES            = 0x00C00000,
				ALIGN_4096BYTES            = 0x00D00000,
				ALIGN_8192BYTES            = 0x00E00000,
				//Unused:                  = 0x00F00000,
				ALIGN_MASK                 = 0x00F00000,

				LNK_NRELOC_OVFL            = 0x01000000, // Section contains extended relocations.
				MEM_DISCARDABLE            = 0x02000000, // Section can be discarded.
				MEM_NOT_CACHED             = 0x04000000, // Section is not cacheable.
				MEM_NOT_PAGED              = 0x08000000, // Section is not pageable.
				MEM_SHARED                 = 0x10000000, // Section is shareable.
				MEM_EXECUTE                = 0x20000000, // Section is executable.
				MEM_READ                   = 0x40000000, // Section is readable.
				MEM_WRITE                  = 0x80000000, // Section is writeable.

				TLS_SCALE_INDEX            = 0x00000001, // Tls index is scaled
			};

			byte     Name[8];
			uint32_t VirtualSize; // or PhysicalAddress
			uint32_t VirtualAddress;
			uint32_t SizeOfRawData;
			uint32_t PointerToRawData;
			uint32_t PointerToRelocations;
			uint32_t PointerToLinenumbers;
			uint16_t NumberOfRelocations;
			uint16_t NumberOfLinenumbers;
			uint32_t Characteristics;
		};
		
		//
		// Based relocation format.
		//
		struct BaseRelocation { // IMAGE_BASE_RELOCATION
			enum RelBase : uint16_t { // IMAGE_REL_BASED_*
				ABSOLUTE       = 0,
				HIGH           = 1,
				LOW            = 2,
				HIGHLOW        = 3,
				HIGHADJ        = 4,
				MIPS_JMPADDR   = 5,
				MIPS_JMPADDR16 = 9,
				IA64_IMM64     = 9,
				DIR64          = 10,
			};

			uint32_t VirtualAddress;
			uint32_t SizeOfBlock;
		//  uint16_t TypeOffset[1];
		};

		//
		// Resource Format.
		//
		struct ResourceDirectory { // IMAGE_RESOURCE_DIRECTORY
			uint32_t Characteristics;
			uint32_t TimeDateStamp;
			uint16_t MajorVersion;
			uint16_t MinorVersion;
			uint16_t NumberOfNamedEntries;
			uint16_t NumberOfIdEntries;
		//  IMAGE_RESOURCE_DIRECTORY_ENTRY DirectoryEntries[];
		};
		struct ResourceDirectoryEntry { // IMAGE_RESOURCE_DIRECTORY_ENTRY
			union {
				struct {
					uint32_t NameOffset:31;
					uint32_t NameIsString:1;
				};
				uint32_t Name;
				uint16_t Id;
			};
			union {
				uint32_t OffsetToData;
				struct {
					uint32_t OffsetToDirectory:31;
					uint32_t DataIsDirectory:1;
				};
			};
		};
		//struct ResourceDirectoryString { // IMAGE_RESOURCE_DIRECTORY_STRING
		//	uint16_t Length;
		//	char NameString[1];
		//};
		//struct ResourceDirectoryStringU { // IMAGE_RESOURCE_DIR_STRING_U
		//	uint16_t Length;
		//	wchar_t NameString[1];
		//};
		struct ResourceDataEntry { // IMAGE_RESOURCE_DATA_ENTRY
			uint32_t OffsetToData;
			uint32_t Size;
			uint32_t CodePage;
			uint32_t Reserved;
		};
	}

	namespace ResType {
		static const const_resid CURSOR       = MakeResID(1);
		static const const_resid BITMAP       = MakeResID(2);
		static const const_resid ICON         = MakeResID(3);
		static const const_resid MENU         = MakeResID(4);
		static const const_resid DIALOG       = MakeResID(5);
		static const const_resid STRING       = MakeResID(6);
		static const const_resid FONTDIR      = MakeResID(7);
		static const const_resid FONT         = MakeResID(8);
		static const const_resid ACCELERATOR  = MakeResID(9);
		static const const_resid RCDATA       = MakeResID(10);
		static const const_resid MESSAGETABLE = MakeResID(11);
		static const const_resid GROUP_CURSOR = MakeResID(12);
		static const const_resid GROUP_ICON   = MakeResID(14);
		static const const_resid VERSION      = MakeResID(16);
		static const const_resid DLGINCLUDE   = MakeResID(17);
		static const const_resid PLUGPLAY     = MakeResID(19);
		static const const_resid VXD          = MakeResID(20);
		static const const_resid ANICURSOR    = MakeResID(21);
		static const const_resid ANIICON      = MakeResID(22);
		static const const_resid HTML         = MakeResID(23);
		static const const_resid MANIFEST     = MakeResID(24);
	}
}

#endif