#include "rpt_file.h"

CFileLoader::CFileLoader() {}

CFileLoader::~CFileLoader() {
	if (m_fileHandle != nullptr) close();

	if (m_cache != nullptr) {
		CAlignedSDLAllocator<char, sizeof(void*)>{}.deallocate(m_cache, m_bufferSize);
		m_cache = nullptr;
	}
}

bool CFileLoader::open(const char* path, const char* cAccessStr) {
	FILE* fp = fopen(path, cAccessStr); // only doing reads for now
	if (fp == nullptr) return false;

	m_fileHandle = fp;
	return true;
}

bool CFileLoader::close() {
	if (m_fileHandle == nullptr) return false;
	int result = fclose(m_fileHandle) == 0;
	m_fileHandle = nullptr;
	return result == 0;
}

const char* CFileLoader::read_file(usize* bytesRead) {
	if (m_cache != nullptr) {
		*bytesRead = m_bufferSize;
		return m_cache;
	}

	if (m_fileHandle == nullptr) return nullptr;

	usize bufferSize = 0;
    while(!feof(m_fileHandle)) {
        u8 c = fgetc(m_fileHandle);
        ++bufferSize;
    }
    --bufferSize; // to rewind eof read
    fseek(m_fileHandle, 0, 0);

    char* result = CAlignedSDLAllocator<char, sizeof(void*)>{}.allocate(bufferSize);
    if (result == nullptr) return nullptr;

    usize realBytesRead = fread(result, 1, bufferSize, m_fileHandle);
    Assert_(realBytesRead == bufferSize);

	m_cache = result;
	m_bufferSize = realBytesRead;

    *bytesRead = m_bufferSize;
    return m_cache;
}