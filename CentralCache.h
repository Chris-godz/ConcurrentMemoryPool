#pragma once
#include"Common.h"
#include"PageCache.h"
class CentralCache
{
public:
	static CentralCache& GetInstance()
	{
		return _SingleInstance;
	}
	Span* GetOneSpan(SpanList& list, size_t byte_size);
	size_t FetchRangeObj(void*& start,void*& end,size_t batch_num,size_t align_size);
	void ReleaseListToSpans(void* start, size_t size);
private:
	CentralCache()
	{

	}
	CentralCache(const CentralCache&) = delete;
	static CentralCache _SingleInstance;
	SpanList _spanlists[NFREELIST];
};