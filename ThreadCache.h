#pragma once
#include"common.h"
class ThreadCache
{
public:
	void* allocate(size_t size);
	void  deallocate(void* ptr, size_t size);
	void* FetchFromCentralCache(size_t index, size_t size);
private:
	FreeList _FreeLists[NFREELIST];
};

static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;