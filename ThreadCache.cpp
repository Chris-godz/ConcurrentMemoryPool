#include "ThreadCache.h"
#include "CentralCache.h"
void* ThreadCache::FetchFromCentralCache(size_t index, size_t align_size)
{
	// 慢开始反馈调节算法
	// 1、最开始不会一次向central cache一次批量要太多，因为要太多了可能用不完
	// 2、如果你不要这个size大小内存需求，那么batchNum就会不断增长，直到上限
	// 3、alignsize越大，一次向central cache要的batchNum就越小
	// 4、alignsize越小，一次向central cache要的batchNum就越大
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
		_FreeLists[index].push_range(*(void**)start,end); //把多申请的部分放到对应的空闲队列里面
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
	//我们从_FreeList中抽回MaxSize,返还给CentralCache
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
		//_FreeList过长了
		_FreeListTooLong(_FreeLists[index], _FreeLists[index].MaxSize());
	}
}