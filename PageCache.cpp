#include"PageCache.h"

PageCache PageCache::_SingleInstance;
Span* PageCache::NewSpan(size_t k)
{
	if (k > NPAGES - 1)
	{
		//此时申请的内存大于了1024*1024 bytes ,即1M ,超过PageCache维护的最大限制,此时我们直接去堆上申请内存
		void* ptr = SystemAlloc(k);
		Span* kspan = _Spanpool.New();
		kspan->_n = k;
		kspan->_pageId = (PAGEID)ptr >> PAGESHIFT;
		//对申请的span类建立头指针的映射关系,便于后续销毁工作
		_IdToSpan[kspan->_pageId] = kspan;
		return kspan;
	}
	if (!_Spanlists[k].empty())
	{
		Span* kspan = _Spanlists[k].pop_front();
		for (PAGEID i = 0; i < kspan->_n; i++)
		{
			_IdToSpan[kspan->_pageId + i] = kspan;
		}
		return kspan;
	}
	for (size_t n = k + 1; n < NPAGES; n++)
	{

		if (!_Spanlists[n].empty())
		{
			Span* nspan = _Spanlists[n].pop_front();
			Span* kspan = _Spanpool.New();

			kspan->_pageId = nspan->_pageId;
			kspan->_n = k;

			nspan->_pageId += k;
			nspan->_n -= k;
			// 存储nSpan的首位页号跟nSpan映射，方便page cache回收内存时
			// 进行的合并查找
			_IdToSpan[nspan->_pageId] = nspan;
			_IdToSpan[nspan->_pageId + nspan->_n - 1] = nspan;
			_Spanlists[nspan->_n].push_front(nspan);
			//在分发时建立IdToSpan的映射关系
			for (PAGEID i = 0; i < kspan->_n; i++)
			{
				_IdToSpan[kspan->_pageId + i] = kspan;
			}
			return kspan;
		}
	}
	// 走到这个位置就说明后面没有大页的span了
	// 这时就去找堆要一个128页的span
	Span* maxspan =  _Spanpool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	maxspan->_pageId = (PAGEID)ptr >> PAGESHIFT;
	maxspan->_n = NPAGES - 1;
	_Spanlists[maxspan->_n].push_front(maxspan);
	return NewSpan(k);
}
Span* PageCache::MapObjToSpan(void* obj)
{
	PAGEID id = (PAGEID)obj >> PAGESHIFT;
	std::unique_lock<std::mutex> mtx(_pagemtx);
	auto it = _IdToSpan.find(id);
	if (it != _IdToSpan.end())
	{
		return it->second;
	}
	else
	{
		int x = 0;
		assert(false);
		return nullptr;
	}
}
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//先判断销毁的span类型是不是大于能维护的最大值
	if (span->_n>NPAGES-1)
	{
		//直接调用系统接口销毁
		void* ptr = (void*)(span->_pageId<<PAGESHIFT);
		SystemFree(ptr);
		_Spanpool.Delete(span);
		return;
	}
	// 对span前后的页，尝试进行合并，缓解内存碎片问题
	while (1)
	{
		PAGEID prvid = span->_pageId - 1;
		auto prvit = _IdToSpan.find(prvid);
		if (prvit == _IdToSpan.end()|| prvit->second->_isused == true||(prvit->second->_n+span->_n)>NPAGES-1) //前页不在或在使用或合成后超过最大值
		{
			break;
		}
		span->_pageId = prvit->second->_pageId;
		span->_n += prvit->second->_n;
		_Spanlists[prvit->second->_n].erase(prvit->second);
		_Spanpool.Delete(prvit->second);
	}
	while (1)
	{
		PAGEID nxtid = span->_pageId +span->_n;
		auto nxtit = _IdToSpan.find(nxtid);
		if (nxtit == _IdToSpan.end() || nxtit->second->_isused == true || (nxtit->second->_n + span->_n) > NPAGES - 1) //后页不在或在使用或合成后超过最大值
		{
			break;
		}
		span->_pageId = nxtit->second->_pageId;
		span->_n += nxtit->second->_n;
		_Spanlists[nxtit->second->_n].erase(nxtit->second);
		_Spanpool.Delete(nxtit->second);
	}
	_Spanlists[span->_n].push_front(span);
	span->_isused = false;
	//此时从span->_pageId到span->_pageId + span->_n - 1页的所有Id在_IdToSpan的映射关系还没有更新,但没必要现在更新,在newspan时,会覆盖原来的更新
	//现在更新头尾是防止出现合成后再被别的span合成的情况(兼容我们的前后页合并接口)
	_IdToSpan[span->_pageId] = span;
	_IdToSpan[span->_pageId + span->_n - 1] = span;
}