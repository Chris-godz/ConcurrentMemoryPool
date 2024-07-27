#pragma once
#include"Common.h"
#include"ObjMemoryPool.h"
class PageCache
{
public:
	static PageCache& GetInstance()
	{
		return _SingleInstance;
	}
	void ReleaseSpanToPageCache(Span* span);
	Span* NewSpan(size_t k);
	Span* MapObjToSpan(void* obj);
	std::mutex _pagemtx;
private:
	PageCache()
	{

	}
	PageCache(const PageCache&) = delete;
	static PageCache  _SingleInstance;
	SpanList _Spanlists[NPAGES];
	std::unordered_map<PAGEID, Span*> _IdToSpan = std::unordered_map<PAGEID, Span*>();
	ObjMemoryPool<Span, 18000> _Spanpool= ObjMemoryPool<Span, 18000>(); 
};