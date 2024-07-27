#include"PageCache.h"

PageCache PageCache::_SingleInstance;
Span* PageCache::NewSpan(size_t k)
{
	if (k > NPAGES - 1)
	{
		//��ʱ������ڴ������1024*1024 bytes ,��1M ,����PageCacheά�����������,��ʱ����ֱ��ȥ���������ڴ�
		void* ptr = SystemAlloc(k);
		Span* kspan = _Spanpool.New();
		kspan->_n = k;
		kspan->_pageId = (PAGEID)ptr >> PAGESHIFT;
		//�������span�ཨ��ͷָ���ӳ���ϵ,���ں������ٹ���
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
			// �洢nSpan����λҳ�Ÿ�nSpanӳ�䣬����page cache�����ڴ�ʱ
			// ���еĺϲ�����
			_IdToSpan[nspan->_pageId] = nspan;
			_IdToSpan[nspan->_pageId + nspan->_n - 1] = nspan;
			_Spanlists[nspan->_n].push_front(nspan);
			//�ڷַ�ʱ����IdToSpan��ӳ���ϵ
			for (PAGEID i = 0; i < kspan->_n; i++)
			{
				_IdToSpan[kspan->_pageId + i] = kspan;
			}
			return kspan;
		}
	}
	// �ߵ����λ�þ�˵������û�д�ҳ��span��
	// ��ʱ��ȥ�Ҷ�Ҫһ��128ҳ��span
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
	//���ж����ٵ�span�����ǲ��Ǵ�����ά�������ֵ
	if (span->_n>NPAGES-1)
	{
		//ֱ�ӵ���ϵͳ�ӿ�����
		void* ptr = (void*)(span->_pageId<<PAGESHIFT);
		SystemFree(ptr);
		_Spanpool.Delete(span);
		return;
	}
	// ��spanǰ���ҳ�����Խ��кϲ��������ڴ���Ƭ����
	while (1)
	{
		PAGEID prvid = span->_pageId - 1;
		auto prvit = _IdToSpan.find(prvid);
		if (prvit == _IdToSpan.end()|| prvit->second->_isused == true||(prvit->second->_n+span->_n)>NPAGES-1) //ǰҳ���ڻ���ʹ�û�ϳɺ󳬹����ֵ
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
		if (nxtit == _IdToSpan.end() || nxtit->second->_isused == true || (nxtit->second->_n + span->_n) > NPAGES - 1) //��ҳ���ڻ���ʹ�û�ϳɺ󳬹����ֵ
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
	//��ʱ��span->_pageId��span->_pageId + span->_n - 1ҳ������Id��_IdToSpan��ӳ���ϵ��û�и���,��û��Ҫ���ڸ���,��newspanʱ,�Ḳ��ԭ���ĸ���
	//���ڸ���ͷβ�Ƿ�ֹ���ֺϳɺ��ٱ����span�ϳɵ����(�������ǵ�ǰ��ҳ�ϲ��ӿ�)
	_IdToSpan[span->_pageId] = span;
	_IdToSpan[span->_pageId + span->_n - 1] = span;
}