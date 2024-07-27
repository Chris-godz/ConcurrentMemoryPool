#include"CentralCache.h"

CentralCache CentralCache::_SingleInstance;
Span* CentralCache::GetOneSpan(SpanList& list, size_t align_size)
{
	// �鿴��ǰ��spanlist���Ƿ��л���δ��������span
	Span* it = list.begin();
	while (it != list.end())
	{
		if (it->_freeList != nullptr)
		{
			it->_isused = true;
			return it;
		}
		else 
		{
			it=it->_next;
		}
	}                                                                       
	list._mtx.unlock();
	//û��һ���ֳ���ʹ�õ�span����,�����뼸��span
	PageCache::GetInstance()._pagemtx.lock();
	Span* span = PageCache::GetInstance().NewSpan(SizeTable::PageSizeLimit(align_size));
	PageCache::GetInstance()._pagemtx.unlock();
	//��span�кú�ҵ��Լ���_freelist��
	span->_size = align_size;
	void* start = (void*)(span->_pageId << PAGESHIFT);
	size_t bytes = span->_n << PAGESHIFT;
	void* end = (char*)start + bytes;
	span->_freeList = start;
	void* cur = start;
	void* tail = (char*)start + align_size;
	while (tail < end)
	{
		*(void**)cur = tail;
		cur = tail;
		tail =(char*)tail+ align_size;
	}
	list._mtx.lock();
	list.push_front(span);
	return span;
}
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batch_num, size_t align_size)
{
	size_t index = SizeTable::Index(align_size);
	_spanlists[index]._mtx.lock();
	Span* span = GetOneSpan(_spanlists[index], align_size);
	span->_isused = true;
	assert(span);
	assert(span->_freeList);

	size_t actual_num = 1;
	start = span->_freeList;
	end = start;
	while ( *(void**)end !=nullptr && actual_num < batch_num )
	{
		end = *(void**)end;
		++actual_num;
	}

	span->_freeList = *(void**)end;
	*(void**)end = nullptr;
	span->_used += actual_num;
	_spanlists[index]._mtx.unlock();
	return actual_num;

}
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	//���ҵ��ɷ���_FreeList��span��
	//�ù����л�Ķ�span����Ҫ����
	size_t index = SizeTable::Index(size);
	_spanlists[index]._mtx.lock();
	while (start)
	{
		Span* span=PageCache::GetInstance().MapObjToSpan(start);
		*(void**)start = span->_freeList;
		span->_freeList = start;
		span->_used--;
		if (span->_n == 0)
		{
			_spanlists->erase(span);
			span->_next = nullptr;
			span->_prv = nullptr;
			span->_freeList = nullptr;
			_spanlists[index]._mtx.unlock();

			PageCache::GetInstance()._pagemtx.lock();
			PageCache::GetInstance().ReleaseSpanToPageCache(span);
			PageCache::GetInstance()._pagemtx.unlock();

			_spanlists[index]._mtx.lock();
		}
		start = *(void**)start;
	}
	_spanlists[index]._mtx.unlock();
}
 