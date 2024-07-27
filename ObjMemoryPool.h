#pragma once
#include <iostream>
#include <vector>
#include <time.h>
#include"Common.h"
template<class T,int DefalutNum>
class ObjMemoryPool
{
public:
	ObjMemoryPool()
		:_Free_list(nullptr), _Pool(nullptr), _remainder(0)
	{}
	T* New()
	{
		if (_Free_list)
		{
			T* allocate = (T*)_Free_list;
			_Free_list = *(void**)_Free_list;
			new(allocate) T();
			return allocate;
		}
		if (_remainder < sizeof T)
		{
			_Pool = (char*)SystemAlloc((sizeof T * DefalutNum)>>13==0?1: (sizeof T * DefalutNum) >> 13);
			_remainder = sizeof T * DefalutNum;
			if (!_Pool)
			{
				throw std::bad_alloc();
			}
		}
		T* allocate = (T*)_Pool;
		size_t objsize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
		_Pool += objsize;
		_remainder -= objsize;
		// 定位new，显示调用T的构造函数初始化
		new(allocate) T();
		return allocate;
	}
	void Delete(T* obj)
	{
		obj->~T();
		*(void**)obj = _Free_list;
		_Free_list = obj;
	}
private:
	void* _Free_list;
	char* _Pool;
	size_t _remainder;
};