#include "ThreadCache.h"
#include "CentralCache.h"
void* ThreadCache::FetchFromCentralCache(size_t index, size_t align_size)
{
	// ����ʼ���������㷨
	// 1���ʼ����һ����central cacheһ������Ҫ̫�࣬��ΪҪ̫���˿����ò���
	// 2������㲻Ҫ���size��С�ڴ�������ôbatchNum�ͻ᲻��������ֱ������
	// 3��alignsizeԽ��һ����central cacheҪ��batchNum��ԽС
	// 4��alignsizeԽС��һ����central cacheҪ��batchNum��Խ��
	size_t batch_num = min( _FreeLists[index].MaxSize(), SizeTable::BatchSizeLimit(align_size) );
	if (_FreeLists[index].MaxSize() == batch_num)
	{
		_FreeLists[index].MaxSize()++;
	}
	void* start = nullptr;
	void* end = nullptr;
	size_t actual_num = CentralCache::GetInstance().FetchRangeObj(start,end,batch_num,align_size);
	assert(actual_num > 0);
	if (actual_num== 1)
	{
		assert(start == end);
		return start;
	}
	else
	{
		_FreeLists[index].push_range(*(void**)start,end); //�Ѷ�����Ĳ��ַŵ���Ӧ�Ŀ��ж�������
		return start;
	}
	return nullptr;
}
void* ThreadCache::allocate(size_t size)
{
	assert(size <= MAXSIZE);
	size_t align_size = SizeTable::RoundUp(size);
	size_t index = SizeTable::Index(size);
	if (!_FreeLists[index].empty())
	{
		return _FreeLists[index].pop();
	}
	else
	{
		return FetchFromCentralCache(index,align_size);
	}
}
inline void _FreeListTooLong(FreeList list, size_t size)
{
	//���Ǵ�_FreeList�г��MaxSize,������CentralCache
	void* start = nullptr;
	void* end = nullptr;
	list.pop_range(start, size);
	CentralCache::GetInstance().ReleaseListToSpans(start, size);
}
void ThreadCache::deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAXSIZE);
	size_t index = SizeTable::Index(size);
	_FreeLists[index].push(ptr);
	if (_FreeLists->size() > _FreeLists->MaxSize())
	{
		//_FreeList������
		_FreeListTooLong(_FreeLists[index], _FreeLists[index].MaxSize());
	}
}