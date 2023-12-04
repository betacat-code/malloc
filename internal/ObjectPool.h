#pragma once
#include<iostream>
#include"Common.h"

#ifdef _WIN32
#include<windows.h>
#else
// 包Linux相关头文件
#endif

template<class T>
class ObjectPool {
private:
	char* _memory = nullptr; // 指向大块内存的指针
	size_t _remain_bytes = 0; // 大块内存在切分过程中剩余字节数
	void* _free_list = nullptr; // 还回来过程中链接的自由链表的头指针
public:
	T* New() {
		T* obj = nullptr;
		//内存池存在空间 则直接拿 该链表上悬挂的内存空间一定够（指定了class类型）
		if (_free_list) {//头删
			//*((void**)_freeList)相当访取_freeList前4or8个字节操作
			//(由32位系统->指针4byte and 64位系统->指针8byte决定);
			void* next = *((void**)_free_list);
			obj = (T*)_free_list;
			_free_list = next;
		}
		else {
			if (_remain_bytes < sizeof(T)) {
				// 剩余内存不够一个对象大小时，则重新申请大块空间
				_remain_bytes = 128 * 1024;
				_memory = (char*)SystemAlloc(_remain_bytes >> 13);
				//char*类型内存可以使其+1or-1的单位操作为1字节，方便内存管理;
				if (_memory == nullptr) {
					throw std::bad_alloc();
				}
			}
			int obj_size = max(sizeof(T), sizeof(void*));
			obj = (T*)_memory;
			_memory += obj_size;
			_remain_bytes -= obj_size;
		}
		new(obj)T;
		return obj;
	}
	void Delete(T* obj) {
		obj->~T();
		*(void**)obj = _free_list;
		_free_list = obj;
	}
};
