#pragma once
#include<iostream>
#include"Common.h"

#ifdef _WIN32
#include<windows.h>
#else
// ��Linux���ͷ�ļ�
#endif

template<class T>
class ObjectPool {
private:
	char* _memory = nullptr; // ָ�����ڴ��ָ��
	size_t _remain_bytes = 0; // ����ڴ����зֹ�����ʣ���ֽ���
	void* _free_list = nullptr; // ���������������ӵ����������ͷָ��
public:
	T* New() {
		T* obj = nullptr;
		//�ڴ�ش��ڿռ� ��ֱ���� �����������ҵ��ڴ�ռ�һ������ָ����class���ͣ�
		if (_free_list) {//ͷɾ
			//*((void**)_freeList)�൱��ȡ_freeListǰ4or8���ֽڲ���
			//(��32λϵͳ->ָ��4byte and 64λϵͳ->ָ��8byte����);
			void* next = *((void**)_free_list);
			obj = (T*)_free_list;
			_free_list = next;
		}
		else {
			if (_remain_bytes < sizeof(T)) {
				// ʣ���ڴ治��һ�������Сʱ��������������ռ�
				_remain_bytes = 128 * 1024;
				_memory = (char*)SystemAlloc(_remain_bytes >> 13);
				//char*�����ڴ����ʹ��+1or-1�ĵ�λ����Ϊ1�ֽڣ������ڴ����;
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
