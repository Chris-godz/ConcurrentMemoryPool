#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
static void* ConcurrentAlloc(size_t size)
{
	if(size > MAXSIZE)
	{
		//超过256KB的内存,ThreadCache中已经放不下了,直接申请Span来用
		size_t align_size = SizeTable::RoundUp(size);
		size_t kpage = align_size >> PAGESHIFT; //计算一共要去PageCache中去拿k页Span

		PageCache::GetInstance()._pagemtx.lock();
		Span* span = PageCache::GetInstance().NewSpan(kpage);
		span->_size = align_size;
		PageCache::GetInstance()._pagemtx.unlock();

		return (void*)(span->_pageId << PAGESHIFT);
	}
	if (pTLSThreadCache == nullptr)
	{
		static ObjMemoryPool<ThreadCache, 50> TcPool;
		pTLSThreadCache = TcPool.New();
	}
	return pTLSThreadCache->allocate(size);
}
static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance().MapObjToSpan(ptr);
	size_t size = span->_size;
	if (size > MAXSIZE)
	{
		//超过256KB的内存,ThreadCache中无法维护,都是以Span类型储存并维护
		PageCache::GetInstance()._pagemtx.lock();
		PageCache::GetInstance().ReleaseSpanToPageCache(span);
		PageCache::GetInstance()._pagemtx.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->deallocate(ptr, size);
	}
}	