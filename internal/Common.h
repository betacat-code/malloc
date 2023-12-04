#pragma once
#include <iostream>
#include <time.h>
#include <assert.h>
#include<unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
//inline 声明实现必须在一起

//size_t在32位系统下定义为：usingned int ,在64位系统下位unsigned long int
#ifdef _WIN64
typedef unsigned long long PAGE_ID;//64位精度 
#elif _WIN32
typedef size_t PAGE_ID;
#endif // 补充Linux


using std::cout;
using std::endl;
#include<windows.h>
//规定一页为8K
static const int MAX_BYTES = 256 * 1024;//ThreadCache中允许申请的最大字节;
static const size_t NFREELIST = 208;//通过对齐计算的thread和central中最大哈希桶下表;
static const size_t NPAGES = 129;//最多128页，为了方便映射哈希，从第1页开始计算;
static const size_t PAGE_SHIFT = 13;//以2^13字节对齐，并标上页号方便管理

inline static void* SystemAlloc(size_t kpage) {
	void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	//使用MEM_RESERVE,MEM_RESERVE|MEM_COMMIT将在大于页面大小的边界上保留或保留+提交页面
	//MEM_COMMIT 标志将在页面大小边界上提交页面

	if (ptr == nullptr) throw std::bad_alloc();
	return ptr;
}
inline static void SystemFree(void* ptr) {
	VirtualFree(ptr, 0, MEM_RELEASE);
}
static void*& NextObj(void* obj) {
	assert(obj);
	return *(void**)obj;//取obj前4or8个字节(存放下一个小空间地址的位置)进行操作
}


class FreeList {//小对象的管理链表
private:
	void* _free_list = nullptr;
	size_t _obj_size = 0;
	size_t _max_size = 1;//慢开始算法从1开始
public:
	inline void Push(void* obj) {
		assert(obj);
		NextObj(obj) = _free_list;
		//test();
		_free_list = obj;
		_obj_size += 1;
	}
	//测试链表断了没
	void test() {
		auto t = _free_list;
		for (size_t i = 0; i < _obj_size; i++) {
			if (NextObj(t) == nullptr&&i+1!= _obj_size) cout << t <<" &NextObj " << &NextObj(t) << "  " << i << " " << _obj_size << endl;
			t = NextObj(t);
		}
	}


	//从central中申请一批obj大小的内存块，range插入;
	inline void PushRange(void* start, void* end, size_t n) {
		//test();
		NextObj(end) = _free_list;// 这里断链了	
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
	//threadcache还一段list给central cache
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
		return _free_list == nullptr;//这里不用对象数量
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
	//通过_RoundUp计算对齐数
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
	//选择固定在的桶
	static inline size_t _Index(size_t bytes, size_t align_shift) {
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	//// 计算映射的哪一个自由链表桶
	static inline size_t Index(size_t bytes) {
		assert(bytes <= MAX_BYTES);
		static int group_array[4] = { 16, 56, 56, 56 };//每个区间的桶数量
		if (bytes <= 128) {
			return _Index(bytes, 3);//3表示按8byte
		}
		else if (bytes <= 1024) {
			return _Index(bytes - 128, 4) + group_array[0];
			//4表示按16byte对其， - 128因为前128个字节按照别的对齐规则的
			//剩下的这些按照自己的对其数对其最后+前面总共的桶数量即可计算自己的index;
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

// 一次thread cache从中心缓存获取多少个
//池化技术:当thread cache没有可用obj空间时，会向中心缓存申请一批而不是一个;
	static size_t NumMoveSize(size_t size) {
		//对象大 申请少
		//对象小 申请多
		int num = MAX_BYTES / size;
		if (num < 2) num = 2;
		if (num > 512) num = 512;
		return num;
	}

	static size_t NumMovePage(size_t size) {
		//计算一次向系统获取几个页;当centralCache对应index无可用span时
		//向pagecache按页大小申请，之后再把申请的span切割成n个index大小的obj内存块;
		size_t num = NumMoveSize(size);
		size_t npage = num * size;
		npage = npage >> PAGE_SHIFT;
		if (npage == 0) npage = 1;//不足一页申请一页
		return npage;
	}

};