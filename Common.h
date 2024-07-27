#pragma once
#include<assert.h>
#include<iostream>
#include <thread>
#include<algorithm>
#include<mutex>
#include<unordered_map>
#include<ctime>
using std::cout;
using std::endl;

static const int NFREELIST = 208 ;
static const int MAXSIZE = 256 * 1024;    
static const size_t PAGESHIFT = 13;//指定一个PAGE的大小为13<<1bytes 既8*1024bytes.此时在32位下,有2^19<2^32;但在64位下有2^52<2^61
static const size_t NPAGES = 129;

//管理给定大小的内存链表
#ifdef _WIN64
	typedef long long PAGEID;
#elif _WIN32
	typedef size_t PAGEID;
#endif

#ifdef _WIN32
	#include <windows.h>
#endif
// 直接去堆上按页申请空间`
inline static void* SystemAlloc(size_t kpage)	{
#ifdef _WIN32
		void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
		if (ptr == nullptr)
			throw std::bad_alloc();

		return ptr;
}
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#endif
}



class FreeList
{
public:
	void push(void* obj) //头插
	{
		assert(obj);
		*(void**)obj = _FreeList;
		_FreeList = obj;
	}
	void push_range(void* start,void* end)
	{
		*(void**)end = _FreeList;
		_FreeList = start;
	}
	void* pop() //头删
	{
		assert(_FreeList);
		void* obj = _FreeList;
		_FreeList = *(void**)_FreeList;;
		return obj;
	}
	void pop_range(void*& start,size_t size)
	{
		start = _FreeList;
		void* end = start;
		for (int i = 0; i < size - 1; i++)
		{
			end = *(void**)end;
		}
		_FreeList = *(void**)end;
		*(void**)end = nullptr;
	}
	bool empty() 
	{
		return _FreeList == nullptr;
	}
	size_t& MaxSize()
	{
		return _MaxSize;
	}
	size_t size()
	{
		return _size;
	}
private:
	void* _FreeList = nullptr;
	size_t _MaxSize = 1;
	size_t _size = 0;
};
// 整体控制在最多10%左右的内碎片浪费
// [1,128]					8byte对齐	    freelist [0,16]
// [128+1,1024]				16byte对齐	    freelist [17,72]
// [1024+1,8*1024]			128byte对齐	    freelist [73,128]
// [8*1024+1,64*1024]		1024byte对齐     freelist[129,184]
// [64*1024+1,256*1024]		8*1024byte对齐   freelist[185,208]
//专门处理内存大小的桶映射计算
class SizeTable
{
public:
	static inline size_t _RoundUp(size_t bytes, size_t alignnum)
	{
		return ((bytes + alignnum - 1)& ~(alignnum-1));
	}
	static size_t RoundUp(size_t bytes) //处理内存对齐问题  
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes,8);
		}
		else if (bytes <= 1024)
		{
			return _RoundUp(bytes, 16);
		}
		else if (bytes <= 8 * 1024)
		{
			return _RoundUp(bytes, 128);
		}
		else if (bytes <= 64 * 1024)
		{
			return _RoundUp(bytes, 1024);
		}
		else if(bytes <= 256*1024)
		{
			return _RoundUp(bytes, 8*1024);
		}
		else
		{
			return _RoundUp(bytes, 1 << PAGESHIFT);
		}
	}
	static  size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1<<align_shift) - 1) >> align_shift)-1;
	}
	static size_t Index(size_t bytes) //  计算对应的桶坐标
	{

		assert(bytes <= MAXSIZE);
		if (bytes <= 128)
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes-128, 4)+16;
		}
		else if (bytes <= 8 * 1024)
		{
			return _Index(bytes-1024, 7)+72;
		}
		else if (bytes <= 64 * 1024)
		{
			return _Index(bytes-8*1024, 10)+128;
		}
		else
		{
			return _Index(bytes-64*1024, 13)+184;
		}
	}
	static size_t BatchSizeLimit(size_t size)
	{
		size_t maxnum = MAXSIZE / size;
		if (maxnum < 2)
		{
			maxnum = 2;
		}
		else if (maxnum > 512)
		{
			maxnum = 512;
		}
		return maxnum;
	}
	static size_t PageSizeLimit(size_t align_size)
	{
		size_t batch = BatchSizeLimit(align_size);
		size_t npage = batch * align_size;

		npage >>= PAGESHIFT;
		if (npage == 0)
			npage = 1;

		return npage;
	}
};

struct Span
{
	PAGEID _pageId=0; 
	size_t _n = 0;
	Span* _next = nullptr;
	Span* _prv = nullptr;
	bool _isused = false;
	size_t _used = 0;
	void* _freeList = nullptr;
	size_t _size = 0;
};

class SpanList //带头双向循环链表
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prv = _head;
	}
	bool empty()
	{
		return _head->_next == _head;
	}
	Span* begin()
	{
		return _head->_next;
	}
	Span* end()
	{
		return _head;
	}
	void push_front(Span* span)
	{
		insert(_head->_next, span);
	}
	Span* pop_front()
	{
		return erase(_head->_next);
	}
	void insert(Span* pos, Span* span)
	{
		//插在pos前面
		span->_next = pos;
		span->_prv = pos->_prv;
		pos->_prv->_next = span;
		pos->_prv = span;
	}
	Span* erase(Span* pos)
	{
		pos->_next->_prv = pos->_prv;
		pos->_prv->_next = pos->_next;
		return pos;
	}
	std::mutex _mtx; // 桶锁
private:
	Span* _head;
};