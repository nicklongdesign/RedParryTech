#pragma once
#include "prelude.h"
#include "rpt_allocators.h"
#include <stdio.h>

class CFileLoader {
public:
	// enum class EAccessFlags : u32 {
	// 	Read =					0x00000001,
	// 	Write =					0x00000002,
	// 	Append =				0x00000004,
	// 	Binary =				0x00000008,
	// 	NoOverride =			0x00000010,
	// 	DefaultPermissions =	0x00000020
	// };

	CFileLoader();
	~CFileLoader();

	bool open(const char* path, const char* cAccessFlags);
	bool close();
	const char* read_file(usize* bytesRead);

	// Writing and more nuanced reading to-be bade as needed

private:
	FILE* m_fileHandle = nullptr;
	char* m_cache = nullptr;
	usize m_bufferSize = 0;
};