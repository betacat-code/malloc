#pragma once
#include <iostream>
#include <time.h>
#include <assert.h>
#include<unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
//inline ����ʵ�ֱ�����һ��

//size_t��32λϵͳ�¶���Ϊ��usingned int ,��64λϵͳ��λunsigned long int
#ifdef _WIN64
typedef unsigned long long PAGE_ID;//64λ���� 
#elif _WIN32
typedef size_t PAGE_ID;
#endif // ����Linux


using std::cout;
using std::endl;
#include<windows.h>
//�涨һҳΪ8K
static const int MAX_BYTES = 256 * 1024;//ThreadCache���������������ֽ�;
static const size_t NFREELIST = 208;//ͨ����������thread��central������ϣͰ�±�;
static const size_t NPAGES = 129;//���128ҳ��Ϊ�˷���ӳ���ϣ���ӵ�1ҳ��ʼ����;
static const size_t PAGE_SHIFT = 13;//��2^13�ֽڶ��룬������ҳ�ŷ������

inline static void* SystemAlloc(size_t kpage) {
	void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	//ʹ��MEM_RESERVE,MEM_RESERVE|MEM_COMMIT���ڴ���ҳ���С�ı߽��ϱ�������+�ύҳ��
	//MEM_COMMIT ��־����ҳ���С�߽����ύҳ��

	if (ptr == nullptr) throw std::bad_alloc();
	return ptr;
}
inline static void SystemFree(void* ptr) {
	VirtualFree(ptr, 0, MEM_RELEASE);
}
static void*& NextObj(void* obj) {
	assert(obj);
	return *(void**)obj;//ȡobjǰ4or8���ֽ�(�����һ��С�ռ��ַ��λ��)���в���
}


class FreeList {//С����Ĺ�������
private:
	void* _free_list = nullptr;
	size_t _obj_size = 0;
	size_t _max_size = 1;//����ʼ�㷨��1��ʼ
public:
	inline void Push(void* obj) {
		assert(obj);
		NextObj(obj) = _free_list;
		//test();
		_free_list = obj;
		_obj_size += 1;
	}
	//�����������û
	void test() {
		auto t = _free_list;
		for (size_t i = 0; i < _obj_size; i++) {
			if (NextObj(t) == nullptr&&i+1!= _obj_size) cout << t <<" &NextObj " << &NextObj(t) << "  " << i << " " << _obj_size << endl;
			t = NextObj(t);
		}
	}


	//��central������һ��obj��С���ڴ�飬range����;
	inline void PushRange(void* start, void* end, size_t n) {
		//test();
		NextObj(end) = _free_list;// ���������	
		//cout << " _free_list " << _free_list;
		//cout << "PushRange " << end << " &NextObj(end) " << &NextObj(end) << " next " << NextObj(end) << endl;
		_free_list = start;
		_obj_size += n;
	}
	inline void* Pop() {
		assert(_free_list);
		void* obj = _free_list;
		_free_list = NextObj(_free_list);
		_obj_size--;
		return obj;
	}
	//threadcache��һ��list��central cache
	void PopRange(void*& start, void*& end, size_t n) {
		assert(_free_list);
		start = _free_list;
		end = start;

		for (size_t i = 1; i <= n-1; ++i) {
			end = NextObj(end);
		}
		_obj_size -= n;
		_free_list = NextObj(end);
		NextObj(end) = nullptr;
	}
	inline bool Empty() {
		return _free_list == nullptr;//���ﲻ�ö�������
	}
	inline size_t& maxsize() {
		return _max_size;
	}
	inline size_t size() {
		return _obj_size;
	}
};

class SizeAlignment {
public:
	static inline size_t _RoundUp(size_t bytes, size_t num) {
		return (bytes + num - 1) & ~(num - 1);
	}
	//ͨ��_RoundUp���������
	static inline size_t RoundUp(size_t size) {
		if (size <= 128) {
			return _RoundUp(size, 8);
		}
		else if (size <= 1024) {
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024) {
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024) {
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024) {
			return _RoundUp(size, 8 * 1024);
		}
		else {
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}
	//ѡ��̶��ڵ�Ͱ
	static inline size_t _Index(size_t bytes, size_t align_shift) {
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	//// ����ӳ�����һ����������Ͱ
	static inline size_t Index(size_t bytes) {
		assert(bytes <= MAX_BYTES);
		static int group_array[4] = { 16, 56, 56, 56 };//ÿ�������Ͱ����
		if (bytes <= 128) {
			return _Index(bytes, 3);//3��ʾ��8byte
		}
		else if (bytes <= 1024) {
			return _Index(bytes - 128, 4) + group_array[0];
			//4��ʾ��16byte���䣬 - 128��Ϊǰ128���ֽڰ��ձ�Ķ�������
			//ʣ�µ���Щ�����Լ��Ķ������������+ǰ���ܹ���Ͱ�������ɼ����Լ���index;
		}
		else if (bytes <= 8 * 1024) {
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 64 * 1024) {
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
		}
		else if (bytes <= 256 * 1024) {
			return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
		}
		else {
			assert(false);
		}
		return -1;
	}

// һ��thread cache�����Ļ����ȡ���ٸ�
//�ػ�����:��thread cacheû�п���obj�ռ�ʱ���������Ļ�������һ��������һ��;
	static size_t NumMoveSize(size_t size) {
		//����� ������
		//����С �����
		int num = MAX_BYTES / size;
		if (num < 2) num = 2;
		if (num > 512) num = 512;
		return num;
	}

	static size_t NumMovePage(size_t size) {
		//����һ����ϵͳ��ȡ����ҳ;��centralCache��Ӧindex�޿���spanʱ
		//��pagecache��ҳ��С���룬֮���ٰ������span�и��n��index��С��obj�ڴ��;
		size_t num = NumMoveSize(size);
		size_t npage = num * size;
		npage = npage >> PAGE_SHIFT;
		if (npage == 0) npage = 1;//����һҳ����һҳ
		return npage;
	}

};